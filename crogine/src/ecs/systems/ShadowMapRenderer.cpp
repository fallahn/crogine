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

#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/ShadowCaster.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Skeleton.hpp>
#include <crogine/ecs/Scene.hpp>

#include <crogine/graphics/Spatial.hpp>
#include <crogine/core/Clock.hpp>

#include "../../detail/GLCheck.hpp"

#include <crogine/detail/glm/gtc/type_ptr.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/glm/gtc/matrix_inverse.hpp>
#include <crogine/detail/glm/gtc/matrix_access.hpp>
#include <crogine/detail/glm/gtx/quaternion.hpp>

using namespace cro;

ShadowMapRenderer::ShadowMapRenderer(cro::MessageBus& mb)
    : System(mb, typeid(ShadowMapRenderer))
{
    requireComponent<cro::Model>();
    requireComponent<cro::Transform>();
    requireComponent<cro::ShadowCaster>();

#ifdef PLATFORM_DESKTOP
    //TODO make this variable
    m_target.create(2048, 2048);
#else
    m_target.create(512, 512);
#endif
    //m_target.setRepeated(true);
}


//public
void ShadowMapRenderer::process(float)
{
    getScene()->getSunlight().getComponent<Sunlight>().m_textureID = m_target.getTexture().getGLHandle();

    //do this here so we know it gets updated just once per frame
    //during multi-pass rendering.
    updateDrawList();
    render();
}

//private
void ShadowMapRenderer::updateDrawList()
{
    auto& sunlight = getScene()->getSunlight().getComponent<Sunlight>();
    const auto& sunTx = getScene()->getSunlight().getComponent<Transform>();
    sunlight.m_viewMatrix = glm::inverse(sunTx.getWorldTransform());
    sunlight.m_viewProjectionMatrix = sunlight.m_projectionMatrix * sunlight.m_viewMatrix;

    sunlight.m_aabb = Spatial::updateFrustum(sunlight.m_frustum, sunlight.m_viewProjectionMatrix);

    glm::vec3 forwardVec = glm::column(sunlight.m_viewMatrix, 2);
    m_visibleEntities.clear(); 

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

        auto sphere = model.m_meshData.boundingSphere;
        const auto& tx = entity.getComponent<Transform>();

        sphere.centre = glm::vec3(tx.getWorldTransform() * glm::vec4(sphere.centre, 1.f));
        auto scale = tx.getScale();
        sphere.radius *= ((scale.x + scale.y + scale.z) / 3.f);

        model.m_visible = true;
        std::size_t i = 0;
        while (model.m_visible && i < sunlight.m_frustum.size())
        {
            model.m_visible = (Spatial::intersects(sunlight.m_frustum[i++], sphere) != Planar::Back);
        }

        if (model.m_visible)
        {
            float depth = glm::dot(sunTx.getWorldPosition() - tx.getWorldPosition(), forwardVec);
            m_visibleEntities.push_back(std::make_pair(entity, depth));
        }
    }

    //sort back to front
    std::sort(m_visibleEntities.begin(), m_visibleEntities.end(),
        [](const std::pair<Entity, float>& a, const std::pair<Entity, float>& b)
        {
            return a > b;
        });
}

void ShadowMapRenderer::render()
{
    const auto& sunlight = getScene()->getSunlight().getComponent<Sunlight>();

    //enable face culling and render rear faces
    glCheck(glEnable(GL_CULL_FACE));
    glCheck(glCullFace(GL_FRONT));
    glCheck(glEnable(GL_DEPTH_TEST));

    m_target.clear(cro::Colour::White());

    for (const auto& [e,f] : m_visibleEntities)
    {
        //calc entity transform
        const auto& tx = e.getComponent<Transform>();
        glm::mat4 worldMat = tx.getWorldTransform();
        glm::mat4 worldView = sunlight.m_viewMatrix * worldMat;

        //foreach submesh / material:
        const auto& model = e.getComponent<Model>();
#ifndef PLATFORM_DESKTOP
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, model.m_meshData.vbo));
#endif

        for (auto i = 0u; i < model.m_meshData.submeshCount; ++i)
        {
            const auto& mat = model.m_shadowMaterials[i];

            //bind shader
            glCheck(glUseProgram(mat.shader));

            //apply shader uniforms from material
            for (auto j = 0u; j< mat.optionalUniformCount; ++j)
            {
                switch (mat.optionalUniforms[j])
                {
                default: break;
                case Material::Skinning:
                    glCheck(glUniformMatrix4fv(mat.uniforms[Material::Skinning], static_cast<GLsizei>(model.m_jointCount), GL_FALSE, &model.m_skeleton[0][0].x));
                    break;
                }
            }
            glCheck(glUniformMatrix4fv(mat.uniforms[Material::WorldView], 1, GL_FALSE, glm::value_ptr(worldView)));
            glCheck(glUniformMatrix4fv(mat.uniforms[Material::Projection], 1, GL_FALSE, glm::value_ptr(sunlight.m_projectionMatrix)));

#ifdef PLATFORM_DESKTOP
            const auto& indexData = model.m_meshData.indexData[i];
            glCheck(glBindVertexArray(indexData.vao));
            glCheck(glDrawElements(static_cast<GLenum>(indexData.primitiveType), indexData.indexCount, static_cast<GLenum>(indexData.format), 0));
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

    glCheck(glDisable(GL_DEPTH_TEST));
    glCheck(glDisable(GL_CULL_FACE));
    glCheck(glCullFace(GL_BACK));
    m_target.display();
}

const Texture& ShadowMapRenderer::getDepthMapTexture() const
{
    return m_target.getTexture();
}