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

#include "PhysicsDebug.hpp"
#include "ErrorCheck.hpp"

#include <crogine/graphics/MeshData.hpp>

using namespace cro;
using namespace cro::Detail;

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

    const std::size_t vertexSize = 6; //3 pos, 3 colour
    const std::size_t maxVertices = 10000;

    const std::uint32_t vertexStride = vertexSize * sizeof(float);
    const std::uint32_t vertexColourOffset = 3 * sizeof(float);
}

BulletDebug::BulletDebug()
    : m_vboID       (0),
    m_vertexCount   (0),
    m_uniformIndex  (-1),
    m_debugFlags    (0),
    m_clearVerts    (true)
{
    if (m_shader.loadFromString(vertex, fragment))
    {
        //if (uniforms.count("u_viewProjectionMatrix") != 0)
        {
            m_uniformIndex = m_shader.getUniformID("u_viewProjectionMatrix");
            m_attribIndices[0] = m_shader.getAttribMap()[Mesh::Attribute::Position];
            m_attribIndices[1] = m_shader.getAttribMap()[Mesh::Attribute::Colour];

            m_vertexData.resize(maxVertices * vertexSize);

            glCheck(glGenBuffers(1, &m_vboID));          
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vboID));
            glCheck(glBufferData(GL_ARRAY_BUFFER, m_vertexData.size() * sizeof(float), 0, GL_DYNAMIC_DRAW));
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
        }
        /*else
        {
            Logger::log("Cannot find matrix uniform in physics debug shader", Logger::Type::Error);
        }*/
    }
    else
    {
        Logger::log("Failed creating bullet debug shader", Logger::Type::Error);
    }
}

BulletDebug::~BulletDebug()
{
    if (m_vboID)
    {
        glCheck(glDeleteBuffers(1, &m_vboID));
    }
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

    if (m_vertexCount < maxVertices)
    {
        m_vertexData[m_vertexCount++] = start.x();
        m_vertexData[m_vertexCount++] = start.y();
        m_vertexData[m_vertexCount++] = start.z();
        m_vertexData[m_vertexCount++] = colour.x();
        m_vertexData[m_vertexCount++] = colour.y();
        m_vertexData[m_vertexCount++] = colour.z();

        m_vertexData[m_vertexCount++] = end.x();
        m_vertexData[m_vertexCount++] = end.y();
        m_vertexData[m_vertexCount++] = end.z();
        m_vertexData[m_vertexCount++] = colour.x();
        m_vertexData[m_vertexCount++] = colour.y();
        m_vertexData[m_vertexCount++] = colour.z();
    }
}

void BulletDebug::reportErrorWarning(const char* str)
{
    Logger::log(str, Logger::Type::Info);
}

void BulletDebug::setDebugMode(int mode)
{
    m_debugFlags = mode;
}

int BulletDebug::getDebugMode() const
{
    return m_debugFlags;
}

void BulletDebug::render(glm::mat4 viewProjection)
{
    //TODO use VAOs on desktop

    if (m_vertexCount == 0) return;

    //blend modes/depth testing
    glCheck(glEnable(GL_DEPTH_TEST));
    
    //bind buffer - TODO check if worth double buffering
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vboID));
    
    //update VBO
    glCheck(glBufferSubData(GL_ARRAY_BUFFER, 0, m_vertexCount * sizeof(float), (void*)m_vertexData.data()));

    //bind shader / set projection uniform
    glCheck(glUseProgram(m_shader.getGLHandle()));
    glCheck(glUniformMatrix4fv(m_uniformIndex, 1, GL_FALSE, &viewProjection[0][0]));

    //bind attribs
    glCheck(glEnableVertexAttribArray(m_attribIndices[0]));
    glCheck(glVertexAttribPointer(m_attribIndices[0], 3, GL_FLOAT, GL_FALSE, vertexStride, 0));

    glCheck(glEnableVertexAttribArray(m_attribIndices[1]));
    glCheck(glVertexAttribPointer(m_attribIndices[1], 3, GL_FLOAT, GL_FALSE, vertexStride, reinterpret_cast<void*>(static_cast<std::intptr_t>(vertexColourOffset))));

    //draw arrays
    glCheck(glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_vertexCount)));

    //unbind
    glCheck(glDisableVertexAttribArray(m_attribIndices[1]));
    glCheck(glDisableVertexAttribArray(m_attribIndices[0]));

    //unbind
    glCheck(glUseProgram(0));

    //unbind
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    glCheck(glDisable(GL_DEPTH_TEST));

    //DPRINT("Vert Count", std::to_string(m_vertexCount));
    m_clearVerts = true;
}