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
#include <array>

namespace AchievementID
{
    enum
    {
        Unknown,
        HoleInOne = 1,
        BullsEye,
        LongDistanceClara,
        StrokeOfMidnight,

        Count
    };
}

static const std::array<std::string, AchievementID::Count> AchievementStrings = 
{
    "unknown",
    "hole_in_one",
    "bulls_eye",
    "long_distance_clara",
    "stroke_of_midnight"
};

static const std::array<std::string, AchievementID::Count> AchievementLabels =
{
    "unknown",
    "Hole In One!",
    "Bull's Eye",
    "Long Distance Clara",
    "Stroke of Midnight"
};

//description and whether or not the achievement is hidden until it is unlocked
static const std::array<std::pair<std::string, bool>, AchievementID::Count> AchievementDesc =
{
    std::make_pair("Unknown.", true),
    std::make_pair("Get a hole in one", false),
    std::make_pair("Complete 10 rounds of Golf", false),
    std::make_pair("Putt a total distance of 1km", false),
    std::make_pair("Get a total stroke distance of 12km", false)
};

namespace StatID
{
    enum
    {
        HolesPlayed,
        PuttDistance,
        StrokeDistance,

        Count
    };
}

static const std::array<std::string, StatID::Count> StatStrings =
{
    "putt_distance",
    "stroke_distance",
    "holes_played"
};

struct StatTrigger final
{
    std::int32_t achID = -1;
    std::int32_t threshold = 0;
};

static std::array<std::vector<StatTrigger>, StatID::Count> StatTriggers = {};