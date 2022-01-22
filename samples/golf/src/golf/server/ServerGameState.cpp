/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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
#include "../GameConsts.hpp"
#include "../ClientPacketData.hpp"
#include "../BallSystem.hpp"
#include "ServerGameState.hpp"
#include "ServerMessages.hpp"

#include <crogine/core/Log.hpp>
#include <crogine/core/ConfigFile.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Network.hpp>
#include <crogine/detail/glm/vec3.hpp>

using namespace sv;

namespace
{
    const std::uint8_t MaxStrokes = 12;
    const cro::Time TurnTime = cro::seconds(90.f);
}

GameState::GameState(SharedData& sd)
    : m_returnValue (StateID::Game),
    m_sharedData    (sd),
    m_mapDataValid  (false),
    m_scene         (sd.messageBus, 512),
    m_gameStarted   (false),
    m_allMapsLoaded (false),
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
            bool setNewPlayer = (data.clientID == m_playerInfo[0].client);

            //remove the player data
            m_playerInfo.erase(std::remove_if(m_playerInfo.begin(), m_playerInfo.end(),
                [&, data](const PlayerStatus& p)
                {
                    if (p.client == data.clientID)
                    {
                        //tell clients to remove the ball
                        std::uint32_t idx = p.ballEntity.getIndex();
                        m_sharedData.host.broadcastPacket(PacketID::EntityRemoved, idx, cro::NetFlag::Reliable);

                        m_scene.destroyEntity(p.ballEntity);
                        return true;
                    }
                    return false;
                }),
                m_playerInfo.end());

            if (!m_playerInfo.empty())
            {
                if (setNewPlayer)
                {
                    setNextPlayer();
                }
            }
            else
            {
                //we should quit this state? If no one is
                //connected we could be on our way out anyway
            }
        }
    }
    else if (msg.id == sv::MessageID::BallMessage)
    {
        const auto& data = msg.getData<BallEvent>();
        if (data.type == BallEvent::TurnEnded)
        {
            //check if we reached max strokes
            if (m_playerInfo[0].holeScore[m_currentHole] >= MaxStrokes)
            {
                //set the player as having holed the ball
                m_playerInfo[0].position = m_holeData[m_currentHole].pin;
                m_playerInfo[0].distanceToHole = 0.f;
            }
            else
            {
                m_playerInfo[0].position = data.position;
                m_playerInfo[0].distanceToHole = glm::length(data.position - m_holeData[m_currentHole].pin);
            }
            m_playerInfo[0].terrain = data.terrain;
            setNextPlayer();
        }
        else if (data.type == BallEvent::Holed)
        {
            //set the player as being on the pin so
            //they shouldn't be picked as next player
            m_playerInfo[0].position = m_holeData[m_currentHole].pin;
            m_playerInfo[0].distanceToHole = 0.f;
            m_playerInfo[0].terrain = data.terrain;

            setNextPlayer();
        }
        else if (data.type == BallEvent::Foul)
        {
            m_playerInfo[0].holeScore[m_currentHole]++; //penalty stroke.

            if (m_playerInfo[0].holeScore[m_currentHole] >= MaxStrokes)
            {
                //set the player as having holed the ball
                m_playerInfo[0].position = m_holeData[m_currentHole].pin;
                m_playerInfo[0].distanceToHole = 0.f;
                m_playerInfo[0].terrain = TerrainID::Green;
            }
        }
        else if (data.type == BallEvent::Landed)
        {
            //immediate, pre-pause event when the ball stops moving
            BallUpdate bu;
            bu.terrain = data.terrain;
            bu.position = data.position;
            m_sharedData.host.broadcastPacket(PacketID::BallLanded, bu, cro::NetFlag::Reliable);
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
            if (evt.peer.getID() == m_sharedData.hostID)
            {
                doServerCommand(evt);
            }
            break;
        case PacketID::TransitionComplete:
        {
            auto clientID = evt.packet.as<std::uint8_t>();
            m_sharedData.clients[clientID].mapLoaded = true;
        }
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
    for (const auto& player : m_playerInfo)
    {
        //only send active player's ball
        auto ball = player.ballEntity;
        
        if (ball == m_playerInfo[0].ballEntity)
        {
            auto timestamp = m_serverTime.elapsed().asMilliseconds();

            ActorInfo info;
            info.serverID = static_cast<std::uint32_t>(ball.getIndex());
            info.position = ball.getComponent<cro::Transform>().getPosition();
            info.rotation = cro::Util::Net::compressQuat(ball.getComponent<cro::Transform>().getRotation());
            info.timestamp = timestamp;
            info.clientID = player.client;
            info.playerID = player.player;
            info.state = static_cast<std::uint8_t>(ball.getComponent<Ball>().state);
            m_sharedData.host.broadcastPacket(PacketID::ActorUpdate, info, cro::NetFlag::Unreliable);
        }
    }

    auto wind = cro::Util::Net::compressVec3(m_scene.getSystem<BallSystem>()->getWindDirection());
    m_sharedData.host.broadcastPacket(PacketID::WindDirection, wind, cro::NetFlag::Unreliable);
}

std::int32_t GameState::process(float dt)
{
    if (m_gameStarted)
    {
        //check the turn timer and skip player if they AFK'd
        if (m_turnTimer.elapsed() > TurnTime)
        {
            if (m_sharedData.clients[m_playerInfo[0].client].peer.getID() != m_sharedData.hostID)
            {
                m_playerInfo[0].holeScore[m_currentHole] = MaxStrokes;
                m_playerInfo[0].position = m_holeData[m_currentHole].pin;
                m_playerInfo[0].distanceToHole = 0.f;
                m_playerInfo[0].terrain = TerrainID::Green;
                setNextPlayer(); //resets the timer
            }
        }

        //we have to keep checking this as a client might
        //disconnect mid-transition and the final 'complete'
        //packet will never arrive.
        if (!m_allMapsLoaded)
        {
            bool allReady = true;
            for (const auto& client : m_sharedData.clients)
            {
                if (client.connected && !client.mapLoaded)
                {
                    allReady = false;
                }
            }
            m_allMapsLoaded = allReady;
        }
    }

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
        for (const auto& player : m_playerInfo)
        {
            auto ball = player.ballEntity;

            ActorInfo info;
            info.serverID = static_cast<std::uint32_t>(ball.getIndex());
            info.position = ball.getComponent<cro::Transform>().getPosition();
            info.clientID = player.client;
            info.playerID = player.player;
            info.timestamp = timestamp;
            m_sharedData.host.sendPacket(m_sharedData.clients[clientID].peer, PacketID::ActorSpawn, info, cro::NetFlag::Reliable);
        }
    }

    m_sharedData.clients[clientID].ready = true;

    bool allReady = true;
    for (const auto& client : m_sharedData.clients)
    {
        if (client.connected && !client.ready)
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

        //create an ent which waits for the clients to
        //finish playing the transition animation
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt)
        {
            if (m_allMapsLoaded)
            {
                setNextPlayer();
                e.getComponent<cro::Callback>().active = false;
                m_scene.destroyEntity(e);
            }

            //make sure to keep resetting this to prevent unfairly
            //truncating the next player's turn
            m_turnTimer.restart();
        };
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
        auto& ball = m_playerInfo[0].ballEntity.getComponent<Ball>();

        if (ball.state == Ball::State::Idle)
        {
            ball.velocity = input.impulse;
            ball.state = ball.terrain == TerrainID::Green ? Ball::State::Putt : Ball::State::Flight;
            //this is a kludge to wait for the anim before hitting the ball
            //Ideally we want to read the frame data from the avatar
            //as well as account for a frame of interp delay on the client
            ball.delay = ball.terrain == TerrainID::Green ? 0.42f : 1.32f;
            //ball.delay = 0.32f;
            ball.startPoint = m_playerInfo[0].ballEntity.getComponent<cro::Transform>().getPosition();

            //calc the amount of spin based on if we're going towards the hole
            glm::vec2 pin = { m_holeData[m_currentHole].pin.x, m_holeData[m_currentHole].pin.z };
            glm::vec2 start = { ball.startPoint.x, ball.startPoint.z };
            auto dir = glm::normalize(pin - start);
            auto x = -dir.y;
            dir.y = dir.x;
            dir.x = x;
            ball.spin = glm::dot(dir, glm::normalize(glm::vec2(ball.velocity.x, ball.velocity.z))) + 0.1f;


            m_playerInfo[0].holeScore[m_currentHole]++;

            auto animID = ball.terrain == TerrainID::Green ? AnimationID::Putt : AnimationID::Swing;
            m_sharedData.host.broadcastPacket(PacketID::ActorAnimation, std::uint8_t(animID), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);

            m_turnTimer.restart(); //don't time out mid-shot...
        }
    }
}

