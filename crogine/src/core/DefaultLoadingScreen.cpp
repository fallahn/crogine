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

#include "DefaultLoadingScreen.hpp"
#include "../detail/GLCheck.hpp"

#include <crogine/core/App.hpp>

#include <crogine/detail/glm/gtc/type_ptr.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

#include <string>
#include <vector>

using namespace cro;

namespace
{
    const float rotation = -35.f;
    constexpr uint32 vertexSize = 2 * sizeof(float);

    const std::string vertex = R"(
        attribute vec4 a_position;

        uniform mat4 u_worldMatrix;
        uniform mat4 u_projectionMatrix;

        void main()
        {
            gl_Position = u_projectionMatrix * u_worldMatrix * a_position;
        }
    )";

    const std::string fragment = R"(
        void main()
        {
            gl_FragColor = vec4(1.0);
        }
    )";
}

DefaultLoadingScreen::DefaultLoadingScreen()
    : m_vbo         (0),
    m_transformIndex(0)
{
    m_viewport = App::getWindow().getSize();
    m_projectionMatrix = glm::ortho(0.f, static_cast<float>(m_viewport.x), 0.f, static_cast<float>(m_viewport.y), -0.1f, 10.f);
    
    if (m_shader.loadFromString(vertex, fragment))
    {
        const auto& uniforms = m_shader.getUniformMap();
        m_transformIndex = uniforms.find("u_worldMatrix")->second;
        m_transform = glm::translate(glm::mat4(), { 60.f, 60.f, 0.f });
        m_transform = glm::scale(m_transform, { 60.f, 60.f, 1.f });

        glCheck(glUseProgram(m_shader.getGLHandle()));
        glCheck(glUniformMatrix4fv(uniforms.find("u_projectionMatrix")->second, 1, GL_FALSE, glm::value_ptr(m_projectionMatrix)));
        glCheck(glUseProgram(0));

        //create VBO
        //0------2
        //|      |
        //|      |
        //1------3

        std::vector<float> verts =
        {
            -0.5f, 0.5f,
            -0.5f, -0.5f,
            0.5f, 0.5f,
            0.5f, -0.5f,
        };
        glCheck(glGenBuffers(1, &m_vbo));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));
        glCheck(glBufferData(GL_ARRAY_BUFFER, vertexSize * verts.size(), verts.data(), GL_STATIC_DRAW));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
    }
}

DefaultLoadingScreen::~DefaultLoadingScreen()
{
    if (m_vbo)
    {
        glCheck(glDeleteBuffers(1, &m_vbo));
    }
}

//public
void DefaultLoadingScreen::update()
{
    m_transform = glm::rotate(m_transform, rotation * m_clock.restart().asSeconds(), { 0.f, 0.f, 1.f });
}

void DefaultLoadingScreen::draw()
{
    int32 oldView[4];
    glCheck(glGetIntegerv(GL_VIEWPORT, oldView));

    glCheck(glViewport(0, 0, m_viewport.x, m_viewport.y));
    glCheck(glUseProgram(m_shader.getGLHandle()));
    glCheck(glUniformMatrix4fv(m_transformIndex, 1, GL_FALSE, glm::value_ptr(m_transform)));

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));

    const auto& attribs = m_shader.getAttribMap();
    glCheck(glEnableVertexAttribArray(attribs[0]));
    glCheck(glVertexAttribPointer(attribs[0], 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(vertexSize), reinterpret_cast<void*>(static_cast<intptr_t>(0))));

    glCheck(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));

    glCheck(glDisableVertexAttribArray(attribs[0]));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    glCheck(glUseProgram(0));
    glCheck(glViewport(oldView[0], oldView[1], oldView[2], oldView[3]));
}