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
#include "XPAwardStrings.hpp"
#include "SharedStateData.hpp"

#include <crogine/detail/Types.hpp>
#include <crogine/core/App.hpp>
#include <crogine/core/FileSystem.hpp>

#include <crogine/util/Random.hpp>
#include <crogine/util/Easings.hpp>

#include <string>
#include <fstream>
#include <filesystem>

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
    const std::string DBName("db.dat");

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

    //used to version the league data file
    constexpr std::uint32_t VersionMask = 0x000000ff;
    constexpr std::uint8_t VersionNumber = 2;
}

League::League(std::int32_t id, const SharedStateData& sd)
    : m_id                  (id),
    m_maxIterations         (id == LeagueRoundID::Club ? MaxIterations : MaxIterations / 4),
    m_sharedData            (sd),
    m_playerScore           (0),
    m_currentIteration      (0),
    m_currentSeason         (1),
    m_increaseCount         (0),
    m_currentPosition       (15),
    m_lastIterationPosition (15),
    m_currentBest           (15),
    m_nemesis               (-1),
    m_previousPosition      (17),
    m_scoreCalculator       (sd.clubSet)
{
    CRO_ASSERT(id < LeagueRoundID::Count, "");

    for (auto& scores : m_holeScores)
    {
        std::fill(scores.begin(), scores.end(), 0);
    }

    read();
}

