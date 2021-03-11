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

#include <crogine/detail/glm/vec3.hpp>

#include <vector>

struct EliteNavigator final
{
    glm::vec3 destination = glm::vec3(0.f);
    bool active = false;
    float pauseTime = 1.f;
    std::uint16_t movementCount = 5;
    float firetime = 4.f;
    std::size_t idleIndex = 0;
    float maxEmitRate = 0.f;
    bool dying = false;
    glm::vec3 deathVelocity = glm::vec3(0.f);
};

struct ChoppaNavigator final
{
    std::size_t tableIndex = 0;
    float moveSpeed = -0.8f;
    bool inCombat = true;
    glm::vec3 deathVelocity = glm::vec3(0.f);
    std::uint8_t ident = 0;
    float shootTime = 0.f;
    static constexpr float nextShootTime = 0.08f;
    static constexpr float spacing = 1.4f;
};

struct SpeedrayNavigator final
{
    std::size_t tableIndex = 0;
    float moveSpeed = -3.f;
    std::uint8_t ident = 0;
    float shootTime = 0.f;
    static constexpr float nextShootTime = 0.8f;
    static constexpr std::size_t count = 5;
    static constexpr float spacing = 1.4f;
};

struct WeaverNavigator final
{
    std::size_t tableIndex = 0;
    std::size_t tableStartIndex = 0;
    float moveSpeed = -2.7f;
    float yPos = 0.5f;
    glm::vec3 velocity = glm::vec3(0.f);
    std::uint8_t ident = 0;
    bool dying = false;
    float dyingTime = 0.f;
    float shootTime = 0.f;
    static constexpr float nextShootTime = 2.f;
    static constexpr float spacing = 0.3f;
    static constexpr std::size_t count = 7;
};

struct Npc final
{
    enum
    {
        Elite,
        Choppa,
        Turret,
        Speedray,
        Weaver
    }type;

    bool active = false;
    bool wantsReset = false;
    float health = 0.f;
    float prevHealth = 0.f;
    std::int32_t scoreValue = 0;
    bool hasDyingAnim = false;

    EliteNavigator elite;
    ChoppaNavigator choppa;
    SpeedrayNavigator speedray;
    WeaverNavigator weaver;
};

//used to reference associated
//entities with turrets
struct Family final
{
    cro::Entity parent;
    cro::Entity child;
};

class NpcSystem final : public cro::System
{
public:
    explicit NpcSystem(cro::MessageBus&);

    void handleMessage(const cro::Message&) override;
    void process(float) override;

private:
    glm::vec3 m_playerPosition = glm::vec3(0.f);
    bool m_empFired;
    bool m_awardPoints;

    std::vector<glm::vec3> m_elitePositions;
    std::vector<glm::vec3> m_eliteIdlePositions;
    std::vector<float> m_choppaTable;
    std::vector<float> m_speedrayTable;
    std::vector<float> m_weaverTable;

    void processElite(cro::Entity, float);
    void processChoppa(cro::Entity, float);
    void processTurret(cro::Entity, float);
    void processSpeedray(cro::Entity, float);
    void processWeaver(cro::Entity, float);

    void onEntityAdded(cro::Entity) override;
};

#endif //TL_NPC_SYSTEM_HPP_