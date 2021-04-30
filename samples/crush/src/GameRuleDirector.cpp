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

#include "GameRuleDirector.hpp"
#include "Messages.hpp"
#include "PacketIDs.hpp"
#include "PlayerSystem.hpp"
#include "CommonConsts.hpp"
#include "ServerPacketData.hpp"
#include "ActorIDs.hpp"
#include "GameConsts.hpp"
#include "SnailSystem.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/network/NetHost.hpp>

namespace
{
    const cro::Time RoundTime = cro::seconds(3.f * 60.f);
    const cro::Time SuddenDeathTime = cro::seconds(30.f);

    const cro::Time BalloonTime = cro::seconds(20.f);
}

GameRuleDirector::GameRuleDirector(cro::NetHost& host, std::array<cro::Entity, 4u>& e)
    : m_netHost             (host),
    m_indexedPlayerEntities (e),
    m_suddenDeathWarn       (false),
    m_suddenDeath           (false),
    m_snailCountA           (0),
    m_snailCountB           (0)
{

}

//public
void GameRuleDirector::handleMessage(const cro::Message& msg)
{
    if (msg.id == MessageID::PlayerMessage)
    {
        const auto& data = msg.getData<PlayerEvent>();

        PlayerStateChange psc;
        psc.playerID = data.player.getComponent<Player>().avatar.getComponent<Actor>().id;
        psc.lives = data.player.getComponent<Player>().lives;
        psc.playerState = data.type;
        psc.serverEntityID = data.player.getIndex();

        m_netHost.broadcastPacket(PacketID::PlayerState, psc, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);

        if (data.type == PlayerEvent::Died)
        {
            if (data.data > -1
                && data.data != psc.playerID)
            {
                //crate owner gets some points
                m_indexedPlayerEntities[data.data].getComponent<Player>().lives++;

                psc.playerID = data.data;
                psc.playerState = PlayerEvent::Scored;
                psc.lives = m_indexedPlayerEntities[data.data].getComponent<Player>().lives;
                psc.serverEntityID = m_indexedPlayerEntities[data.data].getIndex();
                m_netHost.broadcastPacket(PacketID::PlayerState, psc, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
            }
        }
    }
    else if (msg.id == MessageID::GameMessage)
    {
        const auto& data = msg.getData<GameEvent>();
        switch (data.type)
        {
        default: break;
        case GameEvent::RequestSpawn:
            if (data.actorID == ActorID::PoopSnail)
            {
                //assume this was spawned succefully and count it
                if (data.position.z > 0)
                {
                    m_snailCountA++;
                }
                else
                {
                    m_snailCountB++;
                }
            }
            break;
        }
    }
    else if (msg.id == MessageID::SnailMessage)
    {
        const auto& data = msg.getData<SnailEvent>();
        if (data.snail.isValid())
        {
            if (data.snail.getComponent<Snail>().state == Snail::Dead)
            {
                if (data.snail.getComponent<cro::Transform>().getPosition().z > 0)
                {
                    m_snailCountA--;
                }
                else
                {
                    m_snailCountB--;
                }
            }
        }
    }
}

void GameRuleDirector::process(float)
{
    //check for balloon spawning
    if (m_balloonClock.elapsed() > BalloonTime)
    {
        m_balloonClock.restart();

        if (m_snailCountA == 0)
        {
            auto* msg = postMessage<GameEvent>(MessageID::GameMessage);
            msg->type = GameEvent::RequestSpawn;
            msg->position = { -MapWidth, MapHeight + 1.f, LayerDepth };
            msg->actorID = ActorID::Balloon;
        }

        if (m_snailCountB == 0)
        {
            auto* msg = postMessage<GameEvent>(MessageID::GameMessage);
            msg->type = GameEvent::RequestSpawn;
            msg->position = { MapWidth, MapHeight + 1.f, -LayerDepth };
            msg->actorID = ActorID::Balloon;
        }
    }


    //check round time
    if (!m_suddenDeath)
    {
        if (!m_suddenDeathWarn)
        {
            if (m_roundTime.elapsed() > (RoundTime - SuddenDeathTime))
            {
                m_netHost.broadcastPacket(PacketID::GameMessage, std::uint8_t(GameEvent::RoundWarn), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
                m_suddenDeathWarn = true;
            }
        }

        if (m_roundTime.elapsed() > RoundTime)
        {
            m_netHost.broadcastPacket(PacketID::GameMessage, std::uint8_t(GameEvent::SuddenDeath), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
            m_suddenDeath = true;

            for (auto player : m_indexedPlayerEntities)
            {
                if (player.isValid())
                {
                    auto& p = player.getComponent<Player>();
                    if (p.lives > 1)
                    {
                        p.lives = p.state == Player::State::Dead ? 2 : 1; //if a player is mid-reset this might kill them
                    }
                }
            }
        }
    }
}

void GameRuleDirector::startGame()
{
    //if this has timed out here already then we have bigger problems :)
    m_roundTime.restart();

    //spawn initial balloons
    auto* msg = postMessage<GameEvent>(MessageID::GameMessage);
    msg->type = GameEvent::RequestSpawn;
    msg->position = { -MapWidth, MapHeight + 1.f, LayerDepth };
    msg->actorID = ActorID::Balloon;

    msg = postMessage<GameEvent>(MessageID::GameMessage);
    msg->type = GameEvent::RequestSpawn;
    msg->position = { MapWidth, MapHeight + 1.f, -LayerDepth };
    msg->actorID = ActorID::Balloon;
}