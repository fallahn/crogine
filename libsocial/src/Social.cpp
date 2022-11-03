#include "Social.hpp"
#include "Achievements.hpp"

#include <crogine/core/App.hpp>
#include <crogine/core/ConfigFile.hpp>

namespace
{
    //TODO make this 'less' hacky with a singleton?

    bool wasLoaded = false;
    std::int32_t experience = 0;
    void readExperience()
    {
        if (!wasLoaded)
        {
            auto path = cro::App::getPreferencePath() + "exp";
            if (cro::FileSystem::fileExists(path))
            {
                auto* file = SDL_RWFromFile(path.c_str(), "rb");
                if (file)
                {
                    SDL_RWread(file, &experience, sizeof(experience), 1);
                }
                SDL_RWclose(file);
            }
        }

        wasLoaded = true;
    }
    void writeExperience()
    {
        auto path = cro::App::getPreferencePath() + "exp";
        auto* file = SDL_RWFromFile(path.c_str(), "wb");
        if (file)
        {
            SDL_RWwrite(file, &experience, sizeof(experience), 1);
        }
        SDL_RWclose(file);
    }


    //XP curve = (level / x) ^ y
    constexpr float XPx = 0.07f;
    constexpr float XPy = 2.f;

    std::int32_t getLevelFromXP(std::int32_t exp)
    {
        float xp = static_cast<float>(exp);
        return static_cast<std::int32_t>(XPx * std::sqrt(xp));
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
        auto oldLevel = experience;

        experience += amount;
        writeExperience();

        //raise event to notify players
        auto* msg = cro::App::postMessage<SocialEvent>(MessageID::SocialMessage);
        msg->type = SocialEvent::XPAwarded;
        msg->level = amount;

        auto level = getLevelFromXP(experience);
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
    readExperience();
    return experience;
}

std::int32_t Social::getLevel()
{
    readExperience();
    return getLevelFromXP(experience);
}

Social::ProgressData Social::getLevelProgress()
{
    readExperience();
    auto currXP = experience;
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