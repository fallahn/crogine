/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2023
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

struct StateID final
{
    enum
    {
        Menu,
        Golf,
        Options,
        Pause,
        Error,
        SplashScreen,
        Tutorial,
        Keyboard,
        Practice,
        DrivingRange,
        PuttingRange,
        Clubhouse,
        Billiards,
        Trophy,
        News,
        Bush,
        Playlist,
        MessageOverlay,
        Credits,
        EventOverlay, //consumes events if the overlay is open
        Unlock,
        Profile, 
        Leaderboard,
        Stats,
        MapOverview,
        GC,
        League,
        PlayerManagement,

        SQLite, //used for testing SQLite features
        Workshop = 1100
    };
};