/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2024
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

#include <crogine/detail/Assert.hpp>

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

    //constexpr std::array<ClubStat, ClubID::Count> ClubStats =
    //{
    //    ClubStat({44.f,   220.f},{46.18f, 240.f},{48.2f,  260.f}), //123
    //    ClubStat({39.6f,  180.f},{42.08f, 200.f},{44.24f, 220.f}), //123
    //    ClubStat({36.3f,  150.f},{37.82f, 160.f},{39.66f, 180.f}), //123

    //    ClubStat({35.44f, 140.f},{36.49f, 150.f},{37.69f, 160.f}), //123
    //    ClubStat({34.16f, 130.f},{35.44f, 140.f},{36.49f, 150.f}), //123
    //    ClubStat({32.85f, 120.f},{34.16f, 130.f},{35.44f, 140.f}), //123
    //    ClubStat({31.33f, 110.f},{32.85f, 120.f},{34.16f, 130.f}), //123
    //    ClubStat({29.91f, 100.f},{31.33f, 110.f},{32.85f, 120.f}), //123
    //    ClubStat({28.4f,  90.f}, {29.91f, 100.f},{31.33f, 110.f}), //123

    //    
    //    ClubStat({25.2f, 70.f}, {25.98f, 75.f}, {27.46f, 80.f}),
    //    ClubStat({17.4f, 30.f}, {18.53f, 32.f}, {19.14f, 35.f}),
    //    //this won't increase in range
    //    ClubStat({10.3f, 10.f}, {10.3f, 10.f}, {10.3f, 10.f}),
    //    ClubStat({9.11f, 10.f}, {9.11f, 10.f}, {9.11f, 10.f}) //except this which is dynamic
    //};

    constexpr std::array<ClubStat, ClubID::Count> ClubStats =
    {
        ClubStat({ 47.199f, 220.000f }, { 49.378f, 240.000f }, { 51.398f, 260.000f }),
        ClubStat({ 41.344f, 180.000f }, { 43.534f, 200.000f }, { 45.694f, 220.000f }),
        ClubStat({ 37.172f, 150.000f }, { 38.402f, 160.000f }, { 40.533f, 180.000f }),
        ClubStat({ 35.149f, 140.000f }, { 36.490f, 150.000f }, { 37.690f, 160.000f }),
        ClubStat({ 33.869f, 130.000f }, { 35.440f, 140.000f }, { 36.490f, 150.000f }),
        ClubStat({ 32.850f, 120.000f }, { 34.160f, 130.000f }, { 35.440f, 140.000f }),
        ClubStat({ 31.330f, 110.000f }, { 32.850f, 120.000f }, { 34.160f, 130.000f }),
        ClubStat({ 29.910f, 100.000f }, { 31.330f, 110.000f }, { 32.850f, 120.000f }),
        ClubStat({ 28.400f, 90.000f }, { 29.910f, 100.000f }, { 31.621f, 110.000f }),
        ClubStat({ 25.491f, 70.000f }, { 26.562f, 75.000f }, { 27.460f, 80.000f }),
        ClubStat({ 17.691f, 30.000f }, { 18.239f, 32.000f }, { 19.140f, 35.000f }),
        ClubStat({ 10.300f, 10.000f }, { 10.300f, 10.000f }, { 10.300f, 10.000f }),
        ClubStat({ 9.110f, 10.000f }, { 9.110f, 10.000f }, { 9.110f, 10.000f }),
    };

    static constexpr float ToYards = 1.094f;
    static constexpr float ToFeet = 3.281f;
    //static constexpr float ToInches = 12.f;


    constexpr std::size_t DebugLevel = 35;
    std::int32_t playerLevel = 0;
}

Club::Club(std::int32_t id, const std::string& name, float angle, float sidespin, float topspin)
    : m_id      (id), 
    m_name      (name), 
    m_angle     (angle * cro::Util::Const::degToRad),
    m_sidespin  (sidespin),
    m_topspin   (topspin)
{

}

//public
std::string Club::getName(bool imperial, float distanceToPin) const
{
    auto t = getTarget(distanceToPin);
    
    if (imperial)
    {
        if (getPower(distanceToPin, imperial) > 10.f)
        {
            auto dist = static_cast<std::int32_t>(t * ToYards);
            return m_name + std::to_string(dist) + "yds";
        }
        else
        {
            auto dist = static_cast<std::int32_t>(std::round(t * ToFeet));
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

std::string Club::getDistanceLabel(bool imperial, std::int32_t level) const
{
    CRO_ASSERT(level > -1 && level < 3, "");

    auto t = getTargetAtLevel(level);
    if (imperial)
    {
        if (getPower(/*distanceToPin*/10.f, imperial) > 10.f)
        {
            auto dist = static_cast<std::int32_t>(t * ToYards);
            return std::to_string(dist) + "y";
        }
        else
        {
            auto dist = static_cast<std::int32_t>(std::round(t * ToFeet));
            return std::to_string(dist) + "\'";
        }
    }
    else
    {
        if (t < 1.f)
        {
            t *= 100.f;
            auto dist = static_cast<std::int32_t>(t);
            return std::to_string(dist) + "cm";
        }
        auto dist = static_cast<std::int32_t>(t);
        return std::to_string(dist) + "m";
    }
}

float Club::getPower(float distanceToPin, bool imperial) const
{
    if (m_id == ClubID::Putter)
    {
        //looks like a bug, but turns out we need the extra power.
        auto p = getScaledValue(ClubStats[m_id].stats[0].target, distanceToPin);
        //return getScaledValue(ClubStats[m_id].stats[0].power, distanceToPin);

        //because imperial display is rounded we need to apply this to the power too
        //if (imperial
        //    && p < 10.f) //only diplayed in feet < 10m
        //{
        //    p = std::round(p * ToFeet);
        //    p /= ToFeet; //still need to actually physic in metres
        //}

        return p;
    }

    //check player level and return further distance
    return ClubStats[m_id].stats[getClubLevel()].power;
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
    return ClubStats[m_id].stats[getClubLevel()].target;
}

float Club::getTargetAtLevel(std::int32_t level) const
{
    return ClubStats[m_id].stats[level].target;
}

float Club::getDefaultTarget() const
{
    return ClubStats[m_id].stats[0].target;
}

std::int32_t Club::getClubLevel()
{
    CRO_ASSERT(playerLevel > -1 && playerLevel < 3, "");

    return playerLevel;
}

void Club::setClubLevel(std::int32_t level)
{
    playerLevel = level;
    CRO_ASSERT(playerLevel > -1 && playerLevel < 3, "");
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