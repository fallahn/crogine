/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include <crogine/core/App.hpp>
#include <crogine/detail/glm/gtc/quaternion.hpp>

#include <cstdint>
#include <cstddef>
#include <array>
#include <string>

struct MixerChannel final
{
    enum
    {
        Music, Effects, Menu,
        Voice, Vehicles, Environment,
        UserMusic,

        Count
    };
};

namespace ConstVal
{
    static constexpr float MinMouseSpeed = 0.5f;
    static constexpr float MaxMouseSpeed = 2.f;

    static constexpr float MinSwingputThresh = 0.2f;
    static constexpr float MaxSwingputThresh = 10.f;

    static constexpr std::int32_t MaxProfiles = 64;
    static constexpr std::uint32_t MaxBalls = 64u;
    static constexpr std::uint32_t MaxHeadwear = 64u;

    //max string vars for name/limiting packet size
    static constexpr std::size_t MaxStringChars = 24;
    static constexpr std::size_t MaxNameChars = 12;
    static constexpr std::size_t MaxIPChars = 15;
    //this is sent as a byte in packet headers - so don't increase MaxStringChars above 32!!
    static constexpr std::size_t MaxStringDataSize = MaxStringChars * sizeof(std::uint32_t);

    static constexpr std::uint16_t GamePort = 16002;
    static constexpr std::uint8_t MaxClients = 8;
    static constexpr std::uint8_t MaxPlayers = 8;
    static constexpr std::uint8_t NullValue = 255;
    static constexpr std::uint8_t NetChannelReliable = 1;
    static constexpr std::uint8_t NetChannelStrings = 2;

    static constexpr std::uint16_t PositionCompressionRange = 4;
    static constexpr std::uint16_t VelocityCompressionRange = 8;

    //rather than tag each player input with the same
    //value and sending over the network, assume this
    //is the delta between updates (as the engine is
    //fixed for this anyway)
    static constexpr float FixedGameUpdate = 1.f / 60.f;

    //root dir for course files prepended to directory
    //received from the hosting client
    static const std::string MapPath("assets/golf/courses/");
    static const std::string UserCoursePath("courses/");
    static const std::string UserMapPath("courses/export/");

    static const std::uint8_t SummaryTimeout = 30;
}