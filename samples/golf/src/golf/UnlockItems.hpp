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

#include <cstdint>
#include <string>
#include <array>

struct UnlockData final
{
    std::int32_t modelID = -1; //preview model
    std::string description;
    std::string name;

    UnlockData(std::int32_t i, const std::string& d, const std::string& n)
        : modelID(i), description(d), name(n) {}
};

namespace ul
{
    struct ModelID final
    {
        enum
        {
            Wood,
            Iron,

            BronzeBall,
            SilverBall,
            GoldBall,
            PlatinumBall,
            DiamondBall,
            AmbassadorBall,

            LevelBadge01,
            LevelBadge10,
            LevelBadge20,
            LevelBadge30,
            LevelBadge40,
            LevelBadge50,
            LevelBadge100,

            Padlock,

            Streak01,
            Streak02,
            Streak03,
            Streak04,
            Streak05,
            Streak06,
            Streak07,

            GolfBag01,
            GolfBag02,

            CareerBall01,
            CareerBall02,
            CareerBall03,
            CareerBall04,
            CareerBall05,
            CareerBall06,

            CareerHair01,
            CareerHair02,
            CareerHair03,
            CareerHair04,
            CareerHair05,
            CareerHair06,

            CareerAV01,
            CareerAV02,
            CareerAV03,
            CareerAV04,
            CareerAV05,
            CareerAV06,

            CareerFirst,
            CareerSecond,
            CareerThird,

            GoldCup,

            Count
        };
    };

    static inline const std::array<std::string, ModelID::Count> ModelPaths = 
    {
        std::string("assets/golf/models/club_wood.cmt"),
        std::string("assets/golf/models/club_iron.cmt"),
        std::string("assets/golf/models/balls/ball_bronze.cmt"),
        std::string("assets/golf/models/balls/ball_silver.cmt"),
        std::string("assets/golf/models/balls/ball_gold.cmt"),
        std::string("assets/golf/models/balls/ball_platinum.cmt"),
        std::string("assets/golf/models/balls/ball_diamond.cmt"),
        std::string("assets/golf/models/balls/centurion.cmt"),
        std::string("assets/golf/models/trophies/level01.cmt"),
        std::string("assets/golf/models/trophies/level10.cmt"),
        std::string("assets/golf/models/trophies/level20.cmt"),
        std::string("assets/golf/models/trophies/level30.cmt"),
        std::string("assets/golf/models/trophies/level40.cmt"),
        std::string("assets/golf/models/trophies/level50.cmt"),
        std::string("assets/golf/models/trophies/level50.cmt"), //update me!!
        std::string("assets/golf/models/trophies/unlock.cmt"),
        std::string("assets/golf/models/trophies/streak01.cmt"),
        std::string("assets/golf/models/trophies/streak02.cmt"),
        std::string("assets/golf/models/trophies/streak03.cmt"),
        std::string("assets/golf/models/trophies/streak04.cmt"),
        std::string("assets/golf/models/trophies/streak05.cmt"),
        std::string("assets/golf/models/trophies/streak06.cmt"),
        std::string("assets/golf/models/trophies/streak07.cmt"),
        std::string("assets/golf/models/golfbag01.cmt"),
        std::string("assets/golf/models/golfbag02.cmt"),

        std::string("assets/golf/models/career/tier0/01.cmt"),
        std::string("assets/golf/models/career/tier0/02.cmt"),
        std::string("assets/golf/models/career/tier0/03.cmt"),
        std::string("assets/golf/models/career/tier0/04.cmt"),
        std::string("assets/golf/models/career/tier0/05.cmt"),
        std::string("assets/golf/models/career/tier0/06.cmt"),

        std::string("assets/golf/models/career/tier1/01.cmt"),
        std::string("assets/golf/models/career/tier1/02.cmt"),
        std::string("assets/golf/models/career/tier1/03.cmt"),
        std::string("assets/golf/models/career/tier1/04.cmt"),
        std::string("assets/golf/models/career/tier1/05.cmt"),
        std::string("assets/golf/models/career/tier1/06.cmt"),

        std::string("assets/golf/models/trophies/c01.cmt"),
        std::string("assets/golf/models/trophies/c02.cmt"),
        std::string("assets/golf/models/trophies/c03.cmt"),
        std::string("assets/golf/models/trophies/c04.cmt"),
        std::string("assets/golf/models/trophies/c05.cmt"),
        std::string("assets/golf/models/trophies/c06.cmt"),

        std::string("assets/golf/models/trophies/trophy04.cmt"),
        std::string("assets/golf/models/trophies/trophy05.cmt"),
        std::string("assets/golf/models/trophies/trophy06.cmt"),
        
        std::string("assets/golf/models/trophies/trophy01.cmt"),
    };

    //these MUST be in the correct order for unlocking
    struct UnlockID final
    {
        enum
        {
            //---these blocks are ordered specifically---//
            //---to match the player as they level up ---//
            FiveWood,
            FourIron,
            SixIron,
            SevenIron,
            NineIron,

            BronzeBall,
            SilverBall,
            GoldBall,
            PlatinumBall,
            DiamondBall,
            AmbassadorBall,

            Level1,
            Level10,
            Level20,
            Level30,
            Level40,
            Level50,
            Level100,

