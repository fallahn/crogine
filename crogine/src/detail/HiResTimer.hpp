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

//the high resolution timer provides a more accurate frame time for the core loop

#include <SDL.h>
#include <crogine/Config.hpp>

namespace cro
{
    /*!
    \brief High Resolution Timer.
    Generally cro::Clock will suffice for millisecond accurate timing
    but when floating point values for representing seconds
    are required cro::Clock can lose precision. In these cases it's
    possible to use the high resolution timer which has greater
    floating point precision.
    */

    CRO_EXPORT_API class HiResTimer final
    {
    public:
        HiResTimer()
        {
            m_start = m_current = SDL_GetPerformanceCounter();
            m_frequency = SDL_GetPerformanceFrequency();
        }

        /*!
        \brief Restarts the timer and returns the time elapsed since the last restart.
        */
        float restart()
        {
            m_start = m_current;
            m_current = SDL_GetPerformanceCounter();
            return static_cast<float>(m_current - m_start) / static_cast<float>(m_frequency);
        }

    private:
        Uint64 m_start = 0;
        Uint64 m_current = 0;
        Uint64 m_frequency = 0;
    };
}