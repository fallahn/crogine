/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

struct Club final
{
    std::string name; //displayed in UI
    float power = 0.f; //magnitude of impulse applied to reach target
    float angle = 0.f; //pitch of shot (should be positive)
    const float target = 0.f; //the max (approx) distance when hit with 100% power

    Club(const std::string& n, float p, float a, float t)
        : name(n), power(p), angle(a * cro::Util::Const::degToRad), target(t) {}

    std::string getName(bool imperial) const
    {
        if (imperial)
        {
            auto dist = static_cast<std::int32_t>(target * 1.094f);
            return name + std::to_string(dist) + "yds";
        }
        else
        {
            auto dist = static_cast<std::int32_t>(target);
            return name + std::to_string(dist) + "m";
        }
    }
};

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

static const std::array<Club, ClubID::Count> Clubs =
{
    Club("Driver ", 46.5f, 45.f, 220.f),
    Club("3 Wood ", 42.02f, 45.f, 180.f),
    Club("5 Iron ", 37.35f, 40.f, 140.f),
    Club("9 Iron ", 31.56f, 40.f, 100.f),
    Club("Pitch Wedge ", 25.9f, 52.f, 70.f),
    Club("Gap Wedge ", 17.8f, 60.f, 30.f),
    Club("Sand Wedge ", 10.65f, 60.f, 10.f),
    Club("Putter ", 7.f, 0.f, 7.f)
};