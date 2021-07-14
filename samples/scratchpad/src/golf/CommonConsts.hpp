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

#include <crogine/detail/glm/gtc/quaternion.hpp>

#include <cstdint>
#include <cstddef>
#include <array>

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
}