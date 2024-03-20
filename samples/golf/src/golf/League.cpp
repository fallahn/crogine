/*-----------------------------------------------------------------------

Matt Marchant 2023 - 2024
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
#include "RandNames.hpp"
#include "XPAwardStrings.hpp"

#include <crogine/detail/Types.hpp>
#include <crogine/core/App.hpp>
#include <crogine/core/FileSystem.hpp>

#include <crogine/util/Random.hpp>
#include <crogine/util/Easings.hpp>

#include <string>
#include <fstream>

namespace
{
    constexpr std::array XPAmount =
    {
        400, 200, 100
    };

    constexpr std::array XPMultiplier =
    {
        100, 50, 25
    };

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
    constexpr float BaseQuality = 0.87f;
    constexpr float MinQuality = BaseQuality - 0.07f; //0.01 * PlayerCount/2

}

League::League(std::int32_t id)
    : m_id              (id),
    m_maxIterations     (id == LeagueRoundID::Club ? MaxIterations : MaxIterations / 4),
    m_playerScore       (0),
    m_currentIteration  (0),
    m_currentSeason     (1),
    m_increaseCount     (0),
    m_currentPosition   (16),
    m_currentBest       (15),
    m_previousPosition  (17)
{
    CRO_ASSERT(id < LeagueRoundID::Count, "");

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
        player.quality = BaseQuality - (0.01f * (player.nameIndex / 2));
    }

    //for career leagues we want to increase the initial difficulty
    //as it remains at that level between seasons
    std::int32_t maxIncrease = 0;
    switch (m_id)
    {
    default: break;
    case LeagueRoundID::RoundTwo:
        maxIncrease = 2;
        break;
    case LeagueRoundID::RoundThree:
        maxIncrease = 4;
        break;
    case LeagueRoundID::RoundFour:
        maxIncrease = 6;
        break;
    case LeagueRoundID::RoundFive:
        maxIncrease = 8;
        break;
    case LeagueRoundID::RoundSix:
        maxIncrease = 10;
        break;
    }

    for (auto i = 0; i < maxIncrease; ++i)
    {
        increaseDifficulty();
    }

    createSortedTable();

    m_currentIteration = 0;
    m_currentSeason = 1;
    m_playerScore = 0;
    m_increaseCount = 0;

    m_currentPosition = 16;
    m_currentBest = 15;
    m_previousPosition = 17;

    const auto path = getFilePath(PrevFileName);
    if (cro::FileSystem::fileExists(path))
    {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }

    write();
}

void League::iterate(const std::array<std::int32_t, 18>& parVals, const std::vector<std::uint8_t>& playerScores, std::size_t holeCount)
{
    //reset the scores if this is the first iteration of a new season
    //moved to end of func when resetting iter
    /*if (m_currentIteration == 0)
    {
        m_playerScore = 0;
        for (auto& player : m_players)
        {
            player.currentScore = 0;
        }
    }*/

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
    
    if (m_id == LeagueRoundID::Club)
    {
        Achievements::incrementStat(StatStrings[StatID::LeagueRounds]);
    }
    //displays the notification overlay
    auto* msg = cro::App::postMessage<Social::SocialEvent>(Social::MessageID::SocialMessage);
    msg->type = Social::SocialEvent::LeagueProgress;
    msg->challengeID = m_id;
    msg->level = m_currentIteration;
    msg->reason = m_maxIterations;
   

    if (m_currentIteration == m_maxIterations)
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
                switch (m_id)
                {
                case LeagueRoundID::Club:
                    Achievements::incrementStat(StatStrings[StatID::LeagueFirst + i]);
                    Achievements::awardAchievement(AchievementStrings[AchievementID::LeagueChampion]);
                    break;
                default:
                    //award XP for each slot above 4th
                    Social::awardXP(XPAmount[i] + (m_id * XPMultiplier[i]), XPStringID::CareerSeasonComplete);
                    
                    //TODO award achievement for completion
                    break;
                }
                playerPos = i;

                //TODO raise a message to notify somewhere?
                break;
            }
        }

        if (playerPos < m_currentBest)
        {
            m_currentBest = playerPos;
        }

        //write the data to a file
        const auto path = getFilePath(PrevFileName);
        cro::RaiiRWops file;
        file.file = SDL_RWFromFile(path.c_str(), "wb");
        if (file.file)
        {
            SDL_RWwrite(file.file, sortData.data(), sizeof(SortData), sortData.size());
            //LogI << "Wrote previous season to " << PrevFileName << std::endl;
        }


        //evaluate all players and adjust skills

        if (playerPos < 2 //increase in top 2
            && m_increaseCount < SkillRoof)
        {
            increaseDifficulty();
            m_increaseCount++;
        }
        else if (m_currentSeason > 1
            && m_currentBest > 3) //if we played a couple of seasons and still not won, make it easier
        {
            decreaseDifficulty();
        }
        else
        {
            //add some small variance
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

        //start a new season - this should be ok here as we saved the previous results
        //out to a file to scroll at the bottom
        m_currentIteration = 0;
        m_playerScore = 0;

        for (auto& player : m_players)
        {
            player.currentScore = 0;
        }

        m_currentSeason++;
    }
    else if (m_id != LeagueRoundID::Club)
    {
        //award some XP for completing a career round
        Social::awardXP(100 + (m_id * 50), XPStringID::CareerRoundComplete);
    }

    createSortedTable();
    write();
}

