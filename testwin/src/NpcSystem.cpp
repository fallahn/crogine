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

#include "NPCSystem.hpp"
#include "Messages.hpp"
#include "ResourceIDs.hpp"
#include "PlayerWeaponsSystem.hpp"

#include <crogine/core/App.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/PhysicsObject.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Wavetable.hpp>

#include <glm/gtx/norm.hpp>
#include <glm/gtx/vector_angle.hpp>

namespace
{
    const float fixedUpdate = 1.f / 60.f;
    const float zDepth = -9.3f;
    const glm::vec3 gravity(0.f, -9.f, 0.f);
    const glm::vec3 chopperPulseOffset(-0.1f, -0.06f, 0.f);

    const cro::int32 eliteScore = 1000;
    const cro::int32 choppaScore = 250;
    const cro::int32 speedrayScore = 50;
    const cro::int32 weaverScore = 10;
}


NpcSystem::NpcSystem(cro::MessageBus& mb)
    : cro::System   (mb, typeid(NpcSystem)),
    m_accumulator   (0.f),
    m_empFired      (false),
    m_awardPoints   (true)
{
    requireComponent<cro::Model>();
    requireComponent<cro::Transform>();
    requireComponent<cro::PhysicsObject>();
    requireComponent<Npc>();

    auto poissonPos = cro::Util::Random::poissonDiscDistribution({ 0.f, -2.f, 4.f, 4.f }, 1.f, 5);
    for (auto p : poissonPos)
    {
        m_elitePositions.emplace_back(p, zDepth);
    }

    poissonPos = cro::Util::Random::poissonDiscDistribution({ -0.8f, -0.8f, 1.6f, 1.6f }, 0.6f, 6);
    for (auto p : poissonPos)
    {
        m_eliteIdlePositions.emplace_back(p, 0.f);
    }

    m_choppaTable = cro::Util::Wavetable::sine(1.f, 0.4f);
    m_speedrayTable = cro::Util::Wavetable::sine(0.5f, 1.6f);
    m_weaverTable = cro::Util::Wavetable::sine(0.62f, 0.06f);
}

//public
void NpcSystem::handleMessage(const cro::Message& msg)
{
    if (msg.id == MessageID::PlayerMessage)
    {
        const auto& data = msg.getData<PlayerEvent>();
        switch (data.type)
        {
        default: break;
        case PlayerEvent::Moved:
            m_playerPosition = data.position;
            break;
        case PlayerEvent::FiredEmp:
            m_empFired = true;
            break;
        }
    }
    else if (msg.id == MessageID::GameMessage)
    {
        const auto& data = msg.getData<GameEvent>();
        switch (data.type)
        {
        default:
            m_empFired = true; //just to kill all active NPCs
            m_awardPoints = false;
            break;
        case GameEvent::GameStart:
        case GameEvent::RoundStart:
            m_awardPoints = true;
            break;
        }
    }
}

