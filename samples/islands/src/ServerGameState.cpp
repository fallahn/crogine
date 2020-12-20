/*-----------------------------------------------------------------------

Matt Marchant 2020
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

#include "ServerGameState.hpp"
#include "PacketIDs.hpp"
#include "CommonConsts.hpp"
#include "ServerPacketData.hpp"
#include "ClientPacketData.hpp"
#include "PlayerSystem.hpp"
#include "ActorSystem.hpp"
#include "ServerMessages.hpp"
#include "GameConsts.hpp"

#include "fastnoise/FastNoiseSIMD.h"

#include <crogine/core/Log.hpp>

#include<crogine/ecs/components/Transform.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/detail/glm/vec3.hpp>

using namespace Sv;

namespace
{
    const float SpawnOffset = 24.f;
    const std::array PlayerSpawns =
    {
        glm::vec3(-SpawnOffset, 0.f, -SpawnOffset),
        glm::vec3(SpawnOffset, 0.f, -SpawnOffset),
        glm::vec3(-SpawnOffset, 0.f, SpawnOffset),
        glm::vec3(SpawnOffset, 0.f, SpawnOffset)
    };
}

GameState::GameState(SharedData& sd)
    : m_returnValue (StateID::Game),
    m_sharedData    (sd),
    m_scene         (sd.messageBus),
    m_heightmap     (IslandTileCount* IslandTileCount, 0.f)
{
    initScene();
    buildWorld();
    LOG("Entered Server Game State", cro::Logger::Type::Info);
}

void GameState::handleMessage(const cro::Message& msg)
{
    if (msg.id == Sv::MessageID::ConnectionMessage)
    {
        const auto& data = msg.getData<ConnectionEvent>();
        if (data.type == ConnectionEvent::Disconnected)
        {
            for (auto i = 0u; i < ConstVal::MaxClients; ++i)
            {
                if (m_playerEntities[data.playerID][i].isValid())
                {
                    auto entityID = m_playerEntities[data.playerID][i].getIndex();
                    m_scene.destroyEntity(m_playerEntities[data.playerID][i]);

                    m_sharedData.host.broadcastPacket(PacketID::EntityRemoved, entityID, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
                }
            }
        }
    }

    m_scene.forwardMessage(msg);
}

void GameState::netEvent(const cro::NetEvent& evt)
{
    if (evt.type == cro::NetEvent::PacketReceived)
    {
        switch (evt.packet.getID())
        {
        default: break;
        case PacketID::ClientReady:
            if (!m_sharedData.clients[evt.packet.as<std::uint8_t>()].ready)
            {
                sendInitialGameState(evt.packet.as<std::uint8_t>());
            }
            break;
        case PacketID::InputUpdate:
            handlePlayerInput(evt.packet);
            break;
        case PacketID::ServerCommand:
            doServerCommand(evt);
            break;
        }
    }
}

void GameState::netBroadcast()
{
    //send reconciliation for each player
    for (auto i = 0u; i < ConstVal::MaxClients; ++i)
    {
        if (m_sharedData.clients[i].connected)
        {
            for (auto j = 0u; j < m_sharedData.clients[i].playerCount; ++j)
            {
                if (m_playerEntities[i][j].isValid())
                {
                    const auto& player = m_playerEntities[i][j].getComponent<Player>();

                    PlayerUpdate update;
                    update.position = m_playerEntities[i][j].getComponent<cro::Transform>().getPosition();
                    update.rotation = Util::compressQuat(m_playerEntities[i][j].getComponent<cro::Transform>().getRotation());
                    update.timestamp = player.inputStack[player.lastUpdatedInput].timeStamp;
                    update.playerID = player.id;

                    m_sharedData.host.sendPacket(m_sharedData.clients[i].peer, PacketID::PlayerUpdate, update, cro::NetFlag::Unreliable);
                }
            }
        }
    }

    //broadcast other actor transforms
    //TODO - remind me how we're filtering out reconcilable entities from this?
    auto timestamp = m_serverTime.elapsed().asMilliseconds();
    const auto& actors = m_scene.getSystem<ActorSystem>().getEntities1();
    for (auto e : actors)
    {
        const auto& actor = e.getComponent<Actor>();
        const auto& tx = e.getComponent<cro::Transform>();

        ActorUpdate update;
        update.actorID = actor.id;
        update.serverID = actor.serverEntityId;
        update.position = tx.getPosition();
        update.rotation = Util::compressQuat(tx.getRotation());
        update.timestamp = timestamp;
        m_sharedData.host.broadcastPacket(PacketID::ActorUpdate, update, cro::NetFlag::Unreliable);
    }
}

std::int32_t GameState::process(float dt)
{
    m_scene.simulate(dt);
    return m_returnValue;
}

//private
void GameState::sendInitialGameState(std::uint8_t playerID)
{
    for (auto i = 0u; i < ConstVal::MaxClients; ++i)
    {
        if (m_sharedData.clients[i].connected)
        {
            //TODO name/skin info is sent on lobby join

            for (auto j = 0u; j < m_sharedData.clients[i].playerCount; ++j)
            {
                PlayerInfo info;
                info.playerID = j;
                info.spawnPosition = m_playerEntities[i][j].getComponent<cro::Transform>().getPosition();
                info.rotation = Util::compressQuat(m_playerEntities[i][j].getComponent<cro::Transform>().getRotation());
                info.serverID = m_playerEntities[i][j].getIndex();
                info.timestamp = m_serverTime.elapsed().asMilliseconds();
                info.connectionID = i;

                m_sharedData.host.sendPacket(m_sharedData.clients[playerID].peer, PacketID::PlayerSpawn, info, cro::NetFlag::Reliable);
            }
        }
    }


    //send map data to start building the world
    m_sharedData.host.sendPacket(m_sharedData.clients[playerID].peer, PacketID::Heightmap, m_heightmap.data(), m_heightmap.size() * sizeof(float), cro::NetFlag::Reliable);

    //client said it was ready, so mark as ready
    //TODO flag each part of this so we know when a client has player/world etc ready
    m_sharedData.clients[playerID].ready = true;
}

void GameState::handlePlayerInput(const cro::NetEvent::Packet& packet)
{
    auto input = packet.as<InputUpdate>();
    CRO_ASSERT(m_playerEntities[input.connectionID][input.playerID].isValid(), "Not a valid player!");
    auto& player = m_playerEntities[input.connectionID][input.playerID].getComponent<Player>();

    //only add new inputs
    auto lastIndex = (player.nextFreeInput + (Player::HistorySize - 1) ) % Player::HistorySize;
    if (input.input.timeStamp > (player.inputStack[lastIndex].timeStamp))
    {
        player.inputStack[player.nextFreeInput] = input.input;
        player.nextFreeInput = (player.nextFreeInput + 1) % Player::HistorySize;
    }
}

void GameState::doServerCommand(const cro::NetEvent& evt)
{
    //TODO validate this sender has permission to request commands
    //by checking evt peer against client data
    //TODO packet data needs to include connection ID

    //auto data = evt.packet.as<ServerCommand>();
    //if (data.target < m_sharedData.clients.size()
    //    && m_sharedData.clients[data.target].connected)
    //{
    //    switch (data.commandID)
    //    {
    //    default: break;
    //    case CommandPacket::SetModeFly:
    //        if (m_playerEntities[data.target].isValid())
    //        {
    //            m_playerEntities[data.target].getComponent<Player>().flyMode = true;
    //            m_sharedData.host.sendPacket(m_sharedData.clients[data.target].peer, PacketID::ServerCommand, data, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
    //        }
    //        break;
    //    case CommandPacket::SetModeWalk:
    //        if (m_playerEntities[data.target].isValid())
    //        {
    //            m_playerEntities[data.target].getComponent<Player>().flyMode = false;
    //            m_sharedData.host.sendPacket(m_sharedData.clients[data.target].peer, PacketID::ServerCommand, data, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
    //        }
    //        break;
    //    }
    //}

}

void GameState::initScene()
{
    auto& mb = m_sharedData.messageBus;

    m_scene.addSystem<ActorSystem>(mb);
    m_scene.addSystem<PlayerSystem>(mb);


}

void GameState::buildWorld()
{
    std::size_t playerCount = 0;

    //create a network game as usual
    for (auto i = 0u; i < ConstVal::MaxClients; ++i)
    {
        if (m_sharedData.clients[i].connected
            && playerCount < ConstVal::MaxClients)
        {
            //insert requested players in this slot
            for (auto j = 0u; j < m_sharedData.clients[i].playerCount && playerCount < ConstVal::MaxClients; ++j)
            {
                m_playerEntities[i][j] = m_scene.createEntity();
                m_playerEntities[i][j].addComponent<cro::Transform>().setPosition(PlayerSpawns[playerCount]);
                m_playerEntities[i][j].addComponent<Player>().id = i+j;
                m_playerEntities[i][j].getComponent<Player>().spawnPosition = PlayerSpawns[playerCount++];
                m_playerEntities[i][j].getComponent<Player>().connectionID = i;

                m_playerEntities[i][j].addComponent<Actor>().id = i+j;
                m_playerEntities[i][j].getComponent<Actor>().serverEntityId = m_playerEntities[i][j].getIndex();
            }
        }
    }

    //create the world data
    createHeightmap();
}

void GameState::createHeightmap()
{
    //height mask
    const float MaxLength = TileSize * IslandFadeSize;
    const std::uint32_t Offset = IslandBorder + IslandFadeSize;

    for (auto y = IslandBorder; y < IslandTileCount - IslandBorder; ++y)
    {
        for (auto x = IslandBorder; x < IslandTileCount - IslandBorder; ++x)
        {
            float val = 0.f;
            float pos = 0.f;

            if (x < IslandTileCount / 2)
            {
                pos = (x - IslandBorder) * TileSize;

            }
            else
            {
                pos = (IslandTileCount - IslandBorder - x) * TileSize;
            }

            val = pos / MaxLength;
            val = std::min(1.f, std::max(0.f, val));

            if (y > Offset && y < IslandTileCount - Offset)
            {
                m_heightmap[y * IslandTileCount + x] = val;
            }

            if (y < IslandTileCount / 2)
            {
                pos = (y - IslandBorder) * TileSize;

            }
            else
            {
                pos = (IslandTileCount - IslandBorder - y) * TileSize;
            }

            val = pos / MaxLength;
            val = std::min(1.f, std::max(0.f, val));

            if (x > Offset && x < IslandTileCount - Offset)
            {
                m_heightmap[y * IslandTileCount + x] = val;
            }
        }
    }

    //mask corners
    auto corner = [&, MaxLength](glm::uvec2 start, glm::uvec2 end, glm::vec2 centre)
    {
        for (auto y = start.t; y < end.t + 1; ++y)
        {
            for (auto x = start.s; x < end.s + 1; ++x)
            {
                glm::vec2 pos = glm::vec2(x, y) * TileSize;
                float val = 1.f - (glm::length(pos - centre) / MaxLength);
                val = std::max(0.f, std::min(1.f, val));
                m_heightmap[y * IslandTileCount + x] = val;
            }
        }
    };
    glm::vec2 centre = glm::vec2(Offset, Offset) * TileSize;
    corner({ IslandBorder, IslandBorder }, { Offset, Offset }, centre);

    centre.x = (IslandTileCount - Offset) * TileSize;
    corner({ IslandTileCount - Offset - 1, IslandBorder }, { IslandTileCount - IslandBorder, Offset }, centre);

    centre.y = (IslandTileCount - Offset) * TileSize;
    corner({ IslandTileCount - Offset - 1, IslandTileCount - Offset - 1 }, { IslandTileCount - IslandBorder, IslandTileCount - IslandBorder }, centre);

    centre.x = Offset * TileSize;
    corner({ IslandBorder, IslandTileCount - Offset - 1 }, { Offset, IslandTileCount - IslandBorder }, centre);


    //fastnoise
    FastNoiseSIMD* myNoise = FastNoiseSIMD::NewFastNoiseSIMD();
    myNoise->SetFrequency(0.05f);
    float* noiseSet = myNoise->GetSimplexFractalSet(0, 0, 0, IslandTileCount * 4, 1, 1);

    //use a 1D noise push the edges of the mask in/out by +/-1
    std::uint32_t i = 0;
    for (auto x = Offset; x < IslandTileCount - Offset; ++x)
    {
        auto val = std::round(noiseSet[i++] * EdgeVariation);
        if (val < 0) //move gradient tiles up one
        {
            for (auto j = 0; j < std::abs(val); ++j)
            {
                //top row
                for (auto y = IslandBorder; y < Offset + 1; ++y)
                {
                    m_heightmap[(y - 1) * IslandTileCount + x] = m_heightmap[y * IslandTileCount + x];
                }

                //bottom row
                for (auto y = IslandTileCount - Offset - 1; y < IslandTileCount - IslandBorder; ++y)
                {
                    m_heightmap[(y - 1) * IslandTileCount + x] = m_heightmap[y * IslandTileCount + x];
                }
            }
        }
        else if (val > 0) //move gradient tiles down one
        {
            for (auto j = 0; j < val; ++j)
            {
                //top row
                for (auto y = Offset; y > IslandBorder; --y)
                {
                    m_heightmap[y * IslandTileCount + x] = m_heightmap[(y - 1) * IslandTileCount + x];
                }

                //bottom row
                for (auto y = IslandTileCount - IslandBorder; y > IslandTileCount - Offset - 1; --y)
                {
                    m_heightmap[y * IslandTileCount + x] = m_heightmap[(y - 1) * IslandTileCount + x];
                }
            }
        }
    }

    for (auto y = Offset; y < IslandTileCount - Offset; ++y)
    {
        auto val = std::round(noiseSet[i++] * EdgeVariation);
        if (val < 0) //move gradient tiles left one
        {
            for (auto j = 0; j < std::abs(val); ++j)
            {
                //left col
                for (auto x = IslandBorder; x < Offset + 1; ++x)
                {
                    m_heightmap[y * IslandTileCount + x] = m_heightmap[y * IslandTileCount + (x + 1)];
                }

                //right col
                for (auto x = IslandTileCount - Offset - 1; x < IslandTileCount - IslandBorder; ++x)
                {
                    m_heightmap[y * IslandTileCount + x] = m_heightmap[y * IslandTileCount + (x + 1)];
                }
            }
        }
        else if (val > 0) //move gradient tiles right one
        {
            for (auto j = 0; j < val; ++j)
            {
                //left col
                for (auto x = Offset; x > IslandBorder; --x)
                {
                    m_heightmap[y * IslandTileCount + (x + 1)] = m_heightmap[y * IslandTileCount + x];
                }

                //right col
                for (auto x = IslandTileCount - IslandBorder; x > IslandTileCount - Offset - 1; --x)
                {
                    m_heightmap[y * IslandTileCount + (x + 1)] = m_heightmap[y * IslandTileCount + x];
                }
            }
        }
    }
    FastNoiseSIMD::FreeNoiseSet(noiseSet);

    //main island noise
    myNoise->SetFrequency(0.01f);
    noiseSet = myNoise->GetSimplexFractalSet(0, 0, 0, IslandTileCount, IslandTileCount, 1);

    myNoise->SetFrequency(0.1f);
    auto* noiseSet2 = myNoise->GetSimplexFractalSet(0, 0, 0, IslandTileCount, IslandTileCount, 1);

    for (auto y = 0u; y < IslandTileCount; ++y)
    {
        for (auto x = 0u; x < IslandTileCount; ++x)
        {
            float val = noiseSet[y * IslandTileCount + x];
            val += 1.f;
            val /= 2.f;

            const float minAmount = IslandLevels / 2.f;
            float multiplier = IslandLevels - minAmount;

            val *= multiplier;
            val = std::floor(val);
            val += minAmount;
            val /= IslandLevels;

            float valMod = noiseSet2[y * IslandTileCount + x];
            valMod += 1.f;
            valMod /= 2.f;
            valMod = 0.8f + valMod * 0.2f;

            m_heightmap[y * IslandTileCount + x] *= val * valMod;
        }
    }

    FastNoiseSIMD::FreeNoiseSet(noiseSet2);
    FastNoiseSIMD::FreeNoiseSet(noiseSet);
}