/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/graphics/MeshData.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/App.hpp>

#include "../../detail/GLCheck.hpp"
#include "../../graphics/shaders/Sprite.hpp"

#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/glm/gtc/type_ptr.hpp>

#include <cfloat>

using namespace cro;

namespace
{
    uint32 MaxSprites = 127u;
    constexpr uint32 vertexSize = (4 + 4 + 2 + 2) * sizeof(float); //pos, colour, UV0, UV1
}

SpriteRenderer::SpriteRenderer(MessageBus& mb)
    : System            (mb, typeid(SpriteRenderer)),
    m_matrixIndex       (0),
    m_textureIndex      (0),
    m_projectionIndex   (0),
    m_depthAxis         (DepthAxis::Z),
    m_pendingRebuild    (false),
    m_pendingSorting    (true)
{
    //this has been known to fail on some platforms - but android can be as low as 64
    //which almost negates the usefulness of GPU bound transforms... :S
    GLint maxVec;
    glCheck(glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &maxVec));
    MaxSprites = maxVec / 4; //4 x 4-components make up a mat4.
    MaxSprites = std::min(MaxSprites - 1, 255u); //one component is used by the projection matrix
    LOG(std::to_string(MaxSprites) + " sprites are available per batch", Logger::Type::Info);
    
    //load shader
    if (!m_shader.loadFromString(Shaders::Sprite::Vertex, Shaders::Sprite::Fragment, "#define MAX_MATRICES " + std::to_string(MaxSprites) + "\n"))
    {
        Logger::log("Failed loading sprite rendering shader. Rendering will be in invalid state.", Logger::Type::Error, Logger::Output::All);
    }

    //get shader texture uniform loc
    const auto& uniforms = m_shader.getUniformMap();
    auto listUniforms = [&uniforms]()
    {
        for (const auto& p : uniforms)
        {
            Logger::log(p.first, Logger::Type::Info);
        }
    };
    if (uniforms.count("u_worldMatrix[0]") != 0)
    {
        m_matrixIndex = uniforms.find("u_worldMatrix[0]")->second;
    }
    else
    {
        Logger::log("u_worldMatrix uniform missing from sprite renderer. Available uniforms are: ", Logger::Type::Info);
        listUniforms();
    }

    if (uniforms.count("u_texture") != 0)
    {
        m_textureIndex = uniforms.find("u_texture")->second;
    }
    else
    {
        Logger::log("u_texture uniform missing from sprite renderer. Available uniforms are: ", Logger::Type::Info);
        listUniforms();
    }

    if (uniforms.count("u_projectionMatrix") != 0)
    {
        m_projectionIndex = uniforms.find("u_projectionMatrix")->second;
    }
    else
    {
        Logger::log("u_projectionMatrix uniform missing from sprite renderer. Available uniforms are: ", Logger::Type::Info);
        listUniforms();
    }

    //map shader attribs
    const auto& attribMap = m_shader.getAttribMap();
    m_attribMap[AttribLocation::Position].size = 4;
    m_attribMap[AttribLocation::Position].location = attribMap[Mesh::Position];
    m_attribMap[AttribLocation::Colour].size = 4;
    m_attribMap[AttribLocation::Colour].location = attribMap[Mesh::Colour];
    m_attribMap[AttribLocation::Colour].offset = m_attribMap[AttribLocation::Colour].size * sizeof(float);
    m_attribMap[AttribLocation::UV0].size = 2;
    m_attribMap[AttribLocation::UV0].location = attribMap[Mesh::UV0];
    m_attribMap[AttribLocation::UV0].offset = m_attribMap[AttribLocation::Colour].offset + (m_attribMap[AttribLocation::Colour].size * sizeof(float));
    m_attribMap[AttribLocation::UV1].size = 2;
    m_attribMap[AttribLocation::UV1].location = attribMap[Mesh::UV1];
    m_attribMap[AttribLocation::UV1].offset = m_attribMap[AttribLocation::UV0].offset + (m_attribMap[AttribLocation::UV0].size * sizeof(float));

    //only want these entities
    requireComponent<Sprite>();
    requireComponent<Transform>();
}

