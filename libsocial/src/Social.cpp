/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2024
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include "Social.hpp"
#include "Achievements.hpp"
#include "AchievementStrings.hpp"
#include "StoredValue.hpp"

#ifdef USE_GJS
#include <libgjs.hpp>
#endif

#include <crogine/core/App.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/core/SysTime.hpp>

#include <ctime>

namespace
{
    struct ValueID final
    {
        enum
        {
            XP,
            Clubset,
            Ballset,
            Level,
            Generic,
            LastLog,
            DayStreak,
            LongestStreak,
            CareerBalls,
            CareerHair,
            CareerAvatar,
            CareerPosition,
            Tournament,

            Count
        };
    };
    //THESE ALL GET RESET WITH RESET PROFILE
    std::array<StoredValue, ValueID::Count> StoredValues =
    {
        StoredValue("exp"),
        StoredValue("clb"),
        StoredValue("bls"),
        StoredValue("lvl"),
        StoredValue("gnc"),
        StoredValue("llg"),
        StoredValue("dsk"),
        StoredValue("lsk"),
        StoredValue("cba"),
        StoredValue("cha"),
        StoredValue("cav"),
        StoredValue("cpo"),
        StoredValue("tul"),
    };

    StoredValue snapperFlags("snp");
    StoredValue scrubScore("scb");

    cro::String scrubString;

    std::vector<Social::Award> awards;
    const std::array<std::string, 12u> MonthStrings =
    {
        "January ", "Febrary ", "March ", "April ", "May ", "June ",
        "July ", "August ", "September ", "October ", "November ", "December "
    };

    //XP curve = (level / x) ^ y
    constexpr float XPx = 0.07f;
    constexpr float XPy = 2.f;

    constexpr std::int32_t DefaultClubSet = 3731;

    std::int32_t getLevelFromXP(std::int32_t exp)
    {
        //cache the level value to reduce calls to sqrt
        static std::int32_t lastXP = 0;
        static std::int32_t lastLevel = 0;

        if (exp != lastXP)
        {
            lastXP = exp;
            
            float xp = static_cast<float>(exp);
            lastLevel = static_cast<std::int32_t>(XPx * std::sqrt(xp));
        }

        return lastLevel;
    }

    std::int32_t getXPFromLevel(std::int32_t level)
    {
        //ew.
        return static_cast<std::int32_t>(std::pow((static_cast<float>(level) / XPx), XPy));
    }

    cro::String socialName;
}

cro::Image Social::userIcon;

cro::String Social::getPlayerName()
{
#ifdef USE_GJS
    return GJ::getActiveName();
#else
    return socialName;
#endif
}

void Social::setPlayerName(const cro::String& name)
{
#ifdef USE_GJS
    GJ::setActiveName(name);
#else
    socialName = name;
#endif
    cro::App::postMessage<SocialEvent>(Social::MessageID::SocialMessage)->type = SocialEvent::PlayerNameChanged;
}

bool Social::isValid()
{
#ifdef USE_GJS
    return GJ::isValid();
#else
    return true;
#endif
}

void Social::awardXP(std::int32_t amount, std::int32_t reason)
{
    if (Achievements::getActive())
    {
        auto oldLevel = getLevelFromXP(StoredValues[ValueID::XP].value);

        StoredValues[ValueID::XP].value += amount;
        StoredValues[ValueID::XP].write();

        //raise event to notify players
        auto* msg = cro::App::postMessage<SocialEvent>(MessageID::SocialMessage);
        msg->type = SocialEvent::XPAwarded;
        msg->level = amount;
        msg->reason = reason;

        auto level = getLevelFromXP(StoredValues[ValueID::XP].value);
        if (oldLevel < level)
        {
            auto* msg2 = cro::App::postMessage<SocialEvent>(MessageID::SocialMessage);
            msg2->type = SocialEvent::LevelUp;
            msg2->level = level;
        }
    }
}

