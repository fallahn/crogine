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

#include "../../graphics/shaders/Unlit.hpp"
#include "../../graphics/shaders/VertexLit.hpp"
#include "../../graphics/shaders/PBR.hpp"

#include "../../detail/GLCheck.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/core/Console.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/util/Matrix.hpp>
#include <crogine/util/Frustum.hpp>

#if defined(CRO_DEBUG_) || defined(BENCHMARK)
#include <crogine/gui/Gui.hpp>
#endif

#include <crogine/detail/Assert.hpp>
#include <crogine/detail/glm/gtc/type_ptr.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/glm/gtc/matrix_inverse.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

//#define PARALLEL_DISABLE
#ifdef PARALLEL_DISABLE
#undef USE_PARALLEL_PROCESSING
#endif

#ifdef USE_PARALLEL_PROCESSING
#include <execution>
#include <mutex>
#endif

using namespace cro;

namespace
{
    float lightMultiplier = 1.f;
}
//void ModelRenderer::setLightMultiplier(float m) { lightMultiplier = m; }


ModelRenderer::ModelRenderer(MessageBus& mb)
    : System                (mb, typeid(ModelRenderer)),
    m_drawLists             (),
    m_pass                  (Mesh::IndexData::Final),
    m_lightUBO              ("LightUniforms")/*,
    m_tree          (1.f),
    m_useTreeQueries(false)*/
{
    requireComponent<Transform>();
    requireComponent<Model>();

#ifdef CRO_DEBUG_
    addStats([&]() 
        {
            for (auto i = 0u; i < m_drawLists.size(); ++i)
            {
                const auto sceneID = getScene()->getInstanceID();
                const auto& dList = m_drawLists[i];
                ImGui::Text("Visisble entities in scene %lu, to Camera %lu: %lu", sceneID, i, dList[0].size());
            }
        });
#endif

#ifdef BENCHMARK
    registerWindow([&]()
        {
            const std::string title = "Frame Time, Scene: " + std::to_string(getScene()->getInstanceID()) + " " + getScene()->getTitle();
            
            if (ImGui::Begin(title.c_str()))
            {
                for (auto i = 0u; i < m_benchmarks.size(); ++i)
                {
                    if ((m_benchmarks[i].index % 10) == 0)
                    {
                        m_benchmarks[i].avgTime = 0.f;
                        for (auto f : m_benchmarks[i].samples)
                        {
                            m_benchmarks[i].avgTime += f;
                        }
                        m_benchmarks[i].avgTime /= MaxBenchSamples;
                        m_benchmarks[i].avgTime *= 1000.f;
                    }
                    ImGui::Text("Avg render time for camera %u: %3.3f ms", i, m_benchmarks[i].avgTime);
                }
            }
            ImGui::End();
        });
#endif
}

