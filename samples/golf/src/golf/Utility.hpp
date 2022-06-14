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

#include "CommonConsts.hpp"

#include <crogine/core/String.hpp>
#include <crogine/network/NetData.hpp>

#include <vector>
#include <cstring>

/*
first byte is string length in bytes, followed by string utf32 data
string is limited to MaxStringChars
*/
static inline std::vector<std::uint8_t> serialiseString(const cro::String& str)
{
    auto len = std::min(ConstVal::MaxStringChars, str.size());
    auto workingString = str.substr(0, len);

    std::uint8_t size = static_cast<std::uint8_t>(len * sizeof(std::uint32_t));
    std::vector<std::uint8_t> buffer(size + 1);
    buffer[0] = size;
    std::memcpy(&buffer[1], workingString.data(), size);

    return buffer;
}

static inline cro::String deserialiseString(const net::NetEvent::Packet& packet)
{
    CRO_ASSERT(packet.getSize() > 0, "TODO we need better error handling");

    std::uint8_t stringSize = 0;
    std::memcpy(&stringSize, packet.getData(), 1);
    CRO_ASSERT(packet.getSize() == stringSize + 1, "");
    CRO_ASSERT(stringSize % sizeof(std::uint32_t) == 0, "");
    
    std::vector<std::uint32_t> buffer(stringSize / sizeof(std::uint32_t));
    std::memcpy(buffer.data(), static_cast<const std::uint8_t*>(packet.getData()) + 1, stringSize);

    cro::String str = cro::String::fromUtf32(buffer.begin(), buffer.end());
    return str;
}