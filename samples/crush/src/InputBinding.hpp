/*-----------------------------------------------------------------------

Matt Marchant 2020
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

#include <SDL_events.h>

#include <array>
#include <cstdint>

namespace InputFlag
{
    enum
    {
        Up         = 0x1,
        Down       = 0x2,
        Left       = 0x4,
        Right      = 0x8,
        CarryDrop  = 0x10,
        Jump       = 0x20,
        Punt       = 0x40,
        Unused0    = 0x80,
        Unused1    = 0x100
    };
}

struct InputBinding final
{
    //buttons come before actions as this indexes into the controller
    //button array as well as the key array
    enum
    {
        Jump, CarryDrop, Punt, Unused0, Unused1, Left, Right, Up, Down, Count
    };

    std::array<std::int32_t, Count> keys =
    {
        SDLK_SPACE,
        SDLK_q,
        SDLK_e,
        SDLK_LCTRL,
        SDLK_LSHIFT,
        SDLK_a,
        SDLK_d,
        SDLK_w,
        SDLK_s
    };

    std::array<std::int32_t, 5u> buttons =
    {
        SDL_CONTROLLER_BUTTON_A,
        SDL_CONTROLLER_BUTTON_X,
        SDL_CONTROLLER_BUTTON_B,
        SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
        SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
    };
    std::int32_t controllerID = -1;
};