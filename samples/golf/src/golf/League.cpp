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
#include "Social.hpp"
#include "Achievements.hpp"
#include "AchievementStrings.hpp"

#include <crogine/detail/Types.hpp>
#include <crogine/core/App.hpp>
#include <crogine/core/FileSystem.hpp>

#include <crogine/util/Random.hpp>
#include <crogine/util/Easings.hpp>

#include <string>
#include <fstream>

namespace
{
    const std::string FileName("lea.gue");

    constexpr std::int32_t MaxCurve = 5;

    constexpr std::array AimSkill =
    {
        0.49f, 0.436f, 0.37f, 0.312f, 0.25f, 0.187f, 0.125f, 0.062f,
        0.f,
        0.062f, 0.125f, 0.187f, 0.25f, 0.312f, 0.375f, 0.437f, 0.49f
    };

    constexpr std::array PowerSkill =
    {
        0.49f, 0.436f, 0.37f, 0.312f, 0.25f, 0.187f, 0.125f, 0.062f,
        0.f,
        0.062f, 0.125f, 0.187f, 0.25f, 0.312f, 0.375f, 0.437f, 0.49f
    };
    constexpr std::int32_t SkillCentre = 8;

    float applyCurve(float ip, std::int32_t curveIndex)
    {
        switch (curveIndex)
        {
        default: return ip;
        case 0:
        case 1:
            return cro::Util::Easing::easeInCubic(ip);
        case 2:
        case 3:
            return cro::Util::Easing::easeInQuad(ip);
        case 4:
        case 5:
            return cro::Util::Easing::easeInSine(ip);
        }
    }

    constexpr std::int32_t SkillRoof = 10; //after this many increments the skills stop getting better - just shift around
}

League::League()
    : m_playerScore     (0),
    m_currentIteration  (0),
    m_currentSeason     (1),
    m_increaseCount     (0)
{
    read();
}

//public
void League::reset()
{
    std::int32_t nameIndex = 0;
    for (auto& player : m_players)
    {
        float dist = static_cast<float>(nameIndex) / PlayerCount;

        player = {};
        player.skill = std::round(dist * (SkillCentre - 1) + 1);
        player.curve = std::round(dist * MaxCurve) + (player.skill % 2);
        player.curve = player.curve + cro::Util::Random::value(-1, 1);
        player.curve = std::clamp(player.curve, 0, MaxCurve);
        player.outlier = cro::Util::Random::value(1, 10);
        player.nameIndex = nameIndex++;

        //this starts small and increase as
        //player level is increased
        player.quality = 0.87f - (0.01f * (player.nameIndex / 2));
    }
    m_currentIteration = 0;
    m_currentSeason = 1;
    m_playerScore = 0;
    m_increaseCount = 0;
    write();
}