//public
void ModelRenderer::updateDrawList(Entity cameraEnt)
{
    const auto& camComponent = cameraEnt.getComponent<Camera>();
    const auto camIndex = camComponent.getDrawListIndex();
    const auto passCount = camComponent.reflectionBuffer.available() ? 2 : 1;

    if (m_drawLists.size() <= camIndex)
    {
        m_drawLists.resize(camIndex + 1);

#ifdef BENCHMARK
        m_benchmarks.resize(m_drawLists.size());
#endif


#ifdef PLATFORM_DESKTOP
        m_cameraUBOs.resize(m_drawLists.size());
        for (auto& uboPair : m_cameraUBOs)
        {
            //hmm the current camera might only have a single pass, though we try creating ALL
            //currently required UBOs - without know which need a second pass, and which don't
            for (auto i = 0; i < /*passCount*/2; ++i) 
            {
                if (!uboPair[i])
                {
                    uboPair[i] = std::make_unique<UniformBuffer<CameraUniformBlock>>("CameraUniforms");

                    for (auto entity : getEntities())
                    {
                        const auto& model = entity.getComponent<Model>();
                        for (auto j = 0u; j < model.getMeshData().submeshCount; ++j)
                        {
                            const auto& mat = model.getMaterialData(Mesh::IndexData::Final, j);
                            if (mat.hasCameraUBO())
                            {
                                uboPair[i]->addShader(mat.shader);
                            }
                        }
                    }
                }
            }
        }
#endif
    }

    /*if (m_useTreeQueries)
    {
        updateDrawListBalancedTree(cameraEnt);
    }
    else*/
    {
        updateDrawListDefault(cameraEnt);
    }


    //DPRINT("Visible 3D ents in Scene " + std::to_string(getScene()->getInstanceID()) 
    //    + ", Camera " + std::to_string(cameraEnt.getIndex()), std::to_string(m_drawLists[camComponent.getDrawListIndex()][0].size()));

    //sort lists by depth
    //flag values make sure transparent materials are rendered last
    //with opaque going front to back and transparent back to front
    auto& drawList = m_drawLists[camIndex];
    for (auto i = 0; i < passCount; ++i)
    {
#ifdef PLATFORM_DESKTOP
        auto& ubo = m_cameraUBOs[camIndex][i];
        if (ubo->hasShaders())
        {
            const auto& pass = camComponent.getPass(i);

            CameraUniformBlock block;
            block.cameraWorldPosition = cameraEnt.getComponent<cro::Transform>().getWorldPosition();
            block.projectionMatrix = camComponent.getProjectionMatrix();
            block.viewMatrix = pass.viewMatrix;
            block.viewProjectionMatrix = pass.viewProjectionMatrix;
            block.clipPlane = glm::vec4(0.f, 1.f, 0.f, -getScene()->getWaterLevel() + (0.08f * pass.getClipPlaneMultiplier())) * pass.getClipPlaneMultiplier();

            ubo->setData(block);
        }
#endif


//apparently this won't work on gcc 9/10/11 (and should be disabled on lower versions anyway)
#ifndef _MSC_VER
#if __GNUC__ <= 14
#ifdef USE_PARALLEL_PROCESSING
#undef USE_PARALLEL_PROCESSING
#define GNUC_UNSUPPORTED
#endif //USE_PARALLEL_PROCESSING
#endif //__GNUC__
#endif //_MSC_VER

#if defined USE_PARALLEL_PROCESSING
        std::sort(std::execution::par, std::begin(drawList[i]), std::end(drawList[i]),
#else
        std::sort(std::begin(drawList[i]), std::end(drawList[i]),
#endif
            [](MaterialPair& a, MaterialPair& b)
            {
                return a.second.flags < b.second.flags;
            });
#ifdef GNUC_UNSUPPORTED
#define USE_PARALLEL_PROCESSING
#undef GNUC_UNSUPPORTED
#endif

    }
}

void ModelRenderer::process(float dt)
{
    m_lightUniforms.lightColour = getScene()->getSunlight().getComponent<Sunlight>().getColour().getVec4();
    m_lightUniforms.lightDirection = getScene()->getSunlight().getComponent<Sunlight>().getDirection();
    m_lightUBO.setData(m_lightUniforms);

    auto& entities = getEntities();

#ifdef USE_PARALLEL_PROCESSING
    std::for_each(std::execution::par, entities.cbegin(), entities.cend(),
        [&, dt](Entity entity)
#else
    for (auto entity : entities)
#endif
    {
        const auto& tx = entity.getComponent<cro::Transform>();

        auto& model = entity.getComponent<Model>();
        model.updateMaterialAnimations(dt);

        //this is used when updating the modelView mat for each
        //draw list, below, as well as during render()
        model.m_activeWorldMatrix = tx.getWorldTransform();
        model.m_activeNormalMatrix = glm::inverseTranspose(glm::mat3(model.m_activeWorldMatrix));



        /*if (m_useTreeQueries)
        {
            if (!entity.destroyed())
            {
                const auto& tx = entity.getComponent<Transform>();
                auto worldPosition = tx.getWorldPosition();
                auto worldBounds = model.getAABB();

                worldBounds += tx.getOrigin();
                worldBounds = tx.getWorldTransform() * worldBounds;

                m_tree.moveNode(model.m_treeID, worldBounds, worldPosition - model.m_lastWorldPosition);

                model.m_lastWorldPosition = worldPosition;
            }
        }*/
    }
#ifdef USE_PARALLEL_PROCESSING
    );
#endif

    //for each camera
    for (auto& drawList : m_drawLists)
    {
        //for each pass
        for (auto i = 0; i < 2; ++i)
        {
            auto& list = drawList[i];
#ifdef USE_PARALLEL_PROCESSING
            std::for_each(std::execution::par, list.begin(), list.end(),
                [&, dt](MaterialPair& pair)
#else
            for (MaterialPair& pair : list)
#endif
            {
                auto& [entity, sortData] = pair;
                sortData.worldViewMatrix = sortData.viewMatrix * entity.getComponent<cro::Model>().m_activeWorldMatrix;
            }
#ifdef USE_PARALLEL_PROCESSING
                );
#endif
        }
    }
}

void ModelRenderer::render(Entity camera, const RenderTarget& rt)
{
#ifdef BENCHMARK
    m_timer.restart();
#endif
    m_lightUBO.bind();
    
    const auto& camComponent = camera.getComponent<Camera>();
    const auto camIndex = camComponent.getDrawListIndex();
    if (camIndex < m_drawLists.size())
    {
        m_cameraUBOs[camIndex][camComponent.getActivePassIndex()]->bind();
        
        const auto& pass = camComponent.getActivePass();

        glm::vec4 clipPlane = glm::vec4(0.f, 1.f, 0.f, -getScene()->getWaterLevel() + (0.08f * pass.getClipPlaneMultiplier())) * pass.getClipPlaneMultiplier();

        const auto& camTx = camera.getComponent<Transform>();
        auto cameraPosition = camTx.getWorldPosition();
        auto screenSize = glm::vec2(rt.getSize());

        glCheck(glCullFace(pass.getCullFace()));


        //DPRINT("Render count", std::to_string(m_visibleEntities.size()));
        const auto& visibleEntities = m_drawLists[camIndex][camComponent.getActivePassIndex()];
        for (const auto& [entity, sortData] : visibleEntities)
        {
            //may have been marked for deletion - OK to draw but will trigger assert
#ifdef CRO_DEBUG_
            if (!entity.isValid())
            {
                continue;
            }
#endif

            //foreach submesh / material:
            const auto& model = entity.getComponent<Model>();
            glCheck(glFrontFace(model.m_facing));


#ifndef PLATFORM_DESKTOP
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, model.m_meshData.vbo));
#endif //PLATFORM

            for (auto i : sortData.matIDs)
            {
                const auto& material = model.m_materials[Mesh::IndexData::Final][i];
                const auto& uniforms = material.uniforms;

                //bind shader
                glCheck(glUseProgram(material.shader));

                //apply shader uniforms from material
                glCheck(glUniformMatrix4fv(uniforms[Material::WorldView], 1, GL_FALSE, glm::value_ptr(sortData.worldViewMatrix)));
                applyProperties(material, model, *getScene(), camComponent);

                //apply standard uniforms
                glCheck(glUniform2f(uniforms[Material::ScreenSize], screenSize.x, screenSize.y));
                
                if (!material.hasCameraUBO())
                {
                    glCheck(glUniform3f(uniforms[Material::Camera], cameraPosition.x, cameraPosition.y, cameraPosition.z));
                    glCheck(glUniformMatrix4fv(uniforms[Material::View], 1, GL_FALSE, glm::value_ptr(pass.viewMatrix)));
                    glCheck(glUniformMatrix4fv(uniforms[Material::ViewProjection], 1, GL_FALSE, glm::value_ptr(pass.viewProjectionMatrix)));
                    glCheck(glUniformMatrix4fv(uniforms[Material::Projection], 1, GL_FALSE, glm::value_ptr(camComponent.getProjectionMatrix())));
                    glCheck(glUniform4f(uniforms[Material::ClipPlane], clipPlane[0], clipPlane[1], clipPlane[2], clipPlane[3]));
                }
                glCheck(glUniformMatrix4fv(uniforms[Material::World], 1, GL_FALSE, glm::value_ptr(model.m_activeWorldMatrix)));
                glCheck(glUniformMatrix3fv(uniforms[Material::Normal], 1, GL_FALSE, glm::value_ptr(model.m_activeNormalMatrix)));

                applyBlendMode(material);

                //TODO move these to custom settings list
                glCheck(material.doubleSided ? glDisable(GL_CULL_FACE) : glEnable(GL_CULL_FACE));
                glCheck(material.enableDepthTest ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST));

                material.enableCustomSettings();

#ifdef PLATFORM_DESKTOP
                model.draw(i, Mesh::IndexData::Final);

#else //GLES 2 doesn't have VAO support without extensions

                //bind attribs
                const auto& attribs = model.m_materials[Mesh::IndexData::Final][i].attribs;
                for (auto j = 0u; j < model.m_materials[Mesh::IndexData::Final][i].attribCount; ++j)
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
                for (auto j = 0u; j < model.m_materials[Mesh::IndexData::Final][i].attribCount; ++j)
                {
                    glCheck(glDisableVertexAttribArray(attribs[j][Material::Data::Index]));
                }
#endif //PLATFORM 
                material.disableCustomSettings();
            }
        }

