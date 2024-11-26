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

void resetTournament(Tournament& src)
{
    const auto id = src.id;
    src = {};
    src.id = id;

    for (auto i = -1; i < static_cast<std::int32_t>(League::PlayerCount); ++i)
    {
        src.tier0[i + 1] = i;
    }

    std::shuffle(src.tier0.begin(), src.tier0.end(), cro::Util::Random::rndEngine);
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
        LogI << "Created tournament " << cro::FileSystem::getFileName(path) << std::endl;
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
            else
            {
                LogI << "Read tournament from " << cro::FileSystem::getFileName(path) << std::endl;
            }
        }
    }
}