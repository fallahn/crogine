/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2022
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

#pragma once

#include <string>

struct StatData final
{
    StatData() : id(-1), intValue(0) {};
    StatData(const std::string& str, std::int32_t i, std::int32_t v) : name(str), id(i), intValue(v) {}

    enum Type
    {
        Integer, Float
    }type = Integer;
    std::string name;
    std::int32_t id;

    union
    {
        float floatValue;
        std::int32_t intValue;
    };
};

struct AchievementData final
{
    AchievementData() {}
    AchievementData(const std::string& str, std::int32_t idx = -1, bool b = false)
        :name(str), achieved(b), id(idx) {}
    std::string name;
    bool achieved = false;
    std::int32_t id = -1;
};

//base class for different implementations
class AchievementImpl
{
public:
    virtual ~AchievementImpl() {};

    virtual void init() {};
    virtual void update() {};

    virtual void registerAchievement(const std::string&) {}
    virtual void awardAchievement(const std::string&) = 0;

    virtual const AchievementData* getAchievement(const std::string&) const { return nullptr; }

    virtual void setStat(const std::string&, float) {}
    virtual void setStat(const std::string&, std::int32_t) {}
    virtual float incrementStat(const std::string&, std::int32_t = 1) { return 0.f; }

    virtual const StatData* getStat(const std::string&) const { return nullptr; }
};

//public facing interface
class Achievements final
{
public:

    static void init(AchievementImpl&);

    static void update();

    static void registerAchievement(const std::string&);

    static void awardAchievement(const std::string&);

    static const AchievementData* getAchievement(const std::string&);

    static void setStat(const std::string&, float);

    static void setStat(const std::string&, std::int32_t);

    static float incrementStat(const std::string&, std::int32_t = 1);

    static const StatData* getStat(const std::string&);

private:
    static AchievementImpl* m_impl;
};
