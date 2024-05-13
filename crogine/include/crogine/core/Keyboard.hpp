/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
http://trederia.blogspot.com

crogine - Zlib license.

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

#include <crogine/detail/Assert.hpp>
#include <crogine/core/App.hpp>
#include <crogine/core/String.hpp>

#include <SDL.h>

#include <cstdint>
#include <cstring>
#include <string>

namespace cro::Keyboard
{
    /*!
    \brief Returns the real time state of the keyboard
    \param scancode The SDL_Scancode of the key to test
    \returns true if the key is pressed else false.
    */
    static inline bool isKeyPressed(SDL_Scancode scancode)
    {
        CRO_ASSERT(scancode < SDL_NUM_SCANCODES, "scancode out of range!");
        auto* state = SDL_GetKeyboardState(nullptr);
        return state[scancode] != 0;
    }

    /*!
    \brief Returns the real time state of the keyboard
    \param key The SDL_Keycode of the key to test
    \returns true if the key is pressed else false.
    */
    static inline bool isKeyPressed(SDL_Keycode key)
    {
        return isKeyPressed(SDL_GetScancodeFromKey(key));
    }

    /*!
    \brief Returns the string representation of the given SDL_Keycode enum
    */
    static inline cro::String keyString(SDL_Keycode key)
    {
        switch (key)
        {
        default:
        {
            const auto* charArr = SDL_GetKeyName(key);
            cro::String str = cro::String::fromUtf8(charArr, charArr + std::strlen(charArr));
            return str;
        }
        case SDLK_LCTRL:
            return "LCtrl";
        case SDLK_RCTRL:
            return "RCtrl";
        case SDLK_LSHIFT:
            return "LShift";
        case SDLK_RSHIFT:
            return "RShift";
        case SDLK_LALT:
            return "LAlt";
        case SDLK_RALT:
            return "RAlt";
        }
    }
}
