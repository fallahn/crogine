/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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

#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/glm/gtc/matrix_inverse.hpp>

#include <crogine/graphics/Spatial.hpp>

#include <crogine/ecs/Scene.hpp>

#include <crogine/ecs/systems/ReflectionMapRenderer.hpp>

#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>

#include <crogine/util/Matrix.hpp>

using namespace cro;

namespace
{
    //this is all but the last bit set. We'll make water plane models
    //the inverse of this so it won't get rendered in the refraction pass.
    const std::uint64_t RenderFlags = std::numeric_limits<std::uint64_t>::max() / 2;
}

ReflectionMapRenderer::ReflectionMapRenderer(MessageBus& mb, std::uint32_t mapSize)
    : System    (mb, typeid(ReflectionMapRenderer)),
    m_plane     (0.f, 1.f, 0.f, 0.f)
{
    requireComponent<Model>();

    if (!m_reflectionMap.create(mapSize, mapSize))
    {
        LogE << "Failed creating Reflection Map buffer." << std::endl;
    }
    else
    {
        m_reflectionMap.setSmooth(true);
    }

    if (!m_refractionMap.create(mapSize, mapSize))
    {
        LogE << "Failed creating Refraction Map buffer." << std::endl;
    }
    else
    {
        m_refractionMap.setSmooth(true);
    }
}

//public
void ReflectionMapRenderer::updateDrawList(Entity camera)
{
    auto& cam = camera.getComponent<Camera>();
    const auto& camTx = camera.getComponent<cro::Transform>();


    cam.reflectedViewMatrix = glm::scale(cam.viewMatrix, glm::vec3(1.f, -1.f, 1.f));
    cam.reflectedViewMatrix = glm::translate(cam.reflectedViewMatrix, glm::vec3(0.f, m_plane.a * 2, 0.f));
    cam.reflectedViewProjectionMatrix = cam.projectionMatrix * cam.reflectedViewMatrix;


    Frustum frustum = {};
    Spatial::updateFrustum(frustum, cam.reflectedViewProjectionMatrix);

    auto forwardVector = -Util::Matrix::getForwardVector(cam.reflectedViewMatrix);

    //save the draw list in the camera for the visible reflected models.
    //we'll rely on the model renderer to keep the visible list of
    //models up to date for the refraction pass.

    auto& entities = getEntities();

    //cull entities by viewable into draw lists by pass
    MaterialList visibleEntities;
    visibleEntities.reserve(entities.size() * 2);

    for (auto& entity : entities)
    {
        auto& model = entity.getComponent<Model>();
        if (model.isHidden())
        {
            continue;
        }

        if ((model.m_renderFlags & RenderFlags) == 0)
        {
            continue;
        }

        auto sphere = model.m_meshData.boundingSphere;
        const auto& tx = entity.getComponent<Transform>();

        sphere.centre = glm::vec3(tx.getWorldTransform() * glm::vec4(sphere.centre, 1.f));
        auto scale = tx.getScale();
        sphere.radius *= ((scale.x + scale.y + scale.z) / 3.f);

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

            //auto worldPos = tx.getWorldPosition();
            auto direction = (sphere.centre - camTx.getWorldPosition());
            float distance = glm::dot(forwardVector, direction);
            //TODO a large model with a centre behind the camera
            //might still intersect the view but register as being
            //further away than smaller objects in front

            //foreach material
            //add ent/index pair to alpha or opaque list
            for (i = 0u; i < model.m_meshData.submeshCount; ++i)
            {
                if (model.m_materials[i].blendMode != Material::BlendMode::None)
                {
                    transparent.second.matIDs.push_back(static_cast<std::int32_t>(i));
                    transparent.second.flags = static_cast<int64>(-distance * 1000000.f); //suitably large number to shift decimal point
                    transparent.second.flags += 0x0FFF000000000000; //gaurentees embiggenment so that sorting places transparent last
                }
                else
                {
                    opaque.second.matIDs.push_back(static_cast<std::int32_t>(i));
                    opaque.second.flags = static_cast<int64>(distance * 1000000.f);
                }
            }

            if (!opaque.second.matIDs.empty())
            {
                visibleEntities.push_back(opaque);
            }

            if (!transparent.second.matIDs.empty())
            {
                visibleEntities.push_back(transparent);
            }
        }
    }

    //sort lists by depth
    //flag values make sure transparent materials are rendered last
    //with opaque going front to back and transparent back to front
    std::sort(std::begin(visibleEntities), std::end(visibleEntities),
        [](MaterialPair& a, MaterialPair& b)
        {
            return a.second.flags < b.second.flags;
        });

    //TODO would this be better to swap a buffer rather than create a new vector every frame?
    cam.drawList[getType()] = std::make_any<MaterialList>(std::move(visibleEntities));
}