void NpcSystem::process(cro::Time dt)
{
    //DPRINT("Player Position", std::to_string(m_playerPosition.x) + ", " + std::to_string(m_playerPosition.y));
    
    auto& entities = getEntities();
    m_accumulator += dt.asSeconds();// std::min(, 1.f);
       
    while (m_accumulator > fixedUpdate)
    {
        m_accumulator -= fixedUpdate;

        for (auto& entity : entities) 
        {
            auto& status = entity.getComponent<Npc>();
           
            bool hasCollision = false;
            if (status.wantsReset) //we're on screen
            {
                //check for collision with player weapons
                auto& phys = entity.getComponent<cro::PhysicsObject>();
                auto count = phys.getCollisionCount();
                for (auto i = 0u; i < count; ++i)
                {
                    auto otherEnt = getScene()->getEntity(phys.getCollisionIDs()[i]);
                    if (otherEnt.getComponent<cro::PhysicsObject>().getCollisionGroups() & CollisionID::PlayerLaser)
                    {
                        //remove some health based on weapon energy
                        const auto& weapon = otherEnt.getComponent<PlayerWeapon>();
                        status.health -= weapon.damage;
                        
                        {
                            auto* msg = postMessage<NpcEvent>(MessageID::NpcMessage);
                            msg->type = NpcEvent::HealthChanged;
                            msg->npcType = status.type;
                            msg->position = entity.getComponent<cro::Transform>().getWorldPosition();
                            msg->entityID = entity.getIndex();
                            msg->value = status.health;
                        }
                    }
                }

                //if EMP was fired kill anyway
                if (m_empFired)
                {
                    status.health = -1.f;
                    //LOG("EMP killed everything", cro::Logger::Type::Info)
                }

                if (status.health <= 0)
                {
                    //raise a message 
                    auto* msg = postMessage<NpcEvent>(MessageID::NpcMessage);
                    msg->type = NpcEvent::Died;
                    msg->npcType = status.type;
                    msg->position = entity.getComponent<cro::Transform>().getWorldPosition();
                    msg->entityID = entity.getIndex();
                    msg->value = m_awardPoints ? static_cast<float>(status.scoreValue) : 0;

                    if (!status.hasDyingAnim)
                    {
                        //TODO make this a case for all with dying animation
                        entity.getComponent<cro::Transform>().setPosition(glm::vec3(-10.f));
                        hasCollision = true;
                    }
                }
            }

            if (status.active)
            {               
                //process logic based on type
                switch (status.type)
                {
                default: break;
                case Npc::Elite:
                    processElite(entity);
                    break;
                case Npc::Choppa:
                    processChoppa(entity);
                    hasCollision = false; //we want to play dying animation
                    break;
                case Npc::Turret:
                    processTurret(entity);
                    continue; //turrets are parented to terrain entities - so don't need following update
                case Npc::Speedray:
                    processSpeedray(entity);
                    break;
                case Npc::Weaver:
                    processWeaver(entity);
                    break;
                }  

                //check if entity has moved off-screen and
                //reset it if it has
                bool visible = (entity.getComponent<cro::Model>().isVisible() && !hasCollision);
                if (!visible && status.wantsReset) //moved out of area
                {
                    status.wantsReset = false;
                    status.active = false;

                    auto* msg = postMessage<NpcEvent>(MessageID::NpcMessage);
                    msg->entityID = entity.getIndex();
                    msg->npcType = status.type;
                    msg->type = NpcEvent::ExitScreen;
                }
                else if (visible)
                {
                    //we've entered the visible area so will eventually want resetting
                    status.wantsReset = true;
                }                
            }
        }
        m_empFired = false;
    }
}

//private
void NpcSystem::processElite(cro::Entity entity)
{
    auto& tx = entity.getComponent<cro::Transform>();
    auto& status = entity.getComponent<Npc>();
    
    status.elite.dying = (status.health <= 0);

    if (status.elite.dying)
    {
        tx.rotate({ 1.f, 0.f, 0.f }, 0.1f + fixedUpdate);
        tx.move(status.elite.deathVelocity * fixedUpdate);
        status.elite.deathVelocity += gravity * fixedUpdate;
    }
    else
    {
        //move toward target
        auto movement = status.elite.destination - tx.getWorldPosition();

        //clamp max movement
        static const float maxMoveSqr = 181.f;
        float l2 = glm::length2(movement);
        if (l2 > maxMoveSqr)
        {
            movement *= (maxMoveSqr / l2);
        }

        if (status.elite.active) //big movements
        {
            movement *= 4.f;
            if (glm::length2(movement) < 1.f)
            {
                status.elite.active = false;
                status.elite.pauseTime = cro::Util::Random::value(1.4f, 2.1f);
                status.elite.movementCount--;

                status.elite.destination = m_eliteIdlePositions[status.elite.idleIndex] + status.elite.destination;// tx.getWorldPosition();
            }
        }
        else
        {
            //do some floaty bobbins               
            /*if (glm::length2(movement) < 0.2f)
            {
                status.elite.idleIndex = (status.elite.idleIndex + 1) % m_eliteIdlePositions.size();
                status.elite.destination = m_eliteIdlePositions[status.elite.idleIndex] + tx.getWorldPosition();
            }*/

            //count down to next movement
            status.elite.pauseTime -= fixedUpdate;
            if (status.elite.pauseTime < 0)
            {
                status.elite.active = true;
                status.elite.destination = (status.elite.movementCount)
                    ? m_elitePositions[cro::Util::Random::value(0, m_elitePositions.size() - 1)]
                    : glm::vec3(-7.f, tx.getWorldPosition().y, zDepth);
            }

            //check weapon fire
            status.elite.firetime -= fixedUpdate;
            if (status.elite.firetime < 0)
            {
                //LOG("Fired Elite laser", cro::Logger::Type::Info);
                status.elite.firetime = cro::Util::Random::value(2.5f, 3.2f);

                auto* msg = postMessage<NpcEvent>(MessageID::NpcMessage);
                msg->entityID = entity.getIndex();
                msg->npcType = Npc::Elite;
                msg->position = entity.getComponent<cro::Transform>().getWorldPosition();
                msg->type = NpcEvent::FiredWeapon;
            }
        }
        status.elite.deathVelocity = movement;
        tx.move(movement * fixedUpdate);
    }
}

