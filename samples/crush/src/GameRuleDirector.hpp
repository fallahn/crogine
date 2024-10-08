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

#pragma once

#include <crogine/ecs/Director.hpp>
#include <crogine/core/Clock.hpp>

#include <array>

namespace cro
{
    class NetHost;
}

class GameRuleDirector final : public cro::Director
{
public:
    GameRuleDirector(cro::NetHost&, std::array<cro::Entity, 4u>&);

    void handleMessage(const cro::Message&) override;

    void process(float) override;

    void startGame();

private:
    cro::NetHost& m_netHost;
    std::array<cro::Entity, 4u>& m_indexedPlayerEntities;

    cro::Clock m_roundTime; //TODO move this stuff to rules director
    bool m_suddenDeathWarn;
    bool m_suddenDeath;

    //TODO track the count of snails and release balloon when zero
    cro::Clock m_balloonClock;
    std::size_t m_snailCountA;
    std::size_t m_snailCountB;
};