std::int32_t Social::getXP()
{
    StoredValues[ValueID::XP].read();
    return StoredValues[ValueID::XP].value;
}

std::int32_t Social::getLevel()
{
    StoredValues[ValueID::XP].read();
    return getLevelFromXP(StoredValues[ValueID::XP].value);
}

std::int32_t Social::getClubLevel()
{
    //check player level and return increased distance
    auto level = getLevel();

    if (level > 29)
    {
        return 2;
    }

    if (level > 14)
    {
        return 1;
    }

    return 0;
}

Social::ProgressData Social::getLevelProgress()
{
    StoredValues[ValueID::XP].read();
    auto currXP = StoredValues[ValueID::XP].value;
    auto currLevel = getLevelFromXP(currXP);

    auto startXP = getXPFromLevel(currLevel);
    auto endXP = getXPFromLevel(currLevel + 1);

    return ProgressData(currXP - startXP, endXP - startXP);
}

std::uint32_t Social::getCurrentStreak()
{
    StoredValues[ValueID::DayStreak].read();
    return StoredValues[ValueID::DayStreak].value;
}

std::uint32_t Social::updateStreak()
{
    StoredValues[ValueID::LastLog].read();
    std::int32_t buff = StoredValues[ValueID::LastLog].value;

    std::uint32_t ts = static_cast<std::uint32_t>(cro::SysTime::epoch());

    if (buff == 0)
    {
        //first time log
        StoredValues[ValueID::LastLog].value = static_cast<std::int32_t>(ts);
        StoredValues[ValueID::LastLog].write();
        StoredValues[ValueID::DayStreak].value = 1;
        StoredValues[ValueID::DayStreak].write();
        StoredValues[ValueID::LongestStreak].value = 1;
        StoredValues[ValueID::LongestStreak].write();

        return 1;
    }

    const std::uint32_t prevTs = static_cast<std::uint32_t>(buff);
    const auto diff = ts - prevTs;

    static constexpr std::uint32_t Day = 24 * 60 * 60;
    auto dayCount = diff / Day;

    bool sunday = false;

    if (dayCount == 0)
    {
        //do a calendar check to see if it's the next day
        std::time_t p = prevTs;
        std::time_t c = ts;

        //we have to copy the results else we just get 2 pointers to the same thing.
        auto prevTm = *std::localtime(&p);
        auto currTm = *std::localtime(&c);

        if ((currTm.tm_yday == 1 //fudge for year wrap around. There are more elegant ways, but brain.
            && prevTm.tm_yday == 365)
            || (currTm.tm_yday - prevTm.tm_yday) == 1)
        {
            dayCount = 1;
            sunday = currTm.tm_wday == 0;
        }
        else
        {
            //already logged in today, so return
            return 0;
        }
    }


    //this is a fresh log so store the time stamp
    StoredValues[ValueID::LastLog].value =  static_cast<std::int32_t>(ts);
    StoredValues[ValueID::LastLog].write();

    //check the streak and increment or reset it
    StoredValues[ValueID::DayStreak].read();
    buff = StoredValues[ValueID::DayStreak].value;

    std::uint32_t streak = static_cast<std::uint32_t>(buff);
    if (dayCount == 1)
    {
        streak++;

        StoredValues[ValueID::LongestStreak].read();
        if (streak > StoredValues[ValueID::LongestStreak].value)
        {
            StoredValues[ValueID::LongestStreak].value = static_cast<std::int32_t>(streak);
            StoredValues[ValueID::LongestStreak].write();
        }
    }
    else
    {
        streak = 1;
    }
    StoredValues[ValueID::DayStreak].value = static_cast<std::int32_t>(streak);
    StoredValues[ValueID::DayStreak].write();

    auto ret = streak % 7;
    if (ret == 0)
    {
        ret = 7;
    }

    switch (streak)
    {
    default: break;
    case 7:
        Achievements::awardAchievement(AchievementStrings[AchievementID::Dedicated]);
        break;
    case 28:
        Achievements::awardAchievement(AchievementStrings[AchievementID::ResidentGolfer]);
        break;
    //case 210: //now actually awarded for playing 30 different sundays
    //    Achievements::awardAchievement(AchievementStrings[AchievementID::MonthOfSundays]);
    //    break;
    }

    //as we retroactively changed the MoS achievement, count any Sundays
    //which may be in the current run and add them to the stat if it's 0
    if (Achievements::getStat(StatStrings[StatID::SundaysPlayed])->value == 0)
    {
        auto sundayStreak = streak;
        if (sunday)
        {
            //we're going to count today below
            sundayStreak--;
        }

        auto sundayCount = static_cast<std::int32_t>(sundayStreak) / 7;
        Achievements::incrementStat(StatStrings[StatID::SundaysPlayed], sundayCount);
    }

    //and increment stat if today is Sunday (we should have already exited this func
    //if this isn't the first time today we checked the streak, above)
    if (sunday)
    {
        Achievements::incrementStat(StatStrings[StatID::SundaysPlayed]);
    }

    return ret;
}

