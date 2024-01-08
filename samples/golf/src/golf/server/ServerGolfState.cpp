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
#include "../Clubs.hpp"
#include "../MessageIDs.hpp"
#include "../WeatherDirector.hpp"
#include "CPUStats.hpp"
#include "ServerGolfState.hpp"
#include "ServerMessages.hpp"

#include <AchievementIDs.hpp>
#include <Social.hpp>

#include <crogine/core/Log.hpp>
#include <crogine/core/ConfigFile.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Network.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/detail/glm/vec3.hpp>

#include <algorithm>

using namespace sv;

namespace
{
    constexpr float MaxScoreboardTime = 10.f;
    constexpr std::uint8_t MaxStrokes = 12;
    constexpr std::uint8_t MaxRandomTargets = 2;
    const cro::Time TurnTime = cro::seconds(90.f);
    const cro::Time WarnTime = cro::seconds(10.f);

    bool hadTennisBounce = false;

    //glm::vec3 randomOffset3()
    //{
    //    auto x = cro::Util::Random::value(0, 1) * 2;
    //    auto y = cro::Util::Random::value(0, 1) * 2;
    //
    //    x -= 1;
    //    y -= 1;
    //
    //    x *= cro::Util::Random::value(3, 8);
    //    y *= cro::Util::Random::value(3, 8);
    //
    //    glm::vec3 ret(x, 0.f, y);
    //    ret = glm::normalize(ret) *= cro::Util::Random::value(3.f, 5.f);
    //    return ret / 100.f;
    //};
}

