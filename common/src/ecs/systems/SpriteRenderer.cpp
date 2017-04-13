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

#include <crogine/ecs/systems/SpriteRenderer.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/graphics/MeshData.hpp>
#include <crogine/core/Clock.hpp>

#include "../../glad/glad.h"
#include "../../glad/GLCheck.hpp"
#include "../../graphics/shaders/Sprite.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace cro;

namespace
{
    uint32 MaxSprites = 0;
    constexpr uint32 vertexSize = (4 + 4 + 2 + 2) * sizeof(float); //pos, colour, UV0, UV1
}

SpriteRenderer::SpriteRenderer()
    : System        (this),
    m_matrixIndex   (0),
    m_textureIndex  (0),
    m_pendingRebuild(false)
{
    //load shader
    GLint maxVec;
    glCheck(glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &maxVec));
    MaxSprites = maxVec / 4; //4 x 4-components make up a mat4.
    MaxSprites -= 1;
	MaxSprites = 31;
    LOG(std::to_string(MaxSprites) + " sprites are available per batch", Logger::Type::Info);

    if (!m_shader.loadFromString(Shaders::Sprite::Vertex, Shaders::Sprite::Fragment, "#define MAX_MATRICES " + std::to_string(MaxSprites) + "\n"))
    {
        Logger::log("Failed loading sprite rendering shader. Rendering will be in invalid state.", Logger::Type::Error, Logger::Output::All);
    }

    //get shader texture uniform loc
    const auto& uniforms = m_shader.getUniformMap();
    for(const auto& p : uniforms)
    {
        Logger::log(p.first, Logger::Type::Info);
    }
    //m_matrixIndex = m_shader.getUniformMap().find("u_worldMatrix[0]")->second;
    //m_textureIndex = m_shader.getUniformMap().find("u_texture")->second;

    //map shader attribs
    const auto& attribMap = m_shader.getAttribMap();
    m_attribMap[AttribLocation::Position].size = 4;
    m_attribMap[AttribLocation::Position].location = attribMap[Mesh::Position];
    m_attribMap[AttribLocation::Colour].size = 4;
    m_attribMap[AttribLocation::Colour].location = attribMap[Mesh::Colour];
    m_attribMap[AttribLocation::UV0].size = 2;
    m_attribMap[AttribLocation::UV0].location = attribMap[Mesh::UV0];
    m_attribMap[AttribLocation::UV1].size = 2;
    m_attribMap[AttribLocation::UV1].location = attribMap[Mesh::UV1];

    //setup projection
    m_projectionMatrix = glm::ortho(0.f, 800.f, 0.f, 600.f, 0.1f, 10.f); //TODO get from current window size
    glCheck(glUseProgram(m_shader.getGLHandle()));
    glCheck(glUniformMatrix4fv(m_shader.getUniformMap().find("u_projectionMatrix")->second, 1, GL_FALSE, glm::value_ptr(m_projectionMatrix)));
    glCheck(glUseProgram(0));

    //only want these entities
    requireComponent<Sprite>();
    requireComponent<Transform>();
}

SpriteRenderer::~SpriteRenderer()
{
    for (auto& p : m_buffers)
    {
        glCheck(glDeleteBuffers(1, &p.first));
    }
}

//public
void SpriteRenderer::process(Time)
{
    if (m_pendingRebuild)
    {
        rebuildBatch();
    }    
    
    //get list of entities (should already be sorted by addEnt callback)
    auto& entities = getEntities();

    bool rebatch = false;
    for (auto i = 0u; i < entities.size(); ++i)
    {
        //check for dirty flag
        //rebatch all sprites with texture, check for resizing
        auto& sprite = entities[i].getComponent<Sprite>();
        if (sprite.m_dirty)
        {
            rebatch = true;
            sprite.m_dirty = false;
        }

        //if depth sorted set Z to -Y
        auto tx = entities[i].getComponent<Transform>();
        auto pos = tx.getPosition();
        pos.z = -pos.y;
        tx.setPosition(pos);

        //get current transforms
        std::size_t buffIdx = (i > MaxSprites) ? i % MaxSprites : 0;
        //m_bufferTransforms[buffIdx][i - (buffIdx * MaxSprites)] = tx.getWorldTransform(entities);
    }

    //TODO
    //if(rebatch){updateSubdata();}
}

