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

#include <crogine/ecs/Entity.hpp>
#include <crogine/detail/glm/vec3.hpp>

struct ActivePlayer
{
    std::uint8_t client = 4;
    std::uint8_t player = 4;
    glm::vec3 position = glm::vec3(0.f);
};

struct PlayerStatus final : public ActivePlayer
{
    std::uint8_t stroke = 0;
    std::uint8_t score = 0;
    float distanceToHole = 0.f; //used for sorting
    cro::Entity ballEntity;
};

struct ActorInfo final
{
    std::uint32_t serverID = 0;
    glm::vec3 position = glm::vec3(0.f);
};

//using CompressedQuat = std::array<std::int16_t, 4u>;

//struct ActorUpdate final
//{
//    CompressedQuat rotation{};
//    glm::vec3 position = glm::vec3(0.f);
//    std::uint32_t serverID = 0;
//    std::int32_t timestamp = 0;
//    std::int8_t actorID = -1;
//};