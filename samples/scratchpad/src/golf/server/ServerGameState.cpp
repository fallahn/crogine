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

#include "../PacketIDs.hpp"
#include "../CommonConsts.hpp"
#include "../ClientPacketData.hpp"
#include "ServerGameState.hpp"
#include "ServerPacketData.hpp"
#include "ServerMessages.hpp"

#include <crogine/core/Log.hpp>

#include<crogine/ecs/components/Transform.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/util/Network.hpp>
#include <crogine/detail/glm/vec3.hpp>

using namespace sv;

namespace
{

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
    if (msg.id == sv::MessageID::ConnectionMessage)
    {
        const auto& data = msg.getData<ConnectionEvent>();
        if (data.type == ConnectionEvent::Disconnected)
        {
            //disconnect notification packet is sent in Server
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
        }
    }
}

void GameState::netBroadcast()
{
    
}

std::int32_t GameState::process(float dt)
{
    m_scene.simulate(dt);
    return m_returnValue;
}

//private
void GameState::sendInitialGameState(std::uint8_t playerID)
{

}

void GameState::handlePlayerInput(const cro::NetEvent::Packet& packet)
{

}


void GameState::initScene()
{
    auto& mb = m_sharedData.messageBus;

}

void GameState::buildWorld()
{

}