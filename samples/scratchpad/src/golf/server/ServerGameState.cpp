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
#include "../BallSystem.hpp"
#include "ServerGameState.hpp"
#include "ServerMessages.hpp"

#include <crogine/core/Log.hpp>
#include <crogine/core/ConfigFile.hpp>

#include <crogine/ecs/components/Transform.hpp>

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
    m_scene         (sd.messageBus),
    m_gameStarted   (false),
    m_currentHole   (0)
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

            //TODO send update? switch to next player? Client already knows so can clear up anyway?
            bool setNewPlayer = (data.clientID == m_playerInfo[0].client);

            //remove the player data
            m_playerInfo.erase(std::remove_if(m_playerInfo.begin(), m_playerInfo.end(),
                [&, data](const PlayerStatus& p)
                {
                    if (p.client == data.clientID)
                    {
                        m_scene.destroyEntity(p.ballEntity);
                        return true;
                    }
                    return false;
                }),
                m_playerInfo.end());

            if (setNewPlayer)
            {
                setNextPlayer();
            }
        }
    }
    else if (msg.id == sv::MessageID::BallMessage)
    {
        const auto& data = msg.getData<BallEvent>();
        if (data.type == BallEvent::Landed)
        {
            m_playerInfo[0].position = data.position;
            m_playerInfo[0].distanceToHole = glm::length(data.position - m_holeData[m_currentHole].pin);
            //TODO update player terrain etc
            setNextPlayer();
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
    if (m_playerInfo.empty())
    {
        return;
    }

    //fetch ball ents and send updates to client
    const auto& balls = m_scene.getSystem<BallSystem>().getEntities1();
    for (auto ball : balls)
    {
        //only send active player's ball? this breaks initial positions for other players
        //if (ball == m_playerInfo[0].ballEntity)
        {
            auto timestamp = m_serverTime.elapsed().asMilliseconds();

            ActorInfo info;
            info.serverID = static_cast<std::uint32_t>(ball.getIndex());
            info.position = ball.getComponent<cro::Transform>().getPosition();
            info.timestamp = timestamp;
            m_sharedData.host.broadcastPacket(PacketID::ActorUpdate, info, cro::NetFlag::Unreliable);
        }
    }
}

std::int32_t GameState::process(float dt)
{
    m_scene.simulate(dt);
    return m_returnValue;
}

//private
void GameState::sendInitialGameState(std::uint8_t clientID)
{
    //only send data the first time
    if (!m_sharedData.clients[clientID].ready)
    {
        if (!m_mapDataValid)
        {
            m_sharedData.host.sendPacket(m_sharedData.clients[clientID].peer, PacketID::ServerError, static_cast<std::uint8_t>(MessageType::MapNotFound), cro::NetFlag::Reliable);
            return;
        }

        //send all the ball positions
        auto timestamp = m_serverTime.elapsed().asMilliseconds();
        const auto& balls = m_scene.getSystem<BallSystem>().getEntities1();
        for (auto ball : balls)
        {
            ActorInfo info;
            info.serverID = static_cast<std::uint32_t>(ball.getIndex());
            info.position = ball.getComponent<cro::Transform>().getPosition();
            m_sharedData.host.sendPacket(m_sharedData.clients[clientID].peer, PacketID::ActorSpawn, info, cro::NetFlag::Reliable);
        }
    }

    m_sharedData.clients[clientID].ready = true;

    bool allReady = true;
    for (const auto& client : m_sharedData.clients)
    {
        if (!client.connected && client.ready)
        {
            allReady = false;
        }
    }

    if (allReady && !m_gameStarted)
    {
        //a guard to make sure this is sent just once
        m_gameStarted = true;

        //send command to set clients to first player
        //this also tells the client to stop requesting state
        setNextPlayer();
    }
}

void GameState::handlePlayerInput(const cro::NetEvent::Packet& packet)
{
    if (m_playerInfo.empty())
    {
        return;
    }

    auto input = packet.as<InputUpdate>();
    if (m_playerInfo[0].client == input.clientID
        && m_playerInfo[0].player == input.playerID)
    {
        m_playerInfo[0].ballEntity.getComponent<Ball>().velocity = input.impulse;
        m_playerInfo[0].ballEntity.getComponent<Ball>().state = Ball::State::Flight;
        //this is a kludge to wait for the anim before hitting the ball
        //Ideally we want to read the frame data from the sprite sheet
        //as well as account for a frame of interp delay on the client
        m_playerInfo[0].ballEntity.getComponent<Ball>().delay = 0.32f;

        m_sharedData.host.broadcastPacket(PacketID::ActorAnimation, m_animIDs[AnimID::Swing], cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
    }
}

void GameState::setNextPlayer()
{
    //TODO broadcast current player's score first?

    //sort players by distance
    std::sort(m_playerInfo.begin(), m_playerInfo.end(),
        [](const PlayerStatus& a, const PlayerStatus& b)
        {
            return a.distanceToHole > b.distanceToHole;
        });

    ActivePlayer player = m_playerInfo[0]; //deliberate slice.

    m_sharedData.host.broadcastPacket(PacketID::SetPlayer, player, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
    m_sharedData.host.broadcastPacket(PacketID::ActorAnimation, m_animIDs[AnimID::Idle], cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
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
    for (auto i = 0u; i < m_sharedData.clients.size(); ++i)
    {
        if (m_sharedData.clients[i].connected)
        {
            for (auto j = 0u; j < m_sharedData.clients[i].playerCount; ++j)
            {
                auto& player = m_playerInfo.emplace_back();
                player.client = i;
                player.player = j;
                player.position = m_holeData[0].tee;
                player.distanceToHole = glm::length(m_holeData[0].tee - m_holeData[0].pin);
            }
        }
    }

    //load the anims so we know which ID to
    //TODO parse the sprite sheet manually as OpenGL 
    //isn't available in the server thread and the default
    //loader wants a texture resource
    /*cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/player.spt", m_textures);
    m_animIDs[AnimID::Idle] = static_cast<std::uint8_t>(spriteSheet.getAnimationIndex("idle", "female"));
    m_animIDs[AnimID::Swing] = static_cast<std::uint8_t>(spriteSheet.getAnimationIndex("swing", "female"));*/
    m_animIDs[AnimID::Idle] = 0;
    m_animIDs[AnimID::Swing] = 1;


    auto& mb = m_sharedData.messageBus;
    m_scene.addSystem<BallSystem>(mb);
}

void GameState::buildWorld()
{
    //create a ball entity for each player
    //TODO sync with clients.
    for (auto& player : m_playerInfo)
    {
        player.ballEntity = m_scene.createEntity();
        player.ballEntity.addComponent<cro::Transform>().setPosition(player.position);
        player.ballEntity.addComponent<Ball>();
    }
}

void GameState::doServerCommand(const cro::NetEvent& evt)
{
#ifdef CRO_DEBUG_
    switch (evt.packet.as<std::uint8_t>())
    {
    default: break;
    case ServerCommand::NextHole:

        break;
    case ServerCommand::NextPlayer:
        //this fakes the ball getting closer to the hole
        m_playerInfo[0].distanceToHole *= 0.99f;
        setNextPlayer();
        break;
    }
#endif
}