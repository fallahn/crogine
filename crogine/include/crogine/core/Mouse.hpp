/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2021
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

#include <SDL_mouse.h>

#include <crogine/detail/glm/vec2.hpp>

#include <cstdint>

/*!
\brief Functions for querying the real-time state of the mouse
*/
namespace cro::Mouse
{
    /*!
    \brief Mouse button index
    \see isButtonPressed()
    */
    enum class Button
    {
        Left = 1, Middle, Right
    };

    /*!
    \brief Returns the global position of the mouse cursor,
    relative to the top left corner of the desktop
    */
    static inline glm::vec2 getGlobalPosition()
    {
        std::int32_t x = 0;
        std::int32_t y = 0;
        SDL_GetGlobalMouseState(&x, &y);
        return { x,y };
    }

    /*!
    \brief Returns the position of the mouse cursor relative to
    the top left corner of the active window.
    */
    static inline glm::vec2 getPosition()
    {
        std::int32_t x = 0;
        std::int32_t y = 0;
        SDL_GetMouseState(&x, &y);
        return { x,y };
    }

    /*!
    \brief Returns the mouse cursor position relative to
    last known cursor position.
    */
    static inline glm::vec2 getRelativePosition()
    {
        std::int32_t x = 0;
        std::int32_t y = 0;
        SDL_GetRelativeMouseState(&x, &y);
        return { x,y };
    }

    /*!
    \brief Returns true if the given button is currently
    pressed, else returns false
    */
    static inline bool isButtonPressed(Button button)
    {
        std::int32_t x, y;
        return (SDL_BUTTON(int(button))& SDL_GetMouseState(&x, &y)) != 0;
    }
}