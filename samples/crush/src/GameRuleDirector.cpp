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

#include <crogine/network/NetHost.hpp>

namespace
{
    const cro::Time RoundTime = cro::seconds(3.f * 60.f);
    const cro::Time SuddenDeathTime = cro::seconds(30.f);
}

GameRuleDirector::GameRuleDirector(cro::NetHost& host, std::array<cro::Entity, 4u>& e)
    : m_netHost             (host),
    m_indexedPlayerEntities (e),
    m_suddenDeathWarn       (false),
    m_suddenDeath           (false)
{

}

//public
void GameRuleDirector::handleMessage(const cro::Message& msg)
{

}

void GameRuleDirector::process(float)
{
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
}