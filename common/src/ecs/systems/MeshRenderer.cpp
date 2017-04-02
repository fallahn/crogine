/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#include <crogine/ecs/systems/MeshRenderer.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>

#include <crogine/core/Clock.hpp>

#include "../../glad/glad.h"
#include "../../glad/GLCheck.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace cro;

MeshRenderer::MeshRenderer()
    : System(this)
{
    requireComponent<Transform>();
    requireComponent<Model>();

    m_projectionMatrix = glm::perspective(1.66f, 4.f/3.f, 0.1f, 50.f);
}

//public
void MeshRenderer::process(cro::Time)
{
    //cull entities by viewable into draw lists by pass

    //sort lists by depth
}

void MeshRenderer::render()
{
    //TODO use draw list instead
    auto& ents = getEntities();
    for (auto& e : ents)
    {
        //calc entity transform
        const auto& tx = e.getComponent<Transform>();
        glm::mat4 worldView = tx.getWorldTransform(ents);

        //foreach submesh / material:
        const auto& model = e.getComponent<Model>();
        for (auto i = 0u; i < model.m_meshData.submeshCount; ++i)
        {
            //bind shader
            glCheck(glUseProgram(model.m_materials[i].shader));

            //apply shader uniforms from material
            glCheck(glUniformMatrix4fv(model.m_materials[i].uniforms[Material::WorldView], 1, GL_FALSE, glm::value_ptr(worldView)));
            applyProperties(model.m_materials[i].properties);

            //apply uniform buffers
            glCheck(glUniformMatrix4fv(model.m_materials[i].uniforms[Material::Projection], 1, GL_FALSE, glm::value_ptr(m_projectionMatrix)));

            //bind winding/cullface/depthfunc

            //bind vaos
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, model.m_meshData.vbo));
            const auto& attribs = model.m_materials[i].attribs;
            for (auto j = 0u; j < model.m_materials[i].attribCount; ++j)
            {
                glCheck(glVertexAttribPointer(attribs[j][Material::Data::Index], attribs[j][Material::Data::Size],
                    GL_FLOAT, GL_FALSE, static_cast<GLsizei>(model.m_meshData.vertexSize), 
                    reinterpret_cast<void*>(static_cast<intptr_t>(attribs[j][Material::Data::Offset]))));
                glCheck(glEnableVertexAttribArray(attribs[j][Material::Data::Index]));
            }

            //bind element/index buffer
            const auto& indexData = model.m_meshData.indexData[i];
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexData.ibo));

            //draw elements
            glCheck(glDrawElements(static_cast<GLenum>(indexData.primitiveType), indexData.indexCount, static_cast<GLenum>(indexData.format), 0));

            //unbind vaos
            for (auto j = 0u; j < model.m_materials[i].attribCount; ++j)
            {
                glCheck(glDisableVertexAttribArray(attribs[j][Material::Data::Index]));
            }
        }
    }
}

//private
void MeshRenderer::applyProperties(const Material::PropertyList& properties)
{
    for (const auto& prop : properties)
    {
        switch (prop.second.second.type)
        {
        default: break;
        case Material::Property::Number:
            glCheck(glUniform1f(prop.second.first, prop.second.second.numberValue));
            break;
        case Material::Property::Texture:
            //TODO get texture binding locations
            break;
        case Material::Property::Vec2:
            glCheck(glUniform2f(prop.second.first, prop.second.second.vecValue[0],
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
        }
    }
}