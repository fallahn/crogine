/*-----------------------------------------------------------------------

Matt Marchant 2020
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include <cstdint>
#include <cstddef>
#include <array>

#include <crogine/detail/glm/gtc/quaternion.hpp>

namespace ConstVal
{
    //max string vars for name/limiting packet size
    static std::size_t MaxStringChars = 24;
    //this is sent as a byte in packet headers - so don't increase MaxStringChars above 32!!
    static std::size_t MaxStringDataSize = MaxStringChars * sizeof(std::uint32_t);

    static const std::uint16_t GamePort = 16002;
    static const std::size_t MaxClients = 4;
    static const uint8_t NetChannelReliable = 1;
    static const uint8_t NetChannelStrings = 2;

    //rather than tag each player input with the same
    //value and sending over the network, assume this
    //is the delta between updates (as the engine is
    //fixed for this anyway)
    static const float FixedGameUpdate = 1.f / 60.f;
}

namespace Util
{
    /*
note that 'range' should be the max (and min when negative) value expected to be compressed
by this function. Exceeding the range will cause the value to overflow. However, the smaller
the range value, the more precision that is preserved, so choose this value carefully. Using
a value divisible by 8 bits will help the compiler optimise the conversion with bitshift division.
*/
    static constexpr std::int16_t compressionRange = std::numeric_limits<std::int16_t>::max();
    static inline std::int16_t compressFloat(float input, std::int16_t range = 1024)
    {
        return static_cast<std::int16_t>(std::round(input * compressionRange / range));
    }
    static inline constexpr float decompressFloat(std::int16_t input, std::int16_t range = 1024)
    {
        return static_cast<float>(input) * range / compressionRange;
    }

    static inline std::array<std::int16_t, 4u> compressQuat(glm::quat q)
    {
        std::int16_t w = compressFloat(q.w, 8);
        std::int16_t x = compressFloat(q.x, 8);
        std::int16_t y = compressFloat(q.y, 8);
        std::int16_t z = compressFloat(q.z, 8);

        return { w,x,y,z };
    }

    static inline constexpr glm::quat decompressQuat(std::array<std::int16_t, 4u> q)
    {
        float w = decompressFloat(q[0], 8);
        float x = decompressFloat(q[1], 8);
        float y = decompressFloat(q[2], 8);
        float z = decompressFloat(q[3], 8);

        return /*glm::normalize*/(glm::quat(w, x, y, z));
    }

    static inline std::array<std::int16_t, 3u> compressVec3(glm::vec3 v, std::int16_t range = 256)
    {
        std::int16_t x = compressFloat(v.x, range);
        std::int16_t y = compressFloat(v.y, range);
        std::int16_t z = compressFloat(v.z, range);

        return { x,y,z };
    }

    static inline glm::vec3 decompressVec3(std::array<std::int16_t, 3u> v, std::int16_t range = 256)
    {
        float x = decompressFloat(v[0], range);
        float y = decompressFloat(v[1], range);
        float z = decompressFloat(v[2], range);

        return { x, y, z };
    }
}