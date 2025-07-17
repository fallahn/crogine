/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2023
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

#pragma once

#include <crogine/graphics/Rectangle.hpp>

#include <string>

namespace cro
{
    class Texture;
}

struct StatData final
{
    StatData() : id(-1), value(0.f) {};
    StatData(const std::string& str, std::int32_t i, float v) : name(str), id(i), value(v) {}

    std::string name;
    std::int32_t id;

    float value = 0.f;
};

struct AchievementData final
{
    AchievementData() {}
    AchievementData(const std::string& str, std::int32_t idx = -1, bool b = false, std::uint64_t ts = 0)
        : name(str), achieved(b), id(idx), timestamp(ts) {}
    std::string name;
    bool achieved = false;
    std::int32_t id = -1;
    std::uint64_t timestamp = 0;
};

struct AchievementImage final
{
    const cro::Texture* texture = nullptr;
    cro::FloatRect textureRect;
};

class MonthlyChallenge;

//base class for different implementations
class AchievementImpl
{
public:
    virtual ~AchievementImpl() {};

    virtual bool init() { return true; };
    virtual void update() {};
    virtual void shutdown() {};

    virtual void awardAchievement(const std::string&) = 0;

    virtual const AchievementData* getAchievement(const std::string&) const { return nullptr; }
    virtual AchievementImage getIcon(const std::string&) const = 0;

    virtual void setStat(const std::string&, float) {}
    virtual void setStat(const std::string&, std::int32_t) {}
    virtual float incrementStat(const std::string&, std::int32_t = 1) { return 0.f; }
    virtual float incrementStat(const std::string&, float) { return 0.f; }

    virtual const StatData* getStat(const std::string&) const { return nullptr; }

    virtual void setAvgStat(const std::string&, float v, float d) const = 0;
    virtual float getAvgStat(const std::string&) const = 0;
};

//public facing interface
class Achievements final
{
public:

    static bool init(AchievementImpl&);

    static void update();

    static void shutdown();

    static void awardAchievement(const std::string&);

    static const AchievementData* getAchievement(const std::string&);

    static AchievementImage getIcon(const std::string&);

    static void setStat(const std::string&, float);

    static void setStat(const std::string&, std::int32_t);

    static float incrementStat(const std::string&, std::int32_t = 1);

    static float incrementStat(const std::string&, float);

    static const StatData* getStat(const std::string&);

    static const StatData* getGlobalStat(const std::string&) { return nullptr; }

    static void setAvgStat(const std::string&, float v, float duration);

    static float getAvgStat(const std::string& s) { return getStat(s)->value; }

    static void setActive(bool);

    static bool getActive() { return m_active; }

    static void refreshGlobalStats() {}

    static MonthlyChallenge* getMonthlyChallenge() { return nullptr; }

private:
    static AchievementImpl* m_impl;
    static bool m_active;
};
