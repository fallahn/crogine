/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2021
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

#include "../../detail/GLCheck.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/core/Console.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/util/Matrix.hpp>

#include <crogine/detail/Assert.hpp>
#include <crogine/detail/glm/gtc/type_ptr.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/glm/gtc/matrix_inverse.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

using namespace cro;

namespace
{
    const std::vector<cro::Colour> colours =
    {
        cro::Colour::Red, cro::Colour::Green, cro::Colour::Cyan
    };
}

ModelRenderer::ModelRenderer(MessageBus& mb)
    : System            (mb, typeid(ModelRenderer))
{
    requireComponent<Transform>();
    requireComponent<Model>();
}

//public
void ModelRenderer::updateDrawList(Entity cameraEnt)
{
    auto& camComponent = cameraEnt.getComponent<Camera>();
    auto cameraPos = cameraEnt.getComponent<Transform>().getWorldPosition();

    auto& entities = getEntities();    

    //cull entities by viewable into draw lists by pass
    for (auto& list : m_visibleEnts)
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

        //render flags are tested when drawing as the flags may have changed
        //between draw calls but without updating the visiblity list.

        auto sphere = model.m_meshData.boundingSphere;
        const auto& tx = entity.getComponent<Transform>();

        sphere.centre = glm::vec3(tx.getWorldTransform() * glm::vec4(sphere.centre, 1.f));
        auto scale = tx.getScale();
        sphere.radius *= ((scale.x + scale.y + scale.z) / 3.f);


        for (auto p = 0u; p < m_visibleEnts.size(); ++p)
        {
            const auto& frustum = camComponent.getPass(p).getFrustum();
            auto forwardVector = cro::Util::Matrix::getForwardVector(camComponent.getPass(p).viewMatrix);

            model.m_visible = true;
            std::size_t i = 0;
            while (model.m_visible && i < frustum.size())
            {
                model.m_visible = (Spatial::intersects(frustum[i++], sphere) != Planar::Back);
            }

            if (model.m_visible)
            {
                auto opaque = std::make_pair(entity, SortData());
                auto transparent = std::make_pair(entity, SortData());

                auto direction = (sphere.centre - cameraPos);
                float distance = glm::dot(forwardVector, direction);
                //TODO a large model with a centre behind the camera
                //might still intersect the view but register as being
                //further away than smaller objects in front

                //foreach material
                //add ent/index pair to alpha or opaque list
                for (i = 0u; i < model.m_meshData.submeshCount; ++i)
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
                    m_visibleEnts[p].push_back(opaque);
                }

                if (!transparent.second.matIDs.empty())
                {
                    m_visibleEnts[p].push_back(transparent);
                }
            }
        }
    }
    DPRINT("Visible 3D ents", std::to_string(m_visibleEnts[0].size()));
    //DPRINT("Total ents", std::to_string(entities.size()));

    //sort lists by depth
    //flag values make sure transparent materials are rendered last
    //with opaque going front to back and transparent back to front
    for (auto i = 0u; i < m_visibleEnts.size(); ++i)
    {
        std::sort(std::begin(m_visibleEnts[i]), std::end(m_visibleEnts[i]),
            [](MaterialPair& a, MaterialPair& b)
            {
                return a.second.flags < b.second.flags;
            });

        //TODO remove this copy with a swap operation somewhere...
        camComponent.getDrawList(i)[getType()] = std::make_any<MaterialList>(m_visibleEnts[i]);
    }
}

void ModelRenderer::process(float)
{

}

