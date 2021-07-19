/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#include <crogine/util/Constants.hpp>

#include <string>
#include <array>

struct Club final
{
    std::string name;
    float distance = 0.f;
    float angle = 0.f;

    Club(const std::string& n, float d, float a)
        : name(n), distance(d), angle(a * cro::Util::Const::degToRad) {}
};

struct ClubID final
{
    enum
    {
        Driver, ThreeWood,
        FiveIron, NineIron,
        PitchWedge, Putter
    };
};

static const std::array Clubs =
{
    Club("Driver", 220.f, 45.f),
    Club("3 Wood", 180.f, 45.f),
    Club("5 Iron", 140.f, 40.f),
    Club("9 Iron", 100.f, 40.f),
    Club("Pitch Wedge", 80.f, 60.f),
    Club("Putter", 40.f, 4.f)
};