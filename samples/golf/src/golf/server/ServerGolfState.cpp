/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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
#include "ServerGolfState.hpp"
#include "ServerMessages.hpp"

#include <AchievementIDs.hpp>

#include <crogine/core/Log.hpp>
#include <crogine/core/ConfigFile.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Network.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/detail/glm/vec3.hpp>

using namespace sv;

namespace
{
    constexpr float MaxScoreboardTime = 10.f;
    constexpr std::uint8_t MaxStrokes = 12;
    const cro::Time TurnTime = cro::seconds(90.f);
}

GolfState::GolfState(SharedData& sd)
    : m_returnValue         (StateID::Golf),
    m_sharedData            (sd),
    m_mapDataValid          (false),
    m_scene                 (sd.messageBus, 512),
    m_scoreboardTime        (0.f),
    m_scoreboardReadyFlags  (0),
    m_gameStarted           (false),
    m_allMapsLoaded         (false),
    m_currentHole           (0),
    m_skinsPot              (1),
    m_currentBest           (MaxStrokes)
{
    if (m_mapDataValid = validateMap(); m_mapDataValid)
    {
        initScene();
        buildWorld();
    }

    LOG("Entered Server Golf State", cro::Logger::Type::Info);
}

void GolfState::handleMessage(const cro::Message& msg)
{
    const auto sendAchievement = [&](std::uint8_t achID)
    {
        //TODO not sure we want to send this blindly to the client
        //but only the client knows if achievements are currently enabled.
        std::array<std::uint8_t, 2u> packet =
        {
            m_playerInfo[0].client, achID
        };
        m_sharedData.host.broadcastPacket(PacketID::ServerAchievement, packet, net::NetFlag::Reliable);
    };

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
                        m_sharedData.host.broadcastPacket(PacketID::EntityRemoved, idx, net::NetFlag::Reliable);

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

                //client may have quit on summary screen so update statuses
                checkReadyQuit(10); //doesn't matter which client number, just update status...
            }
            else
            {
                //we should quit this state? If no one is
                //connected we could be on our way out anyway
                m_returnValue = sv::StateID::Lobby;
            }
        }
    }
    else if (msg.id == sv::MessageID::GolfMessage)
    {
        const auto& data = msg.getData<GolfBallEvent>();
        if (data.type == GolfBallEvent::TurnEnded)
        {
            //check if we reached max strokes
            auto maxStrokes = m_scene.getSystem<BallSystem>()->getPuttFromTee() ? MaxStrokes / 2 : MaxStrokes;
            if (m_playerInfo[0].holeScore[m_currentHole] >= maxStrokes)
            {
                //set the player as having holed the ball
                m_playerInfo[0].position = m_holeData[m_currentHole].pin;
                m_playerInfo[0].distanceToHole = 0.f;

                m_sharedData.host.broadcastPacket(PacketID::MaxStrokes, std::uint8_t(0), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
            }
            else
            {
                m_playerInfo[0].position = data.position;
                m_playerInfo[0].distanceToHole = glm::length(data.position - m_holeData[m_currentHole].pin);
            }
            m_playerInfo[0].terrain = data.terrain;

            //if match/skins play check if our score is even with anyone holed already and forfeit
            if (m_sharedData.scoreType != ScoreType::Stroke)
            {
                if (m_playerInfo[0].holeScore[m_currentHole] >= m_currentBest)
                {
                    m_playerInfo[0].distanceToHole = 0;
                    m_playerInfo[0].holeScore[m_currentHole]++;
                }
            }

            setNextPlayer();
        }
        else if (data.type == GolfBallEvent::Holed)
        {
            //set the player as being on the pin so
            //they shouldn't be picked as next player
            m_playerInfo[0].position = m_holeData[m_currentHole].pin;
            m_playerInfo[0].distanceToHole = 0.f;
            m_playerInfo[0].terrain = data.terrain;


            //if we're playing match play or skins then
            //anyone who has a worse score has already lost
            //so set them to finished.
            if (m_sharedData.scoreType != ScoreType::Stroke)
            {
                //eliminate anyone who can't beat this score
                for (auto i = 1u; i < m_playerInfo.size(); ++i)
                {
                    if ((m_playerInfo[i].holeScore[m_currentHole]) >=
                        m_playerInfo[0].holeScore[m_currentHole])
                    {
                        if (m_playerInfo[i].distanceToHole > 0) //not already holed
                        {
                            m_playerInfo[i].distanceToHole = 0.f;
                            m_playerInfo[i].holeScore[m_currentHole]++; //therefore they lose a stroke and don't draw
                        }
                    }
                }

                //if this is the second hole and it has the same as the current best
                //force a draw by eliminating anyone who can't beat it
                if (m_playerInfo[0].holeScore[m_currentHole] == m_currentBest)
                {
                    for (auto i = 1u; i < m_playerInfo.size(); ++i)
                    {
                        if ((m_playerInfo[i].holeScore[m_currentHole]+1) >=
                            m_currentBest)
                        {
                            if (m_playerInfo[i].distanceToHole > 0)
                            {
                                m_playerInfo[i].distanceToHole = 0.f;
                                m_playerInfo[i].holeScore[m_currentHole] = std::min(m_currentBest, std::uint8_t(m_playerInfo[i].holeScore[m_currentHole] + 1));
                            }
                        }
                    }
                }
            }

            if (m_playerInfo[0].holeScore[m_currentHole] < m_currentBest)
            {
                m_currentBest = m_playerInfo[0].holeScore[m_currentHole];
            }

            setNextPlayer();
        }
        else if (data.type == GolfBallEvent::Foul)
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
        else if (data.type == GolfBallEvent::Landed)
        {
            //immediate, pre-pause event when the ball stops moving
            BallUpdate bu;
            bu.terrain = data.terrain;
            bu.position = data.position;
            m_sharedData.host.broadcastPacket(PacketID::BallLanded, bu, net::NetFlag::Reliable);

            auto dist = glm::length2(m_playerInfo[0].position - data.position);
            if (dist > 2250000.f)
            {
                //1500m
                sendAchievement(AchievementID::IntoOrbit);
            }
        }
        else if (data.type == GolfBallEvent::Gimme)
        {
            m_playerInfo[0].holeScore[m_currentHole]++;
            std::uint16_t inf = (m_playerInfo[0].client << 8) | m_playerInfo[0].player;
            m_sharedData.host.broadcastPacket<std::uint16_t>(PacketID::Gimme, inf, net::NetFlag::Reliable);

            //if match/skins play check if our score is even with anyone holed already and forfeit
            if (m_sharedData.scoreType != ScoreType::Stroke)
            {
                for (auto i = 1u; i < m_playerInfo.size(); ++i)
                {
                    if (m_playerInfo[i].distanceToHole == 0
                        && m_playerInfo[i].holeScore[m_currentHole] < m_playerInfo[0].holeScore[m_currentHole])
                    {
                        m_playerInfo[0].distanceToHole = 0;
                    }
                }
            }
        }
    }
    else if (msg.id == sv::MessageID::TriggerMessage)
    {
        const auto& data = msg.getData<TriggerEvent>();
        switch (data.triggerID)
        {
        default: break;
        case TriggerID::Volcano:
            sendAchievement(AchievementID::HotStuff);
            break;
        case TriggerID::Boat:
            sendAchievement(AchievementID::ISeeNoShips);
            break;
        }
    }

    m_scene.forwardMessage(msg);
}