void SpriteRenderer::render()
{
    //TODO enable / disable depth testing as per setting
    //glCheck(glDisable(GL_CULL_FACE));
    glCheck(glEnable(GL_DEPTH_TEST));
    //glViewport(0, 0, 800, 600);

    //bind shader and attrib arrays
    glCheck(glUseProgram(m_shader.getGLHandle()));
    glCheck(glActiveTexture(GL_TEXTURE0));
    //glCheck(glUniform1i(m_textureIndex, 0));

    //bind attrib pointers
    int offset = 0;
    for (auto i = 0u; i < m_attribMap.size(); ++i)
    {
        glCheck(glEnableVertexAttribArray(m_attribMap[i].location));
        glCheck(glVertexAttribPointer(m_attribMap[i].location, m_attribMap[i].size, GL_FLOAT, GL_FALSE, vertexSize, 
            reinterpret_cast<void*>(static_cast<intptr_t>(offset))));
        offset += m_attribMap[i].size;
    }

    //foreach vbo bind and draw
    std::size_t idx = 0;
    for (const auto& batch : m_buffers)
    {
        const auto& transforms = m_bufferTransforms[0];
        //glCheck(glUniformMatrix4fv(m_matrixIndex, static_cast<GLsizei>(transforms.size()), GL_FALSE, glm::value_ptr(transforms[0])));

        glCheck(glBindBuffer(GL_ARRAY_BUFFER, batch.first));
        for (const auto& batchData : batch.second)
        {
            CRO_ASSERT(batchData.texture > -1, "Missing sprite texture!");
            glCheck(glBindTexture(GL_TEXTURE_2D, batchData.texture));
            glCheck(glDrawArrays(GL_TRIANGLE_STRIP, batchData.start, batchData.end));
        }
        idx++;
    }

    //unbind attrib pointers
    for (auto i = 0u; i < m_attribMap.size(); ++i)
    {
        glCheck(glDisableVertexAttribArray(m_attribMap[i].location));
    }

    
}

//private
void SpriteRenderer::rebuildBatch()
{
    auto& entities = getEntities();
    auto vboCount = (entities.size() > MaxSprites) ? (entities.size() % MaxSprites) + 1 : 1;

    //allocate VBOs if needed
    auto neededVBOs = vboCount - m_buffers.size();
    for (auto i = 0u; i < neededVBOs; ++i)
    {
        uint32 vbo;
        glCheck(glGenBuffers(1, &vbo));
        m_buffers.insert(std::make_pair(vbo, std::vector<Batch>()));
    }

    //create each batch
    uint32 start = 0;
    uint32 batchIdx = 0;
    for (auto& batch : m_buffers)
    {
        Batch batchData;
        batchData.start = start;
        batchData.texture = entities[batchIdx].getComponent<Sprite>().m_textureID;

        std::vector<float> vertexData;
        auto maxCount = std::min(static_cast<uint32>(entities.size()), batchIdx + MaxSprites);
        for (auto i = 0u; i < maxCount; ++i)
        {
            const auto& sprite = entities[i + batchIdx].getComponent<Sprite>();

            if (sprite.m_textureID != batchData.texture)
            {
                //end the batch and start a new one for this buffer
                batchData.end = start - 1;
                batch.second.push_back(batchData);

                batchData.start = start;
                batchData.texture = sprite.m_textureID;
            }


            auto copyVertex = [&](uint32 idx)
            {
                vertexData.push_back(sprite.m_quad[idx].position.x);
                vertexData.push_back(sprite.m_quad[idx].position.y);
                vertexData.push_back(sprite.m_quad[idx].position.z);
                vertexData.push_back(sprite.m_quad[idx].position.w);

                vertexData.push_back(sprite.m_quad[idx].colour.r);
                vertexData.push_back(sprite.m_quad[idx].colour.g);
                vertexData.push_back(sprite.m_quad[idx].colour.b);
                vertexData.push_back(sprite.m_quad[idx].colour.a);

                vertexData.push_back(sprite.m_quad[idx].UV.x);
                vertexData.push_back(sprite.m_quad[idx].UV.y);

                vertexData.push_back(static_cast<float>(i)); //for transform lookup
                vertexData.push_back(0.f);
            };

            if (i > 0)
            {
                //add degenerate tri
                start += 2;

                //copy last vert
                for (auto j = 0u; j < vertexSize; ++j)
                {
                    vertexData.push_back(vertexData[vertexData.size() - vertexSize]);
                }

                //copy impending vert
                copyVertex(0);
            }

            //increase the start point
            start += 4;
            //and append data
            for (auto j = 0; j < 4; ++j) copyVertex(j);
        }
        batchIdx += MaxSprites;
        batchData.end = start - 1;
        batch.second.push_back(batchData);

        //upload to VBO
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, batch.first));
        glCheck(glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_DYNAMIC_DRAW));
    }

    //allocate the space for transforms
    m_bufferTransforms.clear();
    m_bufferTransforms.resize(m_buffers.size());
    std::size_t i = 0;
    for (const auto& buffer : m_buffers)
    {
        m_bufferTransforms[i].resize(buffer.second.back().end / 4);
        i++;
    }

    m_pendingRebuild = false;
}

void SpriteRenderer::onEntityAdded(Entity entity)
{
    auto& entities = getEntities();
    std::sort(entities.begin(), entities.end(), 
        [](Entity& a, Entity& b)
    {
        return (a.getComponent<Sprite>().m_textureID > b.getComponent<Sprite>().m_textureID);
    });

    m_pendingRebuild = true;
}

void SpriteRenderer::onEntityRemoved(Entity entity)
{
    m_pendingRebuild = true;
}