std::uint32_t Social::getLongestStreak()
{
    StoredValues[ValueID::LongestStreak].read();
    return StoredValues[ValueID::LongestStreak].value;
}

void Social::resetProfile()
{
    for (auto& v : StoredValues)
    {
        v.value = 0;
        v.write();
    }
}

void Social::storeDrivingStats(const std::array<float, 3u>& topScores)
{
    const std::string savePath = cro::App::getInstance().getPreferencePath() + "driving.scores";

    cro::ConfigFile cfg;
    cfg.addProperty("five").setValue(topScores[0]);
    cfg.addProperty("nine").setValue(topScores[1]);
    cfg.addProperty("eighteen").setValue(topScores[2]);
    cfg.save(savePath);
}

void Social::readDrivingStats(std::array<float, 3u>& topScores)
{
    const std::string loadPath = cro::App::getInstance().getPreferencePath() + "driving.scores";

    cro::ConfigFile cfg;
    if (cfg.loadFromFile(loadPath, false))
    {
        const auto& props = cfg.getProperties();
        for (const auto& p : props)
        {
            if (p.getName() == "five")
            {
                topScores[0] = std::min(100.f, std::max(0.f, p.getValue<float>()));
            }
            else if (p.getName() == "nine")
            {
                topScores[1] = std::min(100.f, std::max(0.f, p.getValue<float>()));
            }
            else if (p.getName() == "eighteen")
            {
                topScores[2] = std::min(100.f, std::max(0.f, p.getValue<float>()));
            }
        }
    }
}

void Social::courseComplete(const std::string& mapName, std::uint8_t holeCount)
{
    //holecount is 0,1,2 for all, front, back 9
    if (holeCount == 0)
    {
        Achievements::incrementStat(mapName);
    }
}

std::vector<std::byte> Social::setStatus(std::int32_t statusType, const std::vector<const char*>& strings)
{
    static constexpr std::byte PacketID = std::byte(127);
    std::vector<std::byte> ret;

    switch (statusType)
    {
    default: break;
    case InfoID::Billiards:
    {
        cro::String str("Playing ");
        str += cro::String(strings[0]);

        const auto utf = str.toUtf8();
        ret.resize(utf.size() + 1);
        ret[0] = PacketID;
        std::memcpy(ret.data()+1, utf.data(), utf.size());
    }
    return ret;
    case InfoID::Course:
    {
        cro::String str("Hole ");
        str += strings[1];
        str += "/";
        str += strings[2];
        str += " - ";

        std::string s(strings[0]);
        str += cro::String::fromUtf8(s.begin(), s.end());

        const auto utf = str.toUtf8();
        ret.resize(utf.size() + 1);
        ret[0] = PacketID;
        std::memcpy(ret.data() + 1, utf.data(), utf.size());
    }
    return ret;
    case InfoID::Lobby:
    {
        cro::String str("In ");
        str += strings[0];
        str += " Lobby ";
        str += strings[1];
        str += "/";
        str += strings[2];

        const auto utf = str.toUtf8();
        ret.resize(utf.size() + 1);
        ret[0] = PacketID;
        std::memcpy(ret.data() + 1, utf.data(), utf.size());
    }
    return ret;
    case InfoID::Menu:
    {
        cro::String str(strings[0]);

        const auto utf = str.toUtf8();
        ret.resize(utf.size() + 1);
        ret[0] = PacketID;
        std::memcpy(ret.data() + 1, utf.data(), utf.size());
    }
    return ret;
    }

    return ret;
}