std::int32_t League::reward(std::int32_t position) const
{
    switch (position)
    {
    default: return 0;
    case 1:
    case 2:
    case 3:
        return XPAmount[position - 1] + (m_id * XPMultiplier[position - 1]);
    }
}

const cro::String& League::getPreviousResults(const cro::String& playerName) const
{
    const auto path = getFilePath(PrevFileName);
    if ((m_currentIteration == 0 || m_previousResults.empty())
        && cro::FileSystem::fileExists(path))
    {
        cro::RaiiRWops file;
        file.file = SDL_RWFromFile(path.c_str(), "rb");
        if (file.file)
        {
            auto size = SDL_RWseek(file.file, 0, RW_SEEK_END);
            if (size % sizeof(PreviousEntry) == 0)
            {
                auto count = size / sizeof(PreviousEntry);
                std::vector<PreviousEntry> buff(count);

                SDL_RWseek(file.file, 0, RW_SEEK_SET);
                SDL_RWread(file.file, buff.data(), sizeof(PreviousEntry), count);

                //this assumes everything was sorted correctly when it was saved
                m_previousResults = "Previous Season's Results";
                for (auto i = 0u; i < buff.size(); ++i)
                {
                    buff[i].nameIndex = std::clamp(buff[i].nameIndex, -1, static_cast<std::int32_t>(RandomNames.size()) - 1);

                    m_previousResults += " -~- ";
                    m_previousResults += std::to_string(i + 1);
                    if (buff[i].nameIndex > -1)
                    {
                        m_previousResults += ". " + RandomNames[buff[i].nameIndex];
                    }
                    else
                    {
                        m_previousResults += ". " + playerName;
                        m_previousPosition = i + 1;
                    }
                    m_previousResults += " " + std::to_string(buff[i].score);
                }
            }
        }
    }

    return m_previousResults;
}

//private
void League::increaseDifficulty()
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
}

void League::decreaseDifficulty()
{
    for (auto i = 0u; i < PlayerCount; ++i)
    {
        m_players[i].quality = std::max(MinQuality, m_players[i].quality - ((0.02f * ((PlayerCount - 1)-i)) / 10.f));
    }
}

std::string League::getFilePath(const std::string& fn) const
{
    std::string basePath = Social::getBaseContentPath();
    
    const auto assertPath = 
        [&]()
        {
            if (!cro::FileSystem::directoryExists(basePath))
            {
                cro::FileSystem::createDirectory(basePath);
            }
        };
    
    switch (m_id)
    {
    default: break;
    case LeagueRoundID::RoundOne:
        basePath += "career/";
        assertPath();
        basePath += "round_01/";
        assertPath();
        break;
    case LeagueRoundID::RoundTwo:
        basePath += "career/";
        assertPath();
        basePath += "round_02/";
        assertPath();
        break;
    case LeagueRoundID::RoundThree:
        basePath += "career/";
        assertPath();
        basePath += "round_03/";
        assertPath();
        break;
    case LeagueRoundID::RoundFour:
        basePath += "career/";
        assertPath();
        basePath += "round_04/";
        assertPath();
        break;
    case LeagueRoundID::RoundFive:
        basePath += "career/";
        assertPath();
        basePath += "round_05/";
        assertPath();
        break;
    case LeagueRoundID::RoundSix:
        basePath += "career/";
        assertPath();
        basePath += "round_06/";
        assertPath();
        break;
    }

    return basePath + fn;
}