void NpcSystem::processChoppa(cro::Entity entity)
{
    auto& tx = entity.getComponent<cro::Transform>();
    auto& status = entity.getComponent<Npc>();

    status.choppa.inCombat = (status.health > 0);

    if (status.choppa.inCombat)
    {
        //fly
        glm::vec3 movement(status.choppa.moveSpeed, m_choppaTable[status.choppa.tableIndex], 0.f);
        movement.y += (m_playerPosition.y - tx.getWorldPosition().y) * 0.17f;
        tx.move(movement * fixedUpdate);
        status.choppa.tableIndex = (status.choppa.tableIndex + 1) % m_choppaTable.size();

        //shoot
        status.choppa.shootTime -= fixedUpdate;
        if (status.choppa.shootTime < 0)
        {
            status.choppa.shootTime = ChoppaNavigator::nextShootTime;

            auto* msg = postMessage<NpcEvent>(MessageID::NpcMessage);
            msg->entityID = entity.getIndex();
            msg->npcType = Npc::Choppa;
            msg->position = tx.getWorldPosition() + chopperPulseOffset;
            msg->type = NpcEvent::FiredWeapon;
        }
    }
    else
    {
        //diiieeeee
        tx.rotate({ 1.f, 0.f, 0.f }, 0.1f + fixedUpdate);
        tx.move(status.choppa.deathVelocity * fixedUpdate);
        status.choppa.deathVelocity += gravity * fixedUpdate;
    }
}

void NpcSystem::processTurret(cro::Entity entity)
{
    auto& tx = entity.getComponent<cro::Transform>();
    
    glm::vec3 target = m_playerPosition - tx.getWorldPosition();

    float rotation = -atan2(target.x, target.y);
    tx.setRotation({ 0.f, rotation, 0.f });
}

void NpcSystem::processSpeedray(cro::Entity entity)
{
    auto& status = entity.getComponent<Npc>();
    
    std::size_t cosIdx = (status.speedray.tableIndex + (m_speedrayTable.size() / 2)) % m_speedrayTable.size();
    const float zOffset = m_speedrayTable[cosIdx] / 2.f;

    auto& tx = entity.getComponent<cro::Transform>();
    tx.setPosition({ tx.getPosition().x, m_speedrayTable[status.speedray.tableIndex], zDepth + zOffset });

    status.speedray.tableIndex = (status.speedray.tableIndex + 1) % m_speedrayTable.size();

    tx.move({ status.speedray.moveSpeed * fixedUpdate, 0.f, 0.f });

    //shooting
    status.speedray.shootTime -= fixedUpdate;
    if (status.speedray.shootTime < 0)
    {
        status.speedray.shootTime = SpeedrayNavigator::nextShootTime;

        auto* msg = postMessage<NpcEvent>(MessageID::NpcMessage);
        msg->entityID = entity.getIndex();
        msg->npcType = Npc::Speedray;
        msg->position = tx.getWorldPosition();
        msg->type = NpcEvent::FiredWeapon;
    }
}