SpriteRenderer::~SpriteRenderer()
{
    for (auto& p : m_buffers)
    {
        glCheck(glDeleteBuffers(1, &p.first.vbo));

#ifdef PLATFORM_DESKTOP
        if(p.first.vao)
        {
            glCheck(glDeleteVertexArrays(1, &p.first.vao));
        }
#endif //PLATFORM
    }

#ifdef DEBUG_DRAW
    glCheck(glDeleteBuffers(1, &m_debugVBO));
#endif //DEBUG_DRAW
}

//public
void SpriteRenderer::handleMessage(const Message& msg)
{

}

void SpriteRenderer::process(float)
{ 
    //have to do this first, at least once
    if (m_pendingRebuild)
    {
        rebuildBatch();
    }
    
    //get list of entities
    auto& entities = getEntities();
    for (auto i = 0u; i < entities.size(); ++i)
    {
        auto& sprite = entities[i].getComponent<Sprite>();        
        if (sprite.m_dirty)
        {
            //check for culling
            /*bool state = sprite.m_visible;
            sprite.m_visible = m_viewPort.intersects(static_cast<IntRect>(sprite.getGlobalBounds()));*/
            //if (sprite.m_visible != state)
            //{
            //    m_pendingRebuild = true;
            //}
            //if (m_pendingRebuild) continue;
            
            //update buffer subdata
            //TODO depending on how often this happens it might be worth
            //double buffering the VBOs


            //-----TODO THIS DOESN'T UPDATE THE DEGEN TRIANGLES CORRECTLY----//

            //create data
            //std::vector<float> vertexData;
            //auto copyVertex = [&](uint32 idx)
            //{
            //    vertexData.push_back(sprite.m_quad[idx].position.x);
            //    vertexData.push_back(sprite.m_quad[idx].position.y);
            //    vertexData.push_back(sprite.m_quad[idx].position.z);
            //    vertexData.push_back(1.f);

            //    vertexData.push_back(sprite.m_quad[idx].colour.r);
            //    vertexData.push_back(sprite.m_quad[idx].colour.g);
            //    vertexData.push_back(sprite.m_quad[idx].colour.b);
            //    vertexData.push_back(sprite.m_quad[idx].colour.a);
            //    
            //    vertexData.push_back(sprite.m_quad[idx].UV.x);
            //    vertexData.push_back(sprite.m_quad[idx].UV.y);

            //    vertexData.push_back(static_cast<float>(i)); //for transform lookup
            //    vertexData.push_back(0.f);
            //};

            ////update vert data - TODO update degen!!
            //for (auto j = 0; j < 4; ++j)
            //{
            //    copyVertex(j);
            //}

            ////find offset (and VBO id)
            //auto vboIdx = (i > MaxSprites) ? i % MaxSprites : 0;

            ////update sub data
            //glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_buffers[vboIdx].first));
            //glCheck(glBufferSubData(GL_ARRAY_BUFFER, sprite.m_vboOffset,
            //    vertexData.size() * sizeof(float), vertexData.data()));
            //glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

            //updateGlobalBounds(sprite, entities[i].getComponent<Transform>().getWorldTransform());

            //---------------------EOF---------------------//

            sprite.m_dirty = false;
            m_pendingRebuild = true;
        }

        if (sprite.m_needsSorting)
        {
            sprite.m_needsSorting = false;
            m_pendingRebuild = true;
            m_pendingSorting = true;
        }

        //if (sprite.m_visible)
        {
            auto& tx = entities[i].getComponent<Transform>();
            //if depth sorted set Z to -Y
            if (m_depthAxis == DepthAxis::Y)
            {
                auto pos = tx.getPosition();
                pos.z = -(pos.y / 100.f); //reduce this else we surpass clip plane
                tx.setPosition(pos);
            }
            //get current transforms
            std::size_t buffIdx = (i > MaxSprites) ? i % MaxSprites : 0;
            m_bufferTransforms[buffIdx][i - (buffIdx * MaxSprites)] = tx.getWorldTransform();
        }
    }

    if (m_pendingSorting)
    {
        //sort by texture then blend mode
        std::sort(std::begin(entities), std::end(entities),
            [](const Entity& a, const Entity& b)
        {
            //const auto& sprA = a.getComponent<Sprite>();
            //const auto& sprB = b.getComponent<Sprite>();

            //sub-sort by depth
            if (a.getComponent<cro::Transform>().getWorldPosition().z < b.getComponent<cro::Transform>().getWorldPosition().z) return true;  

            //HMMMM grouping by texture breaks depth sorting :(

            /*if (sprA.m_textureID < sprB.m_textureID) return true;
            if (sprB.m_textureID < sprA.m_textureID) return false;

            if (sprA.m_blendMode < sprB.m_blendMode) return true;
            if (sprB.m_blendMode < sprA.m_blendMode) return false;*/

            return false;
        });
        m_pendingSorting = false;
    }
}

