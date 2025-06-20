/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
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
#include "../League.hpp"
#include "../AvatarAnimation.hpp"
#include "CPUStats.hpp"
#include "ServerGolfState.hpp"
#include "ServerMessages.hpp"

#include <AchievementIDs.hpp>
#include <Social.hpp>
#include <Input.hpp>
#include <Content.hpp>

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
    bool hadWallBounce = false;

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
    m_ntpPro                (sd.scoreType == ScoreType::NearestThePinPro),
    m_mapDataValid          (false),
    m_scene                 (sd.messageBus, 512),
    m_scoreboardTime        (0.f),
    m_scoreboardReadyFlags  (0),
    m_gameStarted           (false),
    //m_eliminationStarted    (false),
    m_allMapsLoaded         (false),
    m_skinsFinals           (false),
    m_currentHole           (0),
    m_skinsPot              (1),
    m_currentBest           (MaxStrokes),
    m_randomTargetCount     (0)
{
    if (m_mapDataValid = validateMap(); m_mapDataValid)
    {
        if (sd.scoreType == ScoreType::NearestThePinPro)
        {
            sd.scoreType = ScoreType::NearestThePin;
        }

        initScene();
        buildWorld();
    }

    LOG("Entered Server Golf State", cro::Logger::Type::Info);
}

