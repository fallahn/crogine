/*-----------------------------------------------------------------------

Matt Marchant 2021
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
#include <cstring>
#include <array>
#include <vector>

#include <crogine/core/String.hpp>
#include <crogine/network/NetData.hpp>
#include <crogine/detail/glm/gtc/quaternion.hpp>

namespace ConstVal
{
    //max string vars for name/limiting packet size
    static constexpr std::size_t MaxStringChars = 24;
    //this is sent as a byte in packet headers - so don't increase MaxStringChars above 32!!
    static constexpr std::size_t MaxStringDataSize = MaxStringChars * sizeof(std::uint32_t);

    static constexpr std::uint16_t GamePort = 16002;
    static constexpr std::size_t MaxClients = 4;
    static constexpr uint8_t NetChannelReliable = 1;
    static constexpr uint8_t NetChannelStrings = 2;

    //rather than tag each player input with the same
    //value and sending over the network, assume this
    //is the delta between updates (as the engine is
    //fixed for this anyway)
    static constexpr float FixedGameUpdate = 1.f / 60.f;

    //cos we use Tiled to make the maps we have to rescale pixels into
    //something a bit more sensible - this is Tiled units per metre
    static constexpr float MapUnits = 128.f;
}

namespace Util
{
    static inline cro::FloatRect expand(cro::FloatRect rect, float amount)
    {
        rect.left -= (amount / 2.f);
        rect.bottom -= (amount / 2.f);
        rect.width += amount;
        rect.height += amount;
        return rect;
    }

    static inline std::int32_t direction(std::int32_t i)
    {
        return (1 + (i * -2));
    }

    static inline std::vector<std::uint8_t> createStringPacket(const cro::String& str)
    {
        std::uint8_t size = static_cast<std::uint8_t>(std::min(ConstVal::MaxStringDataSize, str.size() * sizeof(std::uint32_t)));
        std::vector<std::uint8_t> buffer(size + 1);
        buffer[0] = size;

        //don't try and copy empty data...
        if (size > 0)
        {
            std::memcpy(&buffer[1], str.data(), size);
        }

        return buffer;
    }

    static inline cro::String readStringPacket(const cro::NetEvent::Packet& packet)
    {
        if (packet.getSize() > 0)
        {
            std::uint8_t size = static_cast<const std::uint8_t*>(packet.getData())[0];
            size = std::min(size, static_cast<std::uint8_t>(ConstVal::MaxStringDataSize));

            if (size % sizeof(std::uint32_t) == 0)
            {
                std::vector<std::uint32_t> buffer(size / sizeof(std::uint32_t));
                std::memcpy(buffer.data(), static_cast<const std::uint8_t*>(packet.getData()) + 1, size);

                return cro::String::fromUtf32(buffer.begin(), buffer.end());
            }
        }

        return "Bad packet data";
    }
}