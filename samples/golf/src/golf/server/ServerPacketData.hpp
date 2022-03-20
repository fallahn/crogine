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
#include "../Terrain.hpp"

#include <crogine/ecs/Entity.hpp>
#include <crogine/detail/glm/vec3.hpp>

struct ActivePlayer
{
    glm::vec3 position = glm::vec3(0.f);
    std::uint8_t client = 0;
    std::uint8_t player = 0;
    std::uint8_t terrain = TerrainID::Fairway;
};

static inline bool operator == (const ActivePlayer& a, const ActivePlayer& b)
{
    return (a.client == b.client) && (a.player == b.player);
}

static inline bool operator != (const ActivePlayer& a, const ActivePlayer& b)
{
    return !(a == b);
}

struct PlayerStatus final : public ActivePlayer
{
    cro::Entity ballEntity;
    float distanceToHole = 0.f; //used for sorting
    std::vector<std::uint8_t> holeScore;
    std::uint8_t totalScore = 0;

    bool readyQuit = false; //used at round end to see if all players want to skip scores
};

struct ActorInfo final
{
    std::array<std::int16_t, 4u> rotation = {};
    std::uint32_t serverID = 0;
    glm::vec3 position = glm::vec3(0.f);
    std::int32_t timestamp = 0;
    std::uint8_t clientID = 0;
    std::uint8_t playerID = 0;
    std::uint8_t state = 0;
};

static inline bool operator == (const ActorInfo& actor, const ActivePlayer& player)
{
    return (player.player == actor.playerID && player.client == actor.clientID);
}

struct ScoreUpdate final
{
    float strokeDistance = 0.f;
    std::uint8_t client = 0;
    std::uint8_t player = 0;
    std::uint8_t stroke = 0;
    std::uint8_t hole = 0;
    std::uint8_t score = 0;
};

struct BallUpdate final
{
    glm::vec3 position = glm::vec3(0.f);
    std::uint8_t terrain = 0;
};

struct BilliardsUpdate final
{
    std::array<std::int16_t, 4u> rotation = {};
    std::uint32_t serverID = 0;
    glm::vec3 position = glm::vec3(0.f);
    std::int32_t timestamp = 0;
};