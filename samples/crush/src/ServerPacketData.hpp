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

#include <crogine/detail/glm/vec3.hpp>
#include <array>

using CompressedQuat = std::array<std::int16_t, 4u>;
using CompressedVec3 = std::array<std::int16_t, 3u>;
using CompressedVec2 = std::array<std::int16_t, 2u>;

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
    std::uint8_t connectionID = ConstVal::MaxClients;
};

struct Player;
struct PlayerUpdate final
{
    CompressedQuat rotation{};
    CompressedVec3 position = {};
    CompressedVec3 velocity = {};
    std::int32_t timestamp = 0;

    std::uint16_t prevInputFlags = 0; //hmmmmmmmmm ought to be able to pull this from the history rather than send it
    std::uint16_t collisionFlags = 0;
    std::uint8_t puntLevel; //normalised 255 range
    std::uint8_t state = 0; //this depends on how many states we'll have in total
    

    /*
    | unused | carry flag | direction | layer | ID
    |  000         0            0         0     00
    */
    std::uint8_t bitfield = 0;
    static constexpr std::uint8_t CarryBit     = (1 << 4);
    static constexpr std::uint8_t DirectionBit = (1 << 3);
    static constexpr std::uint8_t LayerBit     = (1 << 2);

    std::uint8_t getPlayerID() const;
    void pack(const Player&);
    void unpack(Player&) const;
};

struct ActorUpdate final
{
    CompressedQuat rotation{};
    CompressedVec3 position{};
    CompressedVec2 velocity{};
    std::int32_t timestamp = 0;
    std::uint16_t serverID = 0;
    std::int8_t actorID = -1;
};

struct ActorIdleUpdate final
{
    std::int32_t timestamp = 0;
    std::uint16_t serverID = 0;
    std::int8_t actorID = -1;
};

struct ActorRemoved final
{
    std::int32_t timestamp = std::numeric_limits<std::int32_t>::max();
    std::uint16_t serverID = 0;
};

struct CrateState final
{
    std::uint16_t serverEntityID = 0;
    std::int8_t state = -1;
    std::int8_t owner = -1;
    std::uint8_t collisionLayer = 0;
};

struct PlayerStateChange final
{
    std::uint16_t serverEntityID = 0;
    std::int8_t playerState = -1;
    std::int8_t playerID = -1;
    std::uint8_t lives = 3;
};

struct SnailState final
{
    std::uint16_t serverEntityID = 0;
    std::int8_t state = -1;
};