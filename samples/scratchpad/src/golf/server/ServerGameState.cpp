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
#include <crogine/core/ConfigFile.hpp>

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
    m_mapDataValid  (false),
    m_scene         (sd.messageBus)
{
    if (m_mapDataValid = validateMap(); m_mapDataValid)
    {
        initScene();
        buildWorld();
    }

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
void GameState::sendInitialGameState(std::uint8_t clientID)
{
    if (!m_mapDataValid)
    {
        m_sharedData.host.sendPacket(m_sharedData.clients[clientID].peer, PacketID::ServerError, static_cast<std::uint8_t>(MessageType::MapNotFound), cro::NetFlag::Reliable);
        return;
    }


    m_sharedData.clients[clientID].ready = true;
}

void GameState::handlePlayerInput(const cro::NetEvent::Packet& packet)
{

}

bool GameState::validateMap()
{
    auto mapDir = m_sharedData.mapDir.toAnsiString();
    auto mapPath = ConstVal::MapPath + mapDir + "/course.data";

    if (!cro::FileSystem::fileExists(mapPath))
    {
        //TODO what's the best state to leave this in
        //if we fail to load a map? If the clients all
        //error this should be ok because the host will
        //kill the server for us
        return false;
    }

    cro::ConfigFile courseFile;
    if (!courseFile.loadFromFile(mapPath))
    {
        return false;
    }

    //we're only interested in hole data on the server
    std::vector<std::string> holeStrings;
    const auto& props = courseFile.getProperties();
    for (const auto& prop : props)
    {
        const auto& name = prop.getName();
        if (name == "hole")
        {
            holeStrings.push_back(prop.getValue<std::string>());
        }
    }

    if (holeStrings.empty())
    {
        //TODO could be more fine-grained with error info?
        return false;
    }

    cro::ConfigFile holeCfg;
    for (const auto& hole : holeStrings)
    {
        if (!cro::FileSystem::fileExists(hole))
        {
            return false;
        }

        if (!holeCfg.loadFromFile(hole))
        {
            return false;
        }

        static constexpr std::int32_t MaxProps = 4;
        std::int32_t propCount = 0;
        auto& holeData = m_holeData.emplace_back();

        const auto& holeProps = holeCfg.getProperties();
        for (const auto& holeProp : holeProps)
        {
            const auto& name = holeProp.getName();
            if (name == "map")
            {
                if (!holeData.map.loadFromFile(holeProp.getValue<std::string>()))
                {
                    return false;
                }
                propCount++;
            }
            else if (name == "pin")
            {
                //TODO not sure how we ensure these are sane values?
                auto pin = holeProp.getValue<glm::vec2>();
                holeData.pin = { pin.x, 0.f, -pin.y };
                propCount++;
            }
            else if (name == "tee")
            {
                auto tee = holeProp.getValue<glm::vec2>();
                holeData.tee = { tee.x, 0.f, -tee.y };
                propCount++;
            }
            else if (name == "par")
            {
                holeData.par = holeProp.getValue<std::int32_t>();
                if (holeData.par < 1 || holeData.par > 10)
                {
                    return false;
                }
                propCount++;
            }
        }

        if (propCount != MaxProps)
        {
            LogE << "Missing hole property" << std::endl;
            return false;
        }
    }

    return true;
}

void GameState::initScene()
{
    auto& mb = m_sharedData.messageBus;
}

void GameState::buildWorld()
{

}