void League::createSortedTable()
{
    std::vector<TableEntry> entries;
    for (const auto& p : m_players)
    {
        entries.emplace_back(p.currentScore, p.outlier + p.curve, p.nameIndex);
    }
    //we'll fake our handicap (it's not a real golf one anyway) with our current level
    entries.emplace_back(m_playerScore, Social::getLevel() / 2, -1);

    std::sort(entries.begin(), entries.end(),
        [](const TableEntry& a, const TableEntry& b)
        {
            if (a.score == b.score)
            {
                if (a.name > -1)
                {
                    return a.handicap > b.handicap;
                }
                return false;
            }
            return a.score > b.score;
        });

    entries.swap(m_sortedTable);

    m_currentPosition = static_cast<std::int32_t>(std::distance(m_sortedTable.begin(),
        std::find_if(m_sortedTable.begin(), m_sortedTable.end(),
            [](const TableEntry& te)
            {
                return te.name == -1;
            })));
}

void League::read()
{
    const auto path = getFilePath(FileName);
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

        //since 1.16 we padded out 4 extra ints to reserve some for future use (and attempt not to break existing)
        static constexpr std::size_t OldExpectedSize = (sizeof(std::int32_t) * 4) + (sizeof(LeaguePlayer) * PlayerCount);
        static constexpr std::size_t ExpectedSize = (sizeof(std::int32_t) * 8) + (sizeof(LeaguePlayer) * PlayerCount);
        
        const auto size = file.file->seek(file.file, 0, RW_SEEK_END);

        if (size != ExpectedSize && size != OldExpectedSize)
        {
            file.file->close(file.file);

            LogE << path << " file not expected size!" << std::endl;
            reset();
            return;
        }
        /*if (size == OldExpectedSize)
        {
            LogI << "found old style league... updating" << std::endl;
        }*/

        file.file->seek(file.file, 0, RW_SEEK_SET);
        file.file->read(file.file, &m_currentIteration, sizeof(std::int32_t), 1);
        file.file->read(file.file, &m_currentSeason, sizeof(std::int32_t), 1);
        file.file->read(file.file, &m_playerScore, sizeof(std::int32_t), 1);
        file.file->read(file.file, &m_increaseCount, sizeof(std::int32_t), 1);

        if (size == ExpectedSize)
        {
            //read the personal best, and skip padding
            std::int32_t padding = 0;
            file.file->read(file.file, &m_currentBest, sizeof(std::int32_t), 1);
            file.file->read(file.file, &padding, sizeof(std::int32_t), 1);
            file.file->read(file.file, &padding, sizeof(std::int32_t), 1);
            file.file->read(file.file, &padding, sizeof(std::int32_t), 1);
        }

        file.file->read(file.file, m_players.data(), sizeof(LeaguePlayer), PlayerCount);


        //validate the loaded data and clamp to sane values
        m_currentIteration %= m_maxIterations;

        //static constexpr std::int32_t MaxScore = 5 * 18 * MaxIterations;

        std::vector<std::int32_t> currScores;

        //TODO what do we consider sane values for each player?
        for (auto& player : m_players)
        {
            player.curve = std::clamp(player.curve, 0, MaxCurve);
            player.outlier = std::clamp(player.outlier, 1, 10);
            player.skill = std::clamp(player.skill, 1, 20);
            player.nameIndex = std::clamp(player.nameIndex, 0, std::int32_t(PlayerCount) - 1);

            //player.currentScore = std::clamp(player.currentScore, 0, MaxScore);

            currScores.push_back(player.currentScore);
        }
        currScores.push_back(m_playerScore);

        //creates a table which includes player data, ready for display
        //and finds the current player position in the table
        createSortedTable();
    }
    else
    {
        //create a fresh league
        reset();
    }
}

void League::write()
{
    const auto path = getFilePath(FileName);

    cro::RaiiRWops file;
    file.file = SDL_RWFromFile(path.c_str(), "wb");
    if (file.file)
    {
        file.file->write(file.file, &m_currentIteration, sizeof(std::int32_t), 1);
        file.file->write(file.file, &m_currentSeason, sizeof(std::int32_t), 1);
        file.file->write(file.file, &m_playerScore, sizeof(std::int32_t), 1);
        file.file->write(file.file, &m_increaseCount, sizeof(std::int32_t), 1);

        const std::int32_t padding = 0;
        file.file->write(file.file, &m_currentBest, sizeof(std::int32_t), 1);
        file.file->write(file.file, &padding, sizeof(std::int32_t), 1);
        file.file->write(file.file, &padding, sizeof(std::int32_t), 1);
        file.file->write(file.file, &padding, sizeof(std::int32_t), 1);

        file.file->write(file.file, m_players.data(), sizeof(LeaguePlayer), PlayerCount);
    }
    else
    {
        LogE << "Couldn't open " << path << " for writing" << std::endl;
    }
}
