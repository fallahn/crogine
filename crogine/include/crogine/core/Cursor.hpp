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

#include <SDL_mouse.h>

#include <crogine/Config.hpp>
#include <crogine/detail/SDLResource.hpp>

namespace cro
{
    /*!
    \brief Cursor class.
    The cursor class is used to set the cursor pointer for the active window.
    Multiple cursors may be created and swapped as necessary at run time.
    However a cursor instance must last as long as the window is using it, so
    some resource management is required to ensure the proper lifespan is met.

    As such Cursors are non-copyable.

    \see Window::setCursor()
    */

    /*!
    \brief Used to create a cursor in the form on
    a system built-in image. The actual icon varies
    between platforms.
    */
    enum class SystemCursor
    {
        Arrow     = SDL_SYSTEM_CURSOR_ARROW,     //!< Arrow
        IBeam     = SDL_SYSTEM_CURSOR_IBEAM,     //!< i-beam
        Wait      = SDL_SYSTEM_CURSOR_WAIT,      //!< Wait
        Crosshair = SDL_SYSTEM_CURSOR_CROSSHAIR, //!< Crosshair
        SmallWait = SDL_SYSTEM_CURSOR_WAITARROW, //!< Small wait or wait of not available on the current platform
        SizeNWSE  = SDL_SYSTEM_CURSOR_SIZENWSE,  //!< Double arrow pointing northwest / southeast
        SizeNESW  = SDL_SYSTEM_CURSOR_SIZENESW,  //!< Double arrow pointing northeast / southwest
        SizeWE    = SDL_SYSTEM_CURSOR_SIZEWE,    //!< Double arrow pointing west and east
        SizeNS    = SDL_SYSTEM_CURSOR_SIZENS,    //!< Double arrow pointing north and south
        SizeAll   = SDL_SYSTEM_CURSOR_SIZEALL,   //!< Four pointed arrow pointing north, east, south and west
        No        = SDL_SYSTEM_CURSOR_NO,        //!< Slashed circle or crossbones
        Hand      = SDL_SYSTEM_CURSOR_HAND       //!< Hand
    };

    class CRO_EXPORT_API Cursor final : public Detail::SDLResource
    {
    public:
        /*!
        \brief Constructor
        Creates a cursor from one of the built in system types
        \param systemType Type of system cursor to create.
        \see SystemCursor
        */
        explicit Cursor(SystemCursor systemType);

        /*!
        \brief Constructor
        Creates a cursor from the given image path, if possible.
        Colour cursors are available on a per-platform basis, if this
        constructor fails then the default cursor is created and an error
        is printed to the console.
        \param path Path to an image file. Must be compatible with cro::Image
        \param x The x position of the cursor 'hotspot'
        \param y The y position of the cursor 'hotspot'
        */
        Cursor(const std::string& path, std::int32_t x, std::int32_t y);

        ~Cursor();

        Cursor(const Cursor&) = delete;
        Cursor(Cursor&&) = delete;
        Cursor& operator = (const Cursor&) = delete;
        Cursor& operator = (Cursor&&) = delete;

        operator bool() { return m_cursor != nullptr; }

    private:
        std::vector<std::uint8_t> m_imageData;
        SDL_Surface* m_surface;
        SDL_Cursor* m_cursor;
        mutable bool m_inUse;

        friend class Window;
    };
}
