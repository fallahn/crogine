/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2025
http://trederia.blogspot.com

crogine - Zlib license.

This software is provided 'as-is', without any express or
implied warranty.In no event will the authors be held
liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions :

1. The origin of this software must not be misrepresented;
you must not claim that you wrote the original software.
If you use this software in a product, an acknowledgment
in the product documentation would be appreciated but
is not required.

2. Altered source versions must be plainly marked as such,
and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any
source distribution.

-----------------------------------------------------------------------*/

#include <crogine/ecs/systems/ShadowMapRenderer.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/ShadowCaster.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Skeleton.hpp>
#include <crogine/ecs/Scene.hpp>

#include <crogine/graphics/Spatial.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/util/Frustum.hpp>

#include "../../detail/GLCheck.hpp"

#include <crogine/detail/glm/gtc/type_ptr.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/glm/gtc/matrix_inverse.hpp>
#include <crogine/detail/glm/gtc/matrix_access.hpp>
#include <crogine/detail/glm/gtx/quaternion.hpp>

#ifdef PARALLEL_DISABLE
#undef USE_PARALLEL_PROCESSING
#endif

#ifdef USE_PARALLEL_PROCESSING
#include <execution>
#include <mutex>
#endif

#ifdef CRO_DEBUG_
#include <crogine/gui/Gui.hpp>
#endif

using namespace cro;

namespace
{
    const std::string BlurPassFrag =
R"(
uniform sampler2DArray u_texture;
uniform float u_cascadeIndex = 0;

VARYING_IN vec2 v_texCoord;

OUTPUT
void main()
{
    vec4 colour = vec4(0.0);
    vec2 uv = v_texCoord;
    vec2 resolution = textureSize(u_texture, 0).xy;
#if defined(H)
#if defined(B5)
    vec2 off1 = vec2(1.3333333333333333, 0.0) / resolution;
#else
    vec2 off1 = vec2(1.3846153846, 0.0) / resolution;
    vec2 off2 = vec2(3.2307692308, 0.0) / resolution;
#endif

#else
#if defined(B5)
    vec2 off1 = vec2(0.0, 1.3333333333333333) / resolution;
#else
    vec2 off1 = vec2(0.0, 1.3846153846) / resolution;
    vec2 off2 = vec2(0.0, 3.2307692308) / resolution;
#endif
#endif

#if defined(B5)
    colour += texture(u_texture, vec3(uv, u_cascadeIndex)) * 0.29411764705882354;
    colour += texture(u_texture, vec3(uv + off1, u_cascadeIndex)) * 0.35294117647058826;
    colour += texture(u_texture, vec3(uv - off1, u_cascadeIndex)) * 0.35294117647058826;
#else
    colour += texture(u_texture, vec3(uv, u_cascadeIndex)) * 0.2270270270;
    colour += texture(u_texture, vec3(uv + off1, u_cascadeIndex)) * 0.3162162162;
    colour += texture(u_texture, vec3(uv - off1, u_cascadeIndex)) * 0.3162162162;
    colour += texture(u_texture, vec3(uv + off2, u_cascadeIndex)) * 0.0702702703;
    colour += texture(u_texture, vec3(uv - off2, u_cascadeIndex)) * 0.0702702703;
#endif
    FRAG_OUT = colour;
})";
    std::int32_t cascadeUniform = -1;

    std::uint32_t intervalCounter = 0;

    constexpr float CascadeOverlap = 0.5f;

    std::int32_t renderCount = 0;
}

