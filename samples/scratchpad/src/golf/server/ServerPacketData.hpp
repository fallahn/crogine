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

#include "../CommonConsts.hpp"

#include <crogine/detail/glm/vec3.hpp>
#include <array>

using CompressedQuat = std::array<std::int16_t, 4u>;

struct LobbyData final
{
    std::uint8_t playerID = 4;
    std::uint8_t skinFlags = 0;
    std::uint8_t stringSize = 0;
};

struct PlayerInfo final
{
    CompressedQuat rotation{};
    glm::vec3 spawnPosition = glm::vec3(0.f);
    std::uint32_t serverID = 0;
    std::int32_t timestamp = 0;
    std::uint8_t playerID = ConstVal::MaxClients;
};

struct PlayerUpdate final
{
    CompressedQuat rotation{};
    glm::vec3 position = glm::vec3(0.f);
    std::int16_t pitch = 0;
    std::int16_t yaw = 0;
    std::uint32_t timestamp = 0;
};

struct ActorUpdate final
{
    CompressedQuat rotation{};
    glm::vec3 position = glm::vec3(0.f);
    std::uint32_t serverID = 0;
    std::int32_t timestamp = 0;
    std::int8_t actorID = -1;
};