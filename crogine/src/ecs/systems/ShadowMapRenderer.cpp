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
}

ShadowMapRenderer::ShadowMapRenderer(MessageBus& mb)
    : System(mb, typeid(ShadowMapRenderer)),
    m_interval      (1)
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
    m_blurShaderA.loadFromString(SimpleDrawable::getDefaultVertexShader(), BlurPassFrag, "#define H\n#define B5\n");
    m_blurShaderB.loadFromString(SimpleDrawable::getDefaultVertexShader(), BlurPassFrag, "#define B5\n");

    m_inputQuad.setShader(m_blurShaderA);
    m_outputQuad.setShader(m_blurShaderB);

    cascadeUniform = m_blurShaderA.getUniformID("u_cascadeIndex");
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
        m_activeCameras.clear();
    }

    intervalCounter++;
}

void ShadowMapRenderer::updateDrawList(Entity camEnt)
{
    if (intervalCounter % m_interval)
    {
        return;
    }

    //this gets called once for each Camera in the CameraSystem
    //from CameraSystem::process() - so we'll check here if
    //the camera should actually be updated then render all the
    //cameras at once in process(), above

    //TODO this deals with splitting the camera frustum into
    //cascades (if more than one is set) but only applies to
    //desktop builds, as mobile shaders don't support this.
    //do we want to make sure to support this separately, or
    //just consign mobile support to the bin (we've never used
    //it, plus we should probably be targeting ES3 these days
    //anyway, which requires a COMPLETE overhaul).
    //Technically this works on mobile, but is wasted processing

    auto& camera = camEnt.getComponent<Camera>();
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


        //store the results here to use in frustum culling
        std::vector<glm::vec3> lightPositions;
        std::vector<Box> frustums;
#ifdef CRO_DEBUG_
        camera.lightCorners.clear();
#endif

        auto worldMat = camEnt.getComponent<cro::Transform>().getWorldTransform();
        auto corners = camera.getFrustumSplits();
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

        //use depth frusta to cull entities
        auto& entities = getEntities();
        for (auto& entity : entities)
        {
            if (!entity.getComponent<ShadowCaster>().active)
            {
                continue;
            }

            auto& model = entity.getComponent<Model>();
            if (model.isHidden())
            {
                continue;
            }

            if ((model.m_renderFlags & camera.getPass(Camera::Pass::Final).renderFlags) == 0)
            {
                continue;
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
                    drawList[i].emplace_back(entity, distance);
#else
                    //just place them all in the same draw list
                    drawList[0].emplace_back(entity, distance);
#endif
                }
            }
        }

        //sort back to front
        for (auto& cascade : drawList)
        {
            std::sort(cascade.begin(), cascade.end(),
                [](const ShadowMapRenderer::Drawable& a, const ShadowMapRenderer::Drawable& b)
                {
                    return a.distance > b.distance;
                });
        }

#ifdef CRO_DEBUG_
        //used for debug drawing of light positions
        camera.lightPositions.swap(lightPositions);
#endif
    }
}

//private
void ShadowMapRenderer::render()
{
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

        for (auto d = 0u; d < m_drawLists[c].size(); ++d)
        {
#ifdef PLATFORM_DESKTOP
            camera.shadowMapBuffer.clear(d);
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
                    CRO_ASSERT(mat.shader, "Missing Shadow Cast material.");

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

            camera.shadowMapBuffer.display();


            //TODO if blur enabled
            //for each cascade
            for (auto i = 0u; i < camera.m_blurPasses && m_drawLists[c].size(); ++i)
            {
                //TODO we could optimise this a bit by setting up the OpenGL explicitly for
                //the render quads - but only if this damages perf too much

                //render to internal buffer
                const auto passSize = camera.shadowMapBuffer.getSize();
                if (m_blurBuffer.getSize().x < passSize.x
                    || m_blurBuffer.getSize().y < passSize.y)
                {
                    m_blurBuffer.create(passSize.x, passSize.y);
                }
                //we create the internal buffer to the largest shadow map
                //we encounter - so we may also have smaller ones which render
                //to a sub-area of the the buffer.
                const auto bufferSize = m_blurBuffer.getSize();
                m_outputQuad.setTexture(m_blurBuffer.getTexture(), bufferSize);
                
                //we need to select the layer in the shader
                glUseProgram(m_blurShaderA.getGLHandle());
                glUniform1f(cascadeUniform, i);

                m_inputQuad.setTexture(camera.shadowMapBuffer.getTexture(), passSize);
                m_blurBuffer.clear();
                m_inputQuad.draw();
                m_blurBuffer.display();

                //render back to shadowmap
                camera.shadowMapBuffer.clear(i);
                m_outputQuad.draw();
                camera.shadowMapBuffer.display();
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
    if (entity.getComponent<cro::Model>().m_materials[Mesh::IndexData::Shadow][0].shader == 0)
    {
        LogW << "Shadow caster added to model with no shadow material. This will not render." << std::endl;
    }
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