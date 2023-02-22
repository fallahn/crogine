/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
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

#include "Clubs.hpp"
#include <Social.hpp>

namespace
{
    struct Stat final
    {
        constexpr Stat() = default;
        constexpr Stat(float p, float t) : power(p), target(t) {}
        float power = 0.f; //impulse strength
        float target = 0.f; //distance target
    };

    struct ClubStat final
    {
        constexpr ClubStat(Stat a, Stat b, Stat c)
            : stats() { stats = { a, b, c }; }
        std::array<Stat, 3> stats = {};
    };

    constexpr std::array<ClubStat, ClubID::Count> ClubStats =
    {
        ClubStat({46.32f, 220.f},{46.32f, 250.f},{46.32f, 280.f}),
        ClubStat({41.8f,  180.f},{46.32f, 210.f},{46.32f, 230.f}),
        ClubStat({38.15f, 150.f},{46.32f, 175.f},{46.32f, 190.f}),
        ClubStat({37.11f, 140.f},{46.32f, 150.f},{46.32f, 160.f}),
        ClubStat({35.82f, 130.f},{46.32f, 140.f},{46.32f, 150.f}),
        ClubStat({34.4f,  120.f},{46.32f, 130.f},{46.32f, 140.f}),
        ClubStat({32.98f, 110.f},{46.32f, 120.f},{46.32f, 130.f}),
        ClubStat({31.5f,  100.f},{46.32f, 110.f},{46.32f, 120.f}),
        ClubStat({29.9f,  90.f}, {46.32f, 100.f},{46.32f, 110.f}),

        //these don't increase in range
        ClubStat({26.51f, 70.f}, {26.51f, 70.f}, {26.51f, 70.f}),
        ClubStat({18.4f,  30.f}, {18.4f,  30.f}, {18.4f,  30.f}),
        ClubStat({10.65f, 10.f}, {10.65f, 10.f}, {10.65f, 10.f}),
        ClubStat({9.11f,  10.f}, {9.11f,  10.f}, {9.11f,  10.f}) //except this which is dynamic
    };
}

Club::Club(std::int32_t id, const std::string& name, float angle)
    : m_id  (id), 
    m_name  (name), 
    m_angle (angle * cro::Util::Const::degToRad)
{

}

//public
std::string Club::getName(bool imperial, float distanceToPin) const
{
    auto t = getTarget(distanceToPin);
    
    if (imperial)
    {
        static constexpr float ToYards = 1.094f;
        static constexpr float ToFeet = 3.281f;
        //static constexpr float ToInches = 12.f;

        if (getPower(distanceToPin) > 10.f)
        {
            auto dist = static_cast<std::int32_t>(t * ToYards);
            return m_name + std::to_string(dist) + "yds";
        }
        else
        {
            auto dist = static_cast<std::int32_t>(t * ToFeet);
            return m_name + std::to_string(dist) + "ft";
        }
    }
    else
    {
        if (t < 1.f)
        {
            t *= 100.f;
            auto dist = static_cast<std::int32_t>(t);
            return m_name + std::to_string(dist) + "cm";
        }
        auto dist = static_cast<std::int32_t>(t);
        return m_name + std::to_string(dist) + "m";
    }
}

float Club::getPower(float distanceToPin) const
{
    if (m_id == ClubID::Putter)
    {
        return getScaledValue(ClubStats[m_id].stats[0].target, distanceToPin);
    }

    //check player level and return further distance
    auto level = Social::getLevel();
    if (level > 29)
    {
        ClubStats[m_id].stats[2].power;
    }

    if (level > 14)
    {
        ClubStats[m_id].stats[1].power;
    }
    return ClubStats[m_id].stats[0].power;
}

float Club::getTarget(float distanceToPin) const
{
    if (m_id == ClubID::Putter)
    {
        return getScaledValue(ClubStats[m_id].stats[0].target, distanceToPin);
    }

    return getBaseTarget();
}

float Club::getBaseTarget() const
{
    //check player level and return increased distance
    auto level = Social::getLevel();
    if (level > 29)
    {
        ClubStats[m_id].stats[2].target;
    }

    if (level > 14)
    {
        ClubStats[m_id].stats[1].target;
    }
    return ClubStats[m_id].stats[0].target;
}

//private
float Club::getScaledValue(float value, float distanceToPin) const
{
    auto target = getBaseTarget();
    if (distanceToPin < target * ShortRangeThreshold)
    {
        if (distanceToPin < target * TinyRangeThreshold)
        {
            return value * TinyRange;
        }

        return value * ShortRange;
    }
    return value;
}