std::int32_t Social::getUnlockStatus(UnlockType type)
{
    switch (type)
    {
    default: return 0;
    case UnlockType::Club:
        StoredValues[ValueID::Clubset].read();
        if (StoredValues[ValueID::Clubset].value == 0)
        {
            return DefaultClubSet; //default set flags
        }
        return StoredValues[ValueID::Clubset].value;
    case UnlockType::Ball:
        StoredValues[ValueID::Ballset].read();
        return StoredValues[ValueID::Ballset].value;
    case UnlockType::Level:
        StoredValues[ValueID::Level].read();
        return StoredValues[ValueID::Level].value;
    case UnlockType::Generic:
        StoredValues[ValueID::Generic].read();
        return StoredValues[ValueID::Generic].value;
    case UnlockType::CareerAvatar:
        StoredValues[ValueID::CareerAvatar].read();
        return StoredValues[ValueID::CareerAvatar].value;
    case UnlockType::CareerBalls:
        StoredValues[ValueID::CareerBalls].read();
        return StoredValues[ValueID::CareerBalls].value;
    case UnlockType::CareerHair:
        StoredValues[ValueID::CareerHair].read();
        return StoredValues[ValueID::CareerHair].value;
    case UnlockType::CareerPosition:
        StoredValues[ValueID::CareerPosition].read();
        return StoredValues[ValueID::CareerPosition].value;
    case UnlockType::Tournament:
        StoredValues[ValueID::Tournament].read();
        return StoredValues[ValueID::Tournament].value;
    }
}

void Social::setUnlockStatus(UnlockType type, std::int32_t set)
{
    switch (type)
    {
    default: break;
    case UnlockType::Ball:
        StoredValues[ValueID::Ballset].value = set;
        StoredValues[ValueID::Ballset].write();
        break;
    case UnlockType::Club:
        StoredValues[ValueID::Clubset].value = set;
        StoredValues[ValueID::Clubset].write();
        break;
    case UnlockType::Level:
        StoredValues[ValueID::Level].value = set;
        StoredValues[ValueID::Level].write();
        break;
    case UnlockType::Generic:
        StoredValues[ValueID::Generic].value = set;
        StoredValues[ValueID::Generic].write();
        break;
    case UnlockType::CareerAvatar:
        StoredValues[ValueID::CareerAvatar].value = set;
        StoredValues[ValueID::CareerAvatar].write();
        break;
    case UnlockType::CareerBalls:
        StoredValues[ValueID::CareerBalls].value = set;
        StoredValues[ValueID::CareerBalls].write();
        break;
    case UnlockType::CareerHair:
        StoredValues[ValueID::CareerHair].value = set;
        StoredValues[ValueID::CareerHair].write();
        break;
    case UnlockType::CareerPosition:
        StoredValues[ValueID::CareerPosition].value = set;
        StoredValues[ValueID::CareerPosition].write();
        break;
    case UnlockType::Tournament:
        StoredValues[ValueID::Tournament].value = set;
        StoredValues[ValueID::Tournament].write();
        break;
    }    
}

std::string Social::getBaseContentPath()
{
    return cro::App::getPreferencePath() + "user/1234/";
}

