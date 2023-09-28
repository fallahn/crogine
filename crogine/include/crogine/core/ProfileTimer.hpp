/*-----------------------------------------------------------------------

Matt Marchant 2023
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

#include <SDL.h>

#include <crogine/Config.hpp>
#include <crogine/detail/SDLResource.hpp>

#include <array>

namespace cro
{
    /*!
    \brief High resolution timer for profiling
    */
    template <std::uint64_t SampleCount = 10>
    class ProfileTimer final : public cro::Detail::SDLResource
    {
    public:
        /*!
        \brief Starts (or restarts) the timing operation
        */
        void begin()
        {
            m_begin = SDL_GetPerformanceCounter();
        }

        /*!
        \brief Ends the current timing operation and updates
        the result if SampleCount samples have been taken.
        */
        void end()
        {
            static_assert(SampleCount != 0, "Can't be empty");

            m_samples[m_sampleIndex] = static_cast<float>(SDL_GetPerformanceCounter() - m_begin) / SDL_GetPerformanceFrequency();

            m_sampleIndex = (m_sampleIndex + 1) % SampleCount;

            if (m_sampleIndex == 0)
            {
                m_result = 0.f;
                for (auto s : m_samples)
                {
                    m_result += s;
                }
                m_result /= SampleCount;
            }
        }

        /*!
        \brief Returns the average time of the operation based
        on SampleCount, in seconds
        */
        float result() const
        {
            return m_result;
        }
    private:

        std::uint64_t m_begin = 0;
        float m_result = 0.f;

        std::array<float, SampleCount> m_samples = {};
        std::size_t m_sampleIndex = 0;
    };
}