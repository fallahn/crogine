/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#include "League.hpp"

#include <crogine/core/String.hpp>

#include <cstdint>
#include <array>

//this is used to index into the CourseData vector in MenuState
struct CareerRound final
{
    std::uint8_t courseID = 0;
    std::uint8_t holeCount = 0;
};

struct CareerLeague final
{
    static constexpr std::uint32_t MaxRounds = 6;
    std::array<CareerRound, MaxRounds> playlist = {};
    std::int32_t leagueID = 0;
    //TODO apply some sort of clubset limit?
    cro::String title;
};

class Career final
{
public:
    Career();

    Career(const Career&) = delete;
    Career(Career&&) = delete;
    Career& operator = (const Career&) = delete;
    Career& operator = (Career&&) = delete;

    static constexpr std::uint32_t MaxLeagues = 6;

    const auto& getLeagueData() const { return m_leagues; }
    const auto& getLeagueTables() const { return m_leagueTables; }
private:
    std::array<CareerLeague, MaxLeagues> m_leagues = {};
    std::vector<League> m_leagueTables;
};