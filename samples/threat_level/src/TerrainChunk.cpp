/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2025
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

#include "TerrainChunk.hpp"
#include "Messages.hpp"
#include "ErrorCheck.hpp"

#include <crogine/util/Random.hpp>
#include <crogine/util/Wavetable.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/App.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>

#include <crogine/detail/glm/gtc/noise.hpp>


namespace
{
    const float chunkWidth = 21.3f;
    const float chunkHeight = 5.6f;

    const std::array<float, 13> curve = 
    {
        {0.4f, 0.55f, 0.65f, 0.8f, 0.9f, 0.95f,
        1.f,
        0.95f, 0.9f, 0.8f, 0.65f, 0.55f, 0.4f} 
    };

    const glm::vec3 rimColour(0.1f, 0.16f, 0.26f);
}

ChunkSystem::ChunkSystem(cro::MessageBus& mb)
    : cro::System   (mb, typeid(ChunkSystem)),
    m_speed         (0.f),
    m_currentSpeed  (0.f),
    m_offset        (0.f),
    m_topX          (cro::Util::Random::value(-2343.f, 5637.f)),
    m_bottomX       (cro::Util::Random::value(-3965.f, 2842.f)),
    m_lastTop       (0.f),
    m_lastBottom    (0.f),
    m_topIndexShort (0),
    m_topIndexLong  (0)
{
    requireComponent<cro::Model>();
    requireComponent<cro::Transform>();
    requireComponent<TerrainChunk>();

    //build wave tables
    m_shortWavetable = cro::Util::Wavetable::sine(5.f, 0.07f);
    m_longWaveTable = cro::Util::Wavetable::sine(1.6f, 0.21f);
}

//public
void ChunkSystem::process(float dt)
{
    auto& entities = getEntities();

    //move each entity according to current speed
    m_currentSpeed += ((m_speed - m_currentSpeed) * dt);

    for (auto& e : entities)
    {
        auto& tx = e.getComponent<cro::Transform>();
        tx.move({ (-m_currentSpeed + m_offset) * dt, 0.f, 0.f });


        //if out of view move by one width and rebuild from current coords
        //assuming origin is in centre this become -chunkWidth
        if (tx.getPosition().x < -chunkWidth)
        {
            tx.move({ chunkWidth * 2.f, 0.f, 0.f });
            rebuildChunk(e);
        }
    }
}

void ChunkSystem::handleMessage(const cro::Message& msg)
{
    if (msg.id == MessageID::BackgroundSystem)
    {
        const auto& data = msg.getData<BackgroundEvent>();
        if (data.type == BackgroundEvent::SpeedChange)
        {
            m_speed = data.value * chunkWidth;
            m_offset = 0.f;
        }
        else if (data.type == BackgroundEvent::Shake)
        {
            m_offset = data.value * chunkWidth;
            //DPRINT("Offset", std::to_string(m_offset));
        }
    }
    else if (msg.id == MessageID::GameMessage)
    {
        const auto& data = msg.getData<GameEvent>();
        switch (data.type)
        {
        default: break;
        case GameEvent::RoundStart:
        {
            auto& ents = getEntities();
            for (auto& e : ents) rebuildChunk(e);
        }
            break;
        }
    }
}