#ifdef PLATFORM_DESKTOP
        glCheck(glBindVertexArray(0));
#else
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
#endif //PLATFORM

        //glCheck(glUseProgram(0));

        glCheck(glFrontFace(GL_CCW));
        glCheck(glDisable(GL_BLEND));
        glCheck(glDisable(GL_CULL_FACE));
        glCheck(glDisable(GL_DEPTH_TEST));
        glCheck(glDepthMask(GL_TRUE)); //restore this else clearing the depth buffer fails
    }
#ifdef BENCHMARK
    if (m_benchmarks.size() <= camIndex) m_benchmarks.resize(camIndex+1); //hmmm we shouldn't need this
    m_benchmarks[camIndex].samples[m_benchmarks[camIndex].index] = m_timer.restart();
    m_benchmarks[camIndex].index = (m_benchmarks[camIndex].index + 1) % MaxBenchSamples;
#endif
}

std::size_t ModelRenderer::getVisibleCount(std::size_t cameraIndex, std::int32_t passIndex) const
{
    CRO_ASSERT(cameraIndex < m_drawLists.size(), "");
    switch (passIndex)
    {
    default: return 0;
    case Camera::Pass::Final:
    case Camera::Pass::Refraction:
        return m_drawLists[cameraIndex][Camera::Pass::Final].size();
    case Camera::Pass::Reflection:
        return m_drawLists[cameraIndex][Camera::Pass::Reflection].size();
    }
    return 0;
}

