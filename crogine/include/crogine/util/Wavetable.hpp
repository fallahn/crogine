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
#include <crogine/util/Random.hpp>

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
            \brief Returns a vector containing a looping table of 1D noise
            for the given duration at the given sample rate
            \param duration Number of seconds the noise table should last for
            \param amplitude The maxmium value of the voise table. Defaults to 1
            \param sampleRate The rate at which this table will be iterated in fps. Defaults to 60
            */
            static inline std::vector<float> noise(float duration, float amplitude = 1.f, float sampleRate = 60.f)
            {
                CRO_ASSERT(duration > 0.f, "Must be greater than 0");
                CRO_ASSERT(amplitude > 0.f, "Must be greater than 0");
                CRO_ASSERT(sampleRate > 0.f, "Must be greater than 0");


                //TODO we could vary this size with a frequency value
                //although you'd get incorrect results once the frequency >= sample rate
                //and dubious results >= sampleRate/2 (nystrom wossit)
                static constexpr float Frequency = 20.f;
                const auto KeyFrameCount = std::max(1.f, std::floor(Frequency * duration));

                std::vector<float> keyFrames; 
                for (auto i = 0; i < KeyFrameCount; ++i)
                {
                    keyFrames.push_back(cro::Util::Random::value(-amplitude, amplitude));
                }
                keyFrames.push_back(keyFrames[0]); //smooth looping

                const auto FrameCount = duration * sampleRate;
                const auto StepCount = std::max(1.f, std::floor(FrameCount / KeyFrameCount));

                //interp keyframes into output
                std::vector<float> retVal;
                for (auto i = 0u; i < keyFrames.size() - 1; ++i)
                {
                    for (float j = 0.f; j < StepCount; ++j)
                    {
                        retVal.push_back(glm::mix(keyFrames[i], keyFrames[i + 1], j / StepCount));
                    }
                }

                return retVal;
            }
        }
    }
}