/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#include <crogine/core/String.hpp>

class Timeline final
{
public:
    struct GameMode final
    {
        enum
        {
            Playing, Lobby, Menu, LoadingScreen
        };
    };


    struct Event final
    {
        enum
        {
            HIO, Albatross, Eagle, Birdie, HoleOut, TargetHit, BeefStick,

            Bunker, Water, Drone, NewHole, EndOfRound, Elimination
        };
    };

    static void setTimelineDesc(const cro::String&, float = 0.f) {}

    static void setGameMode(std::int32_t) {}

    static void addEvent(std::int32_t, float = 0.f) {}
};