void SpriteRenderer::render(Entity camera)
{
    const auto& camComponent = camera.getComponent<Camera>();
    //applyViewport(camComponent.viewport);
    
    //bind shader and attrib arrays
    glCheck(glUseProgram(m_shader.getGLHandle()));
    glCheck(glUniformMatrix4fv(m_projectionIndex, 1, GL_FALSE, glm::value_ptr(camComponent.viewProjectionMatrix)));
    glCheck(glActiveTexture(GL_TEXTURE0));
    glCheck(glUniform1i(m_textureIndex, 0));

    //foreach vbo bind and draw
    std::size_t idx = 0;
    for (const auto& [batchMap, batch] : m_buffers)
    {
        if (batch.empty())
        {
            continue;
        }

        const auto& transforms = m_bufferTransforms[idx++]; //TODO this should be same index as current buffer
        glCheck(glUniformMatrix4fv(m_matrixIndex, static_cast<GLsizei>(transforms.size()), GL_FALSE, glm::value_ptr(transforms[0])));

#ifdef PLATFORM_DESKTOP
        glCheck(glBindVertexArray(batchMap.vao));
        for (const auto& batchData : batch)
        {
            //CRO_ASSERT(batchData.texture > -1, "Missing sprite texture!");
            applyBlendMode(batchData.blendMode);
            glCheck(glBindTexture(GL_TEXTURE_2D, batchData.texture));
            glCheck(glDrawArrays(GL_TRIANGLE_STRIP, batchData.start, batchData.count));
        }
        glCheck(glBindVertexArray(0));
#else
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, batchMap.vbo));
        
        //bind attrib pointers
        for (auto i = 0u; i < m_attribMap.size(); ++i)
        {
            glCheck(glEnableVertexAttribArray(m_attribMap[i].location));
            glCheck(glVertexAttribPointer(m_attribMap[i].location, m_attribMap[i].size, GL_FLOAT, GL_FALSE, vertexSize, 
                reinterpret_cast<void*>(static_cast<intptr_t>(m_attribMap[i].offset))));      
        }

        for (const auto& batchData : batch)
        {
            //CRO_ASSERT(batchData.texture > -1, "Missing sprite texture!");
            applyBlendMode(batchData.blendMode);
            glCheck(glBindTexture(GL_TEXTURE_2D, batchData.texture));
            glCheck(glDrawArrays(GL_TRIANGLE_STRIP, batchData.start, batchData.count));
        }
  
        //unbind attrib pointers
        for (auto i = 0u; i < m_attribMap.size(); ++i)
        {
            glCheck(glDisableVertexAttribArray(m_attribMap[i].location));
        }
#endif //PLATFORM
    }
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));


    glCheck(glDisable(GL_DEPTH_TEST));
    glCheck(glDisable(GL_CULL_FACE));
    glCheck(glDisable(GL_BLEND));
    glCheck(glDepthMask(GL_TRUE));
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
        BatchMap batchMap;
        glCheck(glGenBuffers(1, &batchMap.vbo));

