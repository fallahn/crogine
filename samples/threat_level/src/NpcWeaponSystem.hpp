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

#ifndef TL_NPC_WEAPONS_SYSTEM_HPP_
#define TL_NPC_WEAPONS_SYSTEM_HPP_

#include <crogine/ecs/System.hpp>

#include <crogine/detail/glm/vec3.hpp>

#include <vector>

struct NpcWeapon final
{
    enum
    {
        Pulse,
        Laser,
        Orb,
        Missile
    }type;
    float damage = 0.f;
    glm::vec3 velocity;
};

class NpcWeaponSystem final : public cro::System
{
public:
    explicit NpcWeaponSystem(cro::MessageBus&);

    void handleMessage(const cro::Message&) override;
    void process(cro::Time) override;

private:
    float m_accumulator;
    float m_backgroundSpeed;

    void onEntityAdded(cro::Entity) override;

    void processPulse(std::size_t&, float);
    void processLaser(cro::Entity);
    void processOrb(std::size_t&, float);
    void processMissile(cro::Entity);

    std::size_t m_orbCount;
    std::vector<cro::int32> m_aliveOrbs;
    std::size_t m_deadOrbCount;
    std::vector<cro::int32> m_deadOrbs;

    std::vector<cro::int32> m_activeLasers;

    std::size_t m_alivePulseCount;
    std::vector<cro::int32> m_alivePulses;
    std::size_t m_deadPulseCount;
    std::vector<cro::int32> m_deadPulses;
};

#endif //TL_NPC_WEAPONS_SYSTEM_HPP_