void League::iterate(const std::array<std::int32_t, 18>& parVals, const std::vector<std::uint8_t>& playerScores, std::size_t holeCount)
{
    //reset the scores if this is the first iteration of a new season
    if (m_currentIteration == 0)
    {
        m_playerScore = 0;
        for (auto& player : m_players)
        {
            player.currentScore = 0;
        }
    }

    CRO_ASSERT(holeCount == 6 || holeCount == 9 || holeCount == 12 || holeCount == 18, "");

    //update all the player scores
    for (auto& player : m_players)
    {
        std::int32_t playerTotal = 0;

        for (auto i = 0u; i < holeCount; ++i)
        {
            //calc aim accuracy
            float aim = 1.f - AimSkill[SkillCentre + cro::Util::Random::value(-player.skill, player.skill)];

            //calc power accuracy
            float power = 1.f - PowerSkill[SkillCentre + cro::Util::Random::value(-player.skill, player.skill)];

            float quality = aim * power;

            //outlier for cock-up
            if (cro::Util::Random::value(0, 49) < player.outlier)
            {
                float errorAmount = static_cast<float>(cro::Util::Random::value(5, 7)) / 10.f;
                quality *= errorAmount;
            }

            //pass through active curve
            quality = applyCurve(quality, MaxCurve - player.curve) * player.quality;

            
            //calc ideal
            float ideal = 3.f; //triple bogey
            switch (parVals[i])
            {
            default:
            case 2:
                ideal += 1.f; //1 under par etc
                break;
            case 3:
            case 4:
                ideal += 2.f;
                break;
            case 5:
                ideal += 3.f;
                break;
            }

            
            //find range of triple bogey - ideal
            float score = std::round(ideal * quality);
            score -= 2.f; //average out to birdie

            //then use the player skill chance to decide if we got an eagle
            if (cro::Util::Random::value(1, 10) > player.skill)
            {
                score -= 1.f;
            }

            //add player score to player total
            std::int32_t holeScore = -score;

            //convert to stableford where par == 2
            holeScore = std::max(0, 2 - holeScore);
            playerTotal += holeScore;
        }

        player.currentScore += playerTotal;
    }

    //as we store the index into the name array
    //as part of the player data we sort the array here
    //and store it pre-sorted.
    std::sort(m_players.begin(), m_players.end(), 
        [](const LeaguePlayer& a, const LeaguePlayer& b)
        {
            return a.currentScore == b.currentScore ? 
                (a.curve + a.outlier) > (b.curve + b.outlier) : //roughly the handicap
                a.currentScore > b.currentScore;
        });

    //calculate the player's stableford score
    for (auto i = 0u; i < holeCount; ++i)
    {
        std::int32_t holeScore = static_cast<std::int32_t>(playerScores[i]) - parVals[i];
        m_playerScore += std::max(0, 2 - holeScore);
    }

    m_currentIteration++;
    Achievements::incrementStat(StatStrings[StatID::LeagueRounds]);

    //displays the notification overlay
    auto* msg = cro::App::postMessage<Social::SocialEvent>(Social::MessageID::SocialMessage);
    msg->type = Social::SocialEvent::MonthlyProgress;
    msg->challengeID = -1;
    msg->level = m_currentIteration;
    msg->reason = MaxIterations;

    if (m_currentIteration == MaxIterations)
    {
        //calculate our final place and update our stats
        std::vector<SortData> sortData;

        //we'll also save this to a file so we
        //can review previous season
        for (auto i = 0u; i < m_players.size(); ++i)
        {
            auto& data = sortData.emplace_back();
            data.score = m_players[i].currentScore;
            data.nameIndex = m_players[i].nameIndex;
            data.handicap = (m_players[i].curve + m_players[i].outlier);
        }
        auto& data = sortData.emplace_back();
        data.score = m_playerScore;
        data.nameIndex = -1;
        data.handicap = Social::getLevel() / 2;

        std::sort(sortData.begin(), sortData.end(),
            [](const SortData& a, const SortData& b)
            {
                return a.score == b.score ?
                    a.handicap > b.handicap :
                a.score > b.score;
            });

        std::int32_t playerPos = 16;
        for (auto i = 0; i < 3; ++i)
        {
            if (sortData[i].nameIndex == -1)
            {
                //this is us
                Achievements::incrementStat(StatStrings[StatID::LeagueFirst + i]);
                Achievements::awardAchievement(AchievementStrings[AchievementID::LeagueChampion]);

                playerPos = i;

                //TODO raise a message to notify somewhere?
                break;
            }
        }

        //write the data to a file
        const auto path = Social::getBaseContentPath() + PrevFileName;
        cro::RaiiRWops file;
        file.file = SDL_RWFromFile(path.c_str(), "wb");
        if (file.file)
        {
            SDL_RWwrite(file.file, sortData.data(), sizeof(SortData), sortData.size());
            //LogI << "Wrote previous season to " << PrevFileName << std::endl;
        }


        //evaluate all players and adjust skills if we came in the top 2
        if (playerPos < 2
            && m_increaseCount < SkillRoof)
        {
            //increase ALL player quality, but show a bigger improvement near the bottom
            for (auto i = 0u; i < PlayerCount; ++i)
            {
                m_players[i].quality = std::min(1.f, m_players[i].quality + ((0.02f * i) / 10.f));

                //modify chance of making mistake 
                auto outlier = m_players[i].outlier;
                if (i < PlayerCount / 2)
                {
                    outlier = std::clamp(outlier + cro::Util::Random::value(0, 1), 1, 10);

                    //modify curve for top 3rd
                    //if (1 < PlayerCount < 3)
                    //{
                    //    auto curve = m_players[i].curve;
                    //    curve = std::max(0, curve - cro::Util::Random::value(0, 1));

                    //    //wait... I've forgotten what I was doing here - though this clearly does nothing.
                    //}
                }
                else
                {
                    outlier = std::clamp(outlier + cro::Util::Random::value(-1, 0), 1, 10);
                }
                m_players[i].outlier = outlier;
            }
            m_increaseCount++;
        }
        else
        {
            for (auto& player : m_players)
            {
                auto rVal = cro::Util::Random::value(0.f, 0.11f);
                if (player.quality > 0.89f)
                {
                    player.quality -= rVal;
                }
                else
                {
                    player.quality += rVal;
                }
            }
        }

        //start a new season
        m_currentIteration = 0;
        m_currentSeason++;
    }

    write();
}

