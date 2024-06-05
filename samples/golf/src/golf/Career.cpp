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

#include "Career.hpp"
#include "Input.hpp"

#include <crogine/core/Log.hpp>
#include <crogine/core/FileSystem.hpp>

namespace
{
    constexpr std::array<std::array<std::int32_t, 2u>, Career::MaxLeagues> CourseIDs =
    {
        {{4, 0},
        {9, 5},
        {7, 3},
        {10,1},
        {8, 2},
        {6,11},}
    };

    const std::array<cro::String, Career::MaxLeagues> LeagueNames =
    {
        cro::String("Country Club Tour"),
        "Badlands Cup",
        "Links Challenge",
        "Hill and Hole Valley Run",
        "Chippers Pitch 'n' Putt",
        "The Woodlands League"
    };
}

Career::Career(const SharedStateData& sd)
{
    for (auto i = 0u; i < MaxLeagues; ++i)
    {
        m_leagues[i].leagueID = i + 1;
        m_leagues[i].title = LeagueNames[i];

        for (auto j = 0u; j < CareerLeague::MaxRounds; ++j)
        {
            m_leagues[i].playlist[j].holeCount = static_cast<std::uint8_t>(((j + 2) % CareerLeague::MaxRounds) / 2); //we want 1,1,2,2,0,0
            m_leagues[i].playlist[j].courseID = static_cast<std::uint8_t>(CourseIDs[i][j % 2]); //alternate courses
        }

        m_leagueTables.emplace_back(m_leagues[i].leagueID, sd);
    }
}

//public
void Career::reset()
{
    for (auto i = 0u; i < m_leagueTables.size(); ++i)
    {
        m_leagueTables[i].reset();

        const auto path = Progress::getFilePath(i + LeagueRoundID::RoundOne);
        if (cro::FileSystem::fileExists(path))
        {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    }
}