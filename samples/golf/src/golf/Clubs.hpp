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

#pragma once

#include <crogine/util/Constants.hpp>

#include <string>
#include <array>

struct ClubID final
{
    enum
    {
        Driver, ThreeWood,
        FiveIron, NineIron,
        PitchWedge, GapWedge, SandWedge,
        Putter,

        Count
    };
};

struct Club final
{
private:
    const std::int32_t id = -1;
    std::string name; //displayed in UI
    float power = 0.f; //magnitude of impulse applied to reach target
    float angle = 0.f; //pitch of shot (should be positive)
    const float target = 0.f; //the max (approx) distance when hit with 100% power

    //putter below this is rescaled
    static constexpr float ShortRange = 1.f / 5.f;
    static constexpr float TinyRange = 1.f / 20.f;

public:

    Club(std::int32_t i, const std::string& n, float p, float a, float t)
        : id(i), name(n), power(p), angle(a * cro::Util::Const::degToRad), target(t) {}

    std::string getName(bool imperial, float distanceToPin) const
    {
        auto t = target;
        if (id == ClubID::Putter)
        {
            if (distanceToPin < (target * ShortRange))
            {
                if (distanceToPin < (target * TinyRange))
                {
                    t *= TinyRange;
                }
                else
                {
                    t *= ShortRange;
                }
            }
        }

        if (imperial)
        {
            static constexpr float ToYards = 1.094f;
            static constexpr float ToFeet = 3.281f;
            //static constexpr float ToInches = 12.f;

            if (power > 10.f)
            {
                auto dist = static_cast<std::int32_t>(t * ToYards);
                return name + std::to_string(dist) + "yds";
            }
            else
            {
                auto dist = static_cast<std::int32_t>(t * ToFeet);
                return name + std::to_string(dist) + "ft";
            }
        }
        else
        {
            if (t < 1.f)
            {
                t *= 100.f;
                auto dist = static_cast<std::int32_t>(t);
                return name + std::to_string(dist) + "cm";
            }
            auto dist = static_cast<std::int32_t>(t);
            return name + std::to_string(dist) + "m";
        }
    }

    float getPower(float distanceToPin) const
    {
        if (id == ClubID::Putter)
        {
            //ugh this is such a fudge...
            if (distanceToPin < target * ShortRange)
            {
                if (distanceToPin < target * TinyRange)
                {
                    return power * TinyRange;
                }

                return power * ShortRange;
            }
        }
        return power;
    }

    float getAngle() const { return angle; }

    float getTarget(float distanceToPin) const
    {
        if (id == ClubID::Putter)
        {
            if (distanceToPin < target * ShortRange)
            {
                if (distanceToPin < target * TinyRange)
                {
                    return target * TinyRange;
                }

                return target * ShortRange;
            }
        }
        return target;
    }
};

static const std::array<Club, ClubID::Count> Clubs =
{
    Club(ClubID::Driver, "Driver ", 44.f, 45.f, 220.f),
    Club(ClubID::ThreeWood, "3 Wood ", 39.5f, 45.f, 180.f),
    Club(ClubID::FiveIron, "5 Iron ", 35.f, 40.f, 140.f),
    Club(ClubID::NineIron, "9 Iron ", 30.1f, 40.f, 100.f),
    Club(ClubID::PitchWedge, "Pitch Wedge ", 25.2f, 52.f, 70.f),
    Club(ClubID::GapWedge, "Gap Wedge ", 17.4f, 60.f, 30.f),
    Club(ClubID::SandWedge, "Sand Wedge ", 10.3f, 60.f, 10.f),
    Club(ClubID::Putter, "Putter ", 9.11f, 0.f, 10.f)
};