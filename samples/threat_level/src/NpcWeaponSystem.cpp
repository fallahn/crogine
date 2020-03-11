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
#include "PhysicsObject.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/core/Clock.hpp>

#include <crogine/detail/glm/gtx/norm.hpp>

namespace
{
    const float speedMultiplier = 21.3f; //approx chunk size of terrain

    const float orbSpeed = 4.f;
    const float pulseSpeed = 24.f;
}

NpcWeaponSystem::NpcWeaponSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(NpcWeaponSystem)),
    m_backgroundSpeed   (0.f),
    m_orbCount          (0),
    m_deadOrbCount      (0),
    m_alivePulseCount   (0),
    m_deadPulseCount    (0)
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
            case Npc::Weaver:
            {
                if (m_deadOrbCount > 0)
                {
                    m_deadOrbCount--;
                    auto entity = getScene()->getEntity(m_deadOrbs[m_deadOrbCount]);
                    m_aliveOrbs[m_orbCount] = m_deadOrbs[m_deadOrbCount];
                    m_orbCount++;

                    entity.getComponent<cro::Transform>().setPosition(data.position);
                    entity.getComponent<NpcWeapon>().velocity = data.velocity * orbSpeed;
                    entity.getComponent<NpcWeapon>().damage = 2.f;
                }
            }
                break;
            case Npc::Elite:
            {
                //auto laserEntID = getScene()->getEntity(data.entityID).getComponent<cro::Transform>().getChildIDs()[0];

                //auto id = getScene()->getEntity(laserEntID).getComponent<cro::Transform>().getChildIDs()[0];
                //auto childEnt = getScene()->getEntity(id);
                //childEnt.getComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.f });
                //childEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent());

                //id = getScene()->getEntity(laserEntID).getComponent<cro::Transform>().getChildIDs()[1];
                //childEnt = getScene()->getEntity(id);
                ////don't place here as we'll collide too soon
                ////childEnt.getComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.f });
                //childEnt.getComponent<cro::Sprite>().setColour(cro::Colour::White());

                //m_activeLasers.push_back(laserEntID);
                LOG("Elite lasers are disabled", cro::Logger::Type::Info);
            }
                break;
            case Npc::Choppa:
            case Npc::Speedray:
            {
                if (m_deadPulseCount > 0)
                {
                    m_deadPulseCount--;
                    auto entity = getScene()->getEntity(m_deadPulses[m_deadPulseCount]);
                    m_alivePulses[m_alivePulseCount] = m_deadPulses[m_deadPulseCount];
                    m_alivePulseCount++;

                    entity.getComponent<cro::Transform>().setPosition(data.position);
                    entity.getComponent<NpcWeapon>().velocity.x = -pulseSpeed;
                    entity.getComponent<NpcWeapon>().damage = 10.f;
                }
            }
                break;
            }
        }
        else if (data.type == NpcEvent::Died || data.type == NpcEvent::ExitScreen)
        {
            auto entity = getScene()->getEntity(data.entityID);
            if (entity.getComponent<Npc>().type == Npc::Elite)
            {
                //kill any firing weapons
                //auto laser = std::find_if(std::begin(m_activeLasers), std::end(m_activeLasers),
                //    [&, data](cro::int32 id)
                //{
                //    return getScene()->getEntity(id).getComponent<cro::Transform>().getParentID() == data.entityID;
                //});

                //if (laser != m_activeLasers.end())
                //{
                //    //move the entities out of shot
                //    auto laserEnt = getScene()->getEntity(*laser);
                //    getScene()->getEntity(laserEnt.getComponent<cro::Transform>().getChildIDs()[0]).getComponent<cro::Transform>().setPosition({ 0.f, -200.f, 0.f });
                //    getScene()->getEntity(laserEnt.getComponent<cro::Transform>().getChildIDs()[1]).getComponent<cro::Transform>().setPosition({ 0.f, -200.f, 0.f });

                //    m_activeLasers.erase(laser);
                //}
            }
        }
    }
}

void NpcWeaponSystem::process(float dt)
{    
    //update orbs
    for (std::size_t i = 0u; i < m_orbCount; ++i)
    {
        processOrb(i, dt);
    }    
    
    //update pulses
    for (std::size_t i = 0u; i < m_alivePulseCount; ++i)
    {
        processPulse(i, dt);
    }


    for (auto l : m_activeLasers)
    {
        processLaser(getScene()->getEntity(l), dt);
    }        

}

//private
void NpcWeaponSystem::processPulse(std::size_t& idx, float dt)
{
    auto entity = getScene()->getEntity(m_alivePulses[idx]);
    auto& status = entity.getComponent<NpcWeapon>();

    auto& tx = entity.getComponent<cro::Transform>();
    tx.move(status.velocity * dt);

    //collision
    if (entity.getComponent<cro::PhysicsObject>().getCollisionCount() > 0)
    {
        //all collisions cause a reset
        tx.setPosition(glm::vec3(10.f));

        m_deadPulses[m_deadPulseCount] = m_alivePulses[idx];
        m_deadPulseCount++;

        m_alivePulseCount--;
        m_alivePulses[idx] = m_alivePulses[m_alivePulseCount];
        idx--;
    }
}

void NpcWeaponSystem::processLaser(cro::Entity entity, float dt) 
{
    //TODO disabled until I can be bothered to refactor

    //auto orbEnt = getScene()->getEntity(entity.getComponent<cro::Transform>().getChildIDs()[0]);
    //auto laserEnt = getScene()->getEntity(entity.getComponent<cro::Transform>().getChildIDs()[1]);
    //
    ////fade in orb
    //auto buns = glm::length2(laserEnt.getComponent<cro::Transform>().getPosition());
    //if (buns > 0.2)
    //{
    //    auto alpha = orbEnt.getComponent<cro::Sprite>().getColour().getAlpha();
    //    alpha = std::min(1.f, alpha + (fixedStep/* * 2.f*/));
    //    orbEnt.getComponent<cro::Sprite>().setColour(cro::Colour(1.f, 1.f, 1.f, alpha));

    //    if (alpha == 1)
    //    {
    //        laserEnt.getComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.f });
    //    }
    //    //LOG(std::to_string(buns), cro::Logger::Type::Info);
    //}
    //else //fade out
    //{
    //    //fade out laser
    //    float alpha = laserEnt.getComponent<cro::Sprite>().getColour().getAlpha();

    //    alpha = std::max(0.f, alpha - (fixedStep * 2.f));
    //    laserEnt.getComponent<cro::Sprite>().setColour(cro::Colour(1.f, 1.f, 1.f, alpha));
    //    orbEnt.getComponent<cro::Sprite>().setColour(cro::Colour(1.f, 1.f, 1.f, alpha));

    //    if (alpha == 0) //remove if complete
    //    {
    //        //move sprites out of the way
    //        orbEnt.getComponent<cro::Transform>().setPosition({ 0.f, -50.f, 0.f });
    //        laserEnt.getComponent<cro::Transform>().setPosition({ 0.f, -50.f, 0.f });

    //        m_activeLasers.erase(std::find(m_activeLasers.begin(), m_activeLasers.end(), entity.getIndex()));
    //    }
    //    //LOG("Fade out", cro::Logger::Type::Info);
    //}
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

void NpcWeaponSystem::processMissile(cro::Entity entity)
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
    case NpcWeapon::Pulse:
        m_deadPulses.push_back(entity.getIndex());
        m_alivePulses.push_back(-1);
        m_deadPulseCount++;
        break;
    }
}