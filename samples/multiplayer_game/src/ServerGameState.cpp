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

#include "ServerState.hpp"
#include "PacketIDs.hpp"
#include "CommonConsts.hpp"
#include "ServerPacketData.hpp"
#include "ClientPacketData.hpp"

#include <crogine/core/Log.hpp>
#include <crogine/detail/glm/vec3.hpp>

using namespace Sv;

namespace
{
    //TODO work out a better way to create spawn points
    const std::array<glm::vec3, ConstVal::MaxClients> playerSpawns =
    {
        glm::vec3(-10.f, 5.f, -10.f),
        glm::vec3(10.f, 5.f, -10.f),
        glm::vec3(10.f, 5.f, 10.f),
        glm::vec3(-10.f, 5.f, 10.f)
    };
}

GameState::GameState(SharedData& sd)
    : m_returnValue (StateID::Game),
    m_sharedData    (sd)
{
    LOG("Entered Server Game State", cro::Logger::Type::Info);
}

void GameState::netUpdate(const cro::NetEvent& evt)
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
        }
    }
}

std::int32_t GameState::process(float dt)
{
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
            info.spawnPosition = playerSpawns[i]; //TODO take this from actual player entity
            //TODO include rotation?

            m_sharedData.host.sendPacket(m_sharedData.clients[playerID].peer, PacketID::PlayerSpawn, info, cro::NetFlag::Reliable);
        }
    }


    //TODO send map data to start building the world
}

void GameState::handlePlayerInput(const cro::NetEvent::Packet& packet)
{
    auto input = packet.as<InputUpdate>();
    //TODO apply this to the correct player

}