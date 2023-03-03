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

#include <array>
#include <string>
#include <limits>

struct RenderFlags final
{
    enum
    {
        MiniMap = 0x1,
        MiniGreen = 0x2,
        Reflection = 0x4,
        Refraction = 0x8,
        Cue = 0x10,

        All = std::numeric_limits<std::uint64_t>::max()
    };
};

struct TerrainID final
{
    //these values are multiplied by 10 in the map's red channel
    //just cos it makes its a tiiiny bit easier to see in the image
    enum
    {
        Rough, Fairway,
        Green, Bunker,
        Water, Scrub,

        Stone,
        
        Hole, //keep this last as it doesn't have a vertex colour assigned to it

        Count
    };
};

//make sure we reserve room for trigger IDs
static_assert(TerrainID::Count < 14, "MAX VALUE REACHED");

struct TriggerID final
{
    //these are stored on the green channel
    //we start from above the terrain ID to
    //stop conflicts/backwards compat with
    //existing terrain models
    enum
    {
        Volcano = 13,
        Boat,


        Count
    };
};
static_assert(TriggerID::Count < 25, "MAX VALUE REACHED");

static const std::array<std::string, TerrainID::Count> TerrainStrings =
{
    "Rough", "Fairway", "Green", "Bunker", "Water", "Scrub", "Stone", "Hole"
};

//how much the stroke is affected by the current terrain
static constexpr std::array<float, TerrainID::Count> Dampening =
{
    0.9f, 1.f, 1.f, 0.85f, 1.f, 1.f, 0.f, 0.f
};

//how much the velocity of the ball is reduced when colliding
static constexpr std::array<float, TerrainID::Count> Restitution =
{
    0.23f, 0.33f, 0.28f, 0.f, 0.f, 0.f, 0.55f, 0.f
};

//how much the spin is reduced when bouncing on the terrain
static constexpr std::array<float, TerrainID::Count> SpinReduction =
{
    0.5f, 0.8f, 0.9f, 0.f, 0.f, 0.f, 0.99f, 0.f
};