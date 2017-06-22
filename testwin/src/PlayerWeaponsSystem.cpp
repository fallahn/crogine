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

#include "PlayerWeaponsSystem.hpp"
#include "Messages.hpp"
#include "ResourceIDs.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/PhysicsObject.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/Scene.hpp>

namespace
{
    const float pulseMoveSpeed = 18.f;
    constexpr float pulseFireRate = 0.5f;
    constexpr float pulseDoubleFireRate = pulseFireRate / 2.f;
    constexpr float pulseTripleFireRate = pulseFireRate / 3.f;
    const float pulseOffset = 0.6f;

    const float laserRate = 0.025f;

    const glm::vec3 idlePos(-10.f);
}

PlayerWeaponSystem::PlayerWeaponSystem(cro::MessageBus& mb)
    : cro::System   (mb, typeid(PlayerWeaponSystem)),
    m_systemActive  (true),
    m_fireMode      (FireMode::Laser),
    m_playerID      (0),
    m_aliveCount    (0),
    m_deadPulseCount(0),
    m_deadLaserCount(0)
{
    requireComponent<PlayerWeapon>();
    requireComponent<cro::Transform>();
    requireComponent<cro::PhysicsObject>();
}

//public
void PlayerWeaponSystem::process(cro::Time dt)
{
    static float fireTime = 0.f;
    fireTime += dt.asSeconds();
    
    auto spawnPulse = [&](glm::vec3 position)
    {
        m_aliveList[m_aliveCount++] = m_deadPulses[--m_deadPulseCount];
        getScene()->getEntity(m_aliveList[m_aliveCount - 1]).getComponent<cro::Transform>().setPosition(m_playerPosition);
    };

    //if active spawn more pulses or activate laser
    if (m_systemActive)
    {
        switch (m_fireMode)
        {
        default:
        case FireMode::Single:
            if (fireTime > pulseFireRate
                && m_deadPulseCount > 0)
            {
                spawnPulse(m_playerPosition);
                fireTime = 0.f;
            }
            break;
        case FireMode::Double:
            if (fireTime > pulseDoubleFireRate
                && m_deadPulseCount > 0)
            {
                static cro::int32 side = 0;
                glm::vec3 offset;
                offset.z = (side) ? -pulseOffset : pulseOffset;

                offset = getScene()->getEntity(m_playerID).getComponent<cro::Transform>().getWorldTransform() * glm::vec4(offset, 1.f);
                spawnPulse(offset);

                side = (side + 1) % 2;
                fireTime = 0.f;
            }
            break;
        case FireMode::Triple:
            if (fireTime > pulseTripleFireRate
                && m_deadPulseCount > 0)
            {
                static cro::int32 side = 0;
                glm::vec3 offset;
                offset.z = -pulseOffset + (pulseOffset * side);

                offset = getScene()->getEntity(m_playerID).getComponent<cro::Transform>().getWorldTransform() * glm::vec4(offset, 1.f);
                spawnPulse(offset);

                side = (side + 1) % 3;
                fireTime = 0.f;
            }
            break;
        case FireMode::Laser:
            if (m_deadLaserCount > 0)
            {
                m_aliveList[m_aliveCount++] = m_deadLasers[--m_deadLaserCount];
                getScene()->getEntity(m_aliveList[m_aliveCount - 1]).getComponent<cro::Transform>().setPosition(glm::vec3());
            }
            break;
        }
    }

    //update alive
    for (auto i = 0u; i < m_aliveCount; ++i)
    {
        auto e = getScene()->getEntity(m_aliveList[i]);
        auto& weaponData = e.getComponent<PlayerWeapon>();

        if (weaponData.type == PlayerWeapon::Type::Pulse)
        {
            //update position
            e.getComponent<cro::Transform>().move({ dt.asSeconds() * pulseMoveSpeed, 0.f, 0.f });

            //handle collision with NPCs or end of map
            const auto& collision = e.getComponent<cro::PhysicsObject>();
            auto count = collision.getCollisionCount();
            const auto& IDs = collision.getCollisionIDs();
            for (auto j = 0u; j < count; ++j)
            {
                //auto otherEnt = getScene()->getEntity(IDs[i]);
                //const auto& otherCollision = otherEnt.getComponent<cro::PhysicsObject>();
                //if (otherCollision.getCollisionGroups() | CollisionID::Bounds)
                //{ 
                //    e.getComponent<cro::Transform>().setPosition(m_playerPosition);
                //    //LOG("Buns", cro::Logger::Type::Info);
                //}

                //other entities handle their own reaction - we just want to reset the pulse
                e.getComponent<cro::Transform>().setPosition(idlePos);

                //move to dead list
                m_deadPulses[m_deadPulseCount++] = m_aliveList[i];

                //and remove from alive list
                m_aliveList[i] = m_aliveList[--m_aliveCount];
                i--; //decrement so the newly inserted ID doesn't get skipped, and we can be sure we're still < m_aliveCount

                count = 0; //only want to handle one collision at most, else we might kill this ent more than once
            }
        }
        else //laser is firing
        {
            static float laserTime = 0.f;
            laserTime += dt.asSeconds();
            
            if (laserTime > laserRate)
            {
                //fade laser colour
                auto& sprite = e.getComponent<cro::Sprite>();
                auto colour = sprite.getColour();
                colour.setAlpha(colour.getAlpha() < 1 ? 1.f : 0.5f);
                sprite.setColour(colour);
                laserTime = 0.f;
            }

            if (m_fireMode != FireMode::Laser || !m_systemActive)
            {
                //remove from alive list
                e.getComponent<cro::Transform>().setPosition(idlePos);

                //move to dead list
                m_deadLasers[m_deadLaserCount++] = m_aliveList[i];

                //and remove from alive list
                m_aliveList[i] = m_aliveList[--m_aliveCount];
                i--;
            }
        }
    }
}

void PlayerWeaponSystem::handleMessage(const cro::Message& msg)
{
    if (msg.id == MessageID::PlayerMessage)
    {
        const auto& data = msg.getData<PlayerEvent>();
        switch (data.type)
        {
        case PlayerEvent::Died:
            m_fireMode = FireMode::Single;
            //m_systemActive = false;
            break;
        case PlayerEvent::Moved:
            m_playerPosition = data.position;
            m_playerID = data.entityID;
            break;
        case PlayerEvent::Spawned:

            break;
        default: break;
        }
    }

    //TODO increase fire mode of player collects power up
}

//private
void PlayerWeaponSystem::onEntityAdded(cro::Entity entity)
{
    //add entity to dead list based on type
    if (entity.getComponent<PlayerWeapon>().type == PlayerWeapon::Type::Pulse)
    {
        m_deadPulses.push_back(entity.getIndex());
        m_deadPulseCount = m_deadPulses.size();
    }
    else
    {
        m_deadLasers.push_back(entity.getIndex());
        m_deadLaserCount = m_deadLasers.size();
    }

    //pad alive list to correct size (we're assuming no entities are actually
    //created or destroyed at runtime)
    m_aliveList.push_back(-1);
}