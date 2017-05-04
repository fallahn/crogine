/*-----------------------------------------------------------------------

Matt Marchant 2017
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

#include <crogine/graphics/PostProcess.hpp>
#include <crogine/graphics/Shader.hpp>
#include <crogine/graphics/MeshData.hpp>
#include <crogine/graphics/Texture.hpp>

#include "../detail/GLCheck.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>

using namespace cro;

namespace
{
    constexpr uint32 vertexSize = 2 * sizeof(float);
}

PostProcess::PostProcess()
    : m_vbo(0)
{
    //0------2
    //|      |
    //|      |
    //1------3

    std::vector<float> verts = 
    {
        0.f, 1.f, //0.f, 1.f,
        0.f, 0.f,// 0.f, 0.f,
        1.f, 1.f, //1.f, 1.f,
        1.f, 0.f, //1.f, 0.f
    };
    glCheck(glGenBuffers(1, &m_vbo));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, vertexSize * verts.size(), verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
}

PostProcess::~PostProcess()
{
    if (m_vbo)
    {
        glCheck(glDeleteBuffers(1, &m_vbo));
    }
}

//public
void PostProcess::resizeBuffer(int32 w, int32 h)
{
    m_currentBufferSize = { w, h };
    bufferResized();
}

//protected
void PostProcess::drawQuad(Shader& shader, FloatRect size)
{
    CRO_ASSERT(m_vbo, "VBO not created!");
    
    //TODO only rebuild as necessary? Else not really a need for member var
    m_transform = glm::scale(glm::mat4(), { size.width, size.height, 0.f });
    m_projection = glm::ortho(0.f, size.width, 0.f, size.height, -0.1f, 10.f);
    const auto& uniforms = shader.getUniformMap();

    glCheck(glUseProgram(shader.getGLHandle()));
    glCheck(glUniformMatrix4fv(uniforms.find("u_worldMatrix")->second, 1, GL_FALSE, glm::value_ptr(m_transform)));
    glCheck(glUniformMatrix4fv(uniforms.find("u_projectionMatrix")->second, 1, GL_FALSE, glm::value_ptr(m_projection)));

    //bind any textures / apply misc uniforms
    const auto& params = m_uniforms[shader.getGLHandle()];
    uint32 currentTexUnit = 0;
    for (const auto& u : params)
    {
        switch (u.second.type)
        {
        default:
        case UniformData::None:
            break;
        case UniformData::Number:
            glCheck(glUniform1f(u.first, u.second.numberValue[0]));
            break;
        case UniformData::Number2:
            glCheck(glUniform2f(u.first, u.second.numberValue[0], u.second.numberValue[1]));
            break;
        case UniformData::Number3:
            glCheck(glUniform3f(u.first, u.second.numberValue[0], u.second.numberValue[1], u.second.numberValue[2]));
            break;
        case UniformData::Number4:
            glCheck(glUniform4f(u.first, u.second.numberValue[0], u.second.numberValue[1], u.second.numberValue[2], u.second.numberValue[3]));
            break;
        case UniformData::Texture:
            glCheck(glActiveTexture(GL_TEXTURE0 + currentTexUnit));
            glCheck(glBindTexture(GL_TEXTURE_2D, u.second.textureID));
            glCheck(glUniform1i(u.first, currentTexUnit++));
            break;
        }
    }

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));

    const auto& attribs = shader.getAttribMap();
    glCheck(glEnableVertexAttribArray(attribs[Mesh::Attribute::Position]));
    glCheck(glVertexAttribPointer(attribs[Mesh::Attribute::Position], 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(vertexSize), reinterpret_cast<void*>(static_cast<intptr_t>(0))));

    glCheck(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));

    glCheck(glDisableVertexAttribArray(attribs[Mesh::Attribute::Position]));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
    glCheck(glUseProgram(0));
}

void PostProcess::setUniform(const std::string& name, float value, const Shader& shader)
{
    CRO_ASSERT(shader.getUniformMap().count(name) != 0, "Uniform doesn't exist");
    
    auto a = shader.getGLHandle();
    auto b = shader.getUniformMap().find(name)->second;
    m_uniforms[a][b].type = UniformData::Number;
    m_uniforms[a][b].numberValue[0] = value;
}

void PostProcess::setUniform(const std::string& name, glm::vec2 value, const Shader& shader)
{
    CRO_ASSERT(shader.getUniformMap().count(name) != 0, "Uniform doesn't exist");
    
    auto a = shader.getGLHandle();
    auto b = shader.getUniformMap().find(name)->second;
    m_uniforms[a][b].type = UniformData::Number2;
    m_uniforms[a][b].numberValue[0] = value.x;
    m_uniforms[a][b].numberValue[1] = value.y;
}

void PostProcess::setUniform(const std::string& name, glm::vec3 value, const Shader& shader)
{
    CRO_ASSERT(shader.getUniformMap().count(name) != 0, "Uniform doesn't exist");

    auto a = shader.getGLHandle();
    auto b = shader.getUniformMap().find(name)->second;
    m_uniforms[a][b].type = UniformData::Number3;
    m_uniforms[a][b].numberValue[0] = value.x;
    m_uniforms[a][b].numberValue[1] = value.y;
    m_uniforms[a][b].numberValue[2] = value.z;
}

void PostProcess::setUniform(const std::string& name, glm::vec4 value, const Shader& shader)
{
    CRO_ASSERT(shader.getUniformMap().count(name) != 0, "Uniform doesn't exist");
    
    auto a = shader.getGLHandle();
    auto b = shader.getUniformMap().find(name)->second;
    m_uniforms[a][b].type = UniformData::Number4;
    m_uniforms[a][b].numberValue[0] = value.x;
    m_uniforms[a][b].numberValue[1] = value.y;
    m_uniforms[a][b].numberValue[2] = value.z;
    m_uniforms[a][b].numberValue[3] = value.w;
}

void PostProcess::setUniform(const std::string& name, Colour value, const Shader& shader)
{
    CRO_ASSERT(shader.getUniformMap().count(name) != 0, "Uniform doesn't exist");

    auto a = shader.getGLHandle();
    auto b = shader.getUniformMap().find(name)->second;
    m_uniforms[a][b].type = UniformData::Number4;
    m_uniforms[a][b].numberValue[0] = value.getRed();
    m_uniforms[a][b].numberValue[1] = value.getGreen();
    m_uniforms[a][b].numberValue[2] = value.getBlue();
    m_uniforms[a][b].numberValue[3] = value.getAlpha();
}

void PostProcess::setUniform(const std::string& name, const Texture& value, const Shader& shader)
{
    CRO_ASSERT(shader.getUniformMap().count(name) != 0, "Uniform doesn't exist");
    
    auto a = shader.getGLHandle();
    auto b = shader.getUniformMap().find(name)->second;
    m_uniforms[a][b].type = UniformData::Texture;
    m_uniforms[a][b].textureID = value.getGLHandle();
}