//private
void League::read()
{
    const auto path = Social::getBaseContentPath() + FileName;
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

        static constexpr std::size_t ExpectedSize = (sizeof(std::int32_t) * 4) + (sizeof(LeaguePlayer) * PlayerCount);
        if (auto size = file.file->seek(file.file, 0, RW_SEEK_END); size != ExpectedSize)
        {
            file.file->close(file.file);

            LogE << path << " file not expected size!" << std::endl;
            reset();
            return;
        }

        file.file->seek(file.file, 0, RW_SEEK_SET);
        file.file->read(file.file, &m_currentIteration, sizeof(std::int32_t), 1);
        file.file->read(file.file, &m_currentSeason, sizeof(std::int32_t), 1);
        file.file->read(file.file, &m_playerScore, sizeof(std::int32_t), 1);
        file.file->read(file.file, &m_increaseCount, sizeof(std::int32_t), 1);

        file.file->read(file.file, m_players.data(), sizeof(LeaguePlayer), PlayerCount);


        //validate the loaded data and clamp to sane values
        m_currentIteration %= MaxIterations;

        //static constexpr std::int32_t MaxScore = 5 * 18 * MaxIterations;

        //TODO what do we consider sane values for each player?
        for (auto& player : m_players)
        {
            player.curve = std::clamp(player.curve, 0, MaxCurve);
            player.outlier = std::clamp(player.outlier, 1, 10);
            player.skill = std::clamp(player.skill, 1, 20);
            player.nameIndex = std::clamp(player.nameIndex, 0, std::int32_t(PlayerCount) - 1);

            //player.currentScore = std::clamp(player.currentScore, 0, MaxScore);
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
    const auto path = Social::getBaseContentPath() + FileName;

    cro::RaiiRWops file;
    file.file = SDL_RWFromFile(path.c_str(), "wb");
    if (file.file)
    {
        file.file->write(file.file, &m_currentIteration, sizeof(std::int32_t), 1);
        file.file->write(file.file, &m_currentSeason, sizeof(std::int32_t), 1);
        file.file->write(file.file, &m_playerScore, sizeof(std::int32_t), 1);
        file.file->write(file.file, &m_increaseCount, sizeof(std::int32_t), 1);

        file.file->write(file.file, m_players.data(), sizeof(LeaguePlayer), PlayerCount);
    }
    else
    {
        LogE << "Couldn't open " << path << " for writing" << std::endl;
    }
}