ShadowMapRenderer::ShadowMapRenderer(MessageBus& mb)
    : System(mb, typeid(ShadowMapRenderer)),
    m_interval      (1),
    m_blurBuffer    (false),
    m_bufferIndices (MaxDepthMaps)
{
    requireComponent<Model>();
    requireComponent<Transform>();
    requireComponent<ShadowCaster>();

#ifdef CRO_DEBUG_
    addStats([&]()
        {
            for (auto i = 0u; i < m_drawLists.size(); ++i)
            {
                std::size_t visible = 0;
                for (const auto& cascade : m_drawLists[i])
                {
                    visible += cascade.size();
                }
                ImGui::Text("Visisble entities in shadow map for scene %lu, to Camera %lu: %lu", getScene()->getInstanceID(), i, visible);
            }
        });
#endif

    //TODO only create these if we get a request for a blur pass
    //TODO make B5/B9 defines optional (number of taps)
    m_blurShaderA.loadFromString(SimpleDrawable::getDefaultVertexShader(), BlurPassFrag, "#define H\n#define B9\n");
    m_blurShaderB.loadFromString(SimpleDrawable::getDefaultVertexShader(), BlurPassFrag, "#define B9\n");

    m_inputQuad.setShader(m_blurShaderA);
    m_outputQuad.setShader(m_blurShaderB);

    cascadeUniform = m_blurShaderA.getUniformID("u_cascadeIndex");

    std::iota(m_bufferIndices.rbegin(), m_bufferIndices.rend(), 0);

#ifdef CRO_DEBUG_
    registerWindow([&]() 
        {
            ImGui::Begin("Buffers");
            for (const auto& buff : m_bufferResources)
            {
                if (buff.depthTexture)
                {
                    const auto s = buff.depthTexture->getSize();
                    const auto l = buff.depthTexture->getLayerCount();
                    ImGui::Text("Width: %u, Height: %u, Layers: %u, Refcount: %u, Assign count: %u", s.x, s.y, l, buff.refcount, buff.useCount);
                }
                else
                {
                    ImGui::Text("Not Used");
                }
            }
            ImGui::Text("Render Count: %d", renderCount);

            ImGui::End();
        }, true);
#endif
}

//public
void ShadowMapRenderer::setMaxDistance(float)
{
    CRO_ASSERT(false, "Max shadow distance is set per-camera");
}

void ShadowMapRenderer::setNumCascades(std::int32_t count)
{
    CRO_ASSERT(false, "Cascade count is set by camera num splits");
}

void ShadowMapRenderer::process(float)
{
    //render here to ensure this only happens once per update
    //remember we might be rendering the Scene in multiple passes
    //which would call render() multiple times unnecessarily.
    if ((intervalCounter % m_interval) == 0)
    {
        render();

        //for each camera mark its resource as now being free
        //to use in the next call to updateDrawList()
        for (auto cam : m_activeCameras)
        {
            const auto& dmap = cam.getComponent<cro::Camera>().shadowMapBuffer;
            if (dmap.m_resourceIndex != -1)
            {
                //this *should* go back to zero as we iterate all cameras
                //if it doesn't it's a more obvious bug than just forcing it to 0...
                m_bufferResources[dmap.m_resourceIndex].useCount--;
            }
        }
        
        m_activeCameras.clear();
    }

    //check buffer resource for updated refs and remove any now at zero
    for (auto i = 0u; i< m_bufferResources.size(); ++i)
    {
        auto& buff = m_bufferResources[i];
        if (buff.gc
            && buff.refcount == 0)
        {
            //remove the texture
            buff.depthTexture.reset();

            //mark index as free
            m_bufferIndices.push_back(i);
        }
        buff.gc = false;
        //ideally we want to early-out but we can't
        //gaurantee unused resources are contiguous.
    }

    intervalCounter++;
}

