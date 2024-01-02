/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2023
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

#include <crogine/detail/Assert.hpp>
#include <crogine/detail/glm/gtc/type_ptr.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/glm/gtc/matrix_inverse.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

using namespace cro;

namespace
{

}

ModelRenderer::ModelRenderer(MessageBus& mb)
    : System        (mb, typeid(ModelRenderer)),
    m_drawLists     (1),
    m_pass          (Mesh::IndexData::Final),
    m_tree          (1.f),
    m_useTreeQueries(false)
{
    requireComponent<Transform>();
    requireComponent<Model>();
}

//public
void ModelRenderer::updateDrawList(Entity cameraEnt)
{
    auto& camComponent = cameraEnt.getComponent<Camera>();
    if (m_drawLists.size() <= camComponent.getDrawListIndex())
    {
        m_drawLists.resize(camComponent.getDrawListIndex() + 1);
    }

    if (m_useTreeQueries)
    {
        updateDrawListBalancedTree(cameraEnt);
    }
    else
    {
        updateDrawListDefault(cameraEnt);
    }
    
    auto passCount = camComponent.reflectionBuffer.available() ? 2 : 1;

    DPRINT("Visible 3D ents in Scene " + std::to_string(getScene()->getInstanceID()) 
        + ", Camera " + std::to_string(cameraEnt.getIndex()), std::to_string(m_drawLists[camComponent.getDrawListIndex()][0].size()));
    //DPRINT("Total ents", std::to_string(entities.size()));

    //sort lists by depth
    //flag values make sure transparent materials are rendered last
    //with opaque going front to back and transparent back to front
    auto& drawList = m_drawLists[camComponent.getDrawListIndex()];
    for (auto i = 0; i < passCount; ++i)
    {
        std::sort(std::begin(drawList[i]), std::end(drawList[i]),
            [](MaterialPair& a, MaterialPair& b)
            {
                return a.second.flags < b.second.flags;
            });
    }
}

void ModelRenderer::process(float dt)
{
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& model = entity.getComponent<Model>();
        model.updateMaterialAnimations(dt);

        if (m_useTreeQueries)
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
        }
    }
}

void ModelRenderer::render(Entity camera, const RenderTarget& rt)
{
    const auto& camComponent = camera.getComponent<Camera>();
    if (camComponent.getDrawListIndex() < m_drawLists.size())
    {
        const auto& pass = camComponent.getActivePass();

        glm::vec4 clipPlane = glm::vec4(0.f, 1.f, 0.f, -getScene()->getWaterLevel() + (0.08f * pass.getClipPlaneMultiplier())) * pass.getClipPlaneMultiplier();

        const auto& camTx = camera.getComponent<Transform>();
        auto cameraPosition = camTx.getWorldPosition();
        auto screenSize = glm::vec2(rt.getSize());

        glCheck(glCullFace(pass.getCullFace()));

        //DPRINT("Render count", std::to_string(m_visibleEntities.size()));
        const auto& visibleEntities = m_drawLists[camComponent.getDrawListIndex()][camComponent.getActivePassIndex()];
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

            //calc entity transform
            const auto& tx = entity.getComponent<Transform>();
            const glm::mat4 worldMat = tx.getWorldTransform();
            const glm::mat4 worldView = pass.viewMatrix * worldMat;
            //hmm for some reason doning this only once breaks rendering
            //const glm::mat4 normalMat = glm::inverseTranspose(glm::mat3(worldMat));

#ifndef PLATFORM_DESKTOP
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, model.m_meshData.vbo));
#endif //PLATFORM

            for (auto i : sortData.matIDs)
            {
                //bind shader
                glCheck(glUseProgram(model.m_materials[Mesh::IndexData::Final][i].shader));

                //apply shader uniforms from material
                glCheck(glUniformMatrix4fv(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::WorldView], 1, GL_FALSE, glm::value_ptr(worldView)));
                applyProperties(model.m_materials[Mesh::IndexData::Final][i], model, *getScene(), camComponent);

                //apply standard uniforms
                glCheck(glUniform3f(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::Camera], cameraPosition.x, cameraPosition.y, cameraPosition.z));
                glCheck(glUniform2f(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::ScreenSize], screenSize.x, screenSize.y));
                glCheck(glUniform4f(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::ClipPlane], clipPlane[0], clipPlane[1], clipPlane[2], clipPlane[3]));
                glCheck(glUniformMatrix4fv(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::View], 1, GL_FALSE, glm::value_ptr(pass.viewMatrix)));
                glCheck(glUniformMatrix4fv(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::ViewProjection], 1, GL_FALSE, glm::value_ptr(pass.viewProjectionMatrix)));
                glCheck(glUniformMatrix4fv(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::Projection], 1, GL_FALSE, glm::value_ptr(camComponent.getProjectionMatrix())));
                glCheck(glUniformMatrix4fv(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::World], 1, GL_FALSE, glm::value_ptr(worldMat)));
                glCheck(glUniformMatrix3fv(model.m_materials[Mesh::IndexData::Final][i].uniforms[Material::Normal], 1, GL_FALSE, glm::value_ptr(glm::inverseTranspose(glm::mat3(worldMat)))));

                applyBlendMode(model.m_materials[Mesh::IndexData::Final][i]/*.blendMode*/);

                glCheck(model.m_materials[Mesh::IndexData::Final][i].doubleSided ? glDisable(GL_CULL_FACE) : glEnable(GL_CULL_FACE));
                glCheck(model.m_materials[Mesh::IndexData::Final][i].enableDepthTest ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST));

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

    model.m_treeID = m_tree.addToTree(entity, model.getAABB());
}