std::string Social::getUserContentPath(std::int32_t contentType)
{
    switch (contentType)
    {
    default:
        assert(false);
        return "";
    case Social::UserContent::Ball:
        return getBaseContentPath() + "balls/";
    case Social::UserContent::Course:
        return getBaseContentPath() + "course/";
    case Social::UserContent::Hair:
        return getBaseContentPath() + "hair/";
    case Social::UserContent::Profile:
        return getBaseContentPath() + "profiles/";
    case Social::UserContent::Career:
        return getBaseContentPath() + "career/";
    case Social::UserContent::Avatar:
        return getBaseContentPath() + "avatars/";
    }
}

float Social::getCompletionCount(const std::string& course, bool)
{
    return Achievements::getStat(course)->value;
    //return std::max(1.f, Achievements::getStat(/*StatStrings[StatID::Course01Complete]*/course)->value);
}

void Social::refreshAwards()
{
    struct AwardData final
    {
        std::uint32_t timestamp = 0; //month|year
        std::int32_t type = -1; //Social::Award::Type
    };
    std::vector<AwardData> awardData;

    auto path = Social::getBaseContentPath() + "awards.awd";

    //check for awards file and load
    if (cro::FileSystem::fileExists(path))
    {
        cro::RaiiRWops file;
        file.file = SDL_RWFromFile(path.c_str(), "rb");
        if (file.file)
        {
            auto size = SDL_RWseek(file.file, 0, RW_SEEK_END);
            if (size % sizeof(AwardData) == 0)
            {
                SDL_RWseek(file.file, 0, RW_SEEK_SET);
                awardData.resize(size / sizeof(AwardData));
                SDL_RWread(file.file, awardData.data(), size, 1);
            }
        }
    }

    //check for anything new and add
    bool newAwards = false;
    auto ts = static_cast<std::time_t>( cro::SysTime::epoch());
    auto* td = std::localtime(&ts);
    
    const std::uint32_t awardTime = (td->tm_mon << 16) | td->tm_year;

    auto challengeProgress = getMonthlyChallenge().getProgress();
    if (challengeProgress.value == challengeProgress.target)
    {
        //see if it exists in the data already
        if (auto result = std::find_if(awardData.begin(), awardData.end(), [awardTime](const AwardData& ad)
            {
                return ad.type == Social::Award::MonthlyChallenge && ad.timestamp == awardTime;
            }); result == awardData.end())
        {
            auto& award = awardData.emplace_back();
            award.timestamp = awardTime;
            award.type = Social::Award::MonthlyChallenge;

            newAwards = true;
        }
    }

    auto level = getLevel() / 10;
    for (auto i = 0; i < level; ++i)
    {
        auto type = Social::Award::Level10 + i;
        if (auto result = std::find_if(awardData.begin(), awardData.end(), [type](const AwardData& ad)
            {
                return ad.type == type;
            }); result == awardData.end())
        {
            awardData.emplace_back().type = type;
            newAwards = true;
        }
    }

    //check for league awards
    for (auto i = 0; i < 3; ++i)
    {
        if (Achievements::getStat(StatStrings[StatID::LeagueFirst + i])->value != 0)
        {
            auto type = Social::Award::LeagueFirst + i;
            if (auto result = std::find_if(awardData.begin(), awardData.end(), [type](const AwardData& ad)
                {
                    return ad.type == type;
                }); result == awardData.end())
            {
                awardData.emplace_back().type = type;
                newAwards = true;
            }
        }
    }


    //if list updated write file
    if (newAwards)
    {
        cro::RaiiRWops file;
        file.file = SDL_RWFromFile(path.c_str(), "wb");
        if (file.file)
        {
            SDL_RWwrite(file.file, awardData.data(), sizeof(AwardData), awardData.size());
        }
    }


    //and finally convert to readable version
    awards.clear();
    for (const auto& a : awardData)
    {
        switch (a.type)
        {
        case Social::Award::MonthlyBronze:
        case Social::Award::MonthlySilver:
        case Social::Award::MonthlyGold:
        default: continue;
        case Social::Award::MonthlyChallenge:
        {
            auto& award = awards.emplace_back();
            award.type = a.type;
            
            auto month = a.timestamp >> 16;
            auto year = a.timestamp & 0x00ff;

            cro::String str("Monthly Challenge ");
            str += MonthStrings[std::min(11u, std::max(0u, month))];
            str += std::to_string(1900 + year);
            award.description = str;
        }
            break;
        case Social::Award::Level10:
        {
            auto& award = awards.emplace_back();
            award.type = a.type;
            award.description = "Reached Level 10";
        }
            break;
        case Social::Award::Level20:
        {
            auto& award = awards.emplace_back();
            award.type = a.type;
            award.description = "Reached Level 20";
        }
            break;
        case Social::Award::Level30:
        {
            auto& award = awards.emplace_back();
            award.type = a.type;
            award.description = "Reached Level 30";
        }
            break;
        case Social::Award::Level40:
        {
            auto& award = awards.emplace_back();
            award.type = a.type;
            award.description = "Reached Level 40";
        }
            break;
        case Social::Award::Level50:
        {
            auto& award = awards.emplace_back();
            award.type = a.type;
            award.description = "Reached Level 50";
        }
            break;
        case Social::Award::LeagueFirst:
        {
            auto& award = awards.emplace_back();
            award.type = a.type;
            award.description = "Placed First in a\nClub League Season";
        }
        break;
        case Social::Award::LeagueSecond:
        {
            auto& award = awards.emplace_back();
            award.type = a.type;
            award.description = "Placed Second in a\nClub League Season";
        }
        break;
        case Social::Award::LeagueThird:
        {
            auto& award = awards.emplace_back();
            award.type = a.type;
            award.description = "Placed Third in a\nClub League Season";
        }
        break;
        }
    }
}

