/*-----------------------------------------------------------------------

Matt Marchant 2023 - 2024
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

#include <crogine/core/Clock.hpp>
#include <crogine/core/String.hpp>

struct ChallengeID final
{
    enum
    {
        Zero,
        One,
        Two,
        Three,
        Four,
        Five,
        Six,
        Seven,
        Eight,
        Nine,
        Ten,
        Eleven,

        Count
    };
};

//names of the stats
static const std::array<std::string, ChallengeID::Count> ChallengeStrings =
{
    "monthly_00",
    "monthly_01",
    "monthly_02",
    "monthly_03",
    "monthly_04",
    "monthly_05",
    "monthly_06",
    "monthly_07",
    "monthly_08",
    "monthly_09",
    "monthly_10",
    "monthly_11",
};

static const std::array<std::string, ChallengeID::Count> ChallengeDescriptions =
{
    "Make 100 So Close chips on to the green",
    "Make 50 Nice Putts",
    "Hit 25 random bull's eyes",
    "Play a full 18 (or 12) hole round in every game mode",
    "Play 9 holes on each course with at least 3 human or CPU players",
    "Get the equivalent of 10 Boomerang achievements",
    "Get 99% or better on each target on the Driving Range",
    "Take 50 gimmies resulting from a Near Miss",
    "Make 250 shots with Great Accuracy",
    "Score 4 Birdies in one round on the front 9 of each course",
    "Score 1 Eagle on the back 9 of each course",
    "Hit the flag stick 50 times"
};

struct Challenge final
{
    enum
    {
        //bitflags or incrementing value
        Flag, Counter
    };

    const std::uint32_t targetValue = 0;
    std::int32_t type = Counter;
    std::uint32_t value = 0;

    explicit Challenge(std::uint32_t target, std::int32_t t = Counter, std::uint32_t v = 0)
        : targetValue(target), type(t), value(v) {}
};

class MonthlyChallenge final
{
public:
    MonthlyChallenge(); //fetch the current active month

    //ignores the value if id != m_month
    //else increments counter (ignoring value)
    //or sets value as flag.
    //raises a message to show progress or completion
    void updateChallenge(std::int32_t id, std::int32_t value);

    //refreshes the status of the current month's stat
    //or resets other month's stats.
    void refresh();

    struct Progress final
    {
        std::int32_t value = 0;
        std::int32_t target = 0;
        std::int32_t index = -1;
        std::uint32_t flags = std::numeric_limits<std::uint32_t>::max();
    };
    Progress getProgress() const;
    cro::String getProgressString() const;

private:
    std::array<Challenge, ChallengeID::Count> m_challenges =
    {
        Challenge(100, Challenge::Counter),
        Challenge(50, Challenge::Counter),
        Challenge(25, Challenge::Counter),
        Challenge(0x7F, Challenge::Flag), //game mode count
        Challenge(0x3ff, Challenge::Flag), //(1 << 0) - (1 << 9)
        Challenge(10, Challenge::Counter),
        Challenge(0x1fff, Challenge::Flag), //(1 << 0) - (1 << 12)
        Challenge(50, Challenge::Counter),
        Challenge(250, Challenge::Counter),
        Challenge(0x3ff, Challenge::Flag),
        Challenge(0x3ff, Challenge::Flag),
        Challenge(50, Challenge::Counter),
    };

    std::int32_t m_month;
    std::int32_t m_day;
    bool m_leapYear;

    cro::Clock m_repeatClock;
};
