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

#include "Tournament.hpp"
#include "Social.hpp"

#include <crogine/detail/Types.hpp>
#include <crogine/util/Random.hpp>

#include <algorithm>
#include <sstream>
#include <iomanip>


Tournament::Tournament()
{
    std::fill(opponentScores.begin(), opponentScores.end(), 0);
    std::fill(scores.begin(), scores.end(), 0);
    std::fill(tier0.begin(), tier0.end(), -2);
    std::fill(tier1.begin(), tier1.end(), -2);
    std::fill(tier2.begin(), tier2.end(), -2);
    std::fill(tier3.begin(), tier3.end(), -2);
}

std::int32_t getTournamentHoleCount(const Tournament& t)
{
    //c++20's templated lambdas would be useful here
    //const auto holeCount = [](const auto& tier)
    //    {
    //        const auto pos = std::find(tier.begin(), tier.end(), -1);
    //        return std::distance(tier.begin(), pos) < (tier.size() / 2) ? 1 : 2;
    //    };

    switch (t.round)
    {
    default:
    case 0:
    {
        const auto pos = std::find(t.tier0.begin(), t.tier0.end(), -1);
        return std::distance(t.tier0.begin(), pos) < (t.tier0.size() / 2) ? 1 : 2;
    }

    case 1:
    {
        const auto pos = std::find(t.tier1.begin(), t.tier1.end(), -1);
        return std::distance(t.tier1.begin(), pos) < (t.tier1.size() / 2) ? 1 : 2;
    }

    case 2:
    {
        const auto pos = std::find(t.tier2.begin(), t.tier2.end(), -1);
        return std::distance(t.tier2.begin(), pos) < (t.tier2.size() / 2) ? 1 : 2;
    }

    case 3:
        return 0; //18 holes for final
    }
}

std::int32_t getTournamentOpponent(const Tournament& t)
{
    switch (t.round)
    {
    default:
    case 0:
    {
        const auto pos = std::find(t.tier0.begin(), t.tier0.end(), -1);
        const auto offset = std::distance(t.tier0.begin(), pos);
        return offset % 2 == 0 ? t.tier0[offset + 1] : t.tier0[offset - 1];
    }

    case 1:
    {
        const auto pos = std::find(t.tier1.begin(), t.tier1.end(), -1);
        const auto offset = std::distance(t.tier1.begin(), pos);
        return offset % 2 == 0 ? t.tier1[offset + 1] : t.tier1[offset - 1];
    }

    case 2:
    {
        const auto pos = std::find(t.tier2.begin(), t.tier2.end(), -1);
        const auto offset = std::distance(t.tier2.begin(), pos);
        return offset % 2 == 0 ? t.tier2[offset + 1] : t.tier2[offset - 1];
    }

    case 3:
        return t.tier3[0] == -1 ? t.tier3[1] : t.tier3[0];
    }
}

void resetTournament(Tournament& src)
{
    const auto id = src.id;
    src = {};
    src.id = id;

    //when the league rolls the CPUs they are degraded in skill from
    //start to end (ie they get worse as their index increases)

    //we can use this to decide where to insert the player based on the
    //current clubset unlocked, so newbies start against lower skilled
    //CPU players instead of the top ranked ones

    for (auto i = 0; i < static_cast<std::int32_t>(League::PlayerCount / 2) + 1; ++i)
    {
        //the left/right bracket is split via front/back of the array
        src.tier0[i] = (i * 2) - 1;
        src.tier0[i+8] = i * 2;
    }

    //use current skill to pick pseudo-random position
    std::int32_t idx = 7 - (Social::getClubLevel() * 3);
    idx -= cro::Util::Random::value(0, 1);
    idx += cro::Util::Random::value(0, 1) * 8;

    //we know player is always at 0
    src.tier0[0] = src.tier0[idx];
    src.tier0[idx] = -1;
}

static inline std::string getFilePath(std::int32_t index)
{
    auto basePath = Social::getBaseContentPath();
    std::stringstream ss;
    ss << std::setw(2) << std::setfill('0') << index << ".tmt";

    return basePath + ss.str();
}

void writeTournamentData(const Tournament& src)
{
    auto path = getFilePath(src.id);
    cro::RaiiRWops file;
    file.file = SDL_RWFromFile(path.c_str(), "wb");
    if (file.file)
    {
        SDL_RWwrite(file.file, &src, sizeof(src), 1);
    }
}

void readTournamentData(Tournament& dst)
{
    auto path = getFilePath(dst.id);

    if (!cro::FileSystem::fileExists(path))
    {
        resetTournament(dst);
        writeTournamentData(dst);
        //LogI << "Created tournament " << cro::FileSystem::getFileName(path) << std::endl;
    }
    else
    {
        cro::RaiiRWops file;
        file.file = SDL_RWFromFile(path.c_str(), "rb");
        if (file.file)
        {
            if (!SDL_RWread(file.file, &dst, sizeof(dst), 1))
            {
                LogW << "Could not read " << cro::FileSystem::getFileName(path) << std::endl;
                resetTournament(dst);
                writeTournamentData(dst);
            }
            /*else
            {
                LogI << "Read tournament from " << cro::FileSystem::getFileName(path) << std::endl;
            }*/
        }
    }

    //debugging
    /*dst.scores = { 10,10,10,10,10,10,10,10,0 };
    dst.opponentScores = { 1,1,1,1,1,1,1,1,0 };*/
    /*dst.scores = { 1,1,1,1,1,1,1,1,0 };
    dst.opponentScores = { 10,10,10,10,10,10,10,10,0 };*/
}