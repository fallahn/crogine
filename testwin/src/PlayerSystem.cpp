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

#include "PlayerSystem.hpp"
#include "VelocitySystem.hpp"
#include "ResourceIDs.hpp"
#include "Messages.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/PhysicsObject.hpp>
#include <crogine/ecs/components/Sprite.hpp>

#include <glm/gtx/norm.hpp>

namespace
{
    const glm::vec3 spawnTarget(-3.f, 0.f, -9.3f);
    const glm::vec3 initialPosition(-15.4f, 0.f, -9.3f);
    const glm::vec3 gravity(0.f, -0.4f, 0.f);

    const float shieldTime = 3.f;

    constexpr float fixedStep = 1.f / 60.f;
}

PlayerSystem::PlayerSystem(cro::MessageBus& mb)
    : cro::System   (mb, typeid(PlayerSystem)),
    m_accumulator   (0.f),
    m_respawnTime   (0.f),
    m_shieldTime    (shieldTime)
{
    requireComponent<PlayerInfo>();
    requireComponent<cro::Transform>();
    requireComponent<cro::PhysicsObject>();
}

//public
void PlayerSystem::process(cro::Time dt)
{
    m_accumulator += dt.asSeconds();

    auto entities = getEntities();
    while(m_accumulator > fixedStep)
    {
        m_accumulator -= fixedStep;

        for (auto& entity : entities)
        {
            float scale = m_shieldTime / shieldTime;

            auto shieldEnt = getScene()->getEntity(entity.getComponent<PlayerInfo>().shieldEntity);
            shieldEnt.getComponent<cro::Transform>().setScale(glm::vec3(scale));

            auto& playerInfo = entity.getComponent<PlayerInfo>();
            switch (playerInfo.state)
            {
            case PlayerInfo::State::Spawning:
                updateSpawning(entity);
                break;
            case PlayerInfo::State::Alive:
                updateAlive(entity);
                break;
            case PlayerInfo::State::Dying:
                updateDying(entity);
                break;
            case PlayerInfo::State::Dead:
                updateDead(entity);
                break;
            }
        }
    }
}

//private
void PlayerSystem::updateSpawning(cro::Entity entity)
{
    auto& tx = entity.getComponent<cro::Transform>();
    auto dist = spawnTarget - tx.getWorldPosition();
    tx.move(dist * fixedStep * 2.f);

    if (glm::length2(dist) < 0.5f)
    {
        entity.getComponent<PlayerInfo>().state = PlayerInfo::State::Alive;
        entity.getComponent<Velocity>().velocity = dist * 2.f;
        
        auto* msg = postMessage<PlayerEvent>(MessageID::PlayerMessage);
        msg->entityID = entity.getIndex();
        msg->type = PlayerEvent::Spawned;
    }
}

void PlayerSystem::updateAlive(cro::Entity entity)
{
    m_shieldTime = std::max(0.f, m_shieldTime - fixedStep);
    
    //Do collision stuff
    const auto& po = entity.getComponent<cro::PhysicsObject>();
    const auto& colliders = po.getCollisionIDs();
    for (auto i = 0u; i < po.getCollisionCount(); ++i)
    {
        auto otherEnt = getScene()->getEntity(colliders[i]);
        const auto& otherPo = otherEnt.getComponent<cro::PhysicsObject>();

        if (((otherPo.getCollisionGroups() & (CollisionID::NPC | CollisionID::Environment)) != 0)
            && m_shieldTime == 0)
        {
            entity.getComponent<PlayerInfo>().state = PlayerInfo::State::Dying;
            entity.getComponent<Velocity>().velocity.x = -13.f;

            auto* msg = postMessage<PlayerEvent>(MessageID::PlayerMessage);
            msg->entityID = entity.getIndex();
            msg->type = PlayerEvent::Died;
        }
        else if ((otherPo.getCollisionGroups() & (CollisionID::Collectable)) != 0)
        {
            //TODO raise message
        }
        else if ((otherPo.getCollisionGroups() & (CollisionID::Bounds)) != 0)
        {
            //keep in area
            auto& tx = entity.getComponent<cro::Transform>();
            glm::vec3 normal;
            normal.x = (tx.getWorldPosition().x < 0)? 1.f : -1.f;

            //discover penetration and reverse it - TODO is it safe to assume we always have at least one point?
            tx.move({ -normal.x * po.getManifolds()[i].points[0].distance, 0.f, 0.f });

            //reflect vel
            auto& vel = entity.getComponent<Velocity>();
            vel.velocity = glm::reflect(vel.velocity, normal) * 0.3f;
        }
    }
}

void PlayerSystem::updateDying(cro::Entity entity)
{
    //drop out of map
    entity.getComponent<Velocity>().velocity += gravity;
    
    auto& tx = entity.getComponent<cro::Transform>();
    tx.rotate({ 1.f, 0.f, 0.f }, 10.f * fixedStep);
    if (tx.getWorldPosition().y < -3.f)
    {
        tx.setPosition(initialPosition);
        tx.setRotation({ 0.f, 0.f, 0.f });
        entity.getComponent<Velocity>().velocity = glm::vec3();
        entity.getComponent<PlayerInfo>().state = PlayerInfo::State::Dead;
        m_respawnTime = 1.f;
    }
}

void PlayerSystem::updateDead(cro::Entity entity)
{
    //count down to respawn
    m_respawnTime -= fixedStep;
    if (m_respawnTime < 0)
    {
        entity.getComponent<PlayerInfo>().state = PlayerInfo::State::Spawning;
        m_shieldTime = shieldTime;
    }
}