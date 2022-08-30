/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2022
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

#include "AchievementIDs.hpp"

#include <string>
#include <array>
#include <vector>

/*
Note that the default achievement system only reserves space
for 256 achivements and 64 stats. Any more than this will not
be read/written to the stat file or will potentially corrupt
the data (and we don't want to upset people by ruining their
achievements!!)
*/



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
    "clued_up",
    "join_the_club",
    "stroke_of_genius",
    "no_match",
    "skin_teeth",
    "socialiser",
    "spots",
    "stripes",
    "easy_pink",
    "practice_perfect",
    "all_of_a_twitter",
    "soaring",
    "hole_in_one_million",
    "gimme_five",
    "gimme_ten",
    "day_job",
    "big_putts",
    "golfin_dolphin",
    "grand_tour",
    "grand_design",
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
    "Clued Up",
    "Join The Club",
    "Stroke of Genius",
    "No Match for You",
    "Skin of Your Teeth",
    "Socialiser",
    "Spots",
    "Stripes",
    "Easy Pink",
    "Practice Makes Perfect",
    "All of a Twitter",
    "Soaring",
    "Hole In One Million",
    "Gimme 5!",
    "Gimme 10!",
    "Day Job",
    "I Like Big Putts",
    "Golfin' Dolphin",
    "Grand Tour",
    "Grand Design"
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
    std::make_pair("Complete the Tutorial", false),
    std::make_pair("Make it to the 19th Hole", false),
    std::make_pair("Win a round of Stroke Play", false),
    std::make_pair("Win a round of Match Play", false),
    std::make_pair("Win a round of Skins", false),
    std::make_pair("Enter the Clubhouse at least once", false),
    std::make_pair("Win a game of Eightball", false),
    std::make_pair("Win a game of Nineball", false),
    std::make_pair("Win a game of Snooker", false),
    std::make_pair("Spend an hour in total on the driving range", false),
    std::make_pair("Get 18 Birdies", false),
    std::make_pair("Get an Eagle", false),
    std::make_pair("Hit the camera drone with a ball", false),
    std::make_pair("Take five gimmies inside the leather", false),
    std::make_pair("Take ten gimmies inside the putter", false),
    std::make_pair("Total over 24 hours play time on the course", false),
    std::make_pair("Sink 15 long putts", false),
    std::make_pair("Hit a water trap on 5 different rounds", false),
    std::make_pair("Play a full round on every course", false),
    std::make_pair("Save your first custom course", false),
};

//assuming trophies load correctly they are:
/*
Gold, silver, bronze cup
Gold, silver, bronze mannequin
Pool, Platinum
*/

struct TrophyID final
{
    enum
    {
        GoldCup, SilverCup, BronzeCup,
        GoldFigure, SilverFigure, BronzeFigure,
        Pool, Platinum
    };
};

static constexpr std::array<std::size_t, AchievementID::Count> AchievementTrophies =
{
    TrophyID::Pool,
    TrophyID::Platinum,
    TrophyID::SilverFigure,
    TrophyID::GoldFigure,
    TrophyID::BronzeFigure,
    TrophyID::GoldCup,
    TrophyID::SilverFigure,
    TrophyID::GoldFigure,
    TrophyID::BronzeCup,
    TrophyID::SilverCup,
    TrophyID::BronzeFigure,
    TrophyID::SilverFigure,
    TrophyID::GoldFigure,
    TrophyID::SilverCup,
    TrophyID::GoldCup,
    TrophyID::GoldFigure,
    TrophyID::GoldFigure,
    TrophyID::GoldFigure,
    TrophyID::SilverCup,
    TrophyID::Pool,
    TrophyID::Pool,
    TrophyID::Pool,
    TrophyID::GoldFigure,
    TrophyID::SilverFigure,
    TrophyID::GoldFigure,
    TrophyID::Platinum,
    TrophyID::BronzeCup,
    TrophyID::BronzeFigure,
    TrophyID::GoldCup,
    TrophyID::GoldCup,
    TrophyID::BronzeFigure,
    TrophyID::GoldFigure,
    TrophyID::BronzeCup
};

//these are indexed by StatID, so do try to get them in the correct order ;)
static const std::array<std::string, StatID::Count> StatStrings =
{
    "holes_played",
    "putt_distance",
    "stroke_distance",
    "gold_average",
    "silver_average",
    "bronze_average",
    "total_rounds",
    "eightball_won",
    "nineball_won",
    "snooker_won",
    "time_on_the_range",
    "birdies",
    "eagles",
    "hios",
    "leather_gimmies",
    "putter_gimmies",
    "time_on_the_course",
    "long_putts",
    "water_traps",
    "water_trap_count",
    "sand_trap_count",
    "course_01", //a tad non-descript but we want these to match the name of the course directories
    "course_02",
    "course_03",
    "course_04",
    "course_05",
    "course_06",
    "course_07",
};

static const std::array<std::string, StatID::Count> StatLabels =
{
    "Full Rounds Completed (18 holes)",
    "Total Putt Distance (metres)",
    "Total Stroke Distance (metres)",
    "Average Gold Wins (multiplayer)",
    "Average Silver Wins (multiplayer)",
    "Average Bronze Wins (multiplayer)",
    "Total Rounds Played",
    "Eightball Games Won (multiplayer)",
    "Nineball Games Won (multiplayer)",
    "Snooker Games Won (multiplayer)",
    "Time Spent At The Driving Range",
    "Birdies Scored",
    "Eagles scored",
    "Holes In One",
    "Gimmies Taken Inside The Leather",
    "Gimmies Taken Inside The Putter",
    "Total Time Spent On A Course",
    "Long Putts Sunk",
    "Rounds Where Water Were Traps Hit",
    "Total Water Traps Hit",
    "Total Sand Traps Hit",
    "Number of times Course 1 Completed",
    "Number of times Course 2 Completed",
    "Number of times Course 3 Completed",
    "Number of times Course 4 Completed",
    "Number of times Course 5 Completed",
    "Number of times Course 6 Completed",
    "Number of times Course 7 Completed",
};

struct StatType final
{
    enum
    {
        Float, Integer, Percent, Time
    };
};

static constexpr std::array<std::int32_t, StatID::Count> StatTypes =
{
    StatType::Integer,
    StatType::Float,
    StatType::Float,
    StatType::Percent,
    StatType::Percent,
    StatType::Percent,
    StatType::Integer,
    StatType::Integer,
    StatType::Integer,
    StatType::Integer,
    StatType::Time,
    StatType::Integer,
    StatType::Integer,
    StatType::Integer,
    StatType::Integer,
    StatType::Integer,
    StatType::Time,
    StatType::Integer,
    StatType::Integer,
    StatType::Integer,
    StatType::Integer,
    StatType::Integer,
    StatType::Integer,
    StatType::Integer,
    StatType::Integer,
    StatType::Integer,
    StatType::Integer,
    StatType::Integer
};

struct StatTrigger final
{
    std::int32_t achID = -1;
    float threshold = std::numeric_limits<float>::max();
};

static std::array<std::vector<StatTrigger>, StatID::Count> StatTriggers = {};