void ReflectionMapRenderer::render(Entity camera, const RenderTarget&)
{
    const auto& camComponent = camera.getComponent<Camera>();


    //enable clipping in OpenGL
    glCheck(glEnable(GL_CLIP_DISTANCE0));

    //render all models in the camera's reflection list using the reflection VP
    m_reflectionMap.clear(cro::Colour::Transparent());
    renderList(camera, m_reflectionMap, camComponent.reflectedViewMatrix, camComponent.reflectedViewProjectionMatrix, getType(), GL_FRONT);
    m_reflectionMap.display();

    //render all models in the camera's regular list using the regular matrices
    m_plane = -m_plane;
    m_refractionMap.clear(cro::Colour::Transparent());
    renderList(camera, m_refractionMap, camComponent.viewMatrix, camComponent.viewProjectionMatrix, UniqueType(typeid(ModelRenderer)), GL_BACK);
    m_refractionMap.display();

    m_plane = -m_plane;

    //disable clipping again
    glCheck(glDisable(GL_CLIP_DISTANCE0));
}

//private
void ReflectionMapRenderer::renderList(Entity camera, const RenderTarget& rt, const glm::mat4& viewMat, const glm::mat4& viewProjMat, UniqueType type, std::uint32_t face)
{
    const auto& camComponent = camera.getComponent<Camera>();
    const auto& camTx = camera.getComponent<Transform>();
    auto cameraPosition = camTx.getWorldPosition();
    auto screenSize = glm::vec2(rt.getSize());

    if (camComponent.drawList.count(type) == 0)
    {
        return;
    }


    glCheck(glCullFace(face));

    const auto& visibleEntities = std::any_cast<const MaterialList&>(camComponent.drawList.at(type));
    for (const auto& [entity, sortData] : visibleEntities)
    {
        //calc entity transform
        const auto& tx = entity.getComponent<Transform>();
        glm::mat4 worldMat = tx.getWorldTransform();
        glm::mat4 worldView = viewMat * worldMat;

        //foreach submesh / material:
        const auto& model = entity.getComponent<Model>();

        //skip water planes
        if ((model.m_renderFlags & RenderFlags) == 0)
        {
            continue;
        }

#ifndef PLATFORM_DESKTOP
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, model.m_meshData.vbo));
#endif //PLATFORM

        for (auto i : sortData.matIDs)
        {
            //bind shader
            glCheck(glUseProgram(model.m_materials[i].shader));

            //apply shader uniforms from material
            glCheck(glUniformMatrix4fv(model.m_materials[i].uniforms[Material::WorldView], 1, GL_FALSE, &worldView[0][0]));
            ModelRenderer::applyProperties(model.m_materials[i], model, *getScene());

            //apply standard uniforms
            glCheck(glUniform3f(model.m_materials[i].uniforms[Material::Camera], cameraPosition.x, cameraPosition.y, cameraPosition.z));
            glCheck(glUniform2f(model.m_materials[i].uniforms[Material::ScreenSize], screenSize.x, screenSize.y));
            glCheck(glUniform4f(model.m_materials[i].uniforms[Material::ClipPlane], m_plane.r, m_plane.g, m_plane.b, m_plane.a));
            glCheck(glUniformMatrix4fv(model.m_materials[i].uniforms[Material::View], 1, GL_FALSE, &viewMat[0][0]));
            glCheck(glUniformMatrix4fv(model.m_materials[i].uniforms[Material::ViewProjection], 1, GL_FALSE, &viewProjMat[0][0]));
            glCheck(glUniformMatrix4fv(model.m_materials[i].uniforms[Material::Projection], 1, GL_FALSE, &camComponent.projectionMatrix[0][0]));
            glCheck(glUniformMatrix4fv(model.m_materials[i].uniforms[Material::World], 1, GL_FALSE, &worldMat[0][0]));
            glCheck(glUniformMatrix3fv(model.m_materials[i].uniforms[Material::Normal], 1, GL_FALSE, &glm::inverseTranspose(glm::mat3(worldMat))[0][0]));

            ModelRenderer::applyBlendMode(model.m_materials[i].blendMode);

            //check for depth test override
            if (!model.m_materials[i].enableDepthTest)
            {
                glCheck(glDisable(GL_DEPTH_TEST));
            }

#ifdef PLATFORM_DESKTOP
            const auto& indexData = model.m_meshData.indexData[i];
            glCheck(glBindVertexArray(indexData.vao));
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