#ifdef PLATFORM_DESKTOP
        glCheck(glGenVertexArrays(1, &batchMap.vao));
        glCheck(glBindVertexArray(batchMap.vao));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, batchMap.vbo));

        for (auto j = 0u; j < m_attribMap.size(); ++j)
        {
            glCheck(glEnableVertexAttribArray(m_attribMap[j].location));
            glCheck(glVertexAttribPointer(m_attribMap[j].location, m_attribMap[j].size, GL_FLOAT, GL_FALSE, vertexSize,
                reinterpret_cast<void*>(static_cast<intptr_t>(m_attribMap[j].offset))));
        }

        glCheck(glEnableVertexAttribArray(0));
        glCheck(glBindVertexArray(0));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
#endif //PLATFORM

        m_buffers.emplace_back(std::make_pair(batchMap, std::vector<Batch>()));
    }

    //create each batch
    uint32 start = 0;
    uint32 batchIdx = 0;
    for (auto& [batchMap, batch] : m_buffers)
    {
        batch.clear();

        if (entities.empty())
        {
            continue;
        }

        const auto& firstSprite = entities[batchIdx].getComponent<Sprite>();
        
        Batch batchData;
        batchData.start = start;
        batchData.texture = firstSprite.m_textureID;
        batchData.blendMode = firstSprite.m_blendMode;

        int32 spritesThisBatch = 0;

        std::vector<float> vertexData;
        auto maxCount = std::min(static_cast<uint32>(entities.size()), batchIdx + MaxSprites);
        for (auto i = 0u; i < maxCount; ++i)
        {
            auto& sprite = entities[i + batchIdx].getComponent<Sprite>();

            if (sprite.m_textureID != batchData.texture
                || sprite.m_blendMode != batchData.blendMode)
            {
                //end the batch and start a new one for this buffer
                batchData.count = start - batchData.start;
                batch.push_back(batchData);

                batchData.start = start;
                batchData.texture = sprite.m_textureID;
                batchData.blendMode = sprite.m_blendMode;

                spritesThisBatch = 0;
            }

            //TODO this is repeated from process function...
            auto copyVertex = [&](uint32 idx)
            {
                vertexData.push_back(sprite.m_quad[idx].position.x);
                vertexData.push_back(sprite.m_quad[idx].position.y);
                vertexData.push_back(sprite.m_quad[idx].position.z);
                vertexData.push_back(1.f);

                vertexData.push_back(sprite.m_quad[idx].colour.r);
                vertexData.push_back(sprite.m_quad[idx].colour.g);
                vertexData.push_back(sprite.m_quad[idx].colour.b);
                vertexData.push_back(sprite.m_quad[idx].colour.a);

                vertexData.push_back(sprite.m_quad[idx].UV.x);
                vertexData.push_back(sprite.m_quad[idx].UV.y);

                vertexData.push_back(static_cast<float>(i)); //for transform lookup
                vertexData.push_back(77.f); //not used right now, just makes it easier to see in debugger
                //vertexData.push_back(1.f);
            };

            if (spritesThisBatch > 0)
            {
                //add degenerate tri
                start += 2;

                //copy last vert
                auto elementCount = vertexSize / sizeof(float);
                for (auto j = 0u; j < elementCount; ++j)
                {
                    vertexData.push_back(vertexData[vertexData.size() - elementCount]);
                }

                //copy impending vert
                copyVertex(0);
            }

            //increase the start point
            start += 4;

            //and append data
            sprite.m_vboOffset = static_cast<int32>(vertexData.size() * sizeof(float));
            for (auto j = 0; j < 4; ++j)
            {
                copyVertex(j);
            }
            spritesThisBatch++;

            updateGlobalBounds(sprite, entities[i + batchIdx].getComponent<Transform>().getWorldTransform());

            sprite.m_dirty = false;
        }
        batchIdx += MaxSprites;
        batchData.count = start - batchData.start;
        batch.push_back(batchData);

        //upload to VBO
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, batchMap.vbo));
        glCheck(glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_DYNAMIC_DRAW));
    }

    //allocate the space for transforms
    m_bufferTransforms.clear();
    m_bufferTransforms.resize(m_buffers.size());
    std::size_t i = 0;
    for (const auto& [batchMap, batch] : m_buffers)
    {
        if (!batch.empty())
        {
            m_bufferTransforms[i].resize((batch.back().start + batch.back().count) / 4);
        }
        i++;
    }

    m_pendingRebuild = false;
}

