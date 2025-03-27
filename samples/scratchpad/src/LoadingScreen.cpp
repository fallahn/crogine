/*-----------------------------------------------------------------------

Matt Marchant 2021
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include "LoadingScreen.hpp"

#include <crogine/core/App.hpp>
#include <crogine/detail/OpenGL.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/util/Wavetable.hpp>

#include <crogine/detail/glm/gtc/type_ptr.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

#include <string>
#include <vector>

#define glCheck(x) x

namespace
{
    constexpr std::uint32_t vertexSize = 2 * sizeof(float);
    constexpr float timestep = 1.f / 60.f;

    const std::string vertex = R"(
        ATTRIBUTE vec4 a_position;

        uniform mat4 u_worldMatrix;
        uniform mat4 u_projectionMatrix;

        VARYING_OUT vec2 v_texCoord;

        void main()
        {
            gl_Position = u_projectionMatrix * u_worldMatrix * a_position;
            v_texCoord = a_position.xy;
        }
    )";

    const std::string fragment = R"(
        uniform sampler2D u_texture;
        
        VARYING_IN vec2 v_texCoord;
        OUTPUT

        void main()
        {
            FRAG_OUT = TEXTURE(u_texture, v_texCoord);
        }
    )";
}

LoadingScreen::LoadingScreen()
    : m_vao             (0),
    m_vbo               (0),
    m_transformIndex    (-1),
    m_transform         (1.f),
    m_projectionMatrix  (1.f),
    m_wavetableIndex    (0)
{
    m_viewport = cro::App::getWindow().getSize();
    m_projectionMatrix = glm::ortho(0.f, static_cast<float>(m_viewport.x), 0.f, static_cast<float>(m_viewport.y), -0.1f, 10.f);

    m_wavetable = cro::Util::Wavetable::sine(2.f, 3.f);

    //TODO if the texture fails we should load a version of the shader with defines which dont require a texture
    if (!m_texture.loadFromFile("assets/images/loading.png"))
    {
        cro::Image img;
        img.create(12, 12, cro::Colour::Magenta);
        m_texture.loadFromImage(img);
    }

    if (m_shader.loadFromString(vertex, fragment))
    {
        const auto& uniforms = m_shader.getUniformMap();
        m_transformIndex = uniforms.find("u_worldMatrix")->second.first;
        m_transform = glm::translate(glm::mat4(1.f), { 60.f, 60.f, 0.f });
        m_transform = glm::scale(m_transform, { 128.f, 64.f, 1.f });

        glCheck(glUseProgram(m_shader.getGLHandle()));
        glCheck(glUniformMatrix4fv(uniforms.find("u_projectionMatrix")->second.first, 1, GL_FALSE, glm::value_ptr(m_projectionMatrix)));
        glCheck(glUniform1i(uniforms.find("u_texture")->second.first, 0));
        glCheck(glUseProgram(0));

        //create VBO
        //0------2
        //|      |
        //|      |
        //1------3

        std::vector<float> verts =
        {
            0.f, 1.f,
            0.f, 0.f,
            1.f, 1.f,
            1.f, 0.f,
        };
        glCheck(glGenBuffers(1, &m_vbo));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));
        glCheck(glBufferData(GL_ARRAY_BUFFER, vertexSize * verts.size(), verts.data(), GL_STATIC_DRAW));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
    }
}

LoadingScreen::~LoadingScreen()
{
    if (m_vbo)
    {
        glCheck(glDeleteBuffers(1, &m_vbo));
    }

    if (m_vao)
    {
        glCheck(glDeleteVertexArrays(1, &m_vao));
    }
}

//public
void LoadingScreen::launch()
{
    if (!m_vao)
    {
        CRO_ASSERT(m_vbo, "");

        glCheck(glGenVertexArrays(1, &m_vao));

        glCheck(glBindVertexArray(m_vao));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));

        //pos
        glCheck(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, vertexSize, (void*)0));
        glCheck(glEnableVertexAttribArray(0));

        glCheck(glBindVertexArray(0));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
    }
}

void LoadingScreen::update()
{
    static float accumulator = 0.f;
    accumulator += m_clock.restart().asSeconds();
    
    while (accumulator > timestep)
    {
        m_transform = glm::translate(m_transform, { 0.f, m_wavetable[m_wavetableIndex] * timestep, 0.f });
        m_wavetableIndex = (m_wavetableIndex + 1) % m_wavetable.size();
        
        accumulator -= timestep;
    }
}

void LoadingScreen::draw()
{
    std::int32_t oldView[4];
    glCheck(glGetIntegerv(GL_VIEWPORT, oldView));
    glCheck(glEnable(GL_BLEND));
    glCheck(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    glCheck(glBlendEquation(GL_FUNC_ADD));

    glCheck(glViewport(0, 0, m_viewport.x, m_viewport.y));
    glCheck(glUseProgram(m_shader.getGLHandle()));
    glCheck(glUniformMatrix4fv(m_transformIndex, 1, GL_FALSE, glm::value_ptr(m_transform)));

    glCheck(glActiveTexture(GL_TEXTURE0));
    glCheck(glBindTexture(GL_TEXTURE_2D, m_texture.getGLHandle()));

    glCheck(glBindVertexArray(m_vao));
    glCheck(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));

    glCheck(glUseProgram(0));
    glCheck(glViewport(oldView[0], oldView[1], oldView[2], oldView[3]));
    glCheck(glDisable(GL_BLEND));
}