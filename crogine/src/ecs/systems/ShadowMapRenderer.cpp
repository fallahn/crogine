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

#include <crogine/core/Clock.hpp>

#include "../../detail/GLCheck.hpp"

#include <crogine/detail/glm/gtc/type_ptr.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/glm/gtc/matrix_inverse.hpp>
#include <crogine/detail/glm/gtx/quaternion.hpp>

using namespace cro;

ShadowMapRenderer::ShadowMapRenderer(cro::MessageBus& mb)
    : System(mb, typeid(ShadowMapRenderer))
{
    requireComponent<cro::Model>();
    requireComponent<cro::Transform>();
    requireComponent<cro::ShadowCaster>();

    m_projectionOffset = { 0.f, 0.5f, -0.5f };

#ifdef PLATFORM_DESKTOP
    m_target.create(1024, 1024);
#else
    m_target.create(512, 512);
#endif
    //m_target.setRepeated(true);
}


//public
void ShadowMapRenderer::process(float)
{
    m_visibleEntities.clear();
    
    auto& entities = getEntities();
    for (auto& entity : entities)
    {
        //basic culling - this relies on the visibility test of ModelRenderer
        if (entity.getComponent<cro::Model>().isVisible())
        {
            m_visibleEntities.push_back(entity);
        }
    }

    getScene()->getSunlight().m_textureID = m_target.getTexture().getGLHandle();
}

void ShadowMapRenderer::render(Entity camera, const RenderTarget&)
{
    //enable face culling and render rear faces
    glCheck(glEnable(GL_CULL_FACE));
    glCheck(glCullFace(GL_FRONT));
    glCheck(glEnable(GL_DEPTH_TEST));
    
    const auto& camTx = camera.getComponent<Transform>();

    auto dir = glm::translate(getScene()->getSunlight().getRotation(), (camTx.getWorldPosition() - m_projectionOffset));
    auto viewMat = glm::inverse(dir);
    //auto viewMat = glm::inverse(camTx.getWorldTransform());

    auto projMat = getScene()->getSunlight().getProjectionMatrix();
    getScene()->getSunlight().setViewProjectionMatrix(projMat * viewMat);

    m_target.clear(cro::Colour::White());

    
    for (const auto& e : m_visibleEntities)
    {
        //calc entity transform
        const auto& tx = e.getComponent<Transform>();
        glm::mat4 worldMat = tx.getWorldTransform();
        glm::mat4 worldView = viewMat * worldMat;

        //foreach submesh / material:
        const auto& model = e.getComponent<Model>();
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, model.m_meshData.vbo));

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
            glCheck(glUniformMatrix4fv(mat.uniforms[Material::Projection], 1, GL_FALSE, glm::value_ptr(projMat)));

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
        }
        glCheck(glUseProgram(0));
    }

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
    glCheck(glDisable(GL_DEPTH_TEST));
    glCheck(glDisable(GL_CULL_FACE));
    glCheck(glCullFace(GL_BACK));
    m_target.display();
}

void ShadowMapRenderer::setProjectionOffset(glm::vec3 offset)
{
    m_projectionOffset = offset;
}

const Texture& ShadowMapRenderer::getDepthMapTexture() const
{
    return m_target.getTexture();
}
