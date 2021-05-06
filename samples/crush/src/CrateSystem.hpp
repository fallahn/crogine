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

#include <crogine/ecs/System.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/detail/glm/vec3.hpp>

struct Crate final
{
    enum State
    {
        Idle, Falling, Ballistic,

        Carried //technically invisible, just disable collision
    }state = Falling;

    std::int32_t owner = -1;

    std::uint16_t collisionFlags = 0;
    std::uint8_t collisionLayer = 0;
    glm::vec3 velocity = glm::vec3(0.f);

    glm::vec3 spawnPosition = glm::vec3(0.f);

    bool local = false;
    static constexpr std::int32_t DefaultHealth = 2;
    std::int32_t health = DefaultHealth;

    float sleepTimer = 0.f;
};

class CrateSystem final : public cro::System
{
public:
    explicit CrateSystem(cro::MessageBus&);

    void process(float) override;

    const std::vector<cro::Entity>& getDeadCrates() const { return m_deadCrates; }

private:

    std::vector<cro::Entity> m_deadCrates;

    void processLocal(cro::Entity);
    
    void processIdle(cro::Entity);
    void processCarried(cro::Entity);
    void processFalling(cro::Entity);
    void processBallistic(cro::Entity);

    std::vector<cro::Entity> doBroadPhase(cro::Entity);

    struct PlayerCollision final
    {
        std::int8_t owner = -1;
        cro::Entity player;
    };
    void killPlayer(PlayerCollision&, Crate&);
};