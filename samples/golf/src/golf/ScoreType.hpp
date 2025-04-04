/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2024
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
#include <cstdint>

namespace ScoreType
{
    enum
    {
        Stroke,
        Stableford,
        StablefordPro,

        Match, Skins,
        MultiTarget,
        ShortRound,
        
        Elimination,
        ClubShuffle,
        NearestThePin,
        NearestThePinPro,

        Count,

        BBB,
        LongestDrive,
    };

    static const inline std::array<std::int32_t, Count> MinPlayerCount =
    {
        1,1,1,2,2,1,1,2,1,2,2//,2
    };

    static const inline std::array<std::int32_t, Count> MaxPlayerCount =
    {
        16,16,16,2,16,16,16,16,16,16,16//,16
    };
}

namespace GimmeSize
{
    enum
    {
        None, Leather, Putter,
        Count
    };
}