void ModelRenderer::onEntityRemoved(Entity entity)
{
    m_tree.removeFromTree(entity.getComponent<Model>().m_treeID);
}

//private
void ModelRenderer::updateDrawListDefault(Entity cameraEnt)
{
    const auto& camComponent = cameraEnt.getComponent<Camera>();
    const auto cameraPos = cameraEnt.getComponent<Transform>().getWorldPosition();
    //assume if there's no reflection buffer there's no need to sort the
    //entities for the second pass...
    const auto passCount = camComponent.reflectionBuffer.available() ? 2 : 1;

    auto& entities = getEntities();
    auto& drawList = m_drawLists[camComponent.getDrawListIndex()];

    //cull entities by viewable into draw lists by pass
    for (auto& list : drawList)
    {
        list.clear();
    }

    for (auto& entity : entities)
    {
        auto& model = entity.getComponent<Model>();
        if (model.isHidden())
        {
            continue;
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
                    }
                    else
                    {
                        opaque.second.matIDs.push_back(static_cast<std::int32_t>(i));
                        opaque.second.flags = static_cast<std::int64_t>(distance * 1000000.f);
                    }
                }

                if (!opaque.second.matIDs.empty())
                {
                    drawList[p].push_back(opaque);
                }

                if (!transparent.second.matIDs.empty())
                {
                    drawList[p].push_back(transparent);
                }
            }
        }
    }
}

void ModelRenderer::updateDrawListBalancedTree(Entity cameraEnt)
{
    const auto& camComponent = cameraEnt.getComponent<Camera>();
    auto cameraPos = cameraEnt.getComponent<Transform>().getWorldPosition();

    auto& drawList = m_drawLists[camComponent.getDrawListIndex()];

    for (auto& list : drawList)
    {
        list.clear();
    }

    auto passCount = camComponent.reflectionBuffer.available() ? 2 : 1;

    for (auto p = 0; p < passCount; ++p)
    {
        const auto& frustumBounds = camComponent.getPass(p).getAABB();
        auto entities = queryTree(frustumBounds);

        for (auto entity : entities)
        {
            auto& model = entity.getComponent<Model>();
            if (model.isHidden())
            {
                continue;
            }

            if ((model.m_renderFlags & camComponent.getPass(p).renderFlags) == 0)
            {
                continue;
            }

            if (model.m_meshBox != model.m_meshData.boundingBox)
            {
                model.updateBounds();
            }

            //use the bounding sphere for depth testing
            auto sphere = model.getBoundingSphere();
            const auto& tx = entity.getComponent<Transform>();

            sphere.centre = glm::vec3(tx.getWorldTransform() * glm::vec4(sphere.centre, 1.f));
            auto scale = tx.getScale();
            sphere.radius *= ((scale.x + scale.y + scale.z) / 3.f);

            auto direction = (sphere.centre - cameraPos);
            float distance = glm::dot(camComponent.getPass(p).forwardVector, direction);

            if (distance < -sphere.radius)
            {
                //model is behind the camera
                continue;
            }

            //frustum test
            bool visible = true;
            //if (camComponent.isOrthographic())
            {
                const auto& frustum = camComponent.getPass(p).getFrustum();

                visible = true;
                std::size_t j = 0;
                while (visible && j < frustum.size())
                {
                    visible = (Spatial::intersects(frustum[j++], sphere) != Planar::Back);
                }
            }
            /*else
            {
                model.m_visible = cro::Util::Frustum::visible(camComponent.getFrustumData(), camComponent.getPass(p).viewMatrix * tx.getWorldTransform(), model.getAABB());
            }*/

            //add visible ents to lists for depth sorting
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
                    }
                    else
                    {
                        opaque.second.matIDs.push_back(static_cast<std::int32_t>(i));
                        opaque.second.flags = static_cast<std::int64_t>(distance * 1000000.f);
                    }
                }

                if (!opaque.second.matIDs.empty())
                {
                    drawList[p].push_back(opaque);
                }

                if (!transparent.second.matIDs.empty())
                {
                    drawList[p].push_back(transparent);
                }
            }
        }
    }
}

std::vector<Entity> ModelRenderer::queryTree(Box area) const
{
    Detail::FixedStack<std::int32_t, 256> stack;
    stack.push(m_tree.getRoot());

    std::vector<Entity> retVal;
    retVal.reserve(256);

    while (stack.size() > 0)
    {
        auto treeID = stack.pop();
        if (treeID == Detail::TreeNode::Null)
        {
            continue;
        }

        const auto& node = m_tree.getNodes()[treeID];
        if (area.intersects(node.fatBounds))
        {
            if (node.isLeaf() && node.entity.isValid())
            {
                //we have a candidate, stash
                retVal.push_back(node.entity);
            }
            else
            {
                stack.push(node.childA);
                stack.push(node.childB);
            }
        }
    }
    return retVal;
}

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
            auto colour = scene.getSunlight().getComponent<Sunlight>().getColour();
            glCheck(glUniform4f(material.uniforms[Material::SunlightColour], colour.getRed(), colour.getGreen(), colour.getBlue(), colour.getAlpha()));
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
