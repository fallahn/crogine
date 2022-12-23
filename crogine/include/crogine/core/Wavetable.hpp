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

#include <crogine/Config.hpp>
#include <crogine/core/Clock.hpp>

#include <vector>

namespace cro
{
    /*!
    \brief Wavetable wrapper class.
    Although raw wavetables are available in Util::Wavetable sometimes
    it is necessary to step through with accurate timing for visual effects.
    As the default timestep in crogine is not fixed this class wraps the
    wavetable vector and ensures that the returned value is accurate for
    the given frame time.
    */
    class CRO_EXPORT_API Wavetable final
    {
    public:
        enum class Waveform
        {
            Sine,
            Square,
            Triangle
        };
        /*!
        \brief Constructs a wavetable object from the given parameters.
        This calls the corresponding wavetable generator from Util::Wavetable
        to populate the internal vector.
        \param waveform Waveform enum which declares the expected shape of the
        waveform stored in the table
        \param frequency The frequency of a single cycle of the wavetable
        \param amplitude Amplitude of the waveform. Default is +- 1.0
        \param updateRate Rate at which the next sample of the wavetable is fetched.
        Lower values require less memory but create aliasing of the waveform.
        Defaults to 30 times per second.
        */
        Wavetable(Waveform wavform, float frequency, float amplitude = 1.f, float updateRate = 30.f);

        /*!
        \brief Creates a Wavetable object from a user array.
        \param array A std::vector of floats containing the wavetable
        \param updateRate The rate at which this waveform expects to be updated.
        */
        Wavetable(const std::vector<float>& array, float updateRate);

        /*!
        \brief Fetches the next sample of the wavetable according to the given frame time
        \param dt Time since the last sample was fetched from the wavetable
        */
        float fetch(float) const;

    private:
        float m_timestep;
        mutable float m_accumulator;
        mutable std::size_t m_currentIndex;
        std::vector<float> m_wavetable;
    };
}