void GameState::setNextPlayer()
{
    //broadcast current player's score first
    ScoreUpdate su;
    su.client = m_playerInfo[0].client;
    su.player = m_playerInfo[0].player;
    su.score = m_playerInfo[0].totalScore;
    su.stroke = m_playerInfo[0].holeScore[m_currentHole];
    su.hole = m_currentHole;
    m_sharedData.host.broadcastPacket(PacketID::ScoreUpdate, su, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);

    //sort players by distance
    std::sort(m_playerInfo.begin(), m_playerInfo.end(),
        [](const PlayerStatus& a, const PlayerStatus& b)
        {
            return a.distanceToHole > b.distanceToHole;
        });

    ActivePlayer player = m_playerInfo[0]; //deliberate slice.

    if (m_playerInfo[0].distanceToHole == 0)
    {
        //we must all be in a hole so move on
        //to next hole
        setNextHole();
    }
    else
    {
        //go to next player
        m_sharedData.host.broadcastPacket(PacketID::SetPlayer, player, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
        m_sharedData.host.broadcastPacket(PacketID::ActorAnimation, std::uint8_t(AnimationID::Idle), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
    }

    m_turnTimer.restart();
}

void GameState::setNextHole()
{
    //broadcast all scores to make sure everyone is up to date
    for (auto& player : m_playerInfo)
    {
        player.totalScore += player.holeScore[m_currentHole];

        ScoreUpdate su;
        su.client = player.client;
        su.player = player.player;
        su.hole = m_currentHole;
        su.score = player.totalScore;
        su.stroke = player.holeScore[m_currentHole];

        m_sharedData.host.broadcastPacket(PacketID::ScoreUpdate, su, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
    }

    //set clients as not yet loaded
    for (auto& client : m_sharedData.clients)
    {
        client.mapLoaded = false;
    }
    m_allMapsLoaded = false;

    m_currentHole++;
    if (m_currentHole < m_holeData.size())
    {
        //reset player positions/strokes
        for (auto& player : m_playerInfo)
        {
            player.position = m_holeData[m_currentHole].tee;
            player.distanceToHole = glm::length(m_holeData[m_currentHole].tee - m_holeData[m_currentHole].pin);
            player.terrain = TerrainID::Fairway;

            auto ball = player.ballEntity;
            ball.getComponent<Ball>().terrain = TerrainID::Fairway;
            ball.getComponent<Ball>().velocity = glm::vec3(0.f);
            ball.getComponent<cro::Transform>().setPosition(player.position);

            auto timestamp = m_serverTime.elapsed().asMilliseconds();

            ActorInfo info;
            info.serverID = static_cast<std::uint32_t>(ball.getIndex());
            info.position = ball.getComponent<cro::Transform>().getPosition();
            info.timestamp = timestamp;
            info.clientID = player.client;
            info.playerID = player.player;
            info.state = static_cast<std::uint8_t>(ball.getComponent<Ball>().state);
            m_sharedData.host.broadcastPacket(PacketID::ActorUpdate, info, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }

        //tell the local ball system which hole we're on
        if (!m_scene.getSystem<BallSystem>()->setHoleData(m_holeData[m_currentHole]))
        {
            m_sharedData.host.broadcastPacket(PacketID::ServerError, static_cast<std::uint8_t>(MessageType::MapNotFound), cro::NetFlag::Reliable);
            return;
        }

        //tell clients to set up next hole
        m_sharedData.host.broadcastPacket(PacketID::SetHole, m_currentHole, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);

        //create an ent which waits for all clients to load the next hole
        //which include playing the transition animation.
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt)
        {
            if (m_allMapsLoaded)
            {
                setNextPlayer();
                e.getComponent<cro::Callback>().active = false;
                m_scene.destroyEntity(e);
            }

            //make sure to keep resetting this to prevent unfairly
            //truncating the next player's turn
            m_turnTimer.restart();
        };
    }
    else
    {
        //end of game baby!
        m_sharedData.host.broadcastPacket(PacketID::GameEnd, std::uint8_t(10), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);

        //create a timer ent which returns to lobby on time out
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<float>(10.f);
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt)
        {
            auto& remain = e.getComponent<cro::Callback>().getUserData<float>();
            remain -= dt;
            if (remain < 0)
            {
                m_returnValue = StateID::Lobby;
                e.getComponent<cro::Callback>().active = false;
            }
        };

        m_gameStarted = false;
    }
}

bool GameState::validateMap()
{
    auto mapDir = m_sharedData.mapDir.toAnsiString();
    auto mapPath = ConstVal::MapPath + mapDir + "/course.data";

    if (!cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + mapPath))
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
        if (name == "hole"
            && holeStrings.size() < MaxHoles)
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
        if (!cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + hole))
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
            if (name == "pin")
            {
                //TODO not sure how we ensure these are sane values?
                //could at leat clamp them to map bounds.
                holeData.pin = holeProp.getValue<glm::vec3>();
                holeData.pin.x = glm::clamp(holeData.pin.x, 0.f, 320.f);
                holeData.pin.z = glm::clamp(holeData.pin.z, -200.f, 0.f);
                propCount++;
            }
            else if (name == "tee")
            {
                holeData.tee = holeProp.getValue<glm::vec3>();
                holeData.tee.x = glm::clamp(holeData.tee.x, 0.f, 320.f);
                holeData.tee.z = glm::clamp(holeData.tee.z, -200.f, 0.f);
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
            else if (name == "model")
            {
                cro::ConfigFile modelConfig;
                if (modelConfig.loadFromFile(holeProp.getValue<std::string>()))
                {
                    const auto& modelProps = modelConfig.getProperties();
                    for (const auto& modelProp : modelProps)
                    {
                        if (modelProp.getName() == "mesh")
                        {
                            auto modelPath = modelProp.getValue<std::string>();
                            if (cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + modelPath))
                            {
                                holeData.modelPath = modelPath;
                                propCount++;
                            }
                        }
                    }
                }
            }
        }

        if (propCount != MaxProps)
        {
            LogE << "Server: Missing hole property" << std::endl;
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

    auto& mb = m_sharedData.messageBus;
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_mapDataValid = m_scene.addSystem<BallSystem>(mb)->setHoleData(m_holeData[0]);
}

