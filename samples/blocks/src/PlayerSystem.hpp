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

#include "InputParser.hpp"

#include <crogine/ecs/System.hpp>
#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/glm/vec3.hpp>
#include <crogine/graphics/BoundingBox.hpp>

#include <array>

struct PlayerUpdate;
struct Player final
{
    static const cro::Box aabb;

    //if we assume (and we should) 60 updates per second
    //this will buffer 2 seconds worth of input
    static constexpr std::size_t HistorySize = 120;
    std::array<Input, HistorySize> inputStack = {};
    std::size_t nextFreeInput = 0; //POST incremented after adding new input to history
    std::size_t lastUpdatedInput = HistorySize - 1; //index of the last parsed input

    glm::vec3 spawnPosition = glm::vec3(0.f);

    float cameraPitch = 0.f; //used to clamp camera
    float cameraYaw = 0.f; //used to calc forward vector

    bool flyMode = true;
    bool waitResync = false; //waiting to resync player with server

    glm::ivec3 targetBlockPosition = glm::ivec3(0);

    std::uint8_t id = 4; //this should be the same as the ActorID for this entity
};

namespace vx
{
    class DataManager;
}

class ChunkManager;
class PlayerSystem final : public cro::System
{
public:
    PlayerSystem(cro::MessageBus&, const ChunkManager&, const vx::DataManager&);

    void handleMessage(const cro::Message&) override;

    void process(float) override;

    void reconcile(cro::Entity, const PlayerUpdate&);

private:

    const ChunkManager& m_chunkManager;
    const vx::DataManager& m_voxelData;

    void processInput(cro::Entity);
    void processMovement(cro::Entity, Input);
    void processCollision(cro::Entity);
};