const std::string& ModelRenderer::getDefaultVertexShader(std::int32_t type)
{
    static const std::string defaultVal;
    switch (type)
    {
    default:
        LogW << type << ": Invalid VertexShaderID" << std::endl;
        return defaultVal;
    case VertexShaderID::Unlit:
        return Shaders::Unlit::Vertex;
    case VertexShaderID::VertexLit:
    case VertexShaderID::PBR:
        return Shaders::VertexLit::Vertex;
    }
    return defaultVal;
}

const std::string& ModelRenderer::getDefaultFragmentShader(std::int32_t type)
{
    static const std::string defaultVal;
    switch (type)
    {
    default: 
        LogW << type << ": Invalid FragmentShaderID" << std::endl;
        return defaultVal;
    case FragmentShaderID::Unlit:
        return Shaders::Unlit::Fragment;
    case FragmentShaderID::VertexLit:
        return Shaders::VertexLit::Fragment;
    case FragmentShaderID::PBR:
        return Shaders::PBR::Fragment;
    }

    return defaultVal;
}

void ModelRenderer::onEntityAdded(Entity entity)
{
    auto& model = entity.getComponent<Model>();
    model.updateBounds();

    //model.m_treeID = m_tree.addToTree(entity, model.getAABB());

#ifdef PLATFORM_DESKTOP
    
    //iterate materials and attempt to add to uniform buffer
    //UBO class automatically checks if this is valid for us.
    for (auto i = 0u; i < model.getMeshData().submeshCount; ++i)
    {
        const auto& mat = model.getMaterialData(Mesh::IndexData::Final, i);
        if (mat.hasCameraUBO())
        {    
            for (auto& uboPair : m_cameraUBOs)
            {
                for (auto& ubo : uboPair)
                {
                    if (ubo)
                    {
                        ubo->addShader(mat.shader);
                    }
                }
            }
        }

        if (mat.hasLightUBO())
        {
            m_lightUBO.addShader(mat.shader);
        }
    }

    //ofc this may all be undone if we change the material on the component...
    model.materialChangedCallback =
        [&](std::uint32_t oldShader, std::uint32_t newShader)
        {
            for (auto& uboPair : m_cameraUBOs)
            {
                for (auto& ubo : uboPair)
                {
                    if (ubo)
                    {
                        ubo->removeShader(oldShader);
                        ubo->addShader(newShader); //hmm this might add a shader which doesn't actually support this block
                    }
                }
            }

            //plus we can't assume that just because it has a camera ubo
            //that it also uses lighting...
            m_lightUBO.removeShader(oldShader);
            m_lightUBO.addShader(newShader);
        };
#endif
}

void ModelRenderer::onEntityRemoved(Entity entity)
{
    //m_tree.removeFromTree(entity.getComponent<Model>().m_treeID);

#ifdef PLATFORM_DESKTOP
    //remove any materials from camera UBOs
    const auto& model = entity.getComponent<Model>();
    for (auto i = 0u; i < model.getMeshData().submeshCount; ++i)
    {
        for (auto& uboPair: m_cameraUBOs)
        {
            for (auto& ubo : uboPair)
            {
                if (ubo)
                {
                    ubo->removeShader(model.getMaterialData(Mesh::IndexData::Final, i).shader);
                }
            }
        }
        m_lightUBO.removeShader(model.getMaterialData(Mesh::IndexData::Final, i).shader);
    }
#endif
}

