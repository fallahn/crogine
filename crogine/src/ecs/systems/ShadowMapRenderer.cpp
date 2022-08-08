/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2022
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

using namespace cro;

namespace
{
    std::uint32_t intervalCounter = 0;
}

ShadowMapRenderer::ShadowMapRenderer(cro::MessageBus& mb)
    : System(mb, typeid(ShadowMapRenderer)),
    m_maxDistance   (100.f),
    m_cascadeCount  (3),
    m_interval      (1)
{
    requireComponent<cro::Model>();
    requireComponent<cro::Transform>();
    requireComponent<cro::ShadowCaster>();
}


//public
void ShadowMapRenderer::setMaxDistance(float distance)
{
    m_maxDistance = std::max(distance, 0.1f);
}

void ShadowMapRenderer::setNumCascades(std::int32_t count)
{
    m_cascadeCount = std::max(1, count);
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

    glm::vec3 lightDir = getScene()->getSunlight().getComponent<Sunlight>().getDirection();

    //this gets called once for each Camera in the CameraSystem
    //from CameraSystem::process() - so we'll check here if
    //the camera should actually be updated then render all the
    //cameras at once in process(), above
    auto& camera = camEnt.getComponent<Camera>();
    if (camera.shadowMapBuffer.available())
    {
        if (m_drawLists.size() <= m_activeCameras.size())
        {
            m_drawLists.emplace_back();
        }

        auto& drawList = m_drawLists[m_activeCameras.size()];
        drawList.clear();

        m_activeCameras.push_back(camEnt);


        //only covers the first part of the frustum...
        //TODO implement the rest as cascaded shadows
        float farPlane = std::min(camera.m_farPlane, m_maxDistance) / m_cascadeCount;

        //calc a position for the directional light
        //this is used for depth sorting the draw list
        auto centre = camEnt.getComponent<cro::Transform>().getWorldPosition() + (camEnt.getComponent<cro::Transform>().getForwardVector() * (farPlane / 2.f));
        auto lightPos = centre - (lightDir * ((camera.m_farPlane - camera.m_nearPlane) / 2.f));

        camera.shadowViewMatrix = glm::lookAt(lightPos, centre, cro::Transform::Y_AXIS);// glm::inverse(glm::toMat4(lightRotation));

        //frustum in camera coords
        float tanHalfFOVY = std::tan(camera.m_verticalFOV / 2.f);
        float tanHalfFOVX = std::tan((camera.m_verticalFOV * camera.m_aspectRatio) / 2.f);

        
        static constexpr float Embiggenment = 1.2f;

        float xNear = (camera.m_nearPlane * tanHalfFOVX) * Embiggenment;
        float xFar = (farPlane * tanHalfFOVX)* Embiggenment;
        float yNear = (camera.m_nearPlane * tanHalfFOVY)* Embiggenment;
        float yFar = (farPlane * tanHalfFOVY)* Embiggenment;

        std::array frustumCorners =
        {
            //near
            glm::vec4(xNear, yNear, -camera.m_nearPlane, 1.f),
            glm::vec4(-xNear, yNear, -camera.m_nearPlane, 1.f),
            glm::vec4(xNear, -yNear, -camera.m_nearPlane, 1.f),
            glm::vec4(-xNear, -yNear, -camera.m_nearPlane, 1.f),

            //far
            glm::vec4(xFar, yFar, -farPlane, 1.f),
            glm::vec4(-xFar, yFar, -farPlane, 1.f),
            glm::vec4(xFar, -yFar, -farPlane, 1.f),
            glm::vec4(-xFar, -yFar, -farPlane, 1.f)
        };

        auto camTx = camEnt.getComponent<Transform>().getWorldTransform();
        std::array<glm::vec4, 8u> lightCorners = {};

        for (auto i = 0u; i < frustumCorners.size(); ++i)
        {
            //convert frustum to world space
            auto worldPoint = camTx * frustumCorners[i];

            //convert to light space
            lightCorners[i] = camera.shadowViewMatrix * worldPoint;
        }

        auto xExtremes = std::minmax_element(lightCorners.begin(), lightCorners.end(),
            [](glm::vec3 l, glm::vec3 r) 
            {
                return l.x < r.x;
            });

        auto yExtremes = std::minmax_element(lightCorners.begin(), lightCorners.end(),
            [](glm::vec3 l, glm::vec3 r)
            {
                return l.y < r.y;
            });

        auto zExtremes = std::minmax_element(lightCorners.begin(), lightCorners.end(),
            [](glm::vec3 l, glm::vec3 r)
            {
                return l.z < r.z;
            });

        float minX = xExtremes.first->x;
        float maxX = xExtremes.second->x;
        float minY = yExtremes.first->y;
        float maxY = yExtremes.second->y;
        float minZ = zExtremes.first->z;
        float maxZ = zExtremes.second->z;

        //convert to ortho projection
        camera.shadowProjectionMatrix = glm::ortho(minX, maxX, minY, maxY, -maxZ, -minZ);
        camera.shadowViewProjectionMatrix = camera.shadowProjectionMatrix * camera.shadowViewMatrix;

#ifdef CRO_DEBUG_
        camera.depthDebug = { minX, maxX, minY, maxY, -maxZ, -minZ };
        camera.depthPosition = lightPos;
#endif //CRO_DEBUG_


        //use depth frustum to cull entities
        Frustum frustum = {};
        Spatial::updateFrustum(frustum, camera.shadowViewProjectionMatrix);

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

            const auto& tx = entity.getComponent<Transform>();
            auto sphere = model.getBoundingSphere();

            sphere.centre = glm::vec3(tx.getWorldTransform() * glm::vec4(sphere.centre, 1.f));
            auto scale = tx.getScale();
            sphere.radius *= ((scale.x + scale.y + scale.z) / 3.f);

            float distance = glm::dot(lightDir, sphere.centre - lightPos);
            if (distance < -sphere.radius)
            {
                //we're behind the frustum
                continue;
            }

            model.m_visible = true;
            std::size_t i = 0;
            while (model.m_visible&& i < frustum.size())
            {
                model.m_visible = (Spatial::intersects(frustum[i++], sphere) != Planar::Back);
            }

            //needs fixing for ortho projection
            //model.m_visible = cro::Util::Frustum::visible(camera.getFrustumData(), camera.shadowViewMatrix * tx.getWorldTransform(), model.getAABB());

            if (model.m_visible)
            {
                drawList.push_back(std::make_pair(entity, distance));
            }
        }

        //sort back to front
        std::sort(drawList.begin(), drawList.end(),
            [](const std::pair<Entity, float>& a, const std::pair<Entity, float>& b)
            {
                return a > b;
            });

        DPRINT("Visible 3D shadow ents", std::to_string(drawList.size()));
    }
}