//private
void ChunkSystem::rebuildChunk(cro::Entity entity)
{
    //0------2
    //|      |
    //|      |
    //1------3
    
    //generate points and store in terrain component.
    //these are also used for collision detection
    auto& chunkComponent = entity.getComponent<TerrainChunk>();
    std::size_t halfCount = chunkComponent.PointCount / 2u;
    
    const float spacing = chunkWidth / (halfCount - 1); //remember there are one fewer 'segments' than points
    for (auto i = 0u; i < halfCount; ++i)
    {
        float xPos =  -(chunkWidth / 2.f) + (spacing * i);
        auto noise = glm::perlin(glm::vec2(m_bottomX, m_bottomX)) * 0.4f;        
        m_bottomX++;
        
        static const float baseline = 2.1f;

        //bottom row
        chunkComponent.points[i] = { xPos, -baseline + 
            m_shortWavetable[(m_topIndexShort + 50) % m_shortWavetable.size()] - 
            m_longWaveTable[(m_topIndexLong + 30) % m_longWaveTable.size()] + noise };


        //top row
        noise = glm::simplex(glm::vec2(m_topX, m_topX)) * 0.5f;
        m_topX++;
        chunkComponent.points[i + halfCount] = { xPos, baseline + 
            m_shortWavetable[m_topIndexShort] - 
            m_longWaveTable[m_topIndexLong] - noise};

        m_topIndexShort = (m_topIndexShort + 1) % m_shortWavetable.size();
        m_topIndexLong = (m_topIndexLong + 1) % m_longWaveTable.size();
    }

    //add random humps
    std::size_t offset = ((cro::Util::Random::value(0, 1) % 2) == 0) ? 0 : halfCount;
    bool bottom = (offset == 0);
    offset += cro::Util::Random::value(0, static_cast<int>(halfCount - curve.size()));
    for (auto i = 0u; i < curve.size(); ++i)
    {
         bottom ? chunkComponent.points[i + offset].y += (curve[i] * cro::Util::Random::value(1.f, 1.6f))
             : chunkComponent.points[i + offset].y -= (curve[i] * cro::Util::Random::value(1.f, 1.6f));
    }

    //send a message with approx centre of hump to position this chunk's turret
    auto centreIndex = offset + (curve.size() / 2);
    auto msg = postMessage<BackgroundEvent>(MessageID::BackgroundSystem);
    msg->type = BackgroundEvent::MountCreated;
    msg->position = chunkComponent.points[centreIndex];
    bottom ? msg->position.y -= 1.f : msg->position.y += 1.f;
    msg->entityID = entity.getIndex();

    //store the last point so that it lines up with next chunk
    chunkComponent.points[0].y = m_lastBottom;
    chunkComponent.points[halfCount].y = m_lastTop;
    m_lastBottom = chunkComponent.points[halfCount - 1].y;
    m_lastTop = chunkComponent.points.back().y;


    //build mesh. first half of points are bottom chunk, then rest are for top
    std::vector<float> vertData;
    vertData.reserve((chunkComponent.PointCount * 2) * 5); //includes colour vals
    for (auto i = 0u; i < halfCount; ++i)
    {
        float zPos = static_cast<float>(i % 2);
        zPos *= 0.2f;
        
        //pos
        vertData.push_back(chunkComponent.points[i].x);
        vertData.push_back(chunkComponent.points[i].y);

        //colour
        vertData.push_back(rimColour.r);
        vertData.push_back(rimColour.g);
        vertData.push_back(rimColour.b);

        vertData.push_back(chunkComponent.points[i].x);
        vertData.push_back(-(chunkHeight / 2.f));
        
        vertData.push_back(0.f);
        vertData.push_back(0.f);
        vertData.push_back(0.f);
    }

    for (auto i = halfCount; i < chunkComponent.PointCount; ++i)
    {
        vertData.push_back(chunkComponent.points[i].x);
        vertData.push_back(chunkHeight / 2.f);
        
        vertData.push_back(0.f);
        vertData.push_back(0.f);
        vertData.push_back(0.f);

        vertData.push_back(chunkComponent.points[i].x);
        vertData.push_back(chunkComponent.points[i].y);

        vertData.push_back(rimColour.r);
        vertData.push_back(rimColour.g);
        vertData.push_back(rimColour.b);
    }

    //update the vertices   
    auto& mesh = entity.getComponent<cro::Model>().getMeshData();
    mesh.boundingSphere.radius = chunkWidth / 2.f; //else this will be culled from the scene

    //cro::Logger::log("Updating verts");
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, mesh.vboAllocation.vboID));
    glCheck(glBufferSubData(GL_ARRAY_BUFFER, 0, mesh.vertexCount * mesh.vertexSize, vertData.data()));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
}