//private
void ModelRenderer::updateDrawListDefault(Entity cameraEnt)
{
    const auto& camComponent = cameraEnt.getComponent<Camera>();
    const auto cameraPos = cameraEnt.getComponent<Transform>().getWorldPosition();
    //assume if there's no reflection buffer there's no need to sort the
    //entities for the second pass...
    const auto passCount = camComponent.reflectionBuffer.available() ? 2 : 1;

    const auto& entities = getEntities();
    auto& drawList = m_drawLists[camComponent.getDrawListIndex()];

    //cull entities by viewable into draw lists by pass
    for (auto& list : drawList)
    {
        list.clear();
    }
#ifdef USE_PARALLEL_PROCESSING
    std::mutex mutex;

    std::for_each(std::execution::par, entities.cbegin(), entities.cend(), 
        [&](Entity entity)
#else
    for (auto entity : entities)
#endif
    {
        auto& model = entity.getComponent<Model>();
        if (model.isHidden())
        {
            EARLY_OUT;
        }

        if (model.m_meshBox != model.m_meshData.boundingBox)
        {
            model.updateBounds();
        }

        //use the bounding sphere for depth testing
        auto sphere = model.getBoundingSphere();
        const auto& tx = entity.getComponent<Transform>();

        sphere.centre = glm::vec3(tx.getWorldTransform() * glm::vec4(sphere.centre, 1.f));
        auto scale = tx.getWorldScale();

        /*if (scale.x * scale.y * scale.z == 0)
        {
            continue;
        }*/

        sphere.radius *= ((scale.x + scale.y + scale.z) / 3.f);

        //for some reason the tighter fitting Spheres cause incorrect culling
        //so this is a hack to mitigate it somewhat
        sphere.radius *= 1.2f;

        //for each pass in the list (different passes may use different projections, eg reflections)
        for (auto p = 0; p < passCount; ++p)
        {
            if ((model.m_renderFlags & camComponent.getPass(p).renderFlags) == 0)
            {
                continue;
            }
            
            //this is a good approximation of distance based on the centre
            //of the model (large models might suffer without face sorting...)
            //assuming the forward vector is normalised - though WHY would you
            //scale the view matrix???
            auto direction = (sphere.centre - cameraPos);
            float distance = glm::dot(camComponent.getPass(p).forwardVector, direction);

            if (distance < -sphere.radius)
            {
                //model is behind the camera
                continue;
            }


            bool visible = true;
            //if (camComponent.isOrthographic())
            {
                const auto& frustum = camComponent.getPass(p).getFrustum();

                std::size_t j = 0;
                while (visible && j < frustum.size())
                {
                    visible = (Spatial::intersects(frustum[j++], sphere) != Planar::Back);
                }
            }
            /*else
            {
                //well measuring this says it's nearly twice as slow...
                model.m_visible = cro::Util::Frustum::visible(camComponent.getFrustumData(), camComponent.getPass(p).viewMatrix * tx.getWorldTransform(), model.getAABB());
            }*/

            if (visible)
            {
                auto opaque = std::make_pair(entity, SortData());
                auto transparent = std::make_pair(entity, SortData());

                //foreach material
                //add ent/index pair to alpha or opaque list
                for (auto i = 0u; i < model.m_meshData.submeshCount; ++i)
                {
                    if (model.m_materials[Mesh::IndexData::Final][i].blendMode != Material::BlendMode::None)
                    {
                        transparent.second.matIDs.push_back(static_cast<std::int32_t>(i));
                        transparent.second.flags = static_cast<std::int64_t>(-distance * 1000000.f); //suitably large number to shift decimal point
                        transparent.second.flags += 0x0FFF000000000000; //gaurentees embiggenment so that sorting places transparent last

                        //we have to set this here while we have a reference to
                        //the camera - it's then used in process() to update the
                        //worldView matrix of each visible entity
                        transparent.second.viewMatrix = camComponent.getPass(p).viewMatrix;
                    }
                    else
                    {
                        opaque.second.matIDs.push_back(static_cast<std::int32_t>(i));
                        opaque.second.flags = static_cast<std::int64_t>(distance * 1000000.f);

                        opaque.second.viewMatrix = camComponent.getPass(p).viewMatrix;
                    }
                }

                if (!opaque.second.matIDs.empty())
                {
#ifdef USE_PARALLEL_PROCESSING
                    std::scoped_lock l(mutex);
#endif
                    drawList[p].push_back(opaque);
                }

                if (!transparent.second.matIDs.empty())
                {
#ifdef USE_PARALLEL_PROCESSING
                    std::scoped_lock l(mutex);
#endif
                    drawList[p].push_back(transparent);
                }
            }
        }
    }
#ifdef USE_PARALLEL_PROCESSING    
    );
#endif
}

