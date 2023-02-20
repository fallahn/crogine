/*-----------------------------------------------------------------------

Matt Marchant 2023
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

            BronzeCup,
            SilverCup,
            GoldCup,
            PlatinumCup,
            DiamondCup,

            LevelBadge,

            Count
        };
    };

    static const std::array<std::string, ModelID::Count> ModelPaths = 
    {
        std::string("assets/golf/models/club_wood.cmt"),
        std::string("assets/golf/models/club_iron.cmt"),
        std::string("assets/golf/models/balls/ball_bronze.cmt"),
        std::string("assets/golf/models/balls/ball_silver.cmt"),
        std::string("assets/golf/models/balls/ball_gold.cmt"),
        std::string("assets/golf/models/balls/ball_platinum.cmt"),
        std::string("assets/golf/models/balls/ball_diamond.cmt"),
        std::string("assets/golf/models/trophies/level20.cmt"),
    };

    //these MUST be in the correct order for unlocking
    struct UnlockID final
    {
        enum
        {
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

            Level1,
            Level10,
            Level20,
            Level30,
            Level40,
            Level50,

            Count
        };
    };

    static const std::array<UnlockData, UnlockID::Count> Items =
    {
        UnlockData(ModelID::Wood, "5 Wood", "New Golf Club!"),
        { ModelID::Iron, "4 Iron", "New Golf Club!" },
        { ModelID::Iron, "6 Iron", "New Golf Club!" },
        { ModelID::Iron, "7 Iron", "New Golf Club!" },
        { ModelID::Iron, "9 Iron", "New Golf Club!" },

        { ModelID::BronzeCup,   "Bronze Ball",   "New Golf Ball!" },
        { ModelID::SilverCup,   "Silver Ball",   "New Golf Ball!" },
        { ModelID::GoldCup,     "Gold Ball",     "New Golf Ball!" },
        { ModelID::PlatinumCup, "Platinum Ball", "New Golf Ball!" },
        { ModelID::DiamondCup,  "Diamond Ball",  "New Golf Ball!" },

        { ModelID::LevelBadge,  "Level One",     "New Level Reached!" },
        { ModelID::LevelBadge,  "Level Ten",     "New Level Reached!" },
        { ModelID::LevelBadge,  "Level Twenty",  "New Level Reached!" },
        { ModelID::LevelBadge,  "Level Thirty",  "New Level Reached!" },
        { ModelID::LevelBadge,  "Level Forty",   "New Level Reached!" },
        { ModelID::LevelBadge,  "Level Fifty",   "New Level Reached!" },
    };
}