void GolfState::netEvent(const net::NetEvent& evt)
{
    if (evt.type == net::NetEvent::PacketReceived)
    {
        switch (evt.packet.getID())
        {
        default: break;
        case PacketID::SkipTurn:
            skipCurrentTurn(evt.packet.as<std::uint8_t>());
            break;
        case PacketID::CPUThink:
            m_sharedData.host.broadcastPacket(PacketID::CPUThink, evt.packet.as<std::uint8_t>(), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
            break;
        case PacketID::Emote:
            m_sharedData.host.broadcastPacket(PacketID::Emote, evt.packet.as<std::uint32_t>(), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
            break;
        case PacketID::LevelUp:
            m_sharedData.host.broadcastPacket(PacketID::LevelUp, evt.packet.as<std::uint64_t>(), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
            break;
        case PacketID::ClientReady:
            if (!m_sharedData.clients[evt.packet.as<std::uint8_t>()].ready)
            {
                sendInitialGameState(evt.packet.as<std::uint8_t>());
            }
            break;
        case PacketID::BallPrediction:
            handlePlayerInput(evt.packet, true);
            break;
        case PacketID::InputUpdate:
            handlePlayerInput(evt.packet, false);
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
        case PacketID::ReadyQuit:
            checkReadyQuit(evt.packet.as<std::uint8_t>());
            break;
        }
    }
}

void GolfState::netBroadcast()
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
        
        //ideally we want to send only non-idle balls but without
        //sending a few pre-movement frames first we get visible pops
        //in the client side interpolation.
        if (ball == m_playerInfo[0].ballEntity/* ||
            ball.getComponent<Ball>().state != Ball::State::Idle*/)
        {
            auto timestamp = m_serverTime.elapsed().asMilliseconds();
            

            ActorInfo info;
            info.serverID = static_cast<std::uint32_t>(ball.getIndex());
            info.position = ball.getComponent<cro::Transform>().getPosition();
            info.rotation = cro::Util::Net::compressQuat(ball.getComponent<cro::Transform>().getRotation());
            //info.velocity = cro::Util::Net::compressVec3(ball.getComponent<Ball>().velocity);
            //info.velocity = ball.getComponent<Ball>().velocity;
            info.windEffect = ball.getComponent<Ball>().windEffect;
            info.timestamp = timestamp;
            info.clientID = player.client;
            info.playerID = player.player;
            info.state = static_cast<std::uint8_t>(ball.getComponent<Ball>().state);
            m_sharedData.host.broadcastPacket(PacketID::ActorUpdate, info, net::NetFlag::Unreliable);
        }
    }

    auto wind = cro::Util::Net::compressVec3(m_scene.getSystem<BallSystem>()->getWindDirection());
    m_sharedData.host.broadcastPacket(PacketID::WindDirection, wind, net::NetFlag::Unreliable);
}

std::int32_t GolfState::process(float dt)
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
void GolfState::sendInitialGameState(std::uint8_t clientID)
{
    //only send data the first time
    if (!m_sharedData.clients[clientID].ready)
    {
        if (!m_mapDataValid)
        {
            m_sharedData.host.sendPacket(m_sharedData.clients[clientID].peer, PacketID::ServerError, static_cast<std::uint8_t>(MessageType::MapNotFound), net::NetFlag::Reliable);
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
            m_sharedData.host.sendPacket(m_sharedData.clients[clientID].peer, PacketID::ActorSpawn, info, net::NetFlag::Reliable);
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

        //send command to set clients to first player / hole
        //this also tells the client to stop requesting state

        std::uint16_t newHole = (m_currentHole << 8) | std::uint8_t(m_holeData[m_currentHole].par);
        m_sharedData.host.broadcastPacket(PacketID::SetPar, newHole, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

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
                m_scoreboardTime += dt;

                if (m_scoreboardTime > MaxScoreboardTime)
                {
                    setNextPlayer();
                    e.getComponent<cro::Callback>().active = false;
                    m_scene.destroyEntity(e);
                }
            }
            else
            {
                m_scoreboardTime = 0.f;
            }

            //make sure to keep resetting this to prevent unfairly
            //truncating the next player's turn
            m_turnTimer.restart();
        };

        m_scoreboardReadyFlags = 0;
    }
}

void GolfState::handlePlayerInput(const net::NetEvent::Packet& packet, bool predict)
{
    if (m_playerInfo.empty())
    {
        return;
    }

    auto input = packet.as<InputUpdate>();
    if (m_playerInfo[0].client == input.clientID
        && m_playerInfo[0].player == input.playerID)
    {
        //we make a copy of this and then re-apply it if not predicting a result
        auto ball = m_playerInfo[0].ballEntity.getComponent<Ball>();

        if (ball.state == Ball::State::Idle)
        {
            ball.velocity = input.impulse;
            ball.state = ball.terrain == TerrainID::Green ? Ball::State::Putt : Ball::State::Flight;
            //this is a kludge to wait for the anim before hitting the ball
            //Ideally we want to read the frame data from the avatar
            //as well as account for a frame of interp delay on the client
            //at the very least we should add the client ping to this
            ball.delay = ball.terrain == TerrainID::Green ? 0.05f : 1.17f;
            ball.delay += static_cast<float>(m_sharedData.clients[input.clientID].peer.getRoundTripTime()) / 1000.f;
            ball.startPoint = m_playerInfo[0].ballEntity.getComponent<cro::Transform>().getPosition();

            ball.spin = input.spin;
            if (glm::length2(input.impulse) != 0)
            {
                ball.initialForwardVector = glm::normalize(glm::vec3(input.impulse.x, 0.f, input.impulse.z));
                ball.initialSideVector = glm::normalize(glm::cross(ball.initialForwardVector, cro::Transform::Y_AXIS));
            }

            //calc the amount of rotation based on if we're going towards the hole
            glm::vec2 pin = { m_holeData[m_currentHole].pin.x, m_holeData[m_currentHole].pin.z };
            glm::vec2 start = { ball.startPoint.x, ball.startPoint.z };
            auto dir = glm::normalize(pin - start);
            auto x = -dir.y;
            dir.y = dir.x;
            dir.x = x;
            ball.rotation = glm::dot(dir, glm::normalize(glm::vec2(ball.velocity.x, ball.velocity.z))) + 0.1f;

            if (!predict)
            {
                m_playerInfo[0].holeScore[m_currentHole]++;

                auto animID = ball.terrain == TerrainID::Green ? AnimationID::Putt : AnimationID::Swing;
                m_sharedData.host.broadcastPacket(PacketID::ActorAnimation, std::uint8_t(animID), net::NetFlag::Reliable, ConstVal::NetChannelReliable);

                m_turnTimer.restart(); //don't time out mid-shot...

                m_playerInfo[0].ballEntity.getComponent<Ball>() = ball;
            }
            else if (m_sharedData.clients[input.clientID].playerData[input.playerID].isCPU)
            {
                //if we want to run a prediction do so on a duplicate entity
                auto e = m_scene.createEntity();
                e.addComponent<cro::Transform>().setPosition(ball.startPoint);
                e.addComponent<Ball>() = ball;
                
                m_scene.simulate(0.f); //run once so entity is properly integrated.
                m_scene.getSystem<BallSystem>()->runPrediction(e, 1.f/60.f);

                //read the result from e
                auto resultPos = e.getComponent<cro::Transform>().getPosition();
                
                //reply to client with result
                m_sharedData.host.sendPacket(m_sharedData.clients[input.clientID].peer, PacketID::BallPrediction, resultPos, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

                m_scene.destroyEntity(e);
            }
        }
    }
}

void GolfState::checkReadyQuit(std::uint8_t clientID)
{
    if (m_gameStarted)
    {
        //we might be waiting for others to start new hole
        m_scoreboardReadyFlags |= (1 << clientID);

        for (auto i = 0u; i < m_playerInfo.size(); ++i)
        {
            if ((m_scoreboardReadyFlags & (1 << m_playerInfo[i].client)) == 0)
            {
                return;
            }
        }
        m_scoreboardTime = MaxScoreboardTime; //skips ahead to timeout if everyone is ready
        return;
    }

    std::uint8_t broadcastFlags = 0;

    for (auto& p : m_playerInfo)
    {
        if (p.client == clientID)
        {
            p.readyQuit = !p.readyQuit;
        }

        if (p.readyQuit)
        {
            broadcastFlags |= (1 << p.client);
        }
        else
        {
            broadcastFlags &= ~(1 << p.client);
        }
    }
    //let clients know to update their display
    m_sharedData.host.broadcastPacket<std::uint8_t>(PacketID::ReadyQuitStatus, broadcastFlags, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

    for (const auto& p : m_playerInfo)
    {
        if (!p.readyQuit)
        {
            //not everyone is ready
            return;
        }
    }
    //if we made it here it's time to quit!
    m_returnValue = StateID::Lobby;
}

void GolfState::setNextPlayer(bool newHole)
{
    //broadcast current player's score first
    ScoreUpdate su;
    su.strokeDistance = m_playerInfo[0].ballEntity.getComponent<Ball>().lastStrokeDistance;
    su.client = m_playerInfo[0].client;
    su.player = m_playerInfo[0].player;
    su.score = m_playerInfo[0].totalScore;
    su.stroke = m_playerInfo[0].holeScore[m_currentHole];
    su.matchScore = m_playerInfo[0].matchWins;
    su.skinsScore = m_playerInfo[0].skins;
    su.hole = m_currentHole;
    m_sharedData.host.broadcastPacket(PacketID::ScoreUpdate, su, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

    m_playerInfo[0].ballEntity.getComponent<Ball>().lastStrokeDistance = 0.f;

    if (!newHole || m_currentHole == 0)
    {
        //sort players by distance
        std::sort(m_playerInfo.begin(), m_playerInfo.end(),
            [](const PlayerStatus& a, const PlayerStatus& b)
            {
                return a.distanceToHole > b.distanceToHole;
            });
    }
    else
    {
        //winner of the last hole goes first
        std::sort(m_playerInfo.begin(), m_playerInfo.end(),
            [&](const PlayerStatus& a, const PlayerStatus& b)
            {
                return a.holeScore[m_currentHole - 1] < b.holeScore[m_currentHole - 1];
            });
    }


    if (m_playerInfo[0].distanceToHole == 0)
    {
        //we must all be in a hole so move on
        //to next hole
        setNextHole();
    }
    else
    {
        //go to next player
        ActivePlayer player = m_playerInfo[0]; //deliberate slice.
        m_sharedData.host.broadcastPacket(PacketID::SetPlayer, player, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
        m_sharedData.host.broadcastPacket(PacketID::ActorAnimation, std::uint8_t(AnimationID::Idle), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
    }

    m_turnTimer.restart();
}

void GolfState::setNextHole()
{
    m_currentBest = MaxStrokes;
    m_scene.getSystem<BallSystem>()->forceWindChange();

    bool gameFinished = false;

    //update player skins/match scores
    if (m_playerInfo.size() > 1)
    {
        auto sortData = m_playerInfo;
        std::sort(sortData.begin(), sortData.end(),
            [&](const PlayerStatus& a, const PlayerStatus& b)
            {
                return a.holeScore[m_currentHole] < b.holeScore[m_currentHole];
            });

        //only score if no player tied
        if (sortData[0].holeScore[m_currentHole] != sortData[1].holeScore[m_currentHole])
        {
            auto player = std::find_if(m_playerInfo.begin(), m_playerInfo.end(), [&sortData](const PlayerStatus& p)
                {
                    return p.client == sortData[0].client && p.player == sortData[0].player;
                });

            player->matchWins++;
            player->skins += m_skinsPot;
            m_skinsPot = 1;

            //check the match score and end the game if this is the mode we're in
            if (m_sharedData.scoreType == ScoreType::Match)
            {
                sortData[0].matchWins++;
                std::sort(sortData.begin(), sortData.end(),
                    [&](const PlayerStatus& a, const PlayerStatus& b)
                    {
                        return a.matchWins > b.matchWins;
                    });


                auto remainingHoles = static_cast<std::uint8_t>(m_holeData.size()) - (m_currentHole + 1);
                //if second place can't beat first even if they win all the holes it's game over
                if(sortData[1].matchWins + remainingHoles < sortData[0].matchWins)
                {
                    gameFinished = true;
                }
            }

            //send notification packet to clients that player won the hole
            std::uint16_t data = (player->client << 8) | player->player;
            m_sharedData.host.broadcastPacket(PacketID::HoleWon, data, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
        else
        {
            m_skinsPot++;

            std::uint16_t data = 0xff00 | m_skinsPot;
            m_sharedData.host.broadcastPacket(PacketID::HoleWon, data, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
    }

    //broadcast all scores to make sure everyone is up to date
    for (auto& player : m_playerInfo)
    {
        player.totalScore += player.holeScore[m_currentHole];

        ScoreUpdate su;
        su.client = player.client;
        su.player = player.player;
        su.hole = m_currentHole;
        su.score = player.totalScore;
        su.matchScore = player.matchWins;
        su.skinsScore = player.skins;
        su.stroke = player.holeScore[m_currentHole];

        m_sharedData.host.broadcastPacket(PacketID::ScoreUpdate, su, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
    }

    //set clients as not yet loaded
    for (auto& client : m_sharedData.clients)
    {
        client.mapLoaded = false;
    }
    m_allMapsLoaded = false;
    m_scoreboardTime = 0.f;

    m_currentHole++;
    if (m_currentHole < m_holeData.size()
        && !gameFinished)
    {
        //tell the local ball system which hole we're on
        auto ballSystem = m_scene.getSystem<BallSystem>();
        if (!ballSystem->setHoleData(m_holeData[m_currentHole]))
        {
            m_sharedData.host.broadcastPacket(PacketID::ServerError, static_cast<std::uint8_t>(MessageType::MapNotFound), net::NetFlag::Reliable);
            return;
        }

        //reset player positions/strokes
        for (auto& player : m_playerInfo)
        {
            player.position = m_holeData[m_currentHole].tee;
            player.distanceToHole = glm::length(m_holeData[m_currentHole].tee - m_holeData[m_currentHole].pin);
            player.terrain = ballSystem->getTerrain(player.position).terrain;

            auto ball = player.ballEntity;
            ball.getComponent<Ball>().terrain = player.terrain;
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
            m_sharedData.host.broadcastPacket(PacketID::ActorUpdate, info, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }

        //tell clients to set up next hole
        std::uint16_t newHole = (m_currentHole << 8) | std::uint8_t(m_holeData[m_currentHole].par);
        m_sharedData.host.broadcastPacket(PacketID::SetHole, newHole, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

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
                m_scoreboardTime += dt;

                if (m_scoreboardTime > MaxScoreboardTime)
                {
                    setNextPlayer(true);
                    e.getComponent<cro::Callback>().active = false;
                    m_scene.destroyEntity(e);
                }
            }
            else
            {
                m_scoreboardTime = 0.f;
            }

            //make sure to keep resetting this to prevent unfairly
            //truncating the next player's turn
            m_turnTimer.restart();
        };

        m_scoreboardReadyFlags = 0;
    }
    else
    {
        //fudge in some delay and hope the final scores reach clients first...
        auto timerEnt = m_scene.createEntity();
        timerEnt.addComponent<cro::Callback>().active = true;
        timerEnt.getComponent<cro::Callback>().setUserData<float>(2.f);
        timerEnt.getComponent<cro::Callback>().function = [&](cro::Entity ent, float dt)
        {
            auto& ct = ent.getComponent<cro::Callback>().getUserData<float>();
            ct -= dt;

            if (ct < 0)
            {
                //end of game baby! TODO really this should include all final scores so
                //clients will make sure to show correct trophy/podium
                m_sharedData.host.broadcastPacket(PacketID::GameEnd, ConstVal::SummaryTimeout, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

                //create a timer ent which returns to lobby on time out
                auto entity = m_scene.createEntity();
                entity.addComponent<cro::Transform>();
                entity.addComponent<cro::Callback>().active = true;
                entity.getComponent<cro::Callback>().setUserData<float>(ConstVal::SummaryTimeout);
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

                ent.getComponent<cro::Callback>().active = false;
                m_scene.destroyEntity(ent);
            }
        };

        m_gameStarted = false;
    }
}

void GolfState::skipCurrentTurn(std::uint8_t clientID)
{
    if (m_playerInfo[0].client == clientID)
    {
        switch (m_playerInfo[0].ballEntity.getComponent<Ball>().state)
        {
        default:
            break;
        case Ball::State::Roll:
        case Ball::State::Flight:
        case Ball::State::Putt:
            m_scene.getSystem<BallSystem>()->fastForward(m_playerInfo[0].ballEntity);
            break;
        }
    }
}

bool GolfState::validateMap()
{
    auto mapDir = m_sharedData.mapDir.toAnsiString();
    auto mapPath = ConstVal::MapPath + mapDir + "/course.data";

    bool isUser = false;
    if (!cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + mapPath))
    {
        mapPath = cro::App::getPreferencePath() + ConstVal::UserMapPath + mapDir + "/course.data";
        isUser = true;

        if (!cro::FileSystem::fileExists(mapPath))
        {
            //TODO what's the best state to leave this in
            //if we fail to load a map? If the clients all
            //error this should be ok because the host will
            //kill the server for us
            return false;
        }
    }

    cro::ConfigFile courseFile;
    if (!courseFile.loadFromFile(mapPath, !isUser))
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

    //check the rules and truncate hole list
    //if requested - 1 front holes, 1 back holes
    if (m_sharedData.holeCount == 1)
    {
        auto size = std::max(std::size_t(1), holeStrings.size() / 2);
        holeStrings.resize(size);
    }
    else if (m_sharedData.holeCount == 2)
    {
        auto start = holeStrings.size() / 2;
        std::vector<std::string> newStrings(holeStrings.begin() + start, holeStrings.end());
        holeStrings.swap(newStrings);
    }

    //reverse the order if game rule requests
    if (m_sharedData.reverseCourse)
    {
        std::reverse(holeStrings.begin(), holeStrings.end());
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

void GolfState::initScene()
{
    auto& mb = m_sharedData.messageBus;
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_mapDataValid = m_scene.addSystem<BallSystem>(mb)->setHoleData(m_holeData[0]);
    m_scene.getSystem<BallSystem>()->setGimmeRadius(m_sharedData.gimmeRadius);
    
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

    std::shuffle(m_playerInfo.begin(), m_playerInfo.end(), cro::Util::Random::rndEngine);
}

void GolfState::buildWorld()
{
    //create a ball entity for each player
    for (auto& player : m_playerInfo)
    {
        player.terrain = m_scene.getSystem<BallSystem>()->getTerrain(player.position).terrain;

        player.ballEntity = m_scene.createEntity();
        player.ballEntity.addComponent<cro::Transform>().setPosition(player.position);
        player.ballEntity.addComponent<Ball>().terrain = player.terrain;

        player.holeScore.resize(m_holeData.size());
        std::fill(player.holeScore.begin(), player.holeScore.end(), 0);
    }
}

void GolfState::doServerCommand(const net::NetEvent& evt)
{
#ifdef CRO_DEBUG_
    switch (evt.packet.as<std::uint8_t>())
    {
    default: break;
    case ServerCommand::ChangeWind:
        m_scene.getSystem<BallSystem>()->forceWindChange();
        break;
    case ServerCommand::NextHole:
        m_currentHole = m_holeData.size() - 1;
        //if (m_currentHole < m_holeData.size() - 1)
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
        m_sharedData.host.broadcastPacket(PacketID::GameEnd, std::uint8_t(10), net::NetFlag::Reliable, ConstVal::NetChannelReliable);

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