void GolfState::handleMessage(const cro::Message& msg)
{
    const auto sendAchievement = [&](std::uint8_t achID, std::uint8_t client, std::uint8_t player)
    {
        //TODO not sure we want to send this blindly to the client
        //but only the client knows if achievements are currently enabled.
        std::array<std::uint8_t, 3u> packet =
        {
           client, player, achID
        };
        m_sharedData.host.broadcastPacket(PacketID::ServerAchievement, packet, net::NetFlag::Reliable);
    };

    if (msg.id == sv::MessageID::ConnectionMessage)
    {
        const auto& data = msg.getData<ConnectionEvent>();
        if (data.type == ConnectionEvent::Disconnected)
        {
            //disconnect notification packet is sent in Server
            std::int32_t setNewPlayer = -1;
            auto& group = m_playerInfo[m_groupAssignments[data.clientID]];
            auto& playerInfo = group.playerInfo;
            //for (auto i = 0; i < m_playerInfo.size(); ++i)
            {
                //if this client was the current player check if there's
                //enother player to take over
                bool wantNewPlayer = false;
                if (playerInfo[0].client == data.clientID)
                {
                    wantNewPlayer = true;
                }

                //remove the player data
                group.playerCount -= data.playerCount;
                group.clientIDs.erase(std::remove_if(group.clientIDs.begin(), group.clientIDs.end(), 
                    [data](std::uint8_t i)
                    {
                        return data.clientID == i;
                    }), group.clientIDs.end());

                playerInfo.erase(std::remove_if(playerInfo.begin(), playerInfo.end(),
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
                    playerInfo.end());

                if (playerInfo.empty()
                    || wantNewPlayer)
                {
                    //if this group is empty setNextPlayer() will still
                    //test if we want the next hole if other groups are waiting
                    setNewPlayer = m_groupAssignments[playerInfo[0].client];
                }
            }

            if (m_sharedData.teamMode)
            {
                for (auto& team : m_teams)
                {
                    for (auto i = 0u; i < team.players.size(); ++i)
                    {
                        //if this was the removed client set to the other player
                        //this just creates an empty team if no players are left
                        if (team.players[i][0] == data.clientID)
                        {
                            const auto idx = (i + 1) % 2;
                            team.players[i][0] = team.players[idx][0];
                            team.players[i][1] = team.players[idx][1];
                        }
                    }
                }
            }


            if (!m_playerInfo.empty())
            {
                if (setNewPlayer != -1
                    && m_gameStarted) //don't do this on the summary screen
                {
                    setNextPlayer(setNewPlayer);
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
        const auto groupID = m_groupAssignments[data.client];
        auto& playerInfo = m_playerInfo[groupID].playerInfo;

        if (!playerInfo.empty())
        {
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
                    //case ScoreType::NearestThePin:
                case ScoreType::LongestDrive:
                    //each player only has one turn
                    maxStrokes = 1;
                    break;
                case ScoreType::MultiTarget:
                    if (!m_scene.getSystem<BallSystem>()->getPuttFromTee()
                        && data.terrain == TerrainID::Green
                        && !playerInfo[0].targetHit)
                    {
                        //player forfeits because they didn't hit the target
                        maxStrokes = playerInfo[0].holeScore[m_currentHole];
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

                if (playerInfo[0].holeScore[m_currentHole] >= maxStrokes)
                {
                    //set the player as having holed the ball
                    playerInfo[0].position = m_holeData[m_currentHole].pin;
                    playerInfo[0].distanceToHole = 0.f;

                    //and penalise based on game mode
                    switch (m_sharedData.scoreType)
                    {
                    default: break;
                    case ScoreType::Stableford:
                        playerInfo[0].holeScore[m_currentHole]++;
                        break;
                    case ScoreType::MultiTarget:
                        playerInfo[0].holeScore[m_currentHole] = m_scene.getSystem<BallSystem>()->getPuttFromTee() ? MaxStrokes / 2 : MaxStrokes;
                        break;
                    }

                    for (auto c : m_playerInfo[groupID].clientIDs)
                    {
                        m_sharedData.host.sendPacket(m_sharedData.clients[c].peer, PacketID::MaxStrokes, reason, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                    }
                    //broadcast a hole complete for this client (to update client scores)
                    std::uint16_t pkt = (playerInfo[0].client << 8) | playerInfo[0].player;
                    m_sharedData.host.broadcastPacket(PacketID::HoleComplete, pkt, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                }
                else
                {
                    playerInfo[0].position = data.position;
                    playerInfo[0].distanceToHole = glm::length(data.position - m_holeData[m_currentHole].pin);
                }
                playerInfo[0].terrain = data.terrain;

                handleRules(groupID, data);

                setNextPlayer(groupID);

                m_sharedData.host.broadcastPacket(PacketID::GroupTurnEnded, groupID, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
            }
            else if (data.type == GolfBallEvent::Holed)
            {
                //set the player as being on the pin so
                //they shouldn't be picked as next player
                playerInfo[0].position = m_holeData[m_currentHole].pin;
                playerInfo[0].distanceToHole = 0.f;
                playerInfo[0].terrain = data.terrain;

                handleRules(groupID, data);

                if (playerInfo[0].holeScore[m_currentHole] < m_currentBest)
                {
                    m_currentBest = playerInfo[0].holeScore[m_currentHole];
                }

                setNextPlayer(groupID);

                m_sharedData.host.broadcastPacket(PacketID::GroupHoled, groupID, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
            }
            else if (data.type == GolfBallEvent::Foul)
            {
                playerInfo[0].holeScore[m_currentHole]++; //penalty stroke.

                if (playerInfo[0].holeScore[m_currentHole] >= MaxStrokes)
                {
                    //set the player as having holed the ball
                    playerInfo[0].position = m_holeData[m_currentHole].pin;
                    playerInfo[0].distanceToHole = 0.f;
                    playerInfo[0].terrain = TerrainID::Green;
                }

                if (m_sharedData.scoreType == ScoreType::NearestThePin)
                {
                    playerInfo[0].holeScore[m_currentHole] = MaxNTPStrokes + 1;
                }
            }
            else if (data.type == GolfBallEvent::Landed)
            {
                if (m_sharedData.scoreType == ScoreType::Elimination)
                {
                    if (data.terrain == TerrainID::Hole
                        && (playerInfo[0].eliminated || playerInfo[0].matchWins)) //tagged as lost a life
                    {
                        playerInfo[0].matchWins = 0;
                        return;
                    }
                }

                //immediate, pre-pause event when the ball stops moving
                BallUpdate bu;
                bu.terrain = data.terrain;
                bu.position = data.position;

                for (auto c : m_playerInfo[groupID].clientIDs)
                {
                    m_sharedData.host.sendPacket(m_sharedData.clients[c].peer, PacketID::BallLanded, bu, net::NetFlag::Reliable);
                }

                auto dist = glm::length2(playerInfo[0].position - data.position);
                if (dist > 2250000.f)
                {
                    //1500m
                    sendAchievement(AchievementID::IntoOrbit, playerInfo[0].client, playerInfo[0].player);
                }

                if (hadTennisBounce && data.terrain == TerrainID::Fairway)
                {
                    //send tennis achievement
                    sendAchievement(AchievementID::CauseARacket, playerInfo[0].client, playerInfo[0].player);
                }

                if (hadWallBounce && (data.terrain == TerrainID::Rough || data.terrain == TerrainID::Green || data.terrain == TerrainID::Fairway))
                {
                    sendAchievement(AchievementID::OffTheWall, playerInfo[0].client, playerInfo[0].player);
                }
            }
            else if (data.type == GolfBallEvent::Gimme)
            {
                playerInfo[0].holeScore[m_currentHole]++;

                std::uint16_t inf = (playerInfo[0].client << 8) | playerInfo[0].player;
                //m_sharedData.host.broadcastPacket<std::uint16_t>(PacketID::Gimme, inf, net::NetFlag::Reliable);

                for (auto c : m_playerInfo[groupID].clientIDs)
                {
                    m_sharedData.host.sendPacket<std::uint16_t>(m_sharedData.clients[c].peer, PacketID::Gimme, inf, net::NetFlag::Reliable);
                }
                handleRules(groupID, data);
            }
        }
    }
    else if (msg.id == sv::MessageID::TriggerMessage)
    {
        const auto& data = msg.getData<TriggerEvent>();
        const auto& group = m_playerInfo[m_groupAssignments[data.client]];
        switch (data.triggerID)
        {
        default: break;
        case TriggerID::Volcano:
            sendAchievement(AchievementID::HotStuff, group.playerInfo[0].client, group.playerInfo[0].player);
            break;
        case TriggerID::Boat:
            sendAchievement(AchievementID::ISeeNoShips, group.playerInfo[0].client, group.playerInfo[0].player);
            break;
        case TriggerID::TennisCourt:
            LogI << "Deuce!" << std::endl;
            hadTennisBounce = true;
            break;
        case TriggerID::BackWall:
            LogI << "FORE" << std::endl;
            hadWallBounce = true;
            break;
        }
    }
    else if (msg.id == sv::MessageID::BullsEyeMessage)
    {
        const auto& data = msg.getData<BullsEyeEvent>();
        auto& group = m_playerInfo[m_groupAssignments[data.client]];

        BullHit bh;
        bh.accuracy = data.accuracy;
        bh.position = data.position;
        bh.player = group.playerInfo[0].player;
        bh.client = group.playerInfo[0].client;
        
        for (auto c : group.clientIDs)
        {
            m_sharedData.host.sendPacket(m_sharedData.clients[c].peer, PacketID::BullHit, bh, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
        //let all clients know to update the score boards
        std::uint16_t pkt = (bh.client << 8) | bh.player;
        m_sharedData.host.broadcastPacket(PacketID::TargetHit, pkt, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

        group.playerInfo[0].targetHit = true;
    }
    else if (msg.id == cl::MessageID::CollisionMessage)
    {
        const auto& data = msg.getData<CollisionEvent>();
        if (data.terrain == CollisionEvent::FlagPole)
        {
            BullHit pkt;
            pkt.client = m_playerInfo[m_groupAssignments[data.client]].playerInfo[0].client;
            pkt.player = m_playerInfo[m_groupAssignments[data.client]].playerInfo[0].player;
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
        case PacketID::Mulligan:
            applyMulligan();
            break;
        case PacketID::ClubChanged:
        {
            auto data = evt.packet.as<std::uint16_t>();
            auto client = data & 0xff;
            const auto group = m_playerInfo[m_groupAssignments[client]];
            for (auto c : group.clientIDs)
            {
                m_sharedData.host.sendPacket(m_sharedData.clients[c].peer, PacketID::ClubChanged, data, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
            }
        }
            break;
        case PacketID::DronePosition:
            m_sharedData.host.broadcastPacket(PacketID::DronePosition, evt.packet.as<std::array<std::int16_t, 3u>>(), net::NetFlag::Unreliable);
            break;
        case PacketID::AvatarRotation:
            m_sharedData.host.broadcastPacket(PacketID::AvatarRotation, evt.packet.as<std::uint32_t>(), net::NetFlag::Unreliable);
            break;
        case PacketID::NewPlayer:
            //checks if player is CPU and requires fast move
            //makeCPUMove();
            break;
        case PacketID::SkipTurn:
            skipCurrentTurn(evt.packet.as<std::uint8_t>());
            break;
        case PacketID::Activity:
        {
            const auto pkt = evt.packet.as<Activity>();
            const auto& group = m_playerInfo[m_groupAssignments[pkt.client]];
            for (auto c : group.clientIDs)
            {
                m_sharedData.host.sendPacket(m_sharedData.clients[c].peer, PacketID::Activity, pkt, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
            }
        }
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
    for (const auto& group : m_playerInfo)
    {
        for (const auto& player : group.playerInfo)
        {
            //only send active player's ball
            auto ball = player.ballEntity;

            //ideally we want to send only non-idle balls but without
            //sending a few pre-movement frames first we get visible pops
            //in the client side interpolation.
            if (ball == group.playerInfo[0].ballEntity/* ||
                ball.getComponent<Ball>().state != Ball::State::Idle*/)
            {
                const auto timestamp = m_serverTime.elapsed().asMilliseconds();
                auto& ballC = ball.getComponent<Ball>();

                ActorInfo info;
                info.serverID = static_cast<std::uint32_t>(ball.getIndex());
                info.position = ball.getComponent<cro::Transform>().getPosition();
                info.rotation = cro::Util::Net::compressQuat(ball.getComponent<cro::Transform>().getRotation());
                info.windEffect = ballC.windEffect;
                info.timestamp = timestamp;
                info.clientID = player.client;
                info.playerID = player.player;
                info.state = static_cast<std::uint8_t>(ballC.state);
                info.lie = ballC.lie;
                info.groupID = m_groupAssignments[player.client];
                //as these are only used for sound effects only send the events where we bounce on something
                info.collisionTerrain = ballC.state == Ball::State::Flight ? ballC.lastTerrain : ConstVal::NullValue;
                ballC.lastTerrain = ConstVal::NullValue;
                m_sharedData.host.broadcastPacket(PacketID::ActorUpdate, info, net::NetFlag::Unreliable);
            }
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
        for (auto& group : m_playerInfo)
        {
            if (!group.playerInfo.empty()) //players may have quit
            {
                if (group.playerInfo[0].distanceToHole == 0)
                {
                    //we're waiting for other players to finish so don't time out
                    group.turnTimer.restart();
                }
                else
                {
                    if (group.turnTimer.elapsed() > (TurnTime - WarnTime))
                    {
                        if (!group.warned
                            && m_sharedData.clients[group.playerInfo[0].client].peer.getID() != m_sharedData.hostID)
                        {
                            group.warned = true;
                            m_sharedData.host.broadcastPacket(PacketID::WarnTime, std::uint8_t(10), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                        }

                        if (group.turnTimer.elapsed() > TurnTime)
                        {
                            if (m_sharedData.clients[group.playerInfo[0].client].peer.getID() != m_sharedData.hostID)
                            {
                                group.playerInfo[0].holeScore[m_currentHole] = MaxStrokes;
                                group.playerInfo[0].position = m_holeData[m_currentHole].pin;
                                group.playerInfo[0].distanceToHole = 0.f;
                                group.playerInfo[0].terrain = TerrainID::Green;
                                setNextPlayer(m_groupAssignments[group.playerInfo[0].client]); //resets the timer

                                for (auto c : group.clientIDs)
                                {
                                    m_sharedData.host.sendPacket(m_sharedData.clients[c].peer, PacketID::MaxStrokes, std::uint8_t(MaxStrokeID::IdleTimeout), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                                }
                                //broadcast hole complete message so all clients update scores correctly
                                std::uint16_t pkt = (group.playerInfo[0].client << 8) | group.playerInfo[0].player;
                                m_sharedData.host.broadcastPacket(PacketID::HoleComplete, pkt, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                            }
                        }
                    }
                    else
                    {
                        group.warned = false;
                    }
                }
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
                        if (cro::Util::Random::value(0, 4) == 0)
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

        //tell the client which group we were assigned to
        m_sharedData.host.sendPacket(m_sharedData.clients[clientID].peer, PacketID::GroupID, std::uint8_t(m_groupAssignments[clientID]), net::NetFlag::Reliable, ConstVal::NetChannelReliable);

        //send all the ball positions
        auto timestamp = m_serverTime.elapsed().asMilliseconds();

        for (const auto& group : m_playerInfo)
        {
            for (const auto& player : group.playerInfo)
            {
                auto ball = player.ballEntity;

                ActorInfo info;
                info.serverID = static_cast<std::uint32_t>(ball.getIndex());
                info.position = ball.getComponent<cro::Transform>().getPosition();
                info.clientID = player.client;
                info.playerID = player.player;
                info.timestamp = timestamp;
                info.groupID = m_groupAssignments[player.client];
                
                //all clients should spawn a ball
                m_sharedData.host.sendPacket(m_sharedData.clients[clientID].peer, PacketID::ActorSpawn, info, net::NetFlag::Reliable);
            }

            //make sure to enforce club set if needed
            if (m_sharedData.clubLimit)
            {
                //TODO we don't really need to be sorting this every time we send to a new client...
                std::sort(m_sharedData.clubLevels.begin(), m_sharedData.clubLevels.end(),
                    [](std::uint8_t a, std::uint8_t b)
                    {
                        return a < b;
                    });

                m_sharedData.host.broadcastPacket(PacketID::MaxClubs, m_sharedData.clubLevels[0], net::NetFlag::Reliable, ConstVal::NetChannelReliable);
            }
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
                    for (auto i = 0u; i < m_playerInfo.size(); ++i)
                    {
                        setNextPlayer(static_cast<std::int32_t>(i));
                    }
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
            for (auto& group : m_playerInfo)
            {
                group.turnTimer.restart();
            }
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

    const auto input = packet.as<InputUpdate>();
    auto& group = m_playerInfo[m_groupAssignments[input.clientID]];

    if (group.playerInfo.empty())
    {
        return;
    }

    if (group.playerInfo[0].client == input.clientID
        && group.playerInfo[0].player == input.playerID)
    {
        //we make a copy of this and then re-apply it if not predicting a result
        auto ball = group.playerInfo[0].ballEntity.getComponent<Ball>();

        if (ball.state == Ball::State::Idle)
        {
            const bool isPutt = input.clubID == ClubID::Putter;

            ball.velocity = input.impulse;
            ball.state = isPutt ? Ball::State::Putt : Ball::State::Flight;
            //this is a kludge to wait for the anim before hitting the ball
            //Ideally we want to read the frame data from the avatar
            //as well as account for a frame of interp delay on the client
            //at the very least we should add the client ping to this
            ball.delay = isPutt ? 0.05f : 1.17f;
            ball.delay += static_cast<float>(m_sharedData.clients[input.clientID].peer.getRoundTripTime()) / 1000.f;
            ball.startPoint = group.playerInfo[0].ballEntity.getComponent<cro::Transform>().getPosition();

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
                group.playerInfo[0].previousBallScore = group.playerInfo[0].holeScore[m_currentHole];
                group.playerInfo[0].holeScore[m_currentHole]++;
                group.playerInfo[0].prevBallPos = ball.startPoint;

                auto animID = isPutt ? AnimationID::Putt : AnimationID::Swing;

                for (auto c : group.clientIDs)
                {
                    m_sharedData.host.sendPacket(m_sharedData.clients[c].peer, PacketID::ActorAnimation, std::uint8_t(animID), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                }
                group.turnTimer.restart(); //don't time out mid-shot...
                group.playerInfo[0].ballEntity.getComponent<Ball>() = ball;
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

        for (auto& group : m_playerInfo)
        {
            for (auto i = 0u; i < group.playerInfo.size(); ++i)
            {
                if ((m_scoreboardReadyFlags & (1 << group.playerInfo[i].client)) == 0)
                {
                    return;
                }
            }
        }
        m_scoreboardTime = MaxScoreboardTime; //skips ahead to timeout if everyone is ready
        return;
    }

    std::uint8_t broadcastFlags = 0;
    for (auto& group : m_playerInfo)
    {
        for (auto& p : group.playerInfo)
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
    }
    //let clients know to update their display
    m_sharedData.host.broadcastPacket<std::uint8_t>(PacketID::ReadyQuitStatus, broadcastFlags, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

    for (auto& group : m_playerInfo)
    {
        for (const auto& p : group.playerInfo)
        {
            if (!p.readyQuit)
            {
                //not everyone is ready
                return;
            }
        }
    }
    //if we made it here it's time to quit!
    m_returnValue = StateID::Lobby;
}

void GolfState::setNextPlayer(std::int32_t groupID, bool newHole)
{
    //each player is sequential (ideally with fewest skins, use connect ID to tie break)
    const auto skinsPredicate = 
        [&](const PlayerStatus& a, const PlayerStatus& b)
    {
            return a.holeScore[m_currentHole] == b.holeScore[m_currentHole]
            //return a.totalScore == b.totalScore
                ? a.skins == b.skins ?
                ((a.client * ConstVal::MaxClients) + a.player) > ((b.client * ConstVal::MaxClients) + b.player)
                : a.skins < b.skins
            : a.holeScore[m_currentHole] < b.holeScore[m_currentHole];
            //: a.totalScore < b.totalScore;
    };

    hadTennisBounce = false;
    hadWallBounce = false;

    auto& playerInfo = m_playerInfo[groupID].playerInfo;

    //all players may have disconnected this group mid-game
    if (!playerInfo.empty())
    {
        //broadcast current player's score first
        ScoreUpdate su;
        su.strokeDistance = playerInfo[0].ballEntity.getComponent<Ball>().lastStrokeDistance;
        su.client = playerInfo[0].client;
        su.player = playerInfo[0].player;
        su.score = playerInfo[0].totalScore;
        su.stroke = playerInfo[0].holeScore[m_currentHole];
        su.distanceScore = playerInfo[0].distanceScore[m_currentHole];
        su.matchScore = playerInfo[0].matchWins;
        su.skinsScore = playerInfo[0].skins;
        su.hole = m_currentHole;
    
        m_sharedData.host.broadcastPacket(PacketID::ScoreUpdate, su, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
        playerInfo[0].ballEntity.getComponent<Ball>().lastStrokeDistance = 0.f;

        if (m_sharedData.teamMode)
        {
            //we'll need to send the team mate's score too - this was done
            //in handleRules()
            const auto& team = m_teams[m_playerInfo[0].playerInfo[0].teamIndex];
            const auto& teamMate = team.players[team.currentPlayer];
            auto& pi = m_playerInfo[0].playerInfo;
            auto teamMateInfo = std::find_if(pi.begin(), pi.end(),
                [&teamMate](const PlayerStatus& ps)
                {
                    return ps.client == teamMate[0] && ps.player == teamMate[1];
                });

            if (teamMateInfo != pi.end()
                && !(teamMateInfo->client == playerInfo[0].client //don't do this if it's a one person team
                    && teamMateInfo->player == playerInfo[0].player))
            {
                ScoreUpdate su2;
                su2.strokeDistance = teamMateInfo->ballEntity.getComponent<Ball>().lastStrokeDistance;
                su2.client = teamMateInfo->client;
                su2.player = teamMateInfo->player;
                su2.score = teamMateInfo->totalScore;
                su2.stroke = teamMateInfo->holeScore[m_currentHole];
                su2.distanceScore = teamMateInfo->distanceScore[m_currentHole];
                su2.matchScore = teamMateInfo->matchWins; //not used in teeamplay but let's be consistent
                su2.skinsScore = teamMateInfo->skins;
                su2.hole = m_currentHole;

                m_sharedData.host.broadcastPacket(PacketID::ScoreUpdate, su2, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                teamMateInfo->ballEntity.getComponent<Ball>().lastStrokeDistance = 0.f;
            }
        }
    }

    
    std::uint32_t waitingCount = 0;
    
    //apply the rules to ALL players on the course
    std::vector<PlayerStatus> allPlayers;
    for (auto& group : m_playerInfo)
    {
        //players may have quite the group but the vector isn't cleared
        if (group.playerCount != 0)
        {
            allPlayers.insert(allPlayers.end(), group.playerInfo.begin(), group.playerInfo.end());
        }

        if (group.waitingForHole
            || group.playerCount == 0)
        {
            waitingCount++;
        }
    }


    if (!newHole || m_currentHole == 0)
    {
        if (m_sharedData.scoreType == ScoreType::BBB)
        {
            //if BBB favour players not on green
            //std::sort(playerInfo.begin(), playerInfo.end(),
            //    [](const PlayerStatus& a, const PlayerStatus& b)
            //    {
            //        return a.terrain != TerrainID::Green;
            //    });

            //auto currTerrain = m_playerInfo[groupID].playerInfo[0].terrain;
            //if (currTerrain == TerrainID::Green)
            //{
            //    std::sort(playerInfo.begin(), playerInfo.end(),
            //        [](const PlayerStatus& a, const PlayerStatus& b)
            //        {
            //            return a.distanceToHole > b.distanceToHole;
            //        });

            //    //TODO award Bango for closest if not already awarded
            //}
            //else
            //{
            //    std::sort(playerInfo.begin(), playerInfo.end(),
            //        [](const PlayerStatus& a, const PlayerStatus& b)
            //        {
            //            return (a.distanceToHole > b.distanceToHole)
            //                && a.terrain != TerrainID::Green;
            //        });
            //}
        }
        else if (m_sharedData.scoreType == ScoreType::Elimination)
        {
            const auto predicate = [](const PlayerStatus& a, const PlayerStatus& b)
                {
                    if (!a.eliminated && !b.eliminated)
                    {
                        return a.distanceToHole > b.distanceToHole;
                    }

                    return !a.eliminated;
                };            
            
            //make sure eliminated are last before sorting by distance
            std::sort(playerInfo.begin(), playerInfo.end(), predicate);
            std::sort(allPlayers.begin(), allPlayers.end(), predicate);
        }
        else if (m_sharedData.scoreType == ScoreType::NearestThePin)
        {
            //make sure player hasn't completed all turns
            const auto& predicate = [&](const PlayerStatus& a, const PlayerStatus& b)
                {
                    if (a.holeScore[m_currentHole] < MaxNTPStrokes && b.holeScore[m_currentHole] < MaxNTPStrokes)
                    {
                        return a.distanceToHole > b.distanceToHole;
                    }

                    return a.holeScore[m_currentHole] < MaxNTPStrokes;
                };

            std::sort(playerInfo.begin(), playerInfo.end(), predicate);
            std::sort(allPlayers.begin(), allPlayers.end(), predicate);
        }
        else
        {
            if (m_skinsFinals)
            {
                std::sort(playerInfo.begin(), playerInfo.end(), skinsPredicate);
                std::sort(allPlayers.begin(), allPlayers.end(), skinsPredicate);
            }
            else
            {
                //sort players by distance
                const auto predicate = [&](const PlayerStatus& a, const PlayerStatus& b)
                    {
                        if (m_sharedData.teamMode
                            && a.teamIndex == b.teamIndex)
                        {
                            return m_teams[a.teamIndex].players[m_teams[a.teamIndex].currentPlayer][1] == a.player;
                        }
                        return a.distanceToHole > b.distanceToHole;
                    };

                std::sort(playerInfo.begin(), playerInfo.end(), predicate);
                std::sort(allPlayers.begin(), allPlayers.end(), predicate);
            }
        }
    }
    else
    {
        if (m_skinsFinals)
        {
            std::sort(playerInfo.begin(), playerInfo.end(), skinsPredicate);
            std::sort(allPlayers.begin(), allPlayers.end(), skinsPredicate);
        }
        else
        {
            //winner of the last hole goes first - TODO this doesn't work in group play
            const auto predicate = [&](const PlayerStatus& a, const PlayerStatus& b)
                {
                    if (m_sharedData.teamMode
                        && a.teamIndex == b.teamIndex)
                    {
                        return m_teams[a.teamIndex].players[m_teams[a.teamIndex].currentPlayer][1] == a.player;
                    }
                    return a.holeScore[m_currentHole - 1] < b.holeScore[m_currentHole - 1];
                };

            std::sort(playerInfo.begin(), playerInfo.end(), predicate);
            std::sort(allPlayers.begin(), allPlayers.end(), predicate);

            //check the last honour taker to see if their score matches
            //current first position and swap them in to first if so
            //TODO we could probably include this in the predicate above
            //but ehhhh no harm in being explicit I guess?
            if (!playerInfo.empty())
            {
                if ((playerInfo[0].client != m_honour[0]
                    || playerInfo[0].player != m_honour[1])
                    && m_sharedData.teamMode == 0)
                {
                    auto r = std::find_if(playerInfo.begin(), playerInfo.end(),
                        [&](const PlayerStatus& ps)
                        {
                            return ps.client == m_honour[0] && ps.player == m_honour[1];
                        });

                    if (r != playerInfo.end())
                    {
                        if (r->holeScore[m_currentHole - 1] == playerInfo[0].holeScore[m_currentHole - 1])
                        {
                            std::swap(playerInfo[std::distance(playerInfo.begin(), r)], playerInfo[0]);
                        }
                    }

                    ///TODO if this *is* teams we want to check if this is the team player turn and if not
                    //swap them with their team mate
                }

                //set whoever is first as current honour taker
                m_honour[0] = playerInfo[0].client;
                m_honour[1] = playerInfo[0].player;
            }
        }
    }

    if (!allPlayers.empty())
    {
        bool playersForfeit = ScoreType::MinPlayerCount[m_sharedData.scoreType] > allPlayers.size();

        //TODO move this to some game rule check function
        if ((allPlayers[0].distanceToHole == 0 //all players must be in the hole
            || (m_sharedData.scoreType == ScoreType::NearestThePin && allPlayers[0].holeScore[m_currentHole] >= MaxNTPStrokes) //all players must have taken their turn
            || playersForfeit //players have quit the game so attempt next hole
            || (m_sharedData.scoreType == ScoreType::Elimination && allPlayers[1].eliminated)) //(which triggers the rules to end the game)  
            && waitingCount >= m_playerInfo.size() - 1) //don't move on until all but this group are waiting
        {
            //if we're nearest the pin sort by closest player so current winner goes first
            //and we can award something for winning the hole
            if (m_sharedData.scoreType == ScoreType::NearestThePin)
            {
                const auto predicate = [&](const PlayerStatus& a, const PlayerStatus& b)
                    {
                        return (a.distanceScore[m_currentHole] < b.distanceScore[m_currentHole]);
                    };
                std::sort(playerInfo.begin(), playerInfo.end(), predicate);
                std::sort(allPlayers.begin(), allPlayers.end(), predicate);

                //don't send this if all players forfeit
                if (allPlayers[0].holeScore[m_currentHole] == MaxNTPStrokes)
                {
                    //if we're playing NTPPro we score based on total hole wins
                    auto& group = m_playerInfo[m_groupAssignments[allPlayers[0].client]];
                    auto player = std::find_if(group.playerInfo.begin(), group.playerInfo.end(),
                        [&allPlayers](const PlayerStatus& p)
                        {
                            return p.client == allPlayers[0].client && p.player == allPlayers[0].player;
                        });
                    player->matchWins++;

                    //set this to 1 to show that this player won the hole on the score board
                    player->holeScore[m_currentHole] = 1;

                    std::uint16_t d = (std::uint16_t(allPlayers[0].client) << 8) | allPlayers[0].player;
                    m_sharedData.host.broadcastPacket(PacketID::HoleWon, d, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                }
            }

            setNextHole();
        }
        else if (!playerInfo.empty())
        {
            //go to next player if current front player is not in the hole...
            //otherwise we wait until the above triggers next hole
            if ((m_sharedData.scoreType != ScoreType::NearestThePin && playerInfo[0].distanceToHole != 0)
                || (m_sharedData.scoreType == ScoreType::NearestThePin && playerInfo[0].holeScore[m_currentHole] < MaxNTPStrokes))
            {
                ActivePlayer player = playerInfo[0]; //deliberate slice.
                for (auto c : m_playerInfo[groupID].clientIDs)
                {
                    m_sharedData.host.sendPacket(m_sharedData.clients[c].peer, PacketID::SetPlayer, player, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                    m_sharedData.host.sendPacket(m_sharedData.clients[c].peer, PacketID::ActorAnimation, std::uint8_t(AnimationID::Idle), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                }
            }
            else
            {
                m_playerInfo[groupID].waitingForHole = true;

                std::uint8_t spectateGroup = 0;
                for (auto i = 0u; i < m_playerInfo.size(); ++i)
                {
                    if (!m_playerInfo[i].waitingForHole)
                    {
                        //tell all waiting clients to spectate this group
                        spectateGroup = std::uint8_t(i);
                        m_sharedData.host.broadcastPacket(PacketID::SpectateGroup, spectateGroup, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                        break;
                    }
                }

                //send a message saying we're waiting - hmm trouble is
                for (auto c : m_playerInfo[groupID].clientIDs)
                {
                    m_sharedData.host.sendPacket(m_sharedData.clients[c].peer, PacketID::SetIdle, spectateGroup, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                }
            }
        }
    }
    m_playerInfo[groupID].turnTimer.restart();

    //notify all clients of new position
    if (!playerInfo.empty()
        && playerInfo[0].distanceToHole > 0.5f)
    {
        GroupPosition pos;
        pos.groupID = groupID;
        pos.playerData = playerInfo[0]; //deliberate slice
        m_sharedData.host.broadcastPacket(PacketID::GroupPosition, pos, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
    }
}

void GolfState::setNextHole()
{
    m_currentBest = MaxStrokes;
    
    if (!m_sharedData.randomWind)
    {
        m_scene.getSystem<BallSystem>()->forceWindChange();
    }

    //update player skins/match scores
    auto gameFinished = summariseRules();
    

    //broadcast all scores to make sure everyone is up to date
    //note that in skins games the above summary may have reduced
    //the current hole index if the hole needs repeating
    auto scoreHole = m_skinsFinals ? std::min(m_currentHole + 1, std::uint8_t(m_holeData.size()) - 1) : m_currentHole;
    
    for (auto& group : m_playerInfo)
    {
        for (auto& player : group.playerInfo)
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
            su.distanceScore = player.distanceScore[scoreHole];

            m_sharedData.host.broadcastPacket(PacketID::ScoreUpdate, su, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
        group.waitingForHole = false;
    }

    //set clients as not yet loaded
    for (auto& client : m_sharedData.clients)
    {
        client.mapLoaded = false;
    }
    m_allMapsLoaded = false;
    m_scoreboardTime = 0.f;

    //m_eliminationStarted = true;

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
                    for (auto i = 0u; i < m_playerInfo.size(); ++i)
                    {
                        setNextPlayer(static_cast<std::int32_t>(i), true);
                    }
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
                        for (auto j = 0u; j < m_playerInfo[i].playerInfo.size(); ++j)
                        {
                            auto& player = m_playerInfo[i].playerInfo[j];
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
                            info.groupID = m_groupAssignments[player.client];
                            info.state = static_cast<std::uint8_t>(ball.getComponent<Ball>().state);
                            m_sharedData.host.broadcastPacket(PacketID::ActorUpdate, info, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                        }
                    }
                }
            }

            //make sure to keep resetting this to prevent unfairly
            //truncating the next player's turn
            for (auto& group : m_playerInfo)
            {
                group.turnTimer.restart();
            }
        };

        m_scoreboardReadyFlags = 0;

        //if this is elimination set all eliminated players to forfeit
        if (m_sharedData.scoreType == ScoreType::Elimination)
        {
            for (auto& group : m_playerInfo)
            {
                for (auto& p : group.playerInfo)
                {
                    if (p.eliminated)
                    {
                        p.holeScore[m_currentHole] = m_holeData[m_currentHole].puttFromTee ? 6 : 12;
                        p.distanceToHole = 0.f;
                    }
                    p.matchWins = 0; //reset any elimination flags
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
    const auto groupID = m_groupAssignments[clientID];
    if (!m_playerInfo[groupID].playerInfo.empty() &&
        m_playerInfo[groupID].playerInfo[0].client == clientID)
    {
        switch (m_playerInfo[groupID].playerInfo[0].ballEntity.getComponent<Ball>().state)
        {
        default:
            break;
        case Ball::State::Roll:
        case Ball::State::Flight:
        case Ball::State::Putt:
            m_scene.getSystem<BallSystem>()->fastForward(m_playerInfo[groupID].playerInfo[0].ballEntity);
            break;
        }
    }
}

void GolfState::applyMulligan()
{
    //we're assuming this only ever applies in career, but let's at least
    //try to make this flexible in case we change our mind in the future
    static constexpr std::int32_t gid = 0;
    auto& playerInfo = m_playerInfo[gid].playerInfo;

    //TODO probably want so checks to make sure
    //this is legit in career mode...
    if (playerInfo.size() == 1
        && playerInfo[0].ballEntity.getComponent<Ball>().state == Ball::State::Idle)
    {
        playerInfo[0].holeScore[m_currentHole] = playerInfo[0].previousBallScore;
        playerInfo[0].position = playerInfo[0].prevBallPos;
        playerInfo[0].terrain = m_scene.getSystem<BallSystem>()->getTerrain(playerInfo[0].position).terrain;

        //set the ball to its previous position
        auto ball = playerInfo[0].ballEntity;
        ball.getComponent<cro::Transform>().setPosition(playerInfo[0].prevBallPos);


        auto timestamp = m_serverTime.elapsed().asMilliseconds();
        auto& ballC = ball.getComponent<Ball>();
        ballC.lie = 1;

        ActorInfo info;
        info.serverID = static_cast<std::uint32_t>(ball.getIndex());
        info.position = ball.getComponent<cro::Transform>().getPosition();
        info.rotation = cro::Util::Net::compressQuat(ball.getComponent<cro::Transform>().getRotation());
        info.windEffect = ballC.windEffect;
        info.timestamp = timestamp;
        info.clientID = playerInfo[0].client;
        info.playerID = playerInfo[0].player;
        info.state = static_cast<std::uint8_t>(ballC.state);
        info.lie = ballC.lie;
        info.groupID = m_groupAssignments[playerInfo[0].client];
        m_sharedData.host.broadcastPacket(PacketID::ActorUpdate, info, net::NetFlag::Reliable);

        ballC.lastStrokeDistance = 0.f;

        setNextPlayer(gid);
    }
}

bool GolfState::validateMap()
{
    const auto installPaths = Content::getInstallPaths();

    auto mapDir = m_sharedData.mapDir.toAnsiString();

    //auto mapPath = ConstVal::MapPath + mapDir + "/course.data";
    std::string mapPath;
    for (const auto& dir : installPaths)
    {
        mapPath = dir + ConstVal::MapPath + mapDir;
        if (cro::FileSystem::directoryExists(cro::FileSystem::getResourcePath() + mapPath))
        {
            break;
        }
    }
    mapPath += +"/course.data";

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
                holeData.pin.x = glm::clamp(holeData.pin.x, 0.f, MapSizeFloat.x);
                holeData.pin.z = glm::clamp(holeData.pin.z, -MapSizeFloat.y, 0.f);
                propCount++;
            }
            else if (name == "tee")
            {
                holeData.tee = holeProp.getValue<glm::vec3>();
                holeData.tee.x = glm::clamp(holeData.tee.x, 0.f, MapSizeFloat.x);
                holeData.tee.z = glm::clamp(holeData.tee.z, -MapSizeFloat.y, 0.f);
                propCount++;
            }
            else if (name == "target")
            {
                holeData.target = holeProp.getValue<glm::vec3>();
                holeData.target.x = glm::clamp(holeData.target.x, 0.f, MapSizeFloat.x);
                holeData.target.z = glm::clamp(holeData.target.z, -MapSizeFloat.y, 0.f);
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

        holeData.distanceToPin = glm::length(holeData.pin - holeData.tee);

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
    //this is horroible, but at least it's explicit :)
    if (m_sharedData.scoreType != ScoreType::Stroke
        && m_sharedData.scoreType != ScoreType::ShortRound
        && m_sharedData.scoreType != ScoreType::Stableford
        && m_sharedData.scoreType != ScoreType::StablefordPro
        && m_sharedData.scoreType != ScoreType::MultiTarget)
    {
        m_sharedData.teamMode = 0;
    }

    //make sure to ignore gimme radii on NTP...
    if (m_sharedData.scoreType == ScoreType::NearestThePin)
    {
        m_sharedData.gimmeRadius = 0;
    }

    auto& mb = m_sharedData.messageBus;
    m_scene.addSystem<cro::CallbackSystem>(mb);
    auto* bs = m_scene.addSystem<BallSystem>(mb);
    bs->setGimmeRadius(m_sharedData.gimmeRadius);
    bs->setMaxStrengthMultiplier(m_sharedData.maxWind);
    bs->enableRandomWind(m_sharedData.randomWind);

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
        //applies putt from tee and corrects vertical pos
        //for pin/target/tee
        m_scene.getSystem<BallSystem>()->setHoleData(hole);

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
    
    //calculate number of client groups based on connected client
    //count and the number of requested clients per group
    std::fill(m_groupAssignments.begin(), m_groupAssignments.end(), 0);

    struct ClientSortData final
    {
        std::uint8_t clientID = 0;
        std::uint8_t playerCount = 0;
    };
    std::vector<ClientSortData> clientSortData;

    std::int32_t clientCount = 0;
    std::int32_t playerCount = 0; //used for score setting, below
    for (auto i = 0u; i < m_sharedData.clients.size(); ++i)
    {
        if (m_sharedData.clients[i].connected)
        {
            clientCount++;
            clientSortData.emplace_back().clientID = static_cast<std::uint8_t>(i);
            clientSortData.back().playerCount = m_sharedData.clients[i].playerCount;

            playerCount += m_sharedData.clients[i].playerCount;
        }
    }
    //by sorting clients by the number of players we can more easily balance (although not
    //perfectly) the number of players per group overall.
    std::sort(clientSortData.begin(), clientSortData.end(), 
        [](const ClientSortData& a, const ClientSortData& b) 
        {
            return a.playerCount > b.playerCount;
        });

    auto groupMode = m_sharedData.groupMode;
    if (m_sharedData.scoreType == ScoreType::Skins
        || m_sharedData.scoreType == ScoreType::Match)
    {
        groupMode = ClientGrouping::None;
    }

    if (groupMode != ClientGrouping::None)
    {
        m_sharedData.teamMode = 0;
    }

    if (groupMode == ClientGrouping::Even)
    {
        m_playerInfo.resize(std::min(clientCount, 2));
    }
    else
    {
        m_playerInfo.resize(1);
    }

    auto startLives = StartLives;
    clientCount = 0;
    //for (auto i = 0u; i < m_sharedData.clients.size(); ++i)
    std::int32_t teamCounter = 0;
    for(const auto& d : clientSortData)
    {
        //if (m_sharedData.clients[d.clientID].connected) //this should always be true if the client made it into the sort data
        {
            auto groupID = 0;
            switch (groupMode)
            {
            default: break;
            case ClientGrouping::Even:
                if (clientSortData.size() > 1)
                {
                    groupID = m_playerInfo[0].playerCount < m_playerInfo[1].playerCount ? 0 : 1;
                }
                break;
            case ClientGrouping::One:
                //remember these clients might not be consecutive!
                //eg it's possible only clients 1 & 6 are connected...
                groupID = clientCount;
                clientCount++;
                break;
            case ClientGrouping::Two:
            case ClientGrouping::Three:
            case ClientGrouping::Four:
                //find first group with fewer than X players else create a new group
                for (const auto& g : m_playerInfo)
                {
                    if (g.playerCount >= groupMode - 2)
                    {
                        groupID++;
                    }
                }
                break;
            }

            if (groupID == m_playerInfo.size()) //we should never be greater...
            {
                m_playerInfo.emplace_back();
            }

            m_groupAssignments[d.clientID] = groupID;
            m_playerInfo[groupID].clientIDs.push_back(d.clientID); //tracks all client IDs in this group
            m_playerInfo[groupID].id = std::uint8_t(groupID);
            m_playerInfo[groupID].playerCount += d.playerCount;

            for (auto j = 0u; j < m_sharedData.clients[d.clientID].playerCount; ++j)
            {
                auto& player = m_playerInfo[groupID].playerInfo.emplace_back();
                player.client = d.clientID;
                player.player = j;
                player.position = m_holeData[0].tee;
                player.distanceToHole = glm::length(m_holeData[0].tee - m_holeData[0].pin);

                if (m_sharedData.teamMode)
                {
                    const auto teamPlayerIndex = teamCounter % 2;
                    if (teamPlayerIndex == 0)
                    {
                        auto& team = m_teams.emplace_back();
                        //fill the second slot with first player, in case there isn't a second
                        //player for the team because the roster has an odd number of players
                        m_teams.back().players[1][0] = player.client;
                        m_teams.back().players[1][1] = j;
                    }
                    player.teamIndex = static_cast<std::int32_t>(m_teams.size() - 1);

                    m_teams.back().players[teamPlayerIndex][0] = player.client;
                    m_teams.back().players[teamPlayerIndex][1] = j;

                    teamCounter++;
                }

                startLives = std::max(startLives - 1, 2);
            }
        }
    }

    //fudgenstein - when playing with 4 or 4 players we want 3 lives
    if (playerCount == 4)
    {
        startLives++;
    }

    if (m_sharedData.scoreType == ScoreType::Elimination)
    {
        //store the number of lives in the skins
        for (auto& group : m_playerInfo)
        {
            for (auto& player : group.playerInfo)
            {
                player.skins = startLives;
            }
        }
    }

    for (auto& group : m_playerInfo)
    {
        std::shuffle(group.playerInfo.begin(), group.playerInfo.end(), cro::Util::Random::rndEngine);
    }
}

void GolfState::buildWorld()
{
    //create a ball entity for each player
    for (auto& group : m_playerInfo)
    {
        for (auto& player : group.playerInfo)
        {
            player.terrain = m_scene.getSystem<BallSystem>()->getTerrain(player.position).terrain;

            player.ballEntity = m_scene.createEntity();
            player.ballEntity.addComponent<cro::Transform>().setPosition(player.position);
            player.ballEntity.addComponent<Ball>().terrain = player.terrain;
            player.ballEntity.getComponent<Ball>().client = player.client;

            player.holeScore.resize(m_holeData.size());
            player.distanceScore.resize(m_holeData.size());
            std::fill(player.holeScore.begin(), player.holeScore.end(), 0);
            std::fill(player.distanceScore.begin(), player.distanceScore.end(), 0.f);
        }
    }

    
    const auto applySaveData =
        [&](std::vector<std::uint8_t>& scores, std::uint64_t holeIndex)
        {
            m_currentHole = std::min(std::size_t(holeIndex), m_holeData.size() - 1);

            //if we're here we *should* only have one player...
            auto& player = m_playerInfo[0].playerInfo[0];
            player.holeScore.swap(scores);

            player.position = m_holeData[m_currentHole].tee;
            player.distanceToHole = glm::length(m_holeData[m_currentHole].tee - m_holeData[m_currentHole].pin);
            m_scene.getSystem<BallSystem>()->setHoleData(m_holeData[m_currentHole]);

            player.terrain = m_scene.getSystem<BallSystem>()->getTerrain(player.position).terrain;
            player.ballEntity.getComponent<cro::Transform>().setPosition(player.position);
            player.ballEntity.getComponent<Ball>().terrain = player.terrain;
        };


    
    
    
    //if this is a career league look for a progress file
    //TODO what are the chances of this overlapping with the client?
    if (m_sharedData.leagueID != 0)
    {
        std::uint64_t h = 0;
        std::vector<std::uint8_t> scores(m_holeData.size());
        std::fill(scores.begin(), scores.end(), 0);

        //this is a fudge to let the server know we're
        //actually on a tournament
        if (m_sharedData.leagueID > LeagueRoundID::Count)
        {
            const auto tournamentID = std::numeric_limits<std::int32_t>::max() - m_sharedData.leagueID;
            CRO_ASSERT(tournamentID < 2, "");

            Tournament t;
            t.id = tournamentID;
            readTournamentData(t);

            std::fill(scores.begin(), scores.end(), 0);

            //tournament
            for (auto i = 0u; i < scores.size(); ++i)
            {
                scores[i] = t.scores[i];
                if (scores[i] != 0)
                {
                    h++;
                }
                else
                {
                    break;
                }
            }

            if (h != 0)
            {
                applySaveData(scores, h);
            }
        }
        else
        {
            std::int32_t temp = 0;

            if (Progress::read(m_sharedData.leagueID, h, scores, temp)
                && h != 0)
            {
                applySaveData(scores, h);
            }
        }
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
                const auto client = std::distance(m_sharedData.clients.begin(), result);
                const auto groupID = m_groupAssignments[client];
                if (m_playerInfo[groupID].playerInfo[0].client == client
                    && m_playerInfo[groupID].playerInfo[0].ballEntity.getComponent<Ball>().state == Ball::State::Idle)
                {
                    setNextPlayer(groupID);
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

        const auto groupID = m_groupAssignments[target];

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
                if (m_gameStarted && !m_playerInfo[groupID].playerInfo.empty()
                    /*&& m_playerInfo[0].client == target*/)
                {
                    m_playerInfo[groupID].playerInfo[0].holeScore[m_currentHole] = MaxStrokes; //this should be half on putt from tee but meh, it's a penalty
                    m_playerInfo[groupID].playerInfo[0].position = m_holeData[m_currentHole].pin;
                    m_playerInfo[groupID].playerInfo[0].distanceToHole = 0.f;
                    m_playerInfo[groupID].playerInfo[0].terrain = TerrainID::Green;
                    setNextPlayer(groupID);

                    for (auto c : m_playerInfo[groupID].clientIDs)
                    {
                        m_sharedData.host.sendPacket(m_sharedData.clients[c].peer, PacketID::MaxStrokes, std::uint8_t(MaxStrokeID::HostPunishment), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                    }
                    //broadcast hole complete message so clients can update scoreboards
                    std::uint16_t pkt = (m_playerInfo[groupID].playerInfo[0].client << 8 ) | m_playerInfo[groupID].playerInfo[0].player;
                    m_sharedData.host.broadcastPacket(PacketID::HoleComplete, pkt, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
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
            m_playerInfo[groupID].playerInfo[0].position = m_holeData[m_currentHole].pin;
            m_playerInfo[groupID].playerInfo[0].holeScore[m_currentHole] = MaxStrokes;
            m_playerInfo[groupID].playerInfo[0].distanceToHole = 0.f;
            m_playerInfo[groupID].playerInfo[0].terrain = TerrainID::Green;
            setNextPlayer(groupID);
        }

        break;
        case ServerCommand::ChangeWind:
            m_scene.getSystem<BallSystem>()->forceWindChange();
            break;
        case ServerCommand::NextHole:
            m_currentHole = m_holeData.size() - 1;
            //if (m_currentHole < m_holeData.size() - 1)
            {
                for (auto& group : m_playerInfo)
                {
                    for (auto& p : group.playerInfo)
                    {
                        p.position = m_holeData[m_currentHole].pin;
                        p.distanceToHole = 0.f;
                        p.holeScore[m_currentHole] = MaxStrokes;
                        p.totalScore += MaxStrokes;
                    }
                }
                setNextPlayer(groupID);
            }
            break;
        case ServerCommand::NextPlayer:
            //this fakes the ball getting closer to the hole
            m_playerInfo[groupID].playerInfo[0].distanceToHole *= 0.99f;
            setNextPlayer(groupID);
            break;
        case ServerCommand::GotoGreen:
            //set ball to green position
            //set ball state to paused to trigger updates
        {
            auto pos = m_playerInfo[groupID].playerInfo[0].position - m_holeData[m_currentHole].pin;
            pos = glm::normalize(pos) * 6.f;

            m_playerInfo[groupID].playerInfo[0].ballEntity.getComponent<cro::Transform>().setPosition(pos + m_holeData[m_currentHole].pin);
            m_playerInfo[groupID].playerInfo[0].ballEntity.getComponent<Ball>().terrain = TerrainID::Green;
            m_playerInfo[groupID].playerInfo[0].ballEntity.getComponent<Ball>().state = Ball::State::Paused;
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