void ModelRenderer::updateDrawListBalancedTree(Entity cameraEnt)
{
    //const auto& camComponent = cameraEnt.getComponent<Camera>();
    //auto cameraPos = cameraEnt.getComponent<Transform>().getWorldPosition();

    //auto& drawList = m_drawLists[camComponent.getDrawListIndex()];

    //for (auto& list : drawList)
    //{
    //    list.clear();
    //}

    //auto passCount = camComponent.reflectionBuffer.available() ? 2 : 1;

    //for (auto p = 0; p < passCount; ++p)
    //{
    //    const auto& frustumBounds = camComponent.getPass(p).getAABB();
    //    auto entities = queryTree(frustumBounds);

    //    for (auto entity : entities)
    //    {
    //        auto& model = entity.getComponent<Model>();
    //        if (model.isHidden())
    //        {
    //            continue;
    //        }

    //        if ((model.m_renderFlags & camComponent.getPass(p).renderFlags) == 0)
    //        {
    //            continue;
    //        }

    //        if (model.m_meshBox != model.m_meshData.boundingBox)
    //        {
    //            model.updateBounds();
    //        }

    //        //use the bounding sphere for depth testing
    //        auto sphere = model.getBoundingSphere();
    //        const auto& tx = entity.getComponent<Transform>();

    //        sphere.centre = glm::vec3(tx.getWorldTransform() * glm::vec4(sphere.centre, 1.f));
    //        auto scale = tx.getScale();
    //        sphere.radius *= ((scale.x + scale.y + scale.z) / 3.f);

    //        auto direction = (sphere.centre - cameraPos);
    //        float distance = glm::dot(camComponent.getPass(p).forwardVector, direction);

    //        if (distance < -sphere.radius)
    //        {
    //            //model is behind the camera
    //            continue;
    //        }

    //        //frustum test
    //        bool visible = true;
    //        //if (camComponent.isOrthographic())
    //        {
    //            const auto& frustum = camComponent.getPass(p).getFrustum();

    //            visible = true;
    //            std::size_t j = 0;
    //            while (visible && j < frustum.size())
    //            {
    //                visible = (Spatial::intersects(frustum[j++], sphere) != Planar::Back);
    //            }
    //        }
    //        /*else
    //        {
    //            model.m_visible = cro::Util::Frustum::visible(camComponent.getFrustumData(), camComponent.getPass(p).viewMatrix * tx.getWorldTransform(), model.getAABB());
    //        }*/

    //        //add visible ents to lists for depth sorting
    //        if (visible)
    //        {
    //            auto opaque = std::make_pair(entity, SortData());
    //            auto transparent = std::make_pair(entity, SortData());

    //            //foreach material
    //            //add ent/index pair to alpha or opaque list
    //            for (auto i = 0u; i < model.m_meshData.submeshCount; ++i)
    //            {
    //                if (model.m_materials[Mesh::IndexData::Final][i].blendMode != Material::BlendMode::None)
    //                {
    //                    transparent.second.matIDs.push_back(static_cast<std::int32_t>(i));
    //                    transparent.second.flags = static_cast<std::int64_t>(-distance * 1000000.f); //suitably large number to shift decimal point
    //                    transparent.second.flags += 0x0FFF000000000000; //gaurentees embiggenment so that sorting places transparent last
    //                }
    //                else
    //                {
    //                    opaque.second.matIDs.push_back(static_cast<std::int32_t>(i));
    //                    opaque.second.flags = static_cast<std::int64_t>(distance * 1000000.f);
    //                }
    //            }

    //            if (!opaque.second.matIDs.empty())
    //            {
    //                drawList[p].push_back(opaque);
    //            }

    //            if (!transparent.second.matIDs.empty())
    //            {
    //                drawList[p].push_back(transparent);
    //            }
    //        }
    //    }
    //}
}

//std::vector<Entity> ModelRenderer::queryTree(Box area) const
//{
//    Detail::FixedStack<std::int32_t, 256> stack;
//    stack.push(m_tree.getRoot());
//
//    std::vector<Entity> retVal;
//    retVal.reserve(256);
//
//    while (stack.size() > 0)
//    {
//        auto treeID = stack.pop();
//        if (treeID == Detail::TreeNode::Null)
//        {
//            continue;
//        }
//
//        const auto& node = m_tree.getNodes()[treeID];
//        if (area.intersects(node.fatBounds))
//        {
//            if (node.isLeaf() && node.entity.isValid())
//            {
//                //we have a candidate, stash
//                retVal.push_back(node.entity);
//            }
//            else
//            {
//                stack.push(node.childA);
//                stack.push(node.childB);
//            }
//        }
//    }
//    return retVal;
//}

