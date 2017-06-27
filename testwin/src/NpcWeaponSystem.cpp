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

#include "NpcWeaponSystem.hpp"
#include "NPCSystem.hpp"
#include "Messages.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/PhysicsObject.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/core/Clock.hpp>

namespace
{
    const float fixedStep = 1.f / 60.f;
    const float speedMultiplier = 21.3f; //approx chunk size of terrain

    const float orbSpeed = 4.f;
}

NpcWeaponSystem::NpcWeaponSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(NpcWeaponSystem)),
    m_accumulator       (0.f),
    m_backgroundSpeed   (0.f),
    m_orbCount          (0),
    m_deadOrbCount      (0)
{
    requireComponent<NpcWeapon>();
    requireComponent<cro::Transform>();
    requireComponent<cro::PhysicsObject>();
}

//public
void NpcWeaponSystem::handleMessage(const cro::Message& msg)
{
    if (msg.id == MessageID::BackgroundSystem)
    {
        const auto& data = msg.getData<BackgroundEvent>();
        if (data.type == BackgroundEvent::SpeedChange)
        {
            m_backgroundSpeed = data.value * speedMultiplier;
            //m_offset = 0.f;
        }
        //else if (data.type == BackgroundEvent::Shake)
        //{
        //    m_offset = data.value * chunkWidth;
        //    //DPRINT("Offset", std::to_string(m_offset));
        //}
    }
    else if (msg.id == MessageID::NpcMessage)
    {
        const auto& data = msg.getData<NpcEvent>();
        if (data.type == NpcEvent::FiredWeapon)
        {
            switch (data.npcType)
            {
            default:break;
            case Npc::Turret:
            {
                if (m_deadOrbCount > 0)
                {
                    m_deadOrbCount--;
                    auto entity = getScene()->getEntity(m_deadOrbs[m_deadOrbCount]);
                    m_aliveOrbs[m_orbCount] = m_deadOrbs[m_deadOrbCount];
                    m_orbCount++;

                    entity.getComponent<cro::Transform>().setPosition(data.position);
                    entity.getComponent<NpcWeapon>().velocity = data.velocity * orbSpeed;
                }
            }
                break;
            }
        }
    }
}

void NpcWeaponSystem::process(cro::Time dt)
{
    //update orbs
    for (std::size_t i = 0u; i < m_orbCount; ++i)
    {
        processOrb(i, dt.asSeconds());
    }    
    
    m_accumulator += dt.asSeconds();

    while (m_accumulator > fixedStep)
    {
        m_accumulator -= fixedStep;
        

    }
}

//private
void NpcWeaponSystem::processLaser(cro::Entity entity) 
{

}

void NpcWeaponSystem::processMissile(cro::Entity entity)
{

}

void NpcWeaponSystem::processOrb(std::size_t& idx, float dt)
{
    auto entity = getScene()->getEntity(m_aliveOrbs[idx]);
    auto& status = entity.getComponent<NpcWeapon>();

    //move
    auto& tx = entity.getComponent<cro::Transform>();
    tx.move(status.velocity * dt);
    tx.move({ -m_backgroundSpeed * dt, 0.f, 0.f });

    //check collision
    if (entity.getComponent<cro::PhysicsObject>().getCollisionCount() > 0)
    {
        //only hits the environment / bounds so remove
        tx.setPosition(glm::vec3(50.f));

        //it died, remove it
        m_deadOrbs[m_deadOrbCount] = m_aliveOrbs[idx];
        m_deadOrbCount++;

        //and remove from alive list
        m_orbCount--;
        m_aliveOrbs[idx] = m_aliveOrbs[m_orbCount];
        idx--; //decrement so we process the swapped in ID
    }
}

void NpcWeaponSystem::processPulse(cro::Entity entity)
{

}

void NpcWeaponSystem::onEntityAdded(cro::Entity entity)
{
    switch (entity.getComponent<NpcWeapon>().type)
    {
    default: break;
    case NpcWeapon::Orb:
        m_deadOrbs.push_back(entity.getIndex());
        m_aliveOrbs.push_back(-1);
        m_deadOrbCount++;
        break;
    }
}