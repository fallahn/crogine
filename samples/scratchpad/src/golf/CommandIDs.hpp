/*-----------------------------------------------------------------------

Matt Marchant 2021
http://trederia.blogspot.com

crogine application - Zlib license.

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

struct CommandID final
{
    enum
    {
        Ball            = 0x1,
        StrokeIndicator = 0x2,
        Flag            = 0x4,
        Hole            = 0x8,
        Tee             = 0x10
    };

    struct UI final
    {
        enum
        {
            //FlagSprite           = 0x1,
            PlayerName           = 0x2,
            UIElement            = 0x4, //has its position updated on UI layout
            Root                 = 0x8,
            PlayerSprite         = 0x10,
            WindSock             = 0x20,
            WindString           = 0x40,
            HoleNumber           = 0x80,
            PinDistance          = 0x100,
            Scoreboard           = 0x200,
            ScoreScroll          = 0x400,
            ScoreboardController = 0x800
        };
    };
};