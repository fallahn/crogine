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

#include <crogine/core/Log.hpp>

#include<crogine/ecs/components/Transform.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/detail/glm/vec3.hpp>

using namespace Sv;

namespace
{
    //TODO work out a better way to create spawn points
    const std::array<glm::vec3, ConstVal::MaxClients> playerSpawns =
    {
        glm::vec3(-1.5f, 1.f, -1.5f),
        glm::vec3(1.5f, 1.f, -1.5f),
        glm::vec3(1.5f, 1.f, 1.5f),
        glm::vec3(-1.5f, 1.f, 1.5f)
    };
}

GameState::GameState(SharedData& sd)
    : m_returnValue (StateID::Game),
    m_sharedData    (sd),
    m_scene         (sd.messageBus)
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
            auto entityID = m_playerEntities[data.playerID].getIndex();
            m_scene.destroyEntity(m_playerEntities[data.playerID]);

            m_sharedData.host.broadcastPacket(PacketID::EntityRemoved, entityID, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
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
        if (m_sharedData.clients[i].connected
            && m_playerEntities[i].isValid())
        {
            const auto& player = m_playerEntities[i].getComponent<Player>();

            PlayerUpdate update;
            update.position = m_playerEntities[i].getComponent<cro::Transform>().getPosition();
            update.rotation = Util::compressQuat(m_playerEntities[i].getComponent<cro::Transform>().getRotation());
            update.pitch = Util::compressFloat(player.cameraPitch);
            update.yaw = Util::compressFloat(player.cameraYaw);
            update.timestamp = player.inputStack[player.lastUpdatedInput].timeStamp;

            m_sharedData.host.sendPacket(m_sharedData.clients[i].peer, PacketID::PlayerUpdate, update, cro::NetFlag::Unreliable);
        }
    }

    //broadcast other actor transforms
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

            PlayerInfo info;
            info.playerID = i;
            info.spawnPosition = m_playerEntities[i].getComponent<cro::Transform>().getPosition();
            info.rotation = Util::compressQuat(m_playerEntities[i].getComponent<cro::Transform>().getRotation());
            info.serverID = m_playerEntities[i].getIndex();
            info.timestamp = m_serverTime.elapsed().asMilliseconds();

            m_sharedData.host.sendPacket(m_sharedData.clients[playerID].peer, PacketID::PlayerSpawn, info, cro::NetFlag::Reliable);
        }
    }


    //TODO send map data to start building the world

    //client said it was ready, so mark as ready
    m_sharedData.clients[playerID].ready = true;
}

void GameState::handlePlayerInput(const cro::NetEvent::Packet& packet)
{
    auto input = packet.as<InputUpdate>();
    CRO_ASSERT(m_playerEntities[input.playerID].isValid(), "Not a valid player!");
    auto& player = m_playerEntities[input.playerID].getComponent<Player>();

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

    auto data = evt.packet.as<ServerCommand>();
    if (data.target < m_sharedData.clients.size()
        && m_sharedData.clients[data.target].connected)
    {
        switch (data.commandID)
        {
        default: break;
        case CommandPacket::SetModeFly:
            if (m_playerEntities[data.target].isValid())
            {
                m_playerEntities[data.target].getComponent<Player>().flyMode = true;
                m_sharedData.host.sendPacket(m_sharedData.clients[data.target].peer, PacketID::ServerCommand, data, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
            }
            break;
        case CommandPacket::SetModeWalk:
            if (m_playerEntities[data.target].isValid())
            {
                m_playerEntities[data.target].getComponent<Player>().flyMode = false;
                m_sharedData.host.sendPacket(m_sharedData.clients[data.target].peer, PacketID::ServerCommand, data, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
            }
            break;
        }
    }

}

void GameState::initScene()
{
    auto& mb = m_sharedData.messageBus;

    m_scene.addSystem<ActorSystem>(mb);
    m_scene.addSystem<PlayerSystem>(mb);


}

void GameState::buildWorld()
{
    for (auto i = 0u; i < ConstVal::MaxClients; ++i)
    {
        if (m_sharedData.clients[i].connected)
        {
            //insert a player in this slot
            //TODO get spawn position from generated world data
            //TODO figure out how to get correct initial pitch/yaw from any rotation other than 0
            m_playerEntities[i] = m_scene.createEntity();
            m_playerEntities[i].addComponent<cro::Transform>().setPosition(playerSpawns[i]);
            //m_playerEntities[i].getComponent<cro::Transform>().setRotation( //look at centre of the world
            //    glm::quat_cast(glm::inverse(glm::lookAt(playerSpawns[i], glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f)))));
            m_playerEntities[i].addComponent<Player>().id = i;
            m_playerEntities[i].getComponent<Player>().spawnPosition = playerSpawns[i];
            //m_playerEntities[i].getComponent<Player>().cameraYaw = m_playerEntities[i].getComponent<cro::Transform>().getRotation().y;
            m_playerEntities[i].addComponent<Actor>().id = i;
            m_playerEntities[i].getComponent<Actor>().serverEntityId = m_playerEntities[i].getIndex();
        }
    }
}