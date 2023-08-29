/*-----------------------------------------------------------------------

Matt Marchant 2023
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

#include <cstdint>
#include <array>

struct LeaguePlayer final
{
    std::int32_t skill = 1; //indexes into the skill array (lower is smaller error offset == better)
    std::int32_t curve = 0; //selects skill curve of final result
    std::int32_t outlier = 0; //chance in 100 of making a mess.
    std::int32_t nameIndex = 0;

    std::int32_t currentScore = 0; //this is total par diff so lower is better
};

class League final
{
public:
    static constexpr std::size_t PlayerCount = 15u;
    static constexpr std::int32_t MaxIterations = 24;

    League();

    void reset();
    void iterate(); //TODO indicate which course data to use for iteration

    std::int32_t getCurrentIteration() const { return m_currentIteration; }

    const std::array<LeaguePlayer, PlayerCount>& getTable() const { return m_players; }

private:
    std::array<LeaguePlayer, PlayerCount> m_players = {};

    std::int32_t m_currentIteration;

    void read();
    void write();
};