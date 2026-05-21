/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include "DebugDraw.hpp"

#include <crogine/core/App.hpp>

#include <string>

#include "../ErrorCheck.hpp"

namespace
{
    const std::string vertex = R"(
        ATTRIBUTE MED vec4 a_position;
        ATTRIBUTE LOW vec4 a_colour;
            
        uniform mat4 u_viewProjectionMatrix;

        VARYING_OUT LOW vec4 v_colour;

        void main()
        {
            v_colour = a_colour;
            gl_Position = u_viewProjectionMatrix * a_position;
        })";

    const std::string fragment = R"(
        VARYING_IN LOW vec4 v_colour;
        OUTPUT

        void main()
        {
            FRAG_OUT = v_colour;
        })";

    static constexpr std::size_t MaxVertices = 500000;
    static constexpr std::uint32_t VertSize = (3 * sizeof(float)) + (3 * sizeof(float));
}

BulletDebug::BulletDebug()
    : m_vertexCount (0),
    m_uniformIndex  (-1),
    m_debugFlags    (0),
    m_clearVerts    (true)
{
#ifdef PLATFORM_DESKTOP
    if (m_shader.loadFromString(vertex, fragment))
    {
        m_vertexData.resize(MaxVertices);

        m_uniformIndex = m_shader.getUniformID("u_viewProjectionMatrix");

        glCheck(glGenVertexArrays(1, &m_meshData.vao));
        glCheck(glGenBuffers(1, &m_meshData.vbo));

        const auto& attribs = m_shader.getAttribMap();
        if (!attribs.empty())
        {
            glCheck(glBindVertexArray(m_meshData.vao));
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_meshData.vbo));
            glCheck(glBufferData(GL_ARRAY_BUFFER, VertSize * MaxVertices, nullptr, GL_DYNAMIC_DRAW));

            glCheck(glEnableVertexAttribArray(attribs.at(cro::Mesh::Attribute::Position)));
            glCheck(glVertexAttribPointer(attribs.at(cro::Mesh::Attribute::Position), 3, GL_FLOAT, GL_FALSE, VertSize, 0));

            glCheck(glEnableVertexAttribArray(attribs.at(cro::Mesh::Attribute::Colour)));
            glCheck(glVertexAttribPointer(attribs.at(cro::Mesh::Attribute::Colour), 3, GL_FLOAT, GL_FALSE, VertSize, reinterpret_cast<void*>(static_cast<std::intptr_t>(3 * sizeof(float)))));

            glCheck(glBindVertexArray(0));
        }
    }
#else
#pragma warn("Debug draw not available on mobile")
#endif
}

//public
BulletDebug::~BulletDebug()
{
#ifdef PLATFORM_DESKTOP
    if (m_meshData.vbo)
    {
        glCheck(glDeleteBuffers(1, &m_meshData.vbo));
    }

    if (m_meshData.vao)
    {
        glCheck(glDeleteVertexArrays(1, &m_meshData.vao));
    }
#endif
}

//public
void BulletDebug::drawLine(const btVector3& start, const btVector3& end, const btVector3& colour)
{
    if (m_clearVerts)
    {
        //rewind
        m_vertexCount = 0;
        m_clearVerts = false;
    }

    if (m_vertexCount < MaxVertices)
    {
        m_vertexData[m_vertexCount].position = { start.x(), start.y(), start.z() };
        m_vertexData[m_vertexCount++].colour = { colour.x(), colour.y(), colour.z() };

        m_vertexData[m_vertexCount].position = { end.x(), end.y(), end.z() };
        m_vertexData[m_vertexCount++].colour = { colour.x(), colour.y(), colour.z() };
    }
}

void BulletDebug::reportErrorWarning(const char* str)
{
    LogI << str << std::endl;
}

void BulletDebug::draw3dText(const btVector3& position, const char* text)
{
    //LogI << text << std::endl;
}

void BulletDebug::setDebugMode(int mode)
{
    m_debugFlags = mode;
    m_vertexCount = 0;
}

int BulletDebug::getDebugMode() const
{
    return m_debugFlags;
}

void BulletDebug::render(glm::mat4 viewProjection, glm::uvec2 targetSize)
{
#ifdef PLATFORM_DESKTOP
    if (m_vertexCount == 0) return;

    glCheck(glViewport(0, 0, targetSize.x, targetSize.y));

    //blend modes/depth testing
    glCheck(glEnable(GL_DEPTH_TEST));

    //bind buffer
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_meshData.vbo));

    //update VBO
    glCheck(glBufferSubData(GL_ARRAY_BUFFER, 0, m_vertexCount * sizeof(Vertex), (void*)m_vertexData.data()));

    //bind shader / set projection uniform
    glCheck(glUseProgram(m_shader.getGLHandle()));
    glCheck(glUniformMatrix4fv(m_uniformIndex, 1, GL_FALSE, &viewProjection[0][0]));

    //draw arrays
    glCheck(glBindVertexArray(m_meshData.vao));
    glCheck(glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_vertexCount)));

    //unbind
    glCheck(glUseProgram(0));
    glCheck(glDisable(GL_DEPTH_TEST));

    //DPRINT("Vert Count", std::to_string(m_vertexCount));
    m_clearVerts = true;
#endif
}