            //---ugh this block is used to index flags---//
            //---in the 'generic' flag set            ---//
            RangeExtend01,
            RangeExtend02,
            Clubhouse,
            CourseEditor,
            MonthlyComplete,

            //---starting from RangeExtend01----//


            //---from here things are a bit more sensible--//
            Streak01,
            Streak02,
            Streak03,
            Streak04,
            Streak05,
            Streak06,
            Streak07,

            Ball01,
            Ball02,
            Ball03,
            Ball04,
            Ball05,
            Ball06,

            Hair01,
            Hair02,
            Hair03,
            Hair04,
            Hair05,
            Hair06,

            Avatar01,
            Avatar02,
            Avatar03,
            Avatar04,
            Avatar05,
            Avatar06,

            CareerGold,
            CareerSilver,
            CareerBronze,

            Count
        };
    };
    static_assert(UnlockID::Streak01 - UnlockID::RangeExtend01 < 32, "Max Flags Available");

    static inline const std::array<UnlockData, UnlockID::Count> Items =
    {
        UnlockData(ModelID::Wood, "5 Wood", "New Golf Club!"),
        { ModelID::Iron, "4 Iron", "New Golf Club!" },
        { ModelID::Iron, "6 Iron", "New Golf Club!" },
        { ModelID::Iron, "7 Iron", "New Golf Club!" },
        { ModelID::Iron, "9 Iron", "New Golf Club!" },

        { ModelID::BronzeBall,      "Bronze Ball",        "New Golf Ball!" },
        { ModelID::SilverBall,      "Silver Ball",        "New Golf Ball!" },
        { ModelID::GoldBall,        "Gold Ball",          "New Golf Ball!" },
        { ModelID::PlatinumBall,    "Platinum Ball",      "New Golf Ball!" },
        { ModelID::DiamondBall,     "Diamond Ball",       "New Golf Ball!" },
        { ModelID::AmbassadorBall,  "Ambassador's Ball",  "New Golf Ball!" },

        { ModelID::LevelBadge01,  "Level One",     "New Level Badge!" },
        { ModelID::LevelBadge10,  "Level Ten",     "New Level Badge!" },
        { ModelID::LevelBadge20,  "Level Twenty",  "New Level Badge!" },
        { ModelID::LevelBadge30,  "Level Thirty",  "New Level Badge!" },
        { ModelID::LevelBadge40,  "Level Forty",   "New Level Badge!" },
        { ModelID::LevelBadge50,  "Level Fifty",   "New Level Badge!" },
        { ModelID::LevelBadge100, "Centenary",     "New Level Badge!" },

        { ModelID::GolfBag01,  "Expert Clubs Available!",   "Club Range Extended" },
        { ModelID::GolfBag02,  "Pro Clubs Available!",      "Club Range Extended" },
        { ModelID::Padlock,    "Socialiser",                "Clubhouse Unlocked" },
        { ModelID::Padlock,    "Designer",                  "Course Editor Unlocked" },
        { ModelID::GoldCup,    "Monthly Challege Complete", "+1000XP" },

        { ModelID::Streak01,  "1 Day Streak",    "+5XP" },
        { ModelID::Streak02,  "2 Day Streak",    "+11XP" },
        { ModelID::Streak03,  "3 Day Streak",    "+18XP" },
        { ModelID::Streak04,  "4 Day Streak",    "+26XP" },
        { ModelID::Streak05,  "5 Day Streak",    "+36XP" },
        { ModelID::Streak06,  "6 Day Streak",    "+50XP" },
        { ModelID::Streak07,  "7 Day Streak",    "+100XP" },

        { ModelID::CareerBall01,  "Tennis",          "New Golf Ball!" },
        { ModelID::CareerBall02,  "Cocktail Olive",  "New Golf Ball!" },
        { ModelID::CareerBall03,  "Smiley",          "New Golf Ball!" },
        { ModelID::CareerBall04,  "Toybox",          "New Golf Ball!" },
        { ModelID::CareerBall05,  "Jingle Bell",     "New Golf Ball!" },
        { ModelID::CareerBall06,  "Fortune Teller",  "New Golf Ball!" },

        { ModelID::CareerHair01,  "Mutton Chops",    "New Headwear!" },
        { ModelID::CareerHair02,  "Princess",        "New Headwear!" },
        { ModelID::CareerHair03,  "Fizz! Lite",      "New Headwear!" },
        { ModelID::CareerHair04,  "Bun",             "New Headwear!" },
        { ModelID::CareerHair05,  "Fizz!",           "New Headwear!" },
        { ModelID::CareerHair06,  "Head of State",   "New Headwear!" },

        { ModelID::CareerAV01,  "Unlocked!",   "New Avatar!" },
        { ModelID::CareerAV02,  "Unlocked!",   "New Avatar!" },
        { ModelID::CareerAV03,  "Unlocked!",   "New Avatar!" },
        { ModelID::CareerAV04,  "Unlocked!",   "New Avatar!" },
        { ModelID::CareerAV05,  "Unlocked!",   "New Avatar!" },
        { ModelID::CareerAV06,  "Unlocked!",   "New Avatar!" },

        { ModelID::CareerFirst,   "First Place!",   "New Headwear!" }, //these get overwritten with XP value
        { ModelID::CareerSecond,  "Second Place!",  "New Headwear!" },
        { ModelID::CareerThird,   "Third Place!",   "New Headwear!" },
    };
}