void SpriteRenderer::updateGlobalBounds(Sprite& sprite, const glm::mat4& transform)
{
    std::vector<glm::vec4> points = 
    {
        transform * glm::vec4(sprite.m_quad[0].position.x, sprite.m_quad[0].position.y, 0.f, 1.f),
        transform * glm::vec4(sprite.m_quad[1].position.x, sprite.m_quad[1].position.y, 0.f, 1.f),
        transform * glm::vec4(sprite.m_quad[2].position.x, sprite.m_quad[2].position.y, 0.f, 1.f),
        transform * glm::vec4(sprite.m_quad[3].position.x, sprite.m_quad[3].position.y, 0.f, 1.f)
    };

    sprite.m_globalBounds = { FLT_MAX, FLT_MAX, 0.f, 0.f };
    for (const auto& p : points)
    {
        if (sprite.m_globalBounds.left > p.x)
        {
            sprite.m_globalBounds.left = p.x;
        }
        if (sprite.m_globalBounds.bottom > p.y)
        {
            sprite.m_globalBounds.bottom = p.y;
        }
    }

    for (const auto& p : points)
    {
        auto width = p.x - sprite.m_globalBounds.left;
        if (width > sprite.m_globalBounds.width)
        {
            sprite.m_globalBounds.width = width;
        }

        auto height = p.y - sprite.m_globalBounds.bottom;
        if (height > sprite.m_globalBounds.height)
        {
            sprite.m_globalBounds.height = height;
        }
    }
}

void SpriteRenderer::applyBlendMode(Material::BlendMode mode)
{
    switch (mode)
    {
    default: break;
    case Material::BlendMode::Additive:
        glCheck(glEnable(GL_BLEND));
        glCheck(glEnable(GL_DEPTH_TEST));
        glCheck(glDepthMask(GL_FALSE));
        glCheck(glEnable(GL_CULL_FACE));
        glCheck(glBlendFunc(GL_ONE, GL_ONE));
        glCheck(glBlendEquation(GL_FUNC_ADD));
        break;
    case Material::BlendMode::Alpha:
        glCheck(glEnable(GL_CULL_FACE));
        glCheck(glEnable(GL_DEPTH_TEST));
        glCheck(glDepthMask(GL_FALSE));
        glCheck(glEnable(GL_BLEND));
        glCheck(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        glCheck(glBlendEquation(GL_FUNC_ADD));
        break;
    case Material::BlendMode::Multiply:
        glCheck(glEnable(GL_BLEND));
        glCheck(glEnable(GL_DEPTH_TEST));
        glCheck(glDepthMask(GL_FALSE));
        glCheck(glEnable(GL_CULL_FACE));
        glCheck(glBlendFunc(GL_DST_COLOR, GL_ZERO));
        glCheck(glBlendEquation(GL_FUNC_ADD));
        break;
    case Material::BlendMode::None:
        glCheck(glEnable(GL_DEPTH_TEST));
        glCheck(glDepthMask(GL_TRUE));
        glCheck(glEnable(GL_CULL_FACE));
        glCheck(glDisable(GL_BLEND));
        break;
    }
}

void SpriteRenderer::onEntityAdded(Entity entity)
{
    m_pendingRebuild = true;
    m_pendingSorting = true;
}

void SpriteRenderer::onEntityRemoved(Entity entity)
{
    m_pendingRebuild = true;
}