void ModelRenderer::render(Entity camera, const RenderTarget& rt)
{
    const auto& camComponent = camera.getComponent<Camera>();
    const auto& pass = camComponent.getActivePass();

    glm::vec4 clipPlane = glm::vec4(0.f, 1.f, 0.f, -getScene()->getWaterLevel() + (0.08f * pass.getClipPlaneMultiplier())) * pass.getClipPlaneMultiplier();

    if (pass.drawList.count(getType()) == 0)
    {
        return;
    }
    
    const auto& camTx = camera.getComponent<Transform>();
    auto cameraPosition = camTx.getWorldPosition();
    auto screenSize = glm::vec2(rt.getSize());

    glCheck(glCullFace(pass.getCullFace()));

    //DPRINT("Render count", std::to_string(m_visibleEntities.size()));
    const auto& visibleEntities = std::any_cast<const MaterialList&>(pass.drawList.at(getType()));
    for (const auto& [entity, sortData] : visibleEntities)
    {
        //foreach submesh / material:
        const auto& model = entity.getComponent<Model>();

        if ((model.m_renderFlags & camComponent.renderFlags) == 0)
        {
            continue;
        }        
        
        //calc entity transform
        const auto& tx = entity.getComponent<Transform>();
        glm::mat4 worldMat = tx.getWorldTransform();
        glm::mat4 worldView = pass.viewMatrix * worldMat;

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

            applyBlendMode(model.m_materials[Mesh::IndexData::Final][i].blendMode);

            //check for depth test override
            if (!model.m_materials[Mesh::IndexData::Final][i].enableDepthTest)
            {
                glCheck(glDisable(GL_DEPTH_TEST));
            }

#ifdef PLATFORM_DESKTOP
            const auto& indexData = model.m_meshData.indexData[i];
            glCheck(glBindVertexArray(indexData.vao[Mesh::IndexData::Final]));
            glCheck(glDrawElements(static_cast<GLenum>(indexData.primitiveType), indexData.indexCount, static_cast<GLenum>(indexData.format), 0));

#else //GLES 2 doesn't have VAO support without extensions

            //bind attribs
            const auto& attribs = model.m_materials[i].attribs;
            for (auto j = 0u; j < model.m_materials[i].attribCount; ++j)
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
            for (auto j = 0u; j < model.m_materials[i].attribCount; ++j)
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

    glCheck(glUseProgram(0));
    
    glCheck(glDisable(GL_BLEND));
    glCheck(glDisable(GL_CULL_FACE));
    glCheck(glDisable(GL_DEPTH_TEST));
    glCheck(glDepthMask(GL_TRUE)); //restore this else clearing the depth buffer fails
}

//private
void ModelRenderer::applyProperties(const Material::Data& material, const Model& model, const Scene& scene, const Camera& camera)
{
    std::uint32_t currentTextureUnit = 0;
    for (const auto& prop : material.properties)
    {
        switch (prop.second.second.type)
        {
        default: break;
        case Material::Property::Texture:
            //TODO textures need to track which unit they're currently bound
            //to so that they don't get bounds to multiple units
            glCheck(glActiveTexture(GL_TEXTURE0 + currentTextureUnit));
            glCheck(glBindTexture(GL_TEXTURE_2D, prop.second.second.textureID));
            glCheck(glUniform1i(prop.second.first, currentTextureUnit++));
            break;
        case Material::Property::Cubemap:
            glCheck(glActiveTexture(GL_TEXTURE0 + currentTextureUnit));
            glCheck(glBindTexture(GL_TEXTURE_CUBE_MAP, prop.second.second.textureID));
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
            glCheck(glUniformMatrix4fv(material.uniforms[Material::ShadowMapProjection], 1, GL_FALSE, &camera.depthViewProjectionMatrix[0][0]));
            break;
        case Material::ShadowMapSampler:
            glCheck(glActiveTexture(GL_TEXTURE0 + currentTextureUnit));
            glCheck(glBindTexture(GL_TEXTURE_2D, camera.depthBuffer.getTexture().textureID));
            glCheck(glUniform1i(material.uniforms[Material::ShadowMapSampler], currentTextureUnit++));
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

void ModelRenderer::applyBlendMode(Material::BlendMode mode)
{
    switch (mode)
    {
    default: break;
    case Material::BlendMode::Additive:
        glCheck(glEnable(GL_BLEND));
        glCheck(glEnable(GL_DEPTH_TEST));
        glCheck(glDepthMask(GL_FALSE));
        glCheck(glEnable(GL_CULL_FACE));
        glCheck(glBlendFunc(GL_ONE, GL_ONE));
        glCheck(glBlendEquation(GL_FUNC_ADD));
        break;
    case Material::BlendMode::Alpha:
        //make sure to test existing depth
        //values, just don't write new ones.
        glCheck(glEnable(GL_DEPTH_TEST));
        glCheck(glDepthMask(GL_FALSE));
        glCheck(glEnable(GL_BLEND));
        glCheck(glEnable(GL_CULL_FACE));
        glCheck(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        glCheck(glBlendEquation(GL_FUNC_ADD));
        break;
    case Material::BlendMode::Multiply:
        glCheck(glEnable(GL_BLEND));
        glCheck(glEnable(GL_DEPTH_TEST));
        glCheck(glDepthMask(GL_FALSE));
        glCheck(glEnable(GL_CULL_FACE));
        glCheck(glBlendFunc(GL_DST_COLOR, GL_ZERO));
        glCheck(glBlendEquation(GL_FUNC_ADD));
        break;
    case Material::BlendMode::None:
        glCheck(glEnable(GL_DEPTH_TEST));
        glCheck(glDepthMask(GL_TRUE));
        glCheck(glEnable(GL_CULL_FACE));
        glCheck(glDisable(GL_BLEND));
        break;
    }
}