void GameState::buildWorld()
{
    //create a ball entity for each player
    for (auto& player : m_playerInfo)
    {
        player.ballEntity = m_scene.createEntity();
        player.ballEntity.addComponent<cro::Transform>().setPosition(player.position);
        player.ballEntity.addComponent<Ball>();

        player.holeScore.resize(m_holeData.size());
        std::fill(player.holeScore.begin(), player.holeScore.end(), 0);
    }
}

void GameState::doServerCommand(const cro::NetEvent& evt)
{
#ifdef CRO_DEBUG_
    switch (evt.packet.as<std::uint8_t>())
    {
    default: break;
    case ServerCommand::NextHole:
        if (m_currentHole < m_holeData.size() - 1)
        {
            for (auto& p : m_playerInfo)
            {
                p.position = m_holeData[m_currentHole].pin;
                p.distanceToHole = 0.f;
                p.holeScore[m_currentHole] = MaxStrokes;
                p.totalScore += MaxStrokes;
            }
            setNextPlayer();
        }
        break;
    case ServerCommand::NextPlayer:
        //this fakes the ball getting closer to the hole
        m_playerInfo[0].distanceToHole *= 0.99f;
        setNextPlayer();
        break;
    case ServerCommand::GotoGreen:
        //set ball to greeen position
        //set ball state to paused to trigger updates
    {
        auto pos = m_playerInfo[0].position - m_holeData[m_currentHole].pin;
        pos = glm::normalize(pos) * 6.f;

        m_playerInfo[0].ballEntity.getComponent<cro::Transform>().setPosition(pos + m_holeData[m_currentHole].pin);
        m_playerInfo[0].ballEntity.getComponent<Ball>().terrain = TerrainID::Green;
        m_playerInfo[0].ballEntity.getComponent<Ball>().state = Ball::State::Paused;
    }

        break;
    case ServerCommand::EndGame:
    {
        //end of game baby!
        m_sharedData.host.broadcastPacket(PacketID::GameEnd, std::uint8_t(10), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);

        //create a timer ent which returns to lobby on time out
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<float>(10.f);
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt)
        {
            auto& remain = e.getComponent<cro::Callback>().getUserData<float>();
            remain -= dt;
            if (remain < 0)
            {
                m_returnValue = StateID::Lobby;
                e.getComponent<cro::Callback>().active = false;
            }
        };
    }
        break;
    }
#endif
}