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

#include <SDL.h>

#include <cstdint>
#include <string>

namespace cro::Keyboard
{
    /*!
    \brief Returns the real time state of the keyboard
    \param scancode The SDL scancode of the key to test
    \returns true if the key is pressed else false.
    */
    static inline bool isKeyPressed(std::int32_t scancode)
    {
        CRO_ASSERT(scancode > -1 && scancode < SDL_NUM_SCANCODES, "scancode out of range!");
        //CRO_ASSERT(cro::App::isValid(), "No app instance is running");
        auto* state = SDL_GetKeyboardState(nullptr);
        return state[scancode] != 0;
    }

    /*!
    \brief Returns the string representation of the given SDLK_ enum
    */
    static inline std::string keyString(std::int32_t SDLkey)
    {
        //CRO_ASSERT(cro::App::isValid(), "No app instance is running");
        return SDL_GetKeyName(SDLkey);
    }
}
