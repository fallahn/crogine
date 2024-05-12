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

#include <SDL_events.h>

#include <array>
#include <cstdint>
#include <limits>

namespace InputFlag
{
    enum 
    {
        Up          = 0x1,
        Down        = 0x2,
        Left        = 0x4,
        Right       = 0x8,
        Action      = 0x10,
        NextClub    = 0x20,
        PrevClub    = 0x40,
        SpinMenu    = 0x80,
        Cancel      = 0x100,
        EmoteWheel  = 0x200,

        CamModifier = SpinMenu,
        SwitchView  = EmoteWheel,

        Swingput    = 0x400,
        MiniMap     = 0x800,

        All = 0xFFFF
    };
}
//IMPORTANT if we update this make sure any plugins which copy (yes, *sigh*)
//this struct also get updated.
struct InputBinding final
{
    //buttons come before actions as this indexes into the controller
    //button array as well as the key array
    enum
    {
        Action, NextClub, PrevClub, SpinMenu, EmoteMenu, CancelShot, //buttons
        Left, Right, Up, Down, 
        
        Count,

        //aliases for billiards mode
        CamModifier = SpinMenu,
        SwitchView = EmoteMenu
    };

    std::array<std::int32_t, Count> keys =
    {
        SDLK_SPACE,
        SDLK_e,
        SDLK_q,
        SDLK_LALT,
        SDLK_LCTRL,
        SDLK_LSHIFT,
        SDLK_a,
        SDLK_d,
        SDLK_w,
        SDLK_s
    };

    std::array<std::int32_t, 6u> buttons =
    {
        SDL_CONTROLLER_BUTTON_A,
        SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
        SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
        SDL_CONTROLLER_BUTTON_X,
        SDL_CONTROLLER_BUTTON_Y,
        SDL_CONTROLLER_BUTTON_B
    };
    std::int32_t playerID = 0;

    std::int32_t clubset = std::numeric_limits<std::int32_t>::max();
};

//these are the top row keys which can't be rebound
struct FixedKey final
{
    enum
    {
        DroneCam            = SDLK_1,
        FreeCam             = SDLK_2,
        CameraRotateLeft    = SDLK_3,
        CameraRotateRight   = SDLK_4,
        ZoomMinimap         = SDLK_5,

        EmoteApplaud        = SDLK_7,
        EmoteLaughing       = SDLK_8,
        EmoteHappy          = SDLK_9,
        EmoteAngry          = SDLK_0
    };
};