const std::vector<Social::Award>& Social::getAwards()
{
    return awards;
}

MonthlyChallenge& Social::getMonthlyChallenge()
{
    static MonthlyChallenge mc;
    return mc;
}

std::int32_t Social::getMonth()
{
    return cro::SysTime::now().months() - 1; //we're using this as an index
}

void Social::setScrubScore(std::int32_t score)
{
    scrubScore.read();
    if (score > scrubScore.value)
    {
        scrubScore.value = score;
        scrubScore.write();

        refreshScrubScore();
    }
}

void Social::refreshScrubScore()
{
    scrubScore.read();
    if (scrubScore.value != 0)
    {
        scrubString = "Personal Best: " + std::to_string(scrubScore.value);
    }
    else
    {
        scrubString = "No Score Yet.";
    }

    //lets the game know to refresh UI
    cro::App::postMessage<Social::StatEvent>(Social::MessageID::StatsMessage)->type = Social::StatEvent::ScrubScoresReceived;
}

const cro::String& Social::getScrubScores()
{
    return scrubString;
}

std::int32_t Social::getScrubPB()
{
    return scrubScore.value;
}

void Social::takeScreenshot(const cro::String&, std::size_t courseIndex)
{
    cro::App::getInstance().saveScreenshot();

    snapperFlags.read();
    auto v = snapperFlags.value;
    v |= (1 << courseIndex);
    snapperFlags.value = v;
    snapperFlags.write();

    if (v == 0x0fff)
    {
        Achievements::awardAchievement(AchievementStrings[AchievementID::HappySnapper]);
    }
}

void Social::insertScore(const std::string& course, std::uint8_t hole, std::int32_t score, std::int32_t)
{
#ifdef USE_GJS
    //GJ::insertScore(course, hole, score);
#endif
}

cro::String Social::getTopFive(const std::string& course, std::uint8_t holeCount)
{
#ifdef USE_GJS
    //return GJ::getTopFive(course, holeCount);
    return {};
#else
    return {};
#endif
}

void Social::invalidateTopFive(const std::string& course, std::uint8_t holeCount)
{
#ifdef USE_GJS
    //GJ::invalidateTopFive(course, holeCount);
#endif
}

void Social::readAllStats()
{
    for (auto& v : StoredValues)
    {
        v.read();
    }
}