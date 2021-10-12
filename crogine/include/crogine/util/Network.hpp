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

#include <crogine/Config.hpp>

#include <crogine/detail/glm/gtc/quaternion.hpp>
#include <crogine/detail/glm/vec3.hpp>
#include <crogine/detail/glm/vec2.hpp>

#include <cstdint>
#include <limits>
#include <cmath>
#include <array>
#include <vector>
#include <string>

namespace cro
{
    namespace Util
    {
        namespace Net
        {
            static constexpr std::int16_t CompressionRange = std::numeric_limits<std::int16_t>::max();

            /*
            \brief Compresses a float value into a 16 bit fixed point representation to send over
            a network connection. This compression is lossy.

            \param input The floating point value to compress
            \param range The max/min value within which input is expected to fall

            Note that 'range' should be the max (and min when negative) value expected to be compressed
            by this function. Exceeding the range will cause the value to overflow. However, the smaller
            the range value, the more precision that is preserved, so choose this value carefully. Using
            a value divisible by 8 bits will help the compiler optimise the conversion with bitshift division.

            Based on the stack overflow answer given here: https://stackoverflow.com/a/1659563/

            \returns 16 bit integer containing the compressed value of the float
            \see decompressFloat()
            */
            static inline std::int16_t compressFloat(float input, std::int16_t range = 1024)
            {
                return static_cast<std::int16_t>(std::round(input * CompressionRange / range));
            }

            /*!
            \brief Decompresses the 16 bit value into a floating point number
            \param input The value to decompress
            \param range The range with which the value was compressed. Incorrect values will give unexpected
            results.
            \returns Decompressed floating point value.
            \see compressFloat()
            */
            static inline constexpr float decompressFloat(std::int16_t input, std::int16_t range = 1024)
            {
                return static_cast<float>(input) * range / CompressionRange;
            }

            /*!
            \brief Compresses a quaternion
            Component values are expected to be in the range -1 to 1 and has a max
            limit of +/- 8
            \returns Array of 4 16 bit integers
            \see decompressQuat() compressFloat()
            */
            static inline std::array<std::int16_t, 4u> compressQuat(glm::quat q)
            {
                std::int16_t w = compressFloat(q.w, 8);
                std::int16_t x = compressFloat(q.x, 8);
                std::int16_t y = compressFloat(q.y, 8);
                std::int16_t z = compressFloat(q.z, 8);

                return { w,x,y,z };
            }

            /*!
            \brief Decompresses a quaternion
            \param q An array of 4 16 bit integers compressed with compressQuat()
            \returns A glm::quat. Note that this value is unlikely to be normalised.
            \see compressQuat() compressFloat()
            */
            static inline constexpr glm::quat decompressQuat(std::array<std::int16_t, 4u> q)
            {
                float w = decompressFloat(q[0], 8);
                float x = decompressFloat(q[1], 8);
                float y = decompressFloat(q[2], 8);
                float z = decompressFloat(q[3], 8);

                return /*glm::normalize*/(glm::quat(w, x, y, z));
            }

            /*!
            \brief Compresses a 3 component vector into an array of 3 16 bit integers
            \param v The vector to compress
            \param range The positive/negative range within which the component values
            are expected to fall
            \returns Array of 3 16 bit integers
            \see decompressVec3() compressFloat()
            */
            static inline std::array<std::int16_t, 3u> compressVec3(glm::vec3 v, std::int16_t range = 256)
            {
                std::int16_t x = compressFloat(v.x, range);
                std::int16_t y = compressFloat(v.y, range);
                std::int16_t z = compressFloat(v.z, range);

                return { x,y,z };
            }

            /*!
            \brief Decompresses an array of 16 bit integers compressed with compressVec3
            \param v An array of 16 bit integers
            \param range The range value with which the vector was compressed.
            \returns A glm::vec3
            \see compressVec3() compressFloat()
            */
            static inline glm::vec3 decompressVec3(std::array<std::int16_t, 3u> v, std::int16_t range = 256)
            {
                float x = decompressFloat(v[0], range);
                float y = decompressFloat(v[1], range);
                float z = decompressFloat(v[2], range);

                return { x, y, z };
            }

            /*!
            \brief Compresses a 2 component vector into an array of 2 16 bit integers
            \param v The vector to compress
            \param range The positive/negative range within which the component values
            are expected to fall
            \returns Array of 2 16 bit integers
            \see decompressVec2() compressFloat()
            */
            static inline std::array<std::int16_t, 2u> compressVec2(glm::vec2 v, std::int16_t range = 256)
            {
                std::int16_t x = compressFloat(v.x, range);
                std::int16_t y = compressFloat(v.y, range);

                return { x,y };
            }

            /*!
            \brief Decompresses an array of 16 bit integers compressed with compressVec2
            \param v An array of 16 bit integers
            \param range The range value with which the vector was compressed.
            \returns A glm::vec2
            \see compressVec2() compressFloat()
            */
            static inline glm::vec2 decompressVec2(std::array<std::int16_t, 2u> v, std::int16_t range = 256)
            {
                float x = decompressFloat(v[0], range);
                float y = decompressFloat(v[1], range);

                return { x, y };
            }

            /*!
            \brief Returns a list of IPv4 Addresses associated with available network adapters
            */
            std::vector<std::string> CRO_EXPORT_API getLocalAddresses();
        }
    }
}