/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2026
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

#include <SDL3/SDL_events.h>

#include <array>
#include <cstdint>
#include <limits>
#include <string>

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

static inline const std::array<std::string, 10> InputLabels =
{
    std::string("Action"),
    "Next Club", "Previous Club",
    "Spin Menu", "Emote Menu",
    "Cancel Shot", "Aim Left",
    "Aim Right", "Camera Up",
    "Camera Down"
};

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

    //TODO these should use scancodes so that
    //other keyboard layouts display the correct keys
    std::array<SDL_Keycode, Count> keys =
    {
        SDLK_SPACE,
        SDLK_E,
        SDLK_Q,
        SDLK_LALT,
        SDLK_LCTRL,
        SDLK_LSHIFT,
        SDLK_A,
        SDLK_D,
        SDLK_W,
        SDLK_S
    };

    std::array<std::int32_t, 6u> buttons =
    {
        SDL_GAMEPAD_BUTTON_SOUTH,
        SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
        SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
        SDL_GAMEPAD_BUTTON_WEST,
        SDL_GAMEPAD_BUTTON_NORTH,
        SDL_GAMEPAD_BUTTON_EAST
    };
    std::int32_t playerID = 0;

    std::int32_t clubset = std::numeric_limits<std::int32_t>::max();
};

//these are the top row keys which can't be rebound
//TODO should this include tab for showing scoreboard
//LAlt for zoom to target
struct FixedKey final
{
    enum
    {
        DroneCam            = SDL_SCANCODE_1,
        FreeCam             = SDL_SCANCODE_2,
        CameraRotateLeft    = SDL_SCANCODE_3,
        CameraRotateRight   = SDL_SCANCODE_4,
        ZoomMinimap         = SDL_SCANCODE_5,
        ToggleDOF           = SDL_SCANCODE_6,
        EmoteApplaud        = SDL_SCANCODE_7,
        EmoteLaughing       = SDL_SCANCODE_8,
        EmoteHappy          = SDL_SCANCODE_9,
        EmoteAngry          = SDL_SCANCODE_0
    };
};