void ShadowMapRenderer::updateDrawList(Entity camEnt)
{
    //this gets called once for each Camera in the CameraSystem
    //from CameraSystem::process() - so we'll check here if
    //the camera should actually be updated then render all the
    //cameras at once in process(), above
    auto& camera = camEnt.getComponent<Camera>();


    if ((intervalCounter % m_interval)
        && !camera.activatedThisFrame())
    {
        return;
    }

    //first we check the camera to see if the depth map has been
    //requested or created
    auto& dmap = camera.shadowMapBuffer;
    if (dmap.m_dirty)
    {
        //check if we already has an assignment and remove it, decrementing ref count
        if (dmap.m_resourceIndex != -1)
        {
            m_bufferResources[dmap.m_resourceIndex].refcount--;

            //mark the buffer as having its refcount changed - in process we use this to
            //see if we should garbage collect any textures which are now not referenced
            m_bufferResources[dmap.m_resourceIndex].gc = true;

#ifndef GL41
            dmap.m_textureViews.clear();
#endif
        }

        const auto layerCount = camera.getCascadeCount();
        dmap.m_layers = layerCount; //set this so that it returns the correct value from getLayerCount()

        //search the resource to see if we have and existing buffer we can use
        auto res = std::find_if(m_bufferResources.begin(), m_bufferResources.end(), 
            [&dmap, layerCount](const BufferResource& br)
            {
                return br.depthTexture 
                    && br.depthTexture->getSize() == dmap.m_size 
                    && br.depthTexture->getLayerCount() == layerCount;
            });

        std::int32_t index = -1;
        if (res == m_bufferResources.end())
        {
            //create a new one
            index = m_bufferIndices.back();
            m_bufferIndices.pop_back();

            m_bufferResources[index].depthTexture = std::make_unique<DepthTexture>(false);
            m_bufferResources[index].depthTexture->create(dmap.m_size.x, dmap.m_size.y, layerCount);
        }
        else
        {
            index = static_cast<std::int32_t>(std::distance(m_bufferResources.begin(), res));
        }

        //then assign it and increase the ref count.
        if (index != -1)
        {
            m_bufferResources[index].refcount++;
            dmap.m_textureID = m_bufferResources[index].depthTexture->getTexture();

#ifndef GL41
            for (auto i = 0u; i < layerCount; ++i)
            {
                dmap.m_textureViews.push_back(m_bufferResources[index].depthTexture->getTexture(i));
            }
#endif
        }
        dmap.m_resourceIndex = index; //assign this anyway if we didn't get one so we don't try rendering to it
        dmap.m_dirty = false;
    }


    //TODO this deals with splitting the camera frustum into
    //cascades (if more than one is set) but only applies to
    //desktop builds, as mobile shaders don't support this.
    //do we want to make sure to support this separately, or
    //just consign mobile support to the bin (we've never used
    //it, plus we should probably be targeting ES3 these days
    //anyway, which requires a COMPLETE overhaul).
    //Technically this works on mobile, but is wasted processing

    if (camera.shadowMapBuffer.available())
    {
        if (m_drawLists.size() <= m_activeCameras.size())
        {
            m_drawLists.emplace_back();
        }

        auto& drawList = m_drawLists[m_activeCameras.size()];
        drawList.clear();
        drawList.resize(camera.getCascadeCount());

        m_activeCameras.push_back(camEnt);
        m_bufferResources[camera.shadowMapBuffer.m_resourceIndex].useCount++;

        //TODO if useCount exceeds available texture count, increase textures
        const auto count = m_bufferResources[camera.shadowMapBuffer.m_resourceIndex].useCount;
        /*if (count > 1)
        {
            LogW << "Use count for depth tex is " << count << std::endl;
        }*/


        //store the results here to use in frustum culling
        std::vector<glm::vec3> lightPositions;
        std::vector<Box> frustums; //frustae
#ifdef CRO_DEBUG_
        camera.lightCorners.clear();
#endif

        const auto worldMat = camEnt.getComponent<cro::Transform>().getWorldTransform();
        auto corners = camera.getFrustumSplits(); //copy this as we'll transform it into world coords
        glm::vec3 lightDir = -getScene()->getSunlight().getComponent<Sunlight>().getDirection();

        for (auto i = 0u; i < corners.size(); ++i)
        {
            glm::vec3 centre = glm::vec3(0.f);

            for (auto& c : corners[i])
            {
                c = worldMat * c;
                centre += glm::vec3(c);
            }
            centre /= corners[i].size();

            //position light source
            auto lightPos = centre + lightDir;
            lightPositions.push_back(lightPos);

            const auto lightView = glm::lookAt(lightPos, centre, cro::Transform::Y_AXIS);
            camera.m_shadowViewMatrices[i] = lightView;

            //world coords to light space
            glm::vec3 minPos(std::numeric_limits<float>::max());
            glm::vec3 maxPos(std::numeric_limits<float>::lowest());

            for (const auto& c : corners[i])
            {
                const auto p = lightView * c;
                minPos.x = std::min(minPos.x, p.x);
                minPos.y = std::min(minPos.y, p.y);
                minPos.z = std::min(minPos.z, p.z);

                maxPos.x = std::max(maxPos.x, p.x);
                maxPos.y = std::max(maxPos.y, p.y);
                maxPos.z = std::max(maxPos.z, p.z);
            }
            
            //padding the X and Y allows some overlap of cascades
            //even when the light is perfectly parallel
            minPos.x -= CascadeOverlap;
            minPos.y -= CascadeOverlap;
            maxPos.x += CascadeOverlap;
            maxPos.y += CascadeOverlap;

            //skewing this means we end up with more depth resolution as we're nearer near plane
            //how much to skew is up for debate.
            maxPos.z += camera.m_shadowExpansion * 0.1f;
            minPos.z -= camera.m_shadowExpansion;

            const auto lightProj = glm::ortho(minPos.x, maxPos.x, minPos.y, maxPos.y, minPos.z, maxPos.z);
            camera.m_shadowProjectionMatrices[i] = lightProj;
            camera.m_shadowViewProjectionMatrices[i] = lightProj * lightView;

            frustums.emplace_back(minPos, maxPos);
#ifdef CRO_DEBUG_
            camera.lightCorners.emplace_back() =
            {
                //near
                glm::vec4(maxPos.x, maxPos.y, minPos.z, 1.f),
                glm::vec4(minPos.x, maxPos.y, minPos.z, 1.f),
                glm::vec4(minPos.x, minPos.y, minPos.z, 1.f),
                glm::vec4(maxPos.x, minPos.y, minPos.z, 1.f),
                //far
                glm::vec4(maxPos.x, maxPos.y, maxPos.z, 1.f),
                glm::vec4(minPos.x, maxPos.y, maxPos.z, 1.f),
                glm::vec4(minPos.x, minPos.y, maxPos.z, 1.f),
                glm::vec4(maxPos.x, minPos.y, maxPos.z, 1.f)
            };

#endif
        }

#ifdef USE_PARALLEL_PROCESSING
        std::mutex listMutex;
#endif
        //hmmm how do we make camera immutable from this point on?

        //use depth frusta to cull entities
        auto& entities = getEntities();
#ifdef USE_PARALLEL_PROCESSING
        std::for_each(std::execution::par, entities.cbegin(), entities.cend(),
            [&](Entity entity)
#else
        for (auto entity : entities)
#endif
        {
            if (!entity.getComponent<ShadowCaster>().active)
            {
#ifdef USE_PARALLEL_PROCESSING
                return;
#else
                continue;
#endif
            }

            const auto& model = entity.getComponent<Model>();
            if (model.isHidden())
            {
#ifdef USE_PARALLEL_PROCESSING
                return;
#else
                continue;
#endif
            }

            if ((model.m_renderFlags & camera.getPass(Camera::Pass::Final).renderFlags) == 0)
            {
#ifdef USE_PARALLEL_PROCESSING
                return;
#else
                continue;
#endif
            }

            const auto& tx = entity.getComponent<Transform>();
            auto sphere = model.getBoundingSphere();

            sphere.centre = glm::vec3(tx.getWorldTransform() * glm::vec4(sphere.centre, 1.f));
            auto scale = tx.getWorldScale();

            //if it's approaching zero scale then don't cast shadow
            /*if (scale.x * scale.y * scale.z < 0.01f)
            {
                continue;
            }*/

            sphere.radius *= ((scale.x + scale.y + scale.z) / 3.f);

            std::vector<std::pair<std::size_t, std::size_t>> drawListOffsets;

            for (auto i = 0u; i < camera.getCascadeCount(); ++i)
            {
                float distance = glm::dot(-lightDir, sphere.centre - lightPositions[i]);

                //put sphere into lightspace and do an AABB test on the ortho projection
                auto lightSphere = sphere;
                lightSphere.centre = glm::vec3(camera.m_shadowViewMatrices[i] * glm::vec4(lightSphere.centre, 1.f));
                
                if (frustums[i].intersects(lightSphere))
                {
#ifdef PLATFORM_DESKTOP
#ifdef USE_PARALLEL_PROCESSING
                    std::scoped_lock lock(listMutex);
#endif
                    drawList[i].emplace_back(entity, distance);
#else
                    //just place them all in the same draw list
                    drawList[0].emplace_back(entity, distance);
#endif
                }
            }
        }
#ifdef USE_PARALLEL_PROCESSING
            );
#endif

        //sort back to front
#ifdef USE_PARALLEL_PROCESSING
        std::for_each(std::execution::par, drawList.begin(), drawList.end(),
            [&](std::vector<ShadowMapRenderer::Drawable>& cascade)
        {
#ifdef _WIN32
                std::sort(std::execution::par, cascade.begin(), cascade.end(),
#else
                //gcc complains about no appropriate ctor - not sure why it needs a ctor for parallel sorting??
                std::sort(cascade.begin(), cascade.end(),
#endif
#else
        for (auto& cascade : drawList)
        {
            std::sort(cascade.begin(), cascade.end(),
#endif
                [](const ShadowMapRenderer::Drawable& a, const ShadowMapRenderer::Drawable& b)
                {
                    return a.distance > b.distance;
                });
        }
#ifdef USE_PARALLEL_PROCESSING
            );
#endif

#ifdef CRO_DEBUG_
        //used for debug drawing of light positions
        camera.lightPositions.swap(lightPositions);
#endif

        //if (camera.activatedThisFrame())
        //{
        //    //do an immediate update on the map
        //    render();
        //}
    }
}

//private
void ShadowMapRenderer::render()
{
#ifdef CRO_DEBUG_
    renderCount = 0;
#endif

    for (auto c = 0u; c < m_activeCameras.size(); c++)
    {
        auto& camera = m_activeCameras[c].getComponent<Camera>();
        auto cameraPosition = m_activeCameras[c].getComponent<cro::Transform>().getWorldPosition();
        const auto& camView = camera.getPass(Camera::Pass::Final).viewMatrix;

        //enable face culling and render rear faces
        //glCheck(glEnable(GL_CULL_FACE)); //this is now done per-material as some may be double sided
        glCheck(glCullFace(GL_BACK));
        //glCheck(glCullFace(GL_FRONT));
        glCheck(glEnable(GL_DEPTH_TEST));

        auto& shadowMapBuffer = *m_bufferResources[camera.shadowMapBuffer.m_resourceIndex].depthTexture;

        for (auto d = 0u; d < m_drawLists[c].size(); ++d)
        {
#ifdef PLATFORM_DESKTOP
            shadowMapBuffer.clear(d);
#else
            //this should only ever have one draw list so
            //clearing in this loop only happens once.
            camera.shadowMapBuffer.clear(cro::Colour::White());
#endif
            const auto& list = m_drawLists[c][d];
            for (const auto& [e, _] : list)
            {
                const auto& model = e.getComponent<Model>();

                glCheck(glFrontFace(model.m_facing));

                //calc entity transform
                const auto& tx = e.getComponent<Transform>();
                glm::mat4 worldMat = tx.getWorldTransform();
                glm::mat4 worldView = camera.m_shadowViewMatrices[d] * worldMat;

                //foreach submesh / material:

#ifndef PLATFORM_DESKTOP
                glCheck(glBindBuffer(GL_ARRAY_BUFFER, model.m_meshData.vbo));
#endif

                for (auto i = 0u; i < model.m_meshData.submeshCount; ++i)
                {
                    const auto& mat = model.m_materials[Mesh::IndexData::Shadow][i];
                    //CRO_ASSERT(mat.shader, "Missing Shadow Cast material.");
                    //some sub-meshes aren't written to the depth map if they don't receive shadows
                    if (mat.shader == 0)
                    {
                        continue;
                    }

                    //bind shader
                    glCheck(glUseProgram(mat.shader));

                    //apply shader uniforms from material
                    for (auto j = 0u; j < mat.optionalUniformCount; ++j)
                    {
                        switch (mat.optionalUniforms[j])
                        {
                        default: break;
                        case Material::Skinning:
                            glCheck(glUniformMatrix4fv(mat.uniforms[Material::Skinning], static_cast<GLsizei>(model.m_jointCount), GL_FALSE, &model.m_skeleton[0][0].r));
                            break;
                        }
                    }

                    //check material properties for alpha clipping
                    std::uint32_t currentTextureUnit = 0;
                    for (const auto& prop : mat.properties)
                    {
                        switch (prop.second.second.type)
                        {
                        default: break;
                        case Material::Property::TextureArray:
                            glCheck(glActiveTexture(GL_TEXTURE0 + currentTextureUnit));
                            glCheck(glBindTexture(GL_TEXTURE_2D_ARRAY, prop.second.second.textureID));
                            glCheck(glUniform1i(prop.second.first, currentTextureUnit++));
                            break;
                        case Material::Property::Texture:
                            glCheck(glActiveTexture(GL_TEXTURE0 + currentTextureUnit));
                            glCheck(glBindTexture(GL_TEXTURE_2D, prop.second.second.textureID));
                            glCheck(glUniform1i(prop.second.first, currentTextureUnit++));
                            break;
                        case Material::Property::Number:
                            glCheck(glUniform1f(prop.second.first, prop.second.second.numberValue));
                            break;
                        }
                    }

                    glCheck(glUniformMatrix4fv(mat.uniforms[Material::World], 1, GL_FALSE, glm::value_ptr(worldMat)));
                    glCheck(glUniformMatrix4fv(mat.uniforms[Material::WorldView], 1, GL_FALSE, glm::value_ptr(worldView)));
                    glCheck(glUniformMatrix4fv(mat.uniforms[Material::View], 1, GL_FALSE, glm::value_ptr(camera.m_shadowViewMatrices[d])));
                    glCheck(glUniformMatrix4fv(mat.uniforms[Material::CameraView], 1, GL_FALSE, glm::value_ptr(camView)));
                    glCheck(glUniformMatrix4fv(mat.uniforms[Material::Projection], 1, GL_FALSE, glm::value_ptr(camera.m_shadowProjectionMatrices[d])));
                    glCheck(glUniform3f(mat.uniforms[Material::Camera], cameraPosition.x, cameraPosition.y, cameraPosition.z));
                    //glCheck(glUniformMatrix4fv(mat.uniforms[Material::ViewProjection], 1, GL_FALSE, glm::value_ptr(camera.depthViewProjectionMatrix)));

                    glCheck((/*model.m_materials[Mesh::IndexData::Final][i].doubleSided ||*/ mat.doubleSided) ? glDisable(GL_CULL_FACE) : glEnable(GL_CULL_FACE));

#ifdef PLATFORM_DESKTOP
                    model.draw(i, Mesh::IndexData::Shadow);
#ifdef CRO_DEBUG_
                    renderCount++;
#endif
#else
                    //bind attribs
                    const auto& attribs = mat.attribs;
                    for (auto j = 0u; j < mat.attribCount; ++j)
                    {
                        glCheck(glEnableVertexAttribArray(attribs[j][Material::Data::Index]));
                        glCheck(glVertexAttribPointer(attribs[j][Material::Data::Index], attribs[j][Material::Data::Size],
                            GL_FLOAT, GL_FALSE, static_cast<GLsizei>(model.m_meshData.vertexSize),
                            reinterpret_cast<void*>(static_cast<intptr_t>(attribs[j][Material::Data::Offset]))));
                    }

                    //bind element/index buffer
                    const auto& indexData = model.m_meshData.indexData[i];
                    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexData.ibo));

                    //draw elements
                    glCheck(glDrawElements(static_cast<GLenum>(indexData.primitiveType), indexData.indexCount, static_cast<GLenum>(indexData.format), 0));

                    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

                    //unbind attribs
                    for (auto j = 0u; j < mat.attribCount; ++j)
                    {
                        glCheck(glDisableVertexAttribArray(attribs[j][Material::Data::Index]));
                    }
#endif //PLATFORM
                }

            }
            shadowMapBuffer.display();


            //if blur enabled
            //for each requested cascade
            for (auto i = 0u; i < camera.m_blurPasses && m_drawLists[c].size(); ++i)
            {
                //TODO we could optimise this a bit by setting up the OpenGL explicitly for
                //the render quads - but only if this damages perf too much

                //render to internal buffer
                const auto passSize = shadowMapBuffer.getSize();
                if (m_blurBuffer.getSize().x < passSize.x
                    || m_blurBuffer.getSize().y < passSize.y)
                {
                    m_blurBuffer.create(passSize.x, passSize.y);
                    //LogI << "Resized blur buffer" << std::endl;
                }
                //we create the internal buffer to the largest shadow map
                //we encounter - so we may also have smaller ones which render
                //to a sub-area of the the buffer.
                const auto bufferSize = m_blurBuffer.getSize();
                m_outputQuad.setTexture(m_blurBuffer.getTexture(), bufferSize);
                
                //we need to select the layer in the shader
                glUseProgram(m_blurShaderA.getGLHandle());
                glUniform1f(cascadeUniform, i);

                m_inputQuad.setTexture(shadowMapBuffer.getTexture(), passSize);
                m_blurBuffer.clear();
                m_inputQuad.draw();
                m_blurBuffer.display();

                //render back to shadowmap
                shadowMapBuffer.clear(i);
                m_outputQuad.draw();
                shadowMapBuffer.display();
            }
        }
#ifdef PLATFORM_DESKTOP
        glCheck(glBindVertexArray(0));
#else
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
#endif //PLATFORM

        //glCheck(glUseProgram(0));

        glCheck(glFrontFace(GL_CCW));
        glCheck(glDisable(GL_DEPTH_TEST));
        glCheck(glDisable(GL_CULL_FACE));
        //glCheck(glCullFace(GL_BACK));        
    }
}

void ShadowMapRenderer::onEntityAdded(cro::Entity entity)
{
    //hmm this warning is misleading as a sub-mesh other than 0 might still have a valid material
    //if (entity.getComponent<cro::Model>().m_materials[Mesh::IndexData::Shadow][0].shader == 0)
    //{
        //LogW << entity.getLabel() << std::endl;
        //LogW << "Shadow caster added to model with no shadow material. This will not render." << std::endl;
    //}
}

void ShadowMapRenderer::onEntityRemoved(Entity e)
{
    flushEntity(e);
}

void ShadowMapRenderer::flushEntity(Entity e)
{
    for (auto& a : m_drawLists)
    {
        for (auto& b : a)
        {
            //if (!b.empty())
            {
#ifdef USE_PARALLEL_PROCESSING
                b.erase(std::remove_if(std::execution::par, b.begin(), b.end(),
#else
                b.erase(std::remove_if(b.begin(), b.end(),
#endif
                    [e](const Drawable& d)
                    {
                        return d.entity == e;
                    }), b.end());
            }
        }
    }
}

#ifdef PARALLEL_DISABLE
#ifndef PARALLEL_GLOBAL_DISABLE
#define USE_PARALLEL_PROCESSING
#endif
#endif