/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2024
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

        GettingStarted,
        Junior,
        Amateur,
        Enthusiast,
        SemiPro,
        Pro,

        HotStuff,
        ISeeNoShips,
        IntoOrbit,
        BadSport,
        FullHouse,
        Subscriber,
        BirdsEyeView,

        Dedicated,
        ResidentGolfer,
        MonthOfSundays,

        SpinClass,
        Complete01,
        Complete02,
        Complete03,
        Complete04,
        Complete05,
        Complete06,
        Complete07,
        Complete08,
        Complete09,
        Complete10,

        Master01,
        Master02,
        Master03,
        Master04,
        Master05,
        Master06,
        Master07,
        Master08,
        Master09,
        Master10,

        RoadToSuccess,
        NoMistake,
        NeverGiveUp,
        GreensInRegulation,
        ThreesACrowd,
        ConsistencyIsKey,

        Nested,
        GimmeGimmeGimme,
        DriveItHome,
        Gamer,
        CauseARacket,

        UpForTheChallenge,

        BigBird,
        BarnStormer,
        HitTheSpot,
        BeholdTheImpossible,
        TakeInAShow,

        LeagueChampion,
        LeagueSeasonal,

        NightOwl,
        RainOrShine,

        Complete11,
        Master11,
        Complete12,
        Master12,

        Count
    };
    static_assert(Count <= 256, "Count exceeds maximum 256 Achievements!");
}

//DON'T change this order, it breaks existing save files
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
        Albatrosses,

        LeagueFirst,
        LeagueSecond,
        LeagueThird,
        LeagueRounds,
        ChipIns,
        FlagHits,

        Course11Complete,
        Course12Complete,

        PlaysInClearWeather,
        PlaysInRain,
        PlaysInShowers,
        PlaysInFog,

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
    500, 150, 50,  30, 10, 50,
    50,  100, 200, 300,
    10,  40,  100
};

static constexpr std::array<std::int32_t, 7u> StreakXP =
{
    5,11,18,26,36,50,100
};