void ModelRenderer::applyProperties(const Material::Data& material, const Model& model, const Scene& scene, const Camera& camera)
{
    std::uint32_t currentTextureUnit = 0;
    for (const auto& prop : material.properties)
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
            //TODO textures need to track which unit they're currently bound
            //to so that they don't get bound to multiple units
            glCheck(glActiveTexture(GL_TEXTURE0 + currentTextureUnit));
            glCheck(glBindTexture(GL_TEXTURE_2D, prop.second.second.textureID));
            glCheck(glUniform1i(prop.second.first, currentTextureUnit++));
            break;
        case Material::Property::Cubemap:
            glCheck(glActiveTexture(GL_TEXTURE0 + currentTextureUnit));
            glCheck(glBindTexture(GL_TEXTURE_CUBE_MAP, prop.second.second.textureID));
            glCheck(glUniform1i(prop.second.first, currentTextureUnit++));
            break;
        case Material::Property::CubemapArray:
            glCheck(glActiveTexture(GL_TEXTURE0 + currentTextureUnit));
            glCheck(glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, prop.second.second.textureID));
            glCheck(glUniform1i(prop.second.first, currentTextureUnit++));
            break;
        case Material::Property::Number:
            glCheck(glUniform1f(prop.second.first,
                prop.second.second.numberValue));
            break;
        case Material::Property::Vec2:
            glCheck(glUniform2f(prop.second.first, 
                prop.second.second.vecValue[0],
                prop.second.second.vecValue[1]));
            break;
        case Material::Property::Vec3:
            glCheck(glUniform3f(prop.second.first, prop.second.second.vecValue[0],
                prop.second.second.vecValue[1], prop.second.second.vecValue[2]));
            break;
        case Material::Property::Vec4:
            glCheck(glUniform4f(prop.second.first, prop.second.second.vecValue[0],
                prop.second.second.vecValue[1], prop.second.second.vecValue[2], prop.second.second.vecValue[3]));
            break;
        case Material::Property::Mat4:
            glCheck(glUniformMatrix4fv(prop.second.first, 1, GL_FALSE, &prop.second.second.matrixValue[0].x));
            break;
        }
    }

    //apply 'optional' uniforms
    for (auto i = 0u; i < material.optionalUniformCount; ++i)
    {
        switch (material.optionalUniforms[i])
        {
        default: break;
        case Material::SkyBox:
            glCheck(glActiveTexture(GL_TEXTURE0 + currentTextureUnit));
            glCheck(glBindTexture(GL_TEXTURE_CUBE_MAP, scene.getCubemap().textureID));
            glCheck(glUniform1i(material.uniforms[Material::SkyBox], currentTextureUnit++));
            break;
        case Material::Skinning:
            glCheck(glUniformMatrix4fv(material.uniforms[Material::Skinning], static_cast<GLsizei>(model.m_jointCount), GL_FALSE, &model.m_skeleton[0][0].x));
            break;
        case Material::ProjectionMap:
        {
            const auto p = scene.getActiveProjectionMaps();
            glCheck(glUniformMatrix4fv(material.uniforms[Material::ProjectionMap], static_cast<GLsizei>(p.second), GL_FALSE, p.first));
            glCheck(glUniform1i(material.uniforms[Material::ProjectionMapCount], static_cast<GLint>(p.second)));
        }
            break;
        case Material::ShadowMapProjection:
            glCheck(glUniformMatrix4fv(material.uniforms[Material::ShadowMapProjection], static_cast<GLsizei>(camera.getCascadeCount()), GL_FALSE, &camera.m_shadowViewProjectionMatrices[0][0][0]));
            break;
        case Material::ShadowMapSampler:
            glCheck(glActiveTexture(GL_TEXTURE0 + currentTextureUnit));
#ifdef PLATFORM_DESKTOP
            glCheck(glBindTexture(GL_TEXTURE_2D_ARRAY, camera.shadowMapBuffer.getTexture().textureID));
#else
            glCheck(glBindTexture(GL_TEXTURE_2D, camera.shadowMapBuffer.getTexture().textureID));
#endif
            glCheck(glUniform1i(material.uniforms[Material::ShadowMapSampler], currentTextureUnit++));
            break;
        case Material::CascadeCount:
            glCheck(glUniform1i(material.uniforms[Material::CascadeCount], static_cast<std::int32_t>(camera.getCascadeCount())));
            break;
        case Material::CascadeSplits:
            glCheck(glUniform1fv(material.uniforms[Material::CascadeSplits], static_cast<GLsizei>(camera.getCascadeCount()), camera.getSplitDistances().data()));
            break;
        case Material::SunlightColour:
        {
            //usually in a uniform buffer, here for shaders which don't have the datablock
            const glm::vec4 colour = scene.getSunlight().getComponent<Sunlight>().getColour().getVec4();
            glCheck(glUniform4f(material.uniforms[Material::SunlightColour], colour.r/* * lightMultiplier*/, colour.g/* * lightMultiplier*/, colour.b /** lightMultiplier*/, colour.a));
        }
            break;
        case Material::SunlightDirection:
        {
            auto dir = scene.getSunlight().getComponent<Sunlight>().getDirection();
            glCheck(glUniform3f(material.uniforms[Material::SunlightDirection], dir.x, dir.y, dir.z));
        }
            break;
        case Material::ReflectionMap:
            glCheck(glActiveTexture(GL_TEXTURE0 + currentTextureUnit));
            glCheck(glBindTexture(GL_TEXTURE_2D, camera.reflectionBuffer.getTexture().getGLHandle()));
            glCheck(glUniform1i(material.uniforms[Material::ReflectionMap], currentTextureUnit++));
            break;
        case Material::RefractionMap:
            glCheck(glActiveTexture(GL_TEXTURE0 + currentTextureUnit));
            glCheck(glBindTexture(GL_TEXTURE_2D, camera.refractionBuffer.getTexture().getGLHandle()));
            glCheck(glUniform1i(material.uniforms[Material::RefractionMap], currentTextureUnit++));
            break;
        case Material::ReflectionMatrix:
        {
            //OK this must be a symptom of some obscure bug... we should be setting the vp from REFLECTION here... but that breaks mapping :S
            glCheck(glUniformMatrix4fv(material.uniforms[Material::ReflectionMatrix], 1, GL_FALSE, &camera.getPass(Camera::Pass::Refraction).viewProjectionMatrix[0][0]));
        }
        break;
        }
    }
}