GolfState::GolfState(SharedData& sd)
    : m_returnValue         (StateID::Golf),
    m_sharedData            (sd),
    m_mapDataValid          (false),
    m_scene                 (sd.messageBus, 512),
    m_scoreboardTime        (0.f),
    m_scoreboardReadyFlags  (0),
    m_gameStarted           (false),
    m_eliminationStarted    (false),
    m_allMapsLoaded         (false),
    m_skinsFinals           (false),
    m_currentHole           (0),
    m_skinsPot              (1),
    m_currentBest           (MaxStrokes),
    m_randomTargetCount     (0)
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
        std::array<std::uint8_t, 3u> packet =
        {
            m_playerInfo[0].client, m_playerInfo[0].player, achID
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
            std::uint8_t reason = MaxStrokeID::Default;
            switch (m_sharedData.scoreType)
            {
            default:
                
                break;
            case ScoreType::Stableford:
                maxStrokes = m_holeData[m_currentHole].par + 1;
                break;
            case ScoreType::NearestThePin:
            case ScoreType::LongestDrive:
                //each player only has one turn
                maxStrokes = 1;
                break;
            case ScoreType::MultiTarget:
                if (!m_scene.getSystem<BallSystem>()->getPuttFromTee()
                    && data.terrain == TerrainID::Green
                    && !m_playerInfo[0].targetHit)
                {
                    //player forfeits because they didn't hit the target
                    maxStrokes = m_playerInfo[0].holeScore[m_currentHole];
                    reason = MaxStrokeID::Forfeit;
                }
                break;
            case ScoreType::Skins:
                if (m_skinsFinals)
                {
                    maxStrokes *= 100;
                }
                break;
            }

            if (m_playerInfo[0].holeScore[m_currentHole] >= maxStrokes)
            {
                //set the player as having holed the ball
                m_playerInfo[0].position = m_holeData[m_currentHole].pin;
                m_playerInfo[0].distanceToHole = 0.f;

                //and penalise based on game mode
                switch (m_sharedData.scoreType)
                {
                default: break;
                case ScoreType::Stableford:
                    m_playerInfo[0].holeScore[m_currentHole]++;
                    break;
                case ScoreType::MultiTarget:
                    m_playerInfo[0].holeScore[m_currentHole] = m_scene.getSystem<BallSystem>()->getPuttFromTee() ? MaxStrokes / 2 : MaxStrokes;
                    break;
                }
                m_sharedData.host.broadcastPacket(PacketID::MaxStrokes, reason, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
            }
            else
            {
                m_playerInfo[0].position = data.position;
                m_playerInfo[0].distanceToHole = glm::length(data.position - m_holeData[m_currentHole].pin);
            }
            m_playerInfo[0].terrain = data.terrain;

            handleRules(data);
            
            setNextPlayer();
        }
        else if (data.type == GolfBallEvent::Holed)
        {
            //set the player as being on the pin so
            //they shouldn't be picked as next player
            m_playerInfo[0].position = m_holeData[m_currentHole].pin;
            m_playerInfo[0].distanceToHole = 0.f;
            m_playerInfo[0].terrain = data.terrain;

            handleRules(data);           

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

            if (hadTennisBounce && data.terrain == TerrainID::Fairway)
            {
                //send tennis achievement
                sendAchievement(AchievementID::CauseARacket);
            }
        }
        else if (data.type == GolfBallEvent::Gimme)
        {
            m_playerInfo[0].holeScore[m_currentHole]++;
            std::uint16_t inf = (m_playerInfo[0].client << 8) | m_playerInfo[0].player;
            m_sharedData.host.broadcastPacket<std::uint16_t>(PacketID::Gimme, inf, net::NetFlag::Reliable);

            handleRules(data);
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
        case TriggerID::TennisCourt:
            LogI << "Deuce!" << std::endl;
            hadTennisBounce = true;
            break;
        }
    }
    else if (msg.id == sv::MessageID::BullsEyeMessage)
    {
        const auto& data = msg.getData<BullsEyeEvent>();
        BullHit bh;
        bh.accuracy = data.accuracy;
        bh.position = data.position;
        bh.player = m_playerInfo[0].player;
        bh.client = m_playerInfo[0].client;
        m_sharedData.host.broadcastPacket(PacketID::BullHit, bh, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

        m_playerInfo[0].targetHit = true;
    }
    else if (msg.id == cl::MessageID::CollisionMessage)
    {
        const auto& data = msg.getData<CollisionEvent>();
        if (data.terrain == CollisionEvent::FlagPole)
        {
            BullHit pkt;
            pkt.client = m_playerInfo[0].client;
            pkt.player = m_playerInfo[0].player;
            //only needs to go to the relevant client
            //as this is for stat tracking
            m_sharedData.host.sendPacket(m_sharedData.clients[pkt.client].peer, PacketID::FlagHit, pkt, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
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
        case PacketID::NewPlayer:
            //checks if player is CPU and requires fast move
            //makeCPUMove();
            break;
        case PacketID::SkipTurn:
            skipCurrentTurn(evt.packet.as<std::uint8_t>());
            break;
        case PacketID::Activity:
            m_sharedData.host.broadcastPacket(PacketID::Activity, evt.packet.as<Activity>(), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
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
            doServerCommand(evt);
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
        case PacketID::FastCPU:
            if (evt.peer.getID() == m_sharedData.hostID)
            {
                m_sharedData.fastCPU = evt.packet.as<std::uint8_t>();
                m_sharedData.host.broadcastPacket(PacketID::FastCPU, m_sharedData.fastCPU, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

                //this checks current player is CPU and launch a move if inactive
                //makeCPUMove();
            }
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
            const auto ballC = ball.getComponent<Ball>();

            ActorInfo info;
            info.serverID = static_cast<std::uint32_t>(ball.getIndex());
            info.position = ball.getComponent<cro::Transform>().getPosition();
            info.rotation = cro::Util::Net::compressQuat(ball.getComponent<cro::Transform>().getRotation());
            //info.velocity = cro::Util::Net::compressVec3(ball.getComponent<Ball>().velocity);
            //info.velocity = ball.getComponent<Ball>().velocity;
            info.windEffect = ballC.windEffect;
            info.timestamp = timestamp;
            info.clientID = player.client;
            info.playerID = player.player;
            info.state = static_cast<std::uint8_t>(ballC.state);
            info.lie = ballC.lie;
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
        static bool warned = false;

        //check the turn timer and skip player if they AFK'd
        if (m_turnTimer.elapsed() > (TurnTime - WarnTime))
        {
            if (!warned
                 && m_sharedData.clients[m_playerInfo[0].client].peer.getID() != m_sharedData.hostID)
            {
                warned = true;
                m_sharedData.host.broadcastPacket(PacketID::WarnTime, std::uint8_t(10), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
            }

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
        }
        else
        {
            warned = false;
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

            if (allReady)
            {
                const auto createTarget =
                [&]()
                {
                    if (m_sharedData.scoreType == ScoreType::MultiTarget)
                    {
                        return true;
                    }

                    const auto& progress = Social::getMonthlyChallenge().getProgress();

                    if (Social::getMonth() == 2
                        && (progress.value != progress.target)
                        && m_sharedData.scoreType == ScoreType::Stroke)
                    {
                        if (cro::Util::Random::value(0, 9) == 0)
                        {
                            return m_randomTargetCount++ < MaxRandomTargets;
                        }
                    }

                    return false;
                };

                //if the game mode requires or challenge requires
                //spawn the bullseye.
                if (createTarget())
                {
                    const auto& b = m_scene.getSystem<BallSystem>()->spawnBullsEye();
                    if (b.spawn)
                    {
                        m_sharedData.host.broadcastPacket(PacketID::BullsEye, b, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                    }
                }
            }
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

        //make sure to enforce club set if needed
        if (m_sharedData.clubLimit)
        {
            std::sort(m_sharedData.clubLevels.begin(), m_sharedData.clubLevels.end(),
                [](std::uint8_t a, std::uint8_t b)
                {
                    return a < b;
                });

            m_sharedData.host.broadcastPacket(PacketID::MaxClubs, m_sharedData.clubLevels[0], net::NetFlag::Reliable, ConstVal::NetChannelReliable);
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
    //each player is sequential (ideally with fewest skins, use connect ID to tie break)
    const auto skinsPredicate = 
        [&](const PlayerStatus& a, const PlayerStatus& b)
    {
            return a.holeScore[m_currentHole] == b.holeScore[m_currentHole]
                ? a.skins == b.skins ?
                ((a.client * ConstVal::MaxClients) + a.player) > ((b.client * ConstVal::MaxClients) + b.player)
                : a.skins < b.skins
            : a.holeScore[m_currentHole] < b.holeScore[m_currentHole];
    };

    hadTennisBounce = false;

    //broadcast current player's score first
    ScoreUpdate su;
    su.strokeDistance = m_playerInfo[0].ballEntity.getComponent<Ball>().lastStrokeDistance;
    su.client = m_playerInfo[0].client;
    su.player = m_playerInfo[0].player;
    su.score = m_playerInfo[0].totalScore;
    su.stroke = m_playerInfo[0].holeScore[m_currentHole];
    su.distanceScore = m_playerInfo[0].distanceScore[m_currentHole];
    su.matchScore = m_playerInfo[0].matchWins;
    su.skinsScore = m_playerInfo[0].skins;
    su.hole = m_currentHole;
    m_sharedData.host.broadcastPacket(PacketID::ScoreUpdate, su, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

    m_playerInfo[0].ballEntity.getComponent<Ball>().lastStrokeDistance = 0.f;

    if (!newHole || m_currentHole == 0)
    {
        if (m_sharedData.scoreType == ScoreType::BBB)
        {
            //if BBB favour players not on green
            std::sort(m_playerInfo.begin(), m_playerInfo.end(),
                [](const PlayerStatus& a, const PlayerStatus& b)
                {
                    return a.terrain != TerrainID::Green;
                });

            auto currTerrain = m_playerInfo[0].terrain;
            if (currTerrain == TerrainID::Green)
            {
                std::sort(m_playerInfo.begin(), m_playerInfo.end(),
                    [](const PlayerStatus& a, const PlayerStatus& b)
                    {
                        return a.distanceToHole > b.distanceToHole;
                    });

                //TODO award Bango for closest if not already awarded
            }
            else
            {
                std::sort(m_playerInfo.begin(), m_playerInfo.end(),
                    [](const PlayerStatus& a, const PlayerStatus& b)
                    {
                        return (a.distanceToHole > b.distanceToHole)
                            && a.terrain != TerrainID::Green;
                    });
            }
        }
        else if (m_sharedData.scoreType == ScoreType::BattleRoyale)
        {
            //make sure eliminated are last before sorting by distance
            std::sort(m_playerInfo.begin(), m_playerInfo.end(),
                [](const PlayerStatus& a, const PlayerStatus& b)
                {
                    if (!a.eliminated && !b.eliminated)
                    {
                        return a.distanceToHole > b.distanceToHole;
                    }
                    
                    return !a.eliminated;
                });
        }
        else
        {
            if (m_skinsFinals)
            {
                std::sort(m_playerInfo.begin(), m_playerInfo.end(), skinsPredicate);
            }
            else
            {
                //sort players by distance
                std::sort(m_playerInfo.begin(), m_playerInfo.end(),
                    [](const PlayerStatus& a, const PlayerStatus& b)
                    {
                        return a.distanceToHole > b.distanceToHole;
                    });
            }
        }
    }
    else
    {
        if (m_skinsFinals)
        {
            std::sort(m_playerInfo.begin(), m_playerInfo.end(), skinsPredicate);
        }
        else
        {
            //winner of the last hole goes first
            std::sort(m_playerInfo.begin(), m_playerInfo.end(),
                [&](const PlayerStatus& a, const PlayerStatus& b)
                {
                    return a.holeScore[m_currentHole - 1] < b.holeScore[m_currentHole - 1];
                });

            //check the last honour taker to see if their score matches
            //current first position and swap them in to first if so
            //TODO we could probably include this in the predicate above
            //but ehhhh no harm in being explicit I guess?
            if (m_playerInfo[0].client != m_honour[0]
                || m_playerInfo[0].player != m_honour[1])
            {
                auto r = std::find_if(m_playerInfo.begin(), m_playerInfo.end(),
                    [&](const PlayerStatus& ps)
                    {
                        return ps.client == m_honour[0] && ps.player == m_honour[1];
                    });

                if (r != m_playerInfo.end())
                {
                    if (r->holeScore[m_currentHole - 1] == m_playerInfo[0].holeScore[m_currentHole - 1])
                    {
                        std::swap(m_playerInfo[std::distance(m_playerInfo.begin(), r)], m_playerInfo[0]);
                    }
                }
            }

            //set whoever is first as current honour taker
            m_honour[0] = m_playerInfo[0].client;
            m_honour[1] = m_playerInfo[0].player;
        }
    }


    if (m_playerInfo[0].distanceToHole == 0 //all players must be in the hole
        || (m_sharedData.scoreType == ScoreType::BattleRoyale && m_playerInfo.size() == 1) //players have quit the game so attempt next hole
        || (m_sharedData.scoreType == ScoreType::BattleRoyale && m_playerInfo[1].eliminated)) //(which triggers the rules to end the game)
    {
        setNextHole();
    }
    else
    {
        //go to next player

        //if we're on a putting course and this is the player's first turn
        //offset them from the tee a little
        //TODO hmm this adds vertical offset too for some reason
        //if (m_scene.getSystem<BallSystem>()->getPuttFromTee()
        //    && m_playerInfo[0].holeScore[m_currentHole] == 0)
        //{
        //    m_playerInfo[0].position += randomOffset3();
        //    m_playerInfo[0].ballEntity.getComponent<cro::Transform>().setPosition(m_playerInfo[0].position);
        //    //LogI << "added offset" << std::endl;
        //}

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


    //update player skins/match scores
    auto gameFinished = summariseRules();
    

    //broadcast all scores to make sure everyone is up to date
    //note that in skins games the above summary may have reduced
    //the current hole index if the hole needs repeating
    auto scoreHole = m_skinsFinals ? std::min(m_currentHole + 1, std::uint8_t(m_holeData.size()) - 1) : m_currentHole;
    for (auto& player : m_playerInfo)
    {
        player.totalScore += player.holeScore[scoreHole];
        player.targetHit = false;

        ScoreUpdate su;
        su.client = player.client;
        su.player = player.player;
        su.hole = scoreHole;
        su.score = player.totalScore;
        su.matchScore = player.matchWins;
        su.skinsScore = player.skins;
        su.stroke = player.holeScore[scoreHole];

        m_sharedData.host.broadcastPacket(PacketID::ScoreUpdate, su, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
    }

    //set clients as not yet loaded
    for (auto& client : m_sharedData.clients)
    {
        client.mapLoaded = false;
    }
    m_allMapsLoaded = false;
    m_scoreboardTime = 0.f;

    m_eliminationStarted = true;

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

        //tell clients to remove any bulls eye target
        m_sharedData.host.broadcastPacket(PacketID::BullsEye, BullsEye(), net::NetFlag::Reliable, ConstVal::NetChannelReliable);

        //tell clients to set up next hole
        std::uint16_t newHole = (m_currentHole << 8) | std::uint8_t(m_holeData[m_currentHole].par);
        m_sharedData.host.broadcastPacket(PacketID::SetHole, newHole, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

        //create an ent which waits for all clients to load the next hole
        //which include playing the transition animation.
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<float>(0.f);
        entity.getComponent<cro::Callback>().function =
            [&, ballSystem](cro::Entity e, float dt)
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

                //use a timer so we don't hammer the network with this
                static constexpr float PingTime = 0.5f;
                auto& pingTime = e.getComponent<cro::Callback>().getUserData<float>();
                pingTime += dt;

                if (pingTime > PingTime)
                {
                    pingTime -= PingTime;
                    //apparently we have to do this here else the client just ignores it
                    for (auto i = 0u; i < m_playerInfo.size(); ++i)
                    {
                        auto& player = m_playerInfo[i];
                        player.position = m_holeData[m_currentHole].tee;
                        player.distanceToHole = glm::length(player.position - m_holeData[m_currentHole].pin);
                        player.terrain = ballSystem->getTerrain(player.position).terrain;

                        auto ball = player.ballEntity;
                        ball.getComponent<Ball>().terrain = player.terrain;
                        ball.getComponent<Ball>().velocity = glm::vec3(0.f);
                        ball.getComponent<cro::Transform>().setPosition(player.position);

                        auto timestamp = m_serverTime.elapsed().asMilliseconds();

                        ActorInfo info;
                        info.serverID = static_cast<std::uint32_t>(ball.getIndex());
                        info.position = ball.getComponent<cro::Transform>().getPosition();
                        info.rotation = cro::Util::Net::compressQuat(ball.getComponent<cro::Transform>().getRotation());
                        info.windEffect = ball.getComponent<Ball>().windEffect;
                        info.timestamp = timestamp;
                        info.clientID = player.client;
                        info.playerID = player.player;
                        info.state = static_cast<std::uint8_t>(ball.getComponent<Ball>().state);
                        m_sharedData.host.broadcastPacket(PacketID::ActorUpdate, info, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                    }
                }
            }

            //make sure to keep resetting this to prevent unfairly
            //truncating the next player's turn
            m_turnTimer.restart();
        };

        m_scoreboardReadyFlags = 0;

        //if this is battle royale set all eliminated players to forfeit
        if (m_sharedData.scoreType == ScoreType::BattleRoyale)
        {
            for (auto& p : m_playerInfo)
            {
                if (p.eliminated)
                {
                    p.holeScore[m_currentHole] = m_holeData[m_currentHole].puttFromTee ? 6 : 12;
                    p.distanceToHole = 0.f;
                }
            }
        }
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
                //end of game baby!
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
        //hmmm this check also happens on the client which should make sure it
        //creates the path first to prevent filesystem exception... as
        //this runs in its own thread trying to create it here too is probably not a good idea

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

        static constexpr std::int32_t MaxProps = 5;
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
            else if (name == "target")
            {
                holeData.target = holeProp.getValue<glm::vec3>();
                holeData.target.x = glm::clamp(holeData.target.x, 0.f, 320.f);
                holeData.target.z = glm::clamp(holeData.target.z, -200.f, 0.f);
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
    m_scene.addSystem<BallSystem>(mb)->setGimmeRadius(m_sharedData.gimmeRadius);

    if (m_sharedData.weatherType == WeatherType::Showers)
    {
        m_scene.addDirector<WeatherDirector>(m_sharedData.host);
    }

    //trim the hole data if we play short round
    if (m_sharedData.scoreType == ScoreType::ShortRound
        && getCourseIndex(m_sharedData.mapDir.toAnsiString()) != -1)
    {
        switch (m_sharedData.holeCount)
        {
        default:
        case 0:
        {
            auto size = std::min(m_holeData.size(), std::size_t(12));
            m_holeData.resize(size);
        }
        break;
        case 1:
        case 2:
        {
            auto size = std::min(m_holeData.size(), std::size_t(6));
            m_holeData.resize(size);
        }
        break;
        }
    }

    //check for putt from tee and update any rule properties
    for (auto& hole : m_holeData)
    {
        m_scene.getSystem<BallSystem>()->setHoleData(hole); //applies putt from tee

        switch (m_sharedData.scoreType)
        {
        default: break;
        case ScoreType::MultiTarget:
            if (!hole.puttFromTee)
            {
                hole.par++;
            }
            break;
        }
    }

    m_mapDataValid = m_scene.getSystem<BallSystem>()->setHoleData(m_holeData[0]);
    
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
        player.distanceScore.resize(m_holeData.size());
        std::fill(player.holeScore.begin(), player.holeScore.end(), 0);
        std::fill(player.distanceScore.begin(), player.distanceScore.end(), 0.f);
    }
}

void GolfState::doServerCommand(const net::NetEvent& evt)
{
    switch (evt.packet.as<std::uint16_t>())
    {
    default: break;
    case ServerCommand::SkipTurn:
        if (m_gameStarted && m_allMapsLoaded)
        {
            auto result = std::find_if(m_sharedData.clients.begin(), m_sharedData.clients.end(),
                [&](const sv::ClientConnection& cc)
                {
                    return cc.peer == evt.peer;
                });

            if (result != m_sharedData.clients.end())
            {
                auto client = std::distance(m_sharedData.clients.begin(), result);
                if (m_playerInfo[0].client == client
                    && m_playerInfo[0].ballEntity.getComponent<Ball>().state == Ball::State::Idle)
                {
                    setNextPlayer();
                }
            }
        }
        break;
    }

    if (evt.peer.getID() == m_sharedData.hostID)
    {
        const auto data = evt.packet.as<std::uint16_t>();
        const std::uint8_t command = (data & 0xff);
        const std::uint8_t target = ((data >> 8) & 0xff);

        switch (command)
        {
        default: break;
        case ServerCommand::PokeClient:
            if (/*target != 0 &&*/ target < ConstVal::MaxClients)
            {
                m_sharedData.host.sendPacket(m_sharedData.clients[target].peer, PacketID::Poke, std::uint8_t(0), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
            }
            break;
        case ServerCommand::ForfeitClient:
            //if (target != 0/* && target < ConstVal::MaxClients*/)
            {
                if (m_gameStarted
                    /*&& m_playerInfo[0].client == target*/)
                {
                    m_playerInfo[0].holeScore[m_currentHole] = MaxStrokes; //this should be half on putt from tee but meh, it's a penalty
                    m_playerInfo[0].position = m_holeData[m_currentHole].pin;
                    m_playerInfo[0].distanceToHole = 0.f;
                    m_playerInfo[0].terrain = TerrainID::Green;
                    setNextPlayer();
                }
            }
            break;
        case ServerCommand::KickClient:
            if (target != 0 && target < ConstVal::MaxClients)
            {
                auto* msg = m_sharedData.messageBus.post<ConnectionEvent>(MessageID::ConnectionMessage);
                msg->type = ConnectionEvent::Kicked;
                msg->clientID = target;
            }
            break;
#ifdef CRO_DEBUG_
        case ServerCommand::GotoHole:
        {
            m_playerInfo[0].position = m_holeData[m_currentHole].pin;
            m_playerInfo[0].holeScore[m_currentHole] = MaxStrokes;
            m_playerInfo[0].distanceToHole = 0.f;
            m_playerInfo[0].terrain = TerrainID::Green;
            setNextPlayer();
        }

        break;
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
            //set ball to green position
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
#endif
        break;
        }
    }
}
