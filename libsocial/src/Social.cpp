/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2023
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

#include <crogine/core/App.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/core/SysTime.hpp>

#include <ctime>

namespace
{
    void readValue(std::int32_t& dst, const std::string& fileName)
    {
        auto path = cro::App::getPreferencePath() + fileName;
        if (cro::FileSystem::fileExists(path))
        {
            auto* file = SDL_RWFromFile(path.c_str(), "rb");
            if (file)
            {
                SDL_RWread(file, &dst, sizeof(dst), 1);
            }
            SDL_RWclose(file);
        }
    }

    void writeValue(std::int32_t src, const std::string& fileName)
    {
        auto path = cro::App::getPreferencePath() + fileName;
        auto* file = SDL_RWFromFile(path.c_str(), "wb");
        if (file)
        {
            SDL_RWwrite(file, &src, sizeof(src), 1);
        }
        SDL_RWclose(file);
    }

    struct StoredValue final
    {
        bool loaded = false;
        std::int32_t value = 0;
        const std::string ext;
        void read()
        {
            if (!loaded)
            {
                readValue(value, ext);
                loaded = true;
            }
        }
        void write()
        {
            writeValue(value, ext);
        }

        explicit StoredValue(const std::string& e) :ext(e) {}
    };
    StoredValue experience("exp");
    StoredValue clubset("clb");
    StoredValue ballset("bls");
    StoredValue levelTrophies("lvl");
    StoredValue genericUnlock("gnc");
    StoredValue lastLog("llg");
    StoredValue dayStreak("dsk");
    StoredValue longestStreak("lsk");


    //XP curve = (level / x) ^ y
    constexpr float XPx = 0.07f;
    constexpr float XPy = 2.f;

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
}

void Social::awardXP(std::int32_t amount)
{
    if (Achievements::getActive())
    {
        auto oldLevel = getLevelFromXP(experience.value);

        experience.value += amount;
        experience.write();

        //raise event to notify players
        auto* msg = cro::App::postMessage<SocialEvent>(MessageID::SocialMessage);
        msg->type = SocialEvent::XPAwarded;
        msg->level = amount;

        auto level = getLevelFromXP(experience.value);
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
    experience.read();
    return experience.value;
}

std::int32_t Social::getLevel()
{
    experience.read();
    return getLevelFromXP(experience.value);
}

Social::ProgressData Social::getLevelProgress()
{
    experience.read();
    auto currXP = experience.value;
    auto currLevel = getLevelFromXP(currXP);

    auto startXP = getXPFromLevel(currLevel);
    auto endXP = getXPFromLevel(currLevel + 1);

    return ProgressData(currXP - startXP, endXP - startXP);
}

std::uint32_t Social::getCurrentStreak()
{
    dayStreak.read();
    return dayStreak.value;
}

std::uint32_t Social::updateStreak()
{
    lastLog.read();
    std::int32_t buff = lastLog.value;

    std::uint32_t ts = static_cast<std::uint32_t>( cro::SysTime::epoch());

    if (buff == 0)
    {
        //first time log
        lastLog.value = static_cast<std::int32_t>(ts);
        lastLog.write();
        dayStreak.value = 1;
        dayStreak.write();
        longestStreak.value = 1;
        longestStreak.write();

        return 1;
    }

    const std::uint32_t prevTs = static_cast<std::uint32_t>(buff);
    const auto diff = ts - prevTs;

    static constexpr std::uint32_t Day = 24 * 60 * 60;
    auto dayCount = diff / Day;

    if (dayCount == 0)
    {
        //do a calendar check to see if it's the next day
        std::time_t p = prevTs;
        std::time_t c = ts;

        auto prevTm = std::localtime(&p);
        auto currTm = std::localtime(&c);

        if ((currTm->tm_yday == 1 //fudge for year wrap around. There are more elegant ways, but brain.
            && prevTm->tm_yday == 365)
            || (currTm->tm_yday - prevTm->tm_yday) == 1)
        {
            dayCount = 1;
        }
        else
        {
            //already logged in today, so return
            return 0;
        }
    }


    //this is a fresh log so store the time stamp
    lastLog.value =  static_cast<std::int32_t>(ts);
    lastLog.write();

    //check the streak and increment or reset it
    dayStreak.read();
    buff = dayStreak.value;

    std::uint32_t streak = static_cast<std::uint32_t>(buff);
    if (dayCount == 1)
    {
        streak++;

        longestStreak.read();
        if (streak > longestStreak.value)
        {
            longestStreak.value = static_cast<std::int32_t>(streak);
            longestStreak.write();
        }
    }
    else
    {
        streak = 1;
    }
    dayStreak.value = static_cast<std::int32_t>(streak);
    dayStreak.write();

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
    case 210:
        Achievements::awardAchievement(AchievementStrings[AchievementID::MonthOfSundays]);
        break;
    }

    return ret;
}

std::uint32_t Social::getLongestStreak()
{
    longestStreak.read();
    return longestStreak.value;
}

void Social::resetProfile()
{
    experience.value = 0;
    experience.write();

    ballset.value = 0;
    ballset.write();

    clubset.value = 0;
    clubset.write();
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

std::int32_t Social::getUnlockStatus(UnlockType type)
{
    switch (type)
    {
    default: return 0;
    case UnlockType::Club:
        clubset.read();
        if (clubset.value == 0)
        {
            return 3731; //default set flags
        }
        return clubset.value;
    case UnlockType::Ball:
        ballset.read();
        return ballset.value;
    case UnlockType::Level:
        levelTrophies.read();
        return levelTrophies.value;
    case UnlockType::Generic:
        genericUnlock.read();
        return genericUnlock.value;
    }
}

void Social::setUnlockStatus(UnlockType type, std::int32_t set)
{
    switch (type)
    {
    default: break;
    case UnlockType::Ball:
        ballset.value = set;
        ballset.write();
        break;
    case UnlockType::Club:
        clubset.value = set;
        clubset.write();
        break;
    case UnlockType::Level:
        levelTrophies.value = set;
        levelTrophies.write();
        break;
    case UnlockType::Generic:
        genericUnlock.value = set;
        genericUnlock.write();
        break;
    }    
}

std::string Social::getBaseContentPath()
{
    return cro::App::getPreferencePath() + "user/";
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
    }
}