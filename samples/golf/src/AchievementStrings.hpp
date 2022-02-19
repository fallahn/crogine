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
        LeaderOfThePack,
        PuttStar,
        TopChip,
        Boomerang,
        BetterWithFriends,

        BronzeStar,
        SilverStar,
        GoldStar,

        CluedUp,

        Count
    };
}

static const std::array<std::string, AchievementID::Count> AchievementStrings = 
{
    "unknown",
    "hole_in_one",
    "bulls_eye",
    "long_distance_clara",
    "stroke_of_midnight",
    "leader_of_the_pack",
    "putt_star",
    "top_chip",
    "boomerang",
    "better_with_friends",
    "bronze_star",
    "silver_star",
    "gold_star",
    "clued_up"
};

//appears on the notification
static const std::array<std::string, AchievementID::Count> AchievementLabels =
{
    "unknown",
    "Hole In One!",
    "Bull's Eye",
    "Long Distance Clara",
    "Stroke of Midnight",
    "Leader of the Pack",
    "Putt Star",
    "Top Chip",
    "Boomerang",
    "Better with Friends",
    "Bronze",
    "Silver",
    "Gold",
    "Clued Up"
};

//description and whether or not the achievement is hidden until it is unlocked
static const std::array<std::pair<std::string, bool>, AchievementID::Count> AchievementDesc =
{
    std::make_pair("Unknown.", true),
    std::make_pair("Get a hole in one", false),
    std::make_pair("Complete 10 rounds of Golf", false),
    std::make_pair("Putt a total distance of 1km", false),
    std::make_pair("Get a total stroke distance of 12km", false),
    std::make_pair("Come top of the Leaderboard", false),
    std::make_pair("Sink a putt greater than 5m", false),
    std::make_pair("Sink a hole from outside the green", false),
    std::make_pair("Get a PAR or better after at least one foul", false),
    std::make_pair("Play a network game with 4 clients", false),
    std::make_pair("Get a One Star rating on the driving range", false),
    std::make_pair("Get a Two Star rating on the driving range", false),
    std::make_pair("Get a Three Star rating on the driving range", false),
    std::make_pair("Complete the Tutorial", false)
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

//these are indexed by the above, so do try to get them in the correct order ;)
static const std::array<std::string, StatID::Count> StatStrings =
{
    "holes_played",
    "putt_distance",
    "stroke_distance"
};

struct StatTrigger final
{
    std::int32_t achID = -1;
    float threshold = 0.f;
};

static std::array<std::vector<StatTrigger>, StatID::Count> StatTriggers = {};