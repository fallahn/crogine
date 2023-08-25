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

#include "League.hpp"

#include <crogine/detail/Types.hpp>
#include <crogine/core/App.hpp>
#include <crogine/core/FileSystem.hpp>

#include <crogine/util/Random.hpp>

#include <string>

namespace
{
    const std::array<std::string, League::PlayerCount> PlayerNames =
    {
        std::string("Albert Grey"),
        "Laurie Astable",
        "David Shoesmith",
        "Bern Stein",
        "Mischa van Holt",
        "Kiera Daily",
        "Meana Patel"
    };

    const std::string FileName("lea.gue");
    constexpr std::int32_t MaxCurve = 5;

    std::array<std::int32_t, 18> DummyData =
    {
        3,3,3,4,4,4,3,3,4,3,4,4,3,4,2,2,4,5
    };
}

League::League()
    : m_currentIteration(0)
{
    read();
}

//public
void League::reset()
{
    std::int32_t skill = 1;
    std::int32_t nameIndex = 0;
    for (auto& player : m_players)
    {
        player = {};
        player.skill = skill;
        player.curve = player.skill + cro::Util::Random::value(-1, 1);
        player.curve = std::clamp(player.curve, 0, MaxCurve);
        player.outlier = cro::Util::Random::value(1, 10);
        player.nameIndex = nameIndex++;

        skill += cro::Util::Random::value(1, 3);
    }

    write();
}

void League::iterate()
{
    if (m_currentIteration == MaxIterations)
    {
        //start a new season

        //TODO evaluate all players and adjust skills

        m_currentIteration = 0;
        for (auto& player : m_players)
        {
            player.currentScore = 0;
        }
    }
    
    //TODO this
    for (auto& player : m_players)
    {
        std::int32_t parTotal = 0;
        std::int32_t playerTotal = 0;

        for (auto par : DummyData)
        {
            parTotal += par;

            //TODO calc aim accuracy
            //TODO calc power accuracy
            //TODO if rand(0,100) == outlier calc some amount of cock-up

            //TODO pass through active curve
            //TODO calc ideal
            //TODO find range of triple bogey - ideal
            //TODO add player score to player total
        }

        player.currentScore += (playerTotal - parTotal);
    }

    //as we store the index into the name array
    //as part of the player data we sort the array here
    //and store it pre-sorted.
    std::sort(m_players.begin(), m_players.end(), 
        [](const LeaguePlayer& a, const LeaguePlayer& b)
        {
            return a.currentScore < b.currentScore;
        });

    write();
}

//private
void League::read()
{
    const auto path = cro::App::getPreferencePath() + FileName;
    if (cro::FileSystem::fileExists(path))
    {
        cro::RaiiRWops file;
        file.file = SDL_RWFromFile(path.c_str(), "rb");
        if (!file.file)
        {
            LogE << "Could not open " << path << " for reading" << std::endl;
            //reset();
            return;
        }

        static constexpr std::size_t ExpectedSize = sizeof(std::int32_t) + (sizeof(LeaguePlayer) * PlayerCount);
        if (auto size = file.file->seek(file.file, 0, RW_SEEK_END); size != ExpectedSize)
        {
            file.file->close(file.file);

            LogE << path << " file not expected size!" << std::endl;
            reset();
            return;
        }

        file.file->seek(file.file, 0, RW_SEEK_SET);
        file.file->read(file.file, &m_currentIteration, sizeof(std::int32_t), 1);
        file.file->read(file.file, m_players.data(), sizeof(LeaguePlayer), PlayerCount);

        //validate the loaded data and clamp to sane values
        m_currentIteration = std::clamp(m_currentIteration, 0, MaxIterations);

        static constexpr std::int32_t MaxScore = -2 * 18 * MaxIterations;

        //TODO what do we consider sane values for each player?
        for (auto& player : m_players)
        {
            player.curve = std::clamp(player.curve, 0, MaxCurve);
            player.outlier = std::clamp(player.outlier, 1, 10);
            player.skill = std::clamp(player.skill, 1, 20);
            player.nameIndex = std::clamp(player.nameIndex, 0, std::int32_t(PlayerCount) - 1);

            player.currentScore = std::clamp(player.currentScore, MaxScore, 100);
        }
    }
    else
    {
        //create a fresh league
        reset();
    }
}

void League::write()
{
    const auto path = cro::App::getPreferencePath() + FileName;

    cro::RaiiRWops file;
    file.file = SDL_RWFromFile(path.c_str(), "wb");
    if (file.file)
    {
        file.file->write(file.file, &m_currentIteration, sizeof(std::int32_t), 1);
        file.file->write(file.file, m_players.data(), sizeof(LeaguePlayer), PlayerCount);
    }
    else
    {
        LogE << "Couldn't open " << path << " for writing" << std::endl;
    }
}