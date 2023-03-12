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

#include <crogine/core/App.hpp>
#include <crogine/core/ConfigFile.hpp>

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
        void read(const std::string& ext)
        {
            if (!loaded)
            {
                readValue(value, ext);
                loaded = true;
            }
        }
        void write(const std::string& ext)
        {
            writeValue(value, ext);
        }
    };
    StoredValue experience;
    StoredValue clubset;
    StoredValue ballset;
    StoredValue levelTrophies;
    StoredValue genericUnlock;


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
        experience.write("exp");

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
    experience.read("exp");
    return experience.value;
}

std::int32_t Social::getLevel()
{
    experience.read("exp");
    return getLevelFromXP(experience.value);
}

Social::ProgressData Social::getLevelProgress()
{
    experience.read("exp");
    auto currXP = experience.value;
    auto currLevel = getLevelFromXP(currXP);

    auto startXP = getXPFromLevel(currLevel);
    auto endXP = getXPFromLevel(currLevel + 1);

    return ProgressData(currXP - startXP, endXP - startXP);
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
        clubset.read("clb");
        if (clubset.value == 0)
        {
            return 3731; //default set flags
        }
        return clubset.value;
    case UnlockType::Ball:
        ballset.read("bls");
        return ballset.value;
    case UnlockType::Level:
        levelTrophies.read("lvl");
        return levelTrophies.value;
    case UnlockType::Generic:
        genericUnlock.read("gnc");
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
        ballset.write("bls");
        break;
    case UnlockType::Club:
        clubset.value = set;
        clubset.write("clb");
        break;
    case UnlockType::Level:
        levelTrophies.value = set;
        levelTrophies.write("lvl");
        break;
    case UnlockType::Generic:
        genericUnlock.value = set;
        genericUnlock.write("gnc");
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
    }
}