//public
void League::reset()
{
    rollPlayers(true);
   
    createSortedTable();

    m_currentIteration = 0;
    m_currentSeason = 1;
    m_playerScore = 0;
    m_increaseCount = 0;

    m_currentPosition = 15;
    m_lastIterationPosition = 15;
    m_currentBest = 15;
    m_previousPosition = 17;

    const auto path = getFilePath(PrevFileName);
    if (cro::FileSystem::fileExists(path))
    {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }

    for (auto& scores : m_holeScores)
    {
        std::fill(scores.begin(), scores.end(), 0);
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

    //update all the player totals based on running scores
    std::int32_t p = 0;
    for (auto& player : m_players)
    {
        for (auto i = 0u; i < holeCount; ++i)
        {
            //convert to stableford where par == 2 points
            auto holeScore = m_holeScores[player.nameIndex][i] - parVals[i];
            holeScore = std::max(0, 2 - holeScore);
            player.currentScore += holeScore;
        }

        player.previousPosition = p++;
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

    //look for any position change
    p = 0;
    for (auto& player : m_players)
    {
        player.previousPosition = std::clamp(player.previousPosition - p, -1, 1) + 1;
        p++;
    }


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
   
    //current position will be updated by createSortedTable()
    m_lastIterationPosition = m_currentPosition;

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
        else if (/*m_currentSeason > 1
            &&*/ m_currentBest > 3) //if we played a couple of seasons and still not won, make it easier
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
        m_lastIterationPosition = 15;

        for (auto& player : m_players)
        {
            player.currentScore = 0; 
            player.previousPosition = 1;
        }

        m_currentSeason++;
    }
    else if (m_id != LeagueRoundID::Club)
    {
        //award some XP for completing a career round
        Social::awardXP(100 + (m_id * 50), XPStringID::CareerRoundComplete);
    }

    createSortedTable();

    //reset the hole scores for the next round
    for (auto& scores : m_holeScores)
    {
        std::fill(scores.begin(), scores.end(), 0);
    }
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

void League::updateHoleScores(std::uint32_t hole, std::int32_t par, bool overPar, std::int32_t windChance)
{
    windChance = std::clamp(windChance, 1, 100);
    for (auto& player : m_players)
    {
        m_scoreCalculator.calculate(player, hole, par, overPar, m_holeScores[player.nameIndex]);

        if (cro::Util::Random::value(0, 99) < windChance)
        {
            m_holeScores[player.nameIndex][hole]++;
        }
    }
    updateDB();
}

void League::retrofitHoleScores(const std::vector<std::int32_t>& parVals)
{
    bool writeWhenDone = false;
    for (auto i = 0u; i < parVals.size(); ++i)
    {
        if (m_holeScores[0][i] == 0)
        {
            for (auto& player : m_players)
            {
               m_scoreCalculator.calculate(player, i, parVals[i], false, m_holeScores[player.nameIndex]);
            }
            //LogI << "Retrofit " << parVals.size() << " scores for hole " << i+1 << std::endl;
            writeWhenDone = true;
        }
    }

    if (writeWhenDone)
    {
        updateDB();
    }
}

const LeaguePlayer& League::getPlayer(std::int32_t nameIndex) const
{
    auto r = std::find_if(m_players.begin(), m_players.end(), [nameIndex](const LeaguePlayer& p) {return p.nameIndex == nameIndex; });
    CRO_ASSERT(r != m_players.end(), "");

    return *r;
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
                    buff[i].nameIndex = std::clamp(buff[i].nameIndex, -1, static_cast<std::int32_t>(PlayerCount) - 1);

                    m_previousResults += " -~- ";
                    m_previousResults += std::to_string(i + 1);
                    if (buff[i].nameIndex > -1)
                    {
                        m_previousResults += ". " + m_sharedData.leagueNames[buff[i].nameIndex];
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
void League::rollPlayers(bool resetScores)
{
    std::int32_t nameIndex = 0;
    for (auto& player : m_players)
    {
        float dist = static_cast<float>(std::min(nameIndex, static_cast<std::int32_t>(PlayerCount / 3))) / PlayerCount;

        std::int32_t oldScore = 0;
        std::int32_t oldPosition = 0;
        if (!resetScores)
        {
            oldScore = player.currentScore;
            oldPosition = player.previousPosition;
        }

        player = {};
        player.skill = std::round(dist * (SkillCentre - 2)) + 2;
        player.curve = std::round(dist * MaxCurve) + (player.skill % 2);
        player.curve = player.curve + cro::Util::Random::value(-1, 1);
        player.curve = std::clamp(player.curve, 0, MaxCurve);
        player.outlier = cro::Util::Random::value(2, 10);
        player.nameIndex = nameIndex++;

        //this starts small and increase as
        //player level is increased
        player.quality = BaseQuality - (0.01f * (player.nameIndex / 2));

        player.currentScore = oldScore;
        player.previousPosition = oldPosition;
    }

    //for career leagues we want to increase the initial difficulty
    //as it remains at that level between seasons
    std::int32_t maxIncrease = 0;
    switch (m_id)
    {
    default: break;
    //case LeagueRoundID::RoundTwo:
    //    maxIncrease = 2;
        break;
    case LeagueRoundID::RoundThree:
        maxIncrease = 2;
        break;
    case LeagueRoundID::RoundFour:
        maxIncrease = 3;
        break;
    case LeagueRoundID::RoundFive:
        maxIncrease = 4;
        break;
    case LeagueRoundID::RoundSix:
        maxIncrease = 4;
        break;
    }

    for (auto i = 0; i < maxIncrease; ++i)
    {
        increaseDifficulty();
    }
}

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
    std::int32_t failureMagnitude = std::max(1, m_currentBest - 2);
    failureMagnitude /= 4;
    failureMagnitude += 1;

    for (auto i = 0u; i < PlayerCount; ++i)
    {
        //the worse our last position was the more we decrease difficulty
        for (auto j = 0; j < failureMagnitude; ++j)
        {
            m_players[i].quality = std::max(MinQuality, m_players[i].quality - ((0.02f * ((PlayerCount - 1)-i)) / 10.f));
            if (cro::Util::Random::value(0, 1) == 0)
            {
                m_players[i].skill = std::min(SkillCentre, m_players[i].skill + 1);
            }
        }
    }
    LogI << "League reduced." << std::endl;
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
    m_nemesis = -1;

    std::vector<TableEntry> entries;
    for (const auto& p : m_players)
    {
        entries.emplace_back(p.currentScore, p.outlier + p.curve, p.nameIndex, p.previousPosition);
    }
    //we'll fake our handicap (it's not a real golf one anyway) with our current level
    entries.emplace_back(m_playerScore, Social::getLevel() / 2, -1, 1);

    std::sort(entries.begin(), entries.end(),
        [](const TableEntry& a, const TableEntry& b)
        {
            if (a.score == b.score)
            {
                //favours humans
                if (a.name == -1)
                {
                    return true;
                }

                if (b.name == -1)
                {
                    return false;
                }

                return a.handicap > b.handicap;
            }
            return a.score > b.score;
        });

    entries.swap(m_sortedTable);

    //find our position, and also update any change
    auto result = std::find_if(m_sortedTable.begin(), m_sortedTable.end(),
        [](const TableEntry& te)
        {
            return te.name == -1;
        });
    
    m_currentPosition = static_cast<std::int32_t>(std::distance(m_sortedTable.begin(), result));
    result->positionChange = std::clamp(m_lastIterationPosition - m_currentPosition, -1, 1) + 1;


    //check nearest players to see who our nemesis is
    //TODO this could be refactored to something more sensible
    if (m_currentPosition > 0)
    {
        auto s = m_sortedTable[m_currentPosition - 1].score;

        if (s == m_playerScore + 1)
        {
            m_nemesis = m_sortedTable[m_currentPosition - 1].name;
        }
        else if(m_currentPosition < m_sortedTable.size() - 1)
        {
            //check the player below
            s = m_sortedTable[m_currentPosition + 1].score;
            if (s == m_playerScore || s == m_playerScore + 1)
            {
                m_nemesis = m_sortedTable[m_currentPosition + 1].name;
            }
        }
    }
    else
    {
        const auto s = m_sortedTable[1].score;
        if (s == m_playerScore || s == m_playerScore + 1)
        {
            m_nemesis = m_sortedTable[1].name;
        }
    }
}

void League::read()
{
    assertDB();

    bool rerollPlayers = false;

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
        //one of which was used in 1.16.2 to store our last iteration position
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

        std::int32_t version = 0;
        if (size == ExpectedSize)
        {
            //read the personal best, and skip padding
            std::int32_t padding = 0;
            file.file->read(file.file, &m_currentBest, sizeof(std::int32_t), 1);
            file.file->read(file.file, &m_lastIterationPosition, sizeof(std::int32_t), 1);
            file.file->read(file.file, &version, sizeof(std::int32_t), 1);
            file.file->read(file.file, &padding, sizeof(std::int32_t), 1);
        }

        file.file->read(file.file, m_players.data(), sizeof(LeaguePlayer), PlayerCount);
        version &= VersionMask;

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

            if (version != VersionNumber)
            {
                switch (version)
                {
                default: break;
                case 0:
                    //we added position tracking, so need to set our previous position
                    player.previousPosition = 1;//no change
                    break;
                }
            }

            currScores.push_back(player.currentScore);
        }
        currScores.push_back(m_playerScore);

        //creates a table which includes player data, ready for display
        //and finds the current player position in the table
        createSortedTable();

        //again, check the file version - in case we need to
        //set any default values for the current player
        if (version != VersionNumber)
        {
            switch (version)
            {
            default: break;
            case 0:
                //we added position tracking, so need to set our previous position
                m_lastIterationPosition = m_currentPosition;
                createSortedTable(); //this needs to be rebuilt now it knows what the last iteration ought to be
                [[fallthrough]]; //if we're on V0 we definitely want to fall through to V1
            case 1:
                //updated league rule wants regenned players
                rerollPlayers = true;
                break;
            }
        }


        //read hole scores from DB
        const auto dbPath = Social::getBaseContentPath() + DBName;
        constexpr auto DBSize = LeagueRoundID::Count * sizeof(m_holeScores);
        cro::RaiiRWops dbFile;
        dbFile.file = SDL_RWFromFile(dbPath.c_str(), "rb");
        if (dbFile.file)
        {
            const auto dbSize = dbFile.file->seek(dbFile.file, 0, RW_SEEK_END);
            if (dbSize != DBSize)
            {
                //close the file and delete it
                SDL_RWclose(dbFile.file);

                LogE << "DB File size incorrect, DB will be reset" << std::endl;

                std::error_code ec;
                std::filesystem::remove(dbPath, ec);
                assertDB();
            }
            else
            {
                auto startPoint = m_id * sizeof(m_holeScores);
                dbFile.file->seek(dbFile.file, startPoint, RW_SEEK_SET);
                SDL_RWread(dbFile.file, m_holeScores.data(), sizeof(m_holeScores), 1);
            }
        }
    }
    else
    {
        //create a fresh league
        reset();
    }

    if (rerollPlayers)
    {
        rollPlayers(false);
        write();
    }
}

void League::write()
{
    /*
    File Structure
    int32 current interation
    int32 current season
    int32 player score
    int32 increase count - number of times the difficulty has been increased
    int32 current best
    int32 last iteration position - tracks our player position change between rounds
    int32 metadata - currently reserved except for version byte 0xrrrrrrvv
    int32 reserved

    array of LeaguePlayer[PlayerCount] - 15 players
    
    */

    const auto path = getFilePath(FileName);

    cro::RaiiRWops file;
    file.file = SDL_RWFromFile(path.c_str(), "wb");
    if (file.file)
    {
        file.file->write(file.file, &m_currentIteration, sizeof(std::int32_t), 1);
        file.file->write(file.file, &m_currentSeason, sizeof(std::int32_t), 1);
        file.file->write(file.file, &m_playerScore, sizeof(std::int32_t), 1);
        file.file->write(file.file, &m_increaseCount, sizeof(std::int32_t), 1);

        static constexpr std::int32_t padding = 0;
        static constexpr std::int32_t version = (0 | VersionNumber);
        file.file->write(file.file, &m_currentBest, sizeof(std::int32_t), 1);
        file.file->write(file.file, &m_lastIterationPosition, sizeof(std::int32_t), 1);
        file.file->write(file.file, &version, sizeof(std::int32_t), 1);
        file.file->write(file.file, &padding, sizeof(std::int32_t), 1);

        file.file->write(file.file, m_players.data(), sizeof(LeaguePlayer), PlayerCount);

        //write hole scores to db
        updateDB();
    }
    else
    {
        LogE << "Couldn't open " << path << " for writing" << std::endl;
    }
}

void League::assertDB()
{
    const auto path = Social::getBaseContentPath() + DBName;
    if (!cro::FileSystem::fileExists(path))
    {
        //hmm what do if we failed creating this? I guess the read/write ops
        //will fail anyway when they can't open the file, so no harm, just
        //no player scores either...
        cro::RaiiRWops file;
        file.file = SDL_RWFromFile(path.c_str(), "wb");
        if (file.file)
        {
            //create an empty file big enough to store all the arrays
            constexpr auto size = LeagueRoundID::Count * sizeof(m_holeScores);
            std::vector<std::uint8_t> nullData(size);
            std::fill(nullData.begin(), nullData.end(), 0);

            SDL_RWwrite(file.file, nullData.data(), size, 1);

            LogI << "Created new league DB" << std::endl;
        }
        else
        {
            LogE << "Unable to create player database for League" << std::endl;
        }
    }
}

void League::updateDB()
{
    const auto dbPath = Social::getBaseContentPath() + DBName;
    constexpr auto DBSize = LeagueRoundID::Count * sizeof(m_holeScores);

    //hmm SDL doesn't let us write to arbitrary positions in the file
    //so we have to open it, read the entire thing, update the local data
    //then write the whole ting back again D:
    std::vector<std::uint8_t> temp(DBSize);
    std::fill(temp.begin(), temp.end(), 0);

    cro::RaiiRWops dbFile;
    dbFile.file = SDL_RWFromFile(dbPath.c_str(), "rb");
    if (dbFile.file)
    {
        const auto dbSize = dbFile.file->seek(dbFile.file, 0, RW_SEEK_END);
        if (dbSize != DBSize)
        {
            //close the file and delete it
            SDL_RWclose(dbFile.file);

            LogE << "DB File size incorrect, DB will be reset" << std::endl;

            std::error_code ec;
            std::filesystem::remove(dbPath, ec);
            assertDB();
        }
        else
        {
            SDL_RWread(dbFile.file, temp.data(), DBSize, 1);
            SDL_RWclose(dbFile.file);

            dbFile.file = SDL_RWFromFile(dbPath.c_str(), "wb");
            if (dbFile.file)
            {
                auto startPoint = m_id * sizeof(m_holeScores);
                std::memcpy(&temp[startPoint], m_holeScores.data(), sizeof(m_holeScores));

                SDL_RWwrite(dbFile.file, temp.data(), DBSize, 1);
                //LogI << "Wrote updated DB" << std::endl;
            }
        }
    }
}


//------------Score Calculator---------//
ScoreCalculator::ScoreCalculator(std::int32_t clubset)
    : m_clubset (clubset)
{

}

//public
void ScoreCalculator::calculate(const LeaguePlayer& player, std::uint32_t hole, std::int32_t par, bool overPar, HoleScores& holeScores) const
{
    auto skill = player.skill;
    if (overPar
        && m_clubset != 2)
    {
        //have a little leniency on the player
        skill++;
    }

    //CPU players exhibit better skill when playing with longer club sets
    auto skillOffset = cro::Util::Random::value(0, 2) == 0 ? 0 : 1;
    if (m_clubset == 1)
    {
        auto s = std::max(1, skill - 1);
        skillOffset = cro::Util::Random::value(-s, s);
    }
    else if (m_clubset == 0)
    {
        skillOffset = cro::Util::Random::value(-skill, skill);
    }

    //calc aim accuracy
    const float aim = 1.f - AimSkill[SkillCentre + skillOffset];

    //calc power accuracy
    const float power = 1.f - PowerSkill[SkillCentre + skillOffset];

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
    switch (par)
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
    if (cro::Util::Random::value(0, 10) > skill)
    {
        score -= 1.f;
    }

    //add player score to player total
    std::int32_t holeScore = -score;
    holeScore += par;

    //too many HIOs
    if (holeScore == 1)
    {
        //players with skill 0 (0 good, 2 bad) + good clubs have better chance of keeping the HIO
        //players with skill 2 and short clubs have worse chance of keeping the HIO
        const auto hioSkill = (player.skill + (2 - m_clubset)) * 2;
        if (cro::Util::Random::value(0, (3 + hioSkill)) != 0)
        {
            holeScore += std::max(1, (par - 2));
        }
        else
        {
            //check the previous hole and increase it anyway if it
            //was a HIO just so we don't get consecutive HIOs
            if (hole > 0)
            {
                if (holeScores[hole - 1] == 1)
                {
                    holeScore += cro::Util::Random::value(0, 4) == 0 ? 1 : 2;
                }
            }
        }
    }

    //make sure novice club sets rarely get better than a birdie
    //and reduce the overall chance of eagles on higher clubs
    if (holeScore < (par - 1))
    {
        switch (m_clubset)
        {
        default:
        case 0: //novice
            if (cro::Util::Random::value(0, 2) != 0)
            {
                holeScore += cro::Util::Random::value(2, 4) / 2;
            }
            break;
        case 1: //expert
            if (cro::Util::Random::value(0, 1) != 0)
            {
                holeScore += cro::Util::Random::value(2, 4) / 2;
            }
            break;
        case 2: //pro
            if (cro::Util::Random::value(0, 1) != 0)
            {
                holeScore += cro::Util::Random::value(0, 4) / 2;
            }
            break;
        }
    }
    /*if (m_sharedData.clubSet == 0
        && holeScore < (par - 1)
        && cro::Util::Random::value(0, 2) != 0)
    {
        holeScore += cro::Util::Random::value(2, 4) / 2;
    }*/


    //there's a flaw in my logic here which means the result occasionally
    //comes back as zero - rather than fix my logic I'm going to paste over the cracks.
    if (holeScore == 0)
    {
        holeScore = std::max(2, par + cro::Util::Random::value(-1, 1));
    }


    //write this to the hole scores which get saved in a file / used to display on scoreboard
    CRO_ASSERT(player.nameIndex != -1, "this shouldn't be a human player");
    holeScores[hole] = holeScore;// +par;
}



//----------Friendly Player------------//
FriendlyPlayers::FriendlyPlayers(std::int32_t clubset)
    : m_scoreCalculator   (clubset)
{

}

//public
void FriendlyPlayers::updateHoleScores(std::uint32_t hole, std::int32_t par, bool overPar, std::int32_t windChance)
{
    windChance = std::clamp(windChance, 1, 100);
    for (auto& player : m_players)
    {
        m_scoreCalculator.calculate(player, hole, par, overPar, m_holeScores[player.nameIndex]);

        if (cro::Util::Random::value(0, 99) < windChance)
        {
            m_holeScores[player.nameIndex][hole]++;
        }
    }
}

void FriendlyPlayers::addPlayer(LeaguePlayer p)
{
    m_players.push_back(p);
}

void FriendlyPlayers::setHoleScores(std::int32_t playerNameIndex, const HoleScores& scores)
{
    m_holeScores[playerNameIndex] = scores;
}

const HoleScores& FriendlyPlayers::getHoleScores(std::int32_t playerNameIndex) const
{
    CRO_ASSERT(playerNameIndex > -1 && playerNameIndex < League::PlayerCount, "");
    return m_holeScores[playerNameIndex];
}