void NpcSystem::processWeaver(cro::Entity entity)
{
    auto& status = entity.getComponent<Npc>();
    auto& tx = entity.getComponent<cro::Transform>();
    tx.setPosition({ tx.getPosition().x, (m_weaverTable[status.weaver.tableIndex]* (1.f / tx.getScale().x)) + status.weaver.yPos, zDepth });

    status.weaver.tableIndex = (status.weaver.tableIndex + 1) % m_weaverTable.size();

    tx.move({ status.weaver.moveSpeed * fixedUpdate, 0.f, 0.f });

    //check if part is dying
    if (status.weaver.dying)
    {
        status.weaver.dyingTime -= fixedUpdate;
        if (status.weaver.dyingTime < 0)
        {
            tx.setPosition(glm::vec3(100.f));
            status.wantsReset = false;
            status.active = false;
            status.weaver.dying = false;
            //LOG("Part Died", cro::Logger::Type::Info);
        }
        //DPRINT("Dying", std::to_string(status.weaver.ident));
        status.weaver.velocity += gravity * fixedUpdate;
        tx.move(status.weaver.velocity * fixedUpdate);
    }
    else
    {
        //shooting
        status.weaver.shootTime -= fixedUpdate;
        if (status.weaver.shootTime < 0)
        {
            status.weaver.shootTime = cro::Util::Random::value(WeaverNavigator::nextShootTime - 0.5f, WeaverNavigator::nextShootTime + 2.f);

            const float stepCount = 5.f;
            const float angle = cro::Util::Const::TAU / stepCount;

            for (float i = 0.f; i < stepCount; ++i)
            {
                auto* msg = postMessage<NpcEvent>(MessageID::NpcMessage);
                msg->entityID = entity.getIndex();
                msg->npcType = Npc::Weaver;
                msg->type = NpcEvent::FiredWeapon;
                msg->position = tx.getWorldPosition();
                msg->velocity = glm::rotate(glm::vec3(0.f, 1.f, 0.f), (i * angle), glm::vec3(0.f, 0.f, 1.f));
            }
        }
    }
}

void NpcSystem::onEntityAdded(cro::Entity entity)
{

    auto& status = entity.getComponent<Npc>();
    switch (status.type)
    {
    default:break;
    case Npc::Elite:
        status.elite.active = true;
        status.elite.destination = m_elitePositions[0];
        status.elite.movementCount = cro::Util::Random::value(4, 8);
        status.elite.pauseTime = cro::Util::Random::value(1.2f, 2.2f);
        status.elite.idleIndex = cro::Util::Random::value(0, m_eliteIdlePositions.size());
        status.elite.maxEmitRate = entity.getComponent<cro::ParticleEmitter>().emitterSettings.emitRate;
        status.scoreValue = eliteScore;
        break;
    case Npc::Choppa:
        status.choppa.moveSpeed = cro::Util::Random::value(-8.3f, -7.8f);
        status.choppa.deathVelocity.x = status.choppa.moveSpeed;
        status.choppa.tableIndex = cro::Util::Random::value(0, m_choppaTable.size() - 1);
        status.scoreValue = choppaScore;
        break;
    case Npc::Turret:
        status.active = true;
        break;
    case Npc::Speedray:
        status.speedray.tableIndex = (m_speedrayTable.size() / SpeedrayNavigator::count) * status.speedray.ident;
        status.speedray.tableIndex %= m_speedrayTable.size();
        status.scoreValue = speedrayScore;
        break;
    case Npc::Weaver:
        status.weaver.tableStartIndex = 10 + (m_weaverTable.size() / WeaverNavigator::count) * status.weaver.ident;
        status.weaver.tableStartIndex %= m_weaverTable.size();
        status.scoreValue = weaverScore;
        break;
    }
}