/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2025
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

#include "Clubs.hpp"
#include <AchievementIDs.hpp>

struct XPVal final
{
public:
    const std::int32_t operator[](std::size_t idx) const
    {
        const auto clubLevel = Club::getClubLevel();
        switch (clubLevel)
        {
        default:
        case 0:
            return XPValues[idx];
        case 1:
        {
            auto v = XPValues[idx];
            if (v >= 100)
            {
                v += (v / 4);
            }
            else if (v >= 20)
            {
                v += (v / 2);
            }
            return v;
        }
        case 2:
        {
            auto v = XPValues[idx];
            if (v >= 100)
            {
                v += (v / 2);
            }
            else
            {
                v *= 2;
            }
            return v;
        }
        }
    }
};
//this modifies the default XP value based on the current club set in use
static const XPVal xpValues;