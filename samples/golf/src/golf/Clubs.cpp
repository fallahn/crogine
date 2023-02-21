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

Club::Club(std::int32_t id, const std::string& name, float power, float angle, float target)
    : m_id  (id), 
    m_name  (name), 
    m_power (power), 
    m_angle (angle * cro::Util::Const::degToRad), 
    m_target(target)
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
        return getScaledValue(m_power, distanceToPin);
    }

    //check player level and return further distance
    return getExperiencedValue(m_power);
}

float Club::getTarget(float distanceToPin) const
{
    if (m_id == ClubID::Putter)
    {
        return getScaledValue(m_target, distanceToPin);
    }

    //check player level and return increased distance
    return getExperiencedValue(m_target);
}

//private
float Club::getScaledValue(float value, float distanceToPin) const
{
    if (distanceToPin < m_target * ShortRangeThreshold)
    {
        if (distanceToPin < m_target * TinyRangeThreshold)
        {
            return value * TinyRange;
        }

        return value * ShortRange;
    }
    return value;
}

float Club::getExperiencedValue(float value) const
{
    auto level = Social::getLevel();
    if (level > 29)
    {
        switch (m_id)
        {
        default: return value;
        case ClubID::Driver:
        case ClubID::ThreeWood:
        case ClubID::FiveWood:
            return value * 1.27f;
        case ClubID::FourIron:
        case ClubID::FiveIron:
        case ClubID::SixIron:
            return value * 1.15f;
        case ClubID::SevenIron:
        case ClubID::EightIron:
        case ClubID::NineIron:
            return value * 1.4f;
        }
    }

    if (level > 14)
    {
        switch (m_id)
        {
        default: return value;
        case ClubID::Driver:
        case ClubID::ThreeWood:
        case ClubID::FiveWood:
            return value * 1.15f;
        case ClubID::FourIron:
        case ClubID::FiveIron:
        case ClubID::SixIron:
            return value * 1.07f;
        case ClubID::SevenIron:
        case ClubID::EightIron:
        case ClubID::NineIron:
            return value * 1.3f;
        }
    }

    return value;
}