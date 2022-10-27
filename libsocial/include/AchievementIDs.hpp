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
        JoinTheClub,

        StrokeOfGenius,
        NoMatch,
        SkinOfYourTeeth,

        Socialiser,
        Spots,
        Stripes,
        EasyPink,

        PracticeMakesPerfect,
        AllOfATwitter,
        Soaring,

        HoleInOneMillion,
        GimmeFive,
        GimmeTen,
        DayJob,
        BigPutts,
        GolfinDolphin,

        GrandTour,
        GrandDesign,

        Count
    };
    static_assert(Count <= 256, "Count exceeds maximum 256 Achievements!");
}

namespace StatID
{
    enum
    {
        HolesPlayed,
        PuttDistance,
        StrokeDistance,
        GoldWins,
        SilverWins,
        BronzeWins,
        TotalRounds,
        EightballWon,
        NineballWon,
        SnookerWon,
        TimeOnTheRange,
        Birdies,
        Eagles,
        HIOs,
        LeatherGimmies,
        PutterGimmies,
        TimeOnTheCourse,
        LongPutts,
        WaterTrapRounds,
        WaterTrapCount,
        SandTrapCount,
        Course01Complete,
        Course02Complete,
        Course03Complete,
        Course04Complete,
        Course05Complete,
        Course06Complete,
        Course07Complete,
        Course08Complete,
        Course09Complete,
        Course10Complete,

        Count
    };
    static_assert(Count <= 64, "Count exceeds maximum number of stats");
}

namespace XPID
{
    enum
    {
        HIO, Albatross, Eagle, Birdie, Par, Special,
        CompleteCourse, Third, Second, First,
        Good, NotBad, Excellent,

        Count
    };
}
static constexpr std::array<std::int32_t, XPID::Count> XPValues =
{
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0
};