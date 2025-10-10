/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2025
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

#ifdef _MSC_VER
#define NOMINMAX
#endif //_MSC_VER

#include "Constants.hpp"
#include <crogine/util/Spline.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

#include <type_traits>
#include <algorithm>

namespace cro
{
    namespace Util
    {
        namespace Maths
        {
            template <typename T>
            T clamp(T value, T min, T max)
            {
                static_assert(std::is_pod<T>::value, "Only available on POD");
                return std::min(max, std::max(min, value));
            }

            /*!
            \brief finds the shortest rotation, in radians, between the given start and end angles
            */
            static inline float shortestRotation(float start, float end)
            {
                float diff = end - start;
                if (diff > Const::PI) diff -= Const::TAU;
                else if (diff < -Const::PI) diff += Const::TAU;
                return diff;
            }

            /*!
            \brief Returns the normalised sign of the given value
            \param an int convertible value
            \returns -1 if the given value is negative, 1 if it is positive or 0
            if it has no value.
            */
            template <typename T>
            constexpr std::int32_t sgn(T val)
            {
                static_assert(std::is_convertible<T, std::int32_t>::value);
                return (T(0) < val) - (val < T(0));
            }


            /*!
            \brief Smoothly interpolate two vectors over the given time
            from Game Programming Gems 4 Chapter 1.10
            \param src The current value to start from
            \param dest The value to interpolate towards
            \param currVel The current velocity, to be kept between frames
            \param smoothTime The target amount of time to take to interpolate
            \param dt Current frame time
            \param maxSpeed Maximum velocity of movement
            */
            template <typename T>
            T smoothDamp(T src, T dst, T& currVel, float smoothTime, float dt, float maxSpeed = std::numeric_limits<float>::max())
            {
                smoothTime = std::max(0.0001f, smoothTime);
                const float omega = 2.f / smoothTime;
                const float x = omega * dt;
                const float exp = 1.f / (1.f + x + 0.48f * x * x + 0.235f * x * x * x);

                T diff = src - dst;
                //const T originalDst = dst;

                //clamp maximum speed - a bit of a fudge here depending
                //if T is float or a vector
                const float maxDiff = maxSpeed * smoothTime;

                if constexpr (std::is_same<T, float>::value)
                {
                    diff = std::clamp(diff, -maxDiff, maxDiff);
                }
                else
                {
                    const float maxDiff2 = maxDiff * maxDiff;
                    float len2 = glm::length2(diff);
                    if (len2 > maxDiff2)
                    {
                        auto len = glm::sqrt(len2);
                        diff /= len;
                        diff *= maxDiff;
                    }
                }

                dst = src - diff;

                const T temp = (currVel + (diff * omega)) * dt;
                currVel = (currVel - (temp * omega)) * exp;
                return dst + (diff + temp) * exp;
            }

            /*!
            \brief Converts the given floating point value to a normalised integer
            Note that this removes any whole part and works on the truncated fraction
            */
            template <typename T>
            constexpr T normaliseTo(float v)
            {
                static_assert(std::is_pod<T>::value, "");
                if constexpr (std::is_unsigned<T>::value)
                {
                    CRO_ASSERT(v >= 0, "");
                    return static_cast<T>(static_cast<float>(std::numeric_limits<T>::max()) * v);
                }
                else
                {
                    const auto s = Util::Maths::sgn(v);

                    const auto vs = v * s;
                    v = (vs - std::floor(vs));
                    return static_cast<T>(static_cast<float>(std::numeric_limits<T>::max()) * v) * s;
                }
            }
        }
    }
}