//private
void ShadowMapRenderer::render()
{
    for (auto c = 0u; c < m_activeCameras.size(); c++)
    {
        auto& camera = m_activeCameras[c].getComponent<Camera>();
        auto cameraPosition = m_activeCameras[c].getComponent<cro::Transform>().getWorldPosition();

        //enable face culling and render rear faces
        //glCheck(glEnable(GL_CULL_FACE)); //this is now done per-material as some may be double sided
        glCheck(glCullFace(GL_BACK));
        //glCheck(glCullFace(GL_FRONT));
        glCheck(glEnable(GL_DEPTH_TEST));

#ifdef PLATFORM_DESKTOP
        camera.shadowMapBuffer.clear();
#else
        camera.shadowMapBuffer.clear(cro::Colour::White());
#endif

        for (const auto& [e, f] : m_drawLists[c])
        {
            const auto& model = e.getComponent<Model>();
            //skip this model if its flags don't pass
            if ((model.m_renderFlags & camera.renderFlags) == 0)
            {
                continue;
            }

            glCheck(glFrontFace(model.m_facing));

            //calc entity transform
            const auto& tx = e.getComponent<Transform>();
            glm::mat4 worldMat = tx.getWorldTransform();
            glm::mat4 worldView = camera.shadowViewMatrix * worldMat;

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
                glCheck(glUniformMatrix4fv(mat.uniforms[Material::View], 1, GL_FALSE, glm::value_ptr(camera.shadowViewMatrix)));
                glCheck(glUniformMatrix4fv(mat.uniforms[Material::WorldView], 1, GL_FALSE, glm::value_ptr(worldView)));
                glCheck(glUniformMatrix4fv(mat.uniforms[Material::Projection], 1, GL_FALSE, glm::value_ptr(camera.shadowProjectionMatrix)));
                glCheck(glUniform3f(mat.uniforms[Material::Camera], cameraPosition.x, cameraPosition.y, cameraPosition.z));
                //glCheck(glUniformMatrix4fv(mat.uniforms[Material::ViewProjection], 1, GL_FALSE, glm::value_ptr(camera.depthViewProjectionMatrix)));

                glCheck(model.m_materials[Mesh::IndexData::Final][i].doubleSided ? glDisable(GL_CULL_FACE) : glEnable(GL_CULL_FACE));

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

#ifdef PLATFORM_DESKTOP
        glCheck(glBindVertexArray(0));
#else
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
#endif //PLATFORM

        glCheck(glUseProgram(0));

        glCheck(glFrontFace(GL_CCW));
        glCheck(glDisable(GL_DEPTH_TEST));
        glCheck(glDisable(GL_CULL_FACE));
        //glCheck(glCullFace(GL_BACK));
        camera.shadowMapBuffer.display();
    }
}

void ShadowMapRenderer::onEntityAdded(cro::Entity entity)
{
    if (entity.getComponent<cro::Model>().m_materials[Mesh::IndexData::Shadow][0].shader == 0)
    {
        LogW << "Shadow caster added to model with no shadow material. This will not render." << std::endl;
    }
}