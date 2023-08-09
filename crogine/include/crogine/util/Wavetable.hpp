/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2023
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
#include <crogine/util/Constants.hpp>

#include <vector>
#include <cmath>

namespace cro
{
    namespace Util
    {
        namespace Wavetable
        {
            /*!
            \brief Creates a vector containing a series of floating point values
            representing a single cycle of a sine wave.

            \param frequency Frequency of generated sine wave
            \param amplitude Amplitude of generated sine wave
            \param updateRate Rate at which the wave table is likely to be stepped through, normally
            the same rate at which the framework is updated (60hz)
            Sine waves are useful for a variety of things, including dictating the motion of animated
            objects. Precalculating a wavetable is more efficient than repeatedly calling sin()
            */
            static inline std::vector<float> sine(float frequency, float amplitude = 1.f, float updateRate = 60.f)
            {
                CRO_ASSERT(frequency > 0 && amplitude > 0 && updateRate > 0, "");

                float stepCount = updateRate / frequency;
                float step = Const::TAU / stepCount;
                std::vector<float> wavetable;
                for (float i = 0.f; i < stepCount; ++i)
                {
                    wavetable.push_back(std::sin(step * i) * amplitude);
                }

                return wavetable;
            }

            /*!
            \brief Creates a vector containing a series of floating point values
            representing a single cycle of a triangle wave.

            \param frequency Frequency of generated wave
            \param amplitude Amplitude of generated wave
            \param updateRate Rate at which the wave table is likely to be stepped through, normally
            the same rate at which the framework is updated (60hz). This would also be the sample rate
            should you be creating a wave table for audio purposes
            */
            static inline std::vector<float> triangle(float frequency, float amplitude = 1.f, float samplerate = 60.f)
            {
                CRO_ASSERT(frequency > 0 && amplitude > 0 && samplerate > 0, "");

                std::vector<float> retval;
                const float stepCount = (samplerate / frequency);
                const float step = Const::TAU / stepCount;
                const float invPi = 1.f / Const::PI;

                for (float i = 0.f; i < stepCount; i++)
                {
                    retval.push_back(invPi * std::asin(std::sin(i * step)) * amplitude);
                }

                return retval;
            }

            /*!
            \brief Creates a vector containing a series of floating point values
            representing a single cycle of a square wave.

            \param frequency Frequency of generated wave
            \param amplitude Amplitude of generated wave
            \param updateRate Rate at which the wave table is likely to be stepped through, normally
            the same rate at which the framework is updated (60hz). This would also be the sample rate
            should you be creating a wave table for audio purposes
            */
            static inline std::vector<float> square(float frequency, float amplitude = 1.f, float samplerate = 60.f)
            {
                CRO_ASSERT(frequency > 0 && amplitude > 0 && samplerate > 0, "");

                std::vector<float> retval;
                const std::int32_t sampleCount = static_cast<std::int32_t>(samplerate / frequency);
                for (auto i = 0; i < sampleCount; ++i)
                {
                    if (i < sampleCount / 2)
                    {
                        retval.push_back(amplitude);
                    }
                    else
                    {
                        retval.push_back(-amplitude);
                    }
                }
                return retval;
            }

            /*!
            \brief Returns a vector containing a table of 1D noise
            for the given duration at the given sample rate
            \param duration Number of seconds the noise table should last for
            \param amplitude The maxmium value of the voise table. Defaults to 1
            \param sampleRate The rate at which this table will be iterated in fps. Defaults to 60
            */
            static inline std::vector<float> noise(float duration, float amplitude = 1.f, float sampleRate = 60.f)
            {
                //static const auto fract = [](float f) {return f - static_cast<long>(f); };
                static const auto rand = [](float n) { return glm::fract(std::sin(n) * 43758.5453123f); };

                std::vector<float> retVal;
                const float step = 1.f / (duration * sampleRate);
                float position = 0.f;
                for (auto i = 0; i < (duration * sampleRate); ++i)
                {
                    retVal.push_back(glm::mix(rand(0.f), rand(1.f), position) * amplitude);

                    position += step;
                }
                return retVal;
            }
        }
    }
}