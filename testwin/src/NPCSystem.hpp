/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#ifndef TL_NPC_SYSTEM_HPP_
#define TL_NPC_SYSTEM_HPP_

#include <crogine/ecs/System.hpp>

#include <glm/vec3.hpp>

#include <vector>

struct EliteNavigator final
{
    glm::vec3 destination;
    bool active = false;
    float pauseTime = 1.f;
    cro::uint16 movementCount = 5;
};

struct ChoppaNavigator final
{
    std::size_t tableIndex = 0;
    float moveSpeed = -0.9f;
    bool inCombat = true;
    glm::vec3 deathVelocity;
};

struct Npc final
{
    enum
    {
        Elite,
        Choppa,
        Turret
    }type;

    bool active = true;
    bool wantsReset = false;
    float health = 0.f;


    EliteNavigator elite;
    ChoppaNavigator choppa;
};

class NpcSystem final : public cro::System
{
public:
    NpcSystem(cro::MessageBus&);

    void handleMessage(const cro::Message&) override;
    void process(cro::Time) override;

private:

    float m_accumulator;
    glm::vec3 m_playerPosition;

    std::vector<glm::vec3> m_elitePositions;
    std::vector<float> m_choppaTable;

    void processElite(cro::Entity);
    void processChoppa(cro::Entity);
    void processTurret(cro::Entity);

    void onEntityAdded(cro::Entity) override;
};

#endif //TL_NPC_SYSTEM_HPP_