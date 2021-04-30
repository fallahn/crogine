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

#include <crogine/detail/glm/vec2.hpp>
#include <crogine/ecs/System.hpp>

struct Snail final
{
    enum
    {
        Falling, Idle, Walking,
        Digging, Dead
    }state = Falling;

    float sleepTimer = 0.f;

    glm::vec2 velocity = glm::vec2(0.f);
    std::uint16_t collisionFlags = 0;
    std::uint8_t collisionLayer = 0;

    std::uint8_t idleCount = 0;
    float currentRotation = 0.f;

    bool local = false;
};

class SnailSystem final : public cro::System
{
public:
    explicit SnailSystem(cro::MessageBus&);

    void process(float) override;

    const std::vector<cro::Entity>& getDeadSnails() const { return m_deadSnails; }

private:

    std::vector<cro::Entity> m_deadSnails;

    void processLocal(cro::Entity);

    void processFalling(cro::Entity);
    void processIdle(cro::Entity);
    void processWalking(cro::Entity);
    void processDigging(cro::Entity);

    std::vector<cro::Entity> doBroadPhase(cro::Entity);
    void killPlayer(cro::Entity player, Snail&);
};