void ModelRenderer::applyBlendMode(const Material::Data& material)
{
    //face culling is set by material 'double sided' property

    switch (material.blendMode)
    {
    default: break;
    case Material::BlendMode::Custom:
        CRO_ASSERT(!material.blendData.enableProperties.empty(), "You'll probably want at least GL_BLEND and GL_DEPTH_TEST");
        for (auto e : material.blendData.enableProperties)
        {
            glCheck(glEnable(e));
        }
        glCheck(glDepthMask(material.blendData.writeDepthMask));
        glCheck(glBlendFunc(material.blendData.blendFunc[0], material.blendData.blendFunc[1]));
        glCheck(glBlendEquation(material.blendData.equation));
        break;
    case Material::BlendMode::Additive:
        glCheck(glEnable(GL_BLEND));
        glCheck(glEnable(GL_DEPTH_TEST));
        glCheck(glDepthMask(GL_FALSE));
        glCheck(glBlendFunc(GL_ONE, GL_ONE));
        glCheck(glBlendEquation(GL_FUNC_ADD));
        break;
    case Material::BlendMode::Alpha:
        //make sure to test existing depth
        //values, just don't write new ones.
        glCheck(glEnable(GL_BLEND));
        glCheck(glEnable(GL_DEPTH_TEST));
        glCheck(glDepthMask(GL_FALSE));
        glCheck(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        glCheck(glBlendEquation(GL_FUNC_ADD));
        break;
    case Material::BlendMode::Multiply:
        glCheck(glEnable(GL_BLEND));
        glCheck(glEnable(GL_DEPTH_TEST));
        glCheck(glDepthMask(GL_FALSE));
        glCheck(glBlendFunc(GL_DST_COLOR, GL_ZERO));
        glCheck(glBlendEquation(GL_FUNC_ADD));
        break;
    case Material::BlendMode::None:
        glCheck(glDisable(GL_BLEND));
        glCheck(glEnable(GL_DEPTH_TEST));
        glCheck(glDepthMask(GL_TRUE));
        break;
    }
}

#ifdef PARALLEL_DISABLE
#ifndef PARALLEL_GLOBAL_DISABLE
#define USE_PARALLEL_PROCESSING
#endif
#endif