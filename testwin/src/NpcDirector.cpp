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

#include "NpcDirector.hpp"
#include "Messages.hpp"
#include "ResourceIDs.hpp"
#include "NPCSystem.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Constants.hpp>

#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/quaternion.hpp>

namespace
{
    const float eliteSpawnTime = 24.f;
    const float choppaSpawnTime = 9.f;
    const float zDepth = -9.3f; //bums, this is replicated from NpcSystem.cpp
    const float weaverSpawnTime = 12.f;

    const float eliteHealth = 100.f;
    const float choppaHealth = 8.f;
    const float speedrayHealth = 2.f;
    const float weaverHealth = 4.f;

    const float turretOrbTime = 0.5f;
}

NpcDirector::NpcDirector()
    : m_eliteRespawn    (eliteSpawnTime / 4.f),
    m_choppaRespawn     (choppaSpawnTime / 2.f),
    m_speedrayRespawn   (choppaSpawnTime),
    m_weaverRespawn     (6.f),
    m_turretOrbTime     (turretOrbTime)
{

}

//private
void NpcDirector::handleEvent(const cro::Event& evt)
{

}

void NpcDirector::handleMessage(const cro::Message& msg)
{
    if (msg.id == MessageID::BackgroundSystem)
    {
        const auto& data = msg.getData<BackgroundEvent>();
        if (data.type == BackgroundEvent::MountCreated)
        {
            //if a new chunk is created send a message
            //to the turrets to tell them to reposition
            cro::Command cmd;
            cmd.targetFlags = CommandID::TurretA | CommandID::TurretB;
            cmd.action = [data](cro::Entity entity, cro::Time)
            {
                auto& tx = entity.getComponent<cro::Transform>();
                if (tx.getParentID() == data.entityID)
                {
                    //this our man. Or woman. Turret.
                    tx.setPosition({ data.position, 0.f });
                }
            };
            sendCommand(cmd);
        }
    }
    else if (msg.id == MessageID::NpcMessage)
    {
        const auto& data = msg.getData<NpcEvent>();
        switch (data.type)
        {
        default:break;
        case NpcEvent::Died:

            //if this was a weaver part then set the others to timed destruction
            if (data.npcType == Npc::Weaver)
            {
                const auto entity = getScene().getEntity(data.entityID);
                auto partID = entity.getComponent<Npc>().weaver.ident;

                cro::Command cmd;
                cmd.targetFlags = CommandID::Weaver;
                cmd.action = [partID](cro::Entity entity, cro::Time)
                {
                    auto& status = entity.getComponent<Npc>();
                    if (status.active && status.health > 0)
                    {
                        auto diff = partID;
                        if (status.weaver.ident < diff)
                        {
                            diff -= status.weaver.ident;
                        }
                        else
                        {
                            diff = status.weaver.ident - diff;
                        }
                        status.weaver.dying = true;
                        status.weaver.dyingTime = 0.05f * static_cast<float>(diff) + 0.1f;
                    }
                };
                sendCommand(cmd);
            }
            else if (data.npcType == Npc::Elite)
            {
                cro::Command cmd;
                cmd.targetFlags = CommandID::Elite;
                cmd.action = [](cro::Entity entity, cro::Time)
                {
                    entity.getComponent<cro::ParticleEmitter>().stop();
                };
                sendCommand(cmd);
            }
            else if (data.npcType == Npc::Choppa)
            {
                getScene().getEntity(data.entityID).getComponent<cro::ParticleEmitter>().start();
            }

            break;
        case NpcEvent::FiredWeapon:
            //if a weaver fired reset all the other times to prevent more than one
            //part of the weaver firing at once
            if (data.npcType == Npc::Weaver)
            {
                cro::Command cmd;
                cmd.targetFlags = CommandID::Weaver;
                cmd.action = [](cro::Entity entity, cro::Time)
                {
                    auto& status = entity.getComponent<Npc>();
                    status.weaver.shootTime = cro::Util::Random::value(WeaverNavigator::nextShootTime - 0.3f, WeaverNavigator::nextShootTime);
                    
                };
                sendCommand(cmd);
            }
            break;
        case NpcEvent::HealthChanged:
        {
            if (data.npcType == Npc::Elite)
            {
                //update the smoke effects
                float health = data.value;

                cro::Command cmd;
                cmd.targetFlags = CommandID::Elite;
                cmd.action = [health](cro::Entity entity, cro::Time)
                {
                    float amount = (1.f - (health / 100.f));
                    if (amount > 0)
                    {
                        entity.getComponent<cro::ParticleEmitter>().emitterSettings.emitRate = amount * entity.getComponent<Npc>().elite.maxEmitRate;
                        entity.getComponent<cro::ParticleEmitter>().start();
                    }
                    else
                    {
                        entity.getComponent<cro::ParticleEmitter>().stop();
                    }
                };
                sendCommand(cmd);
            }
        }
            break;
        case NpcEvent::ExitScreen:
            if (data.npcType == Npc::Choppa)
            {
                getScene().getEntity(data.entityID).getComponent<cro::ParticleEmitter>().stop();
            }
            break;
        }
    }
}

void NpcDirector::process(cro::Time dt)
{
    //TODO track how many enemies spawned / died
    //then switch to boss mode when needed
    float dtSec = dt.asSeconds();

    m_eliteRespawn -= dtSec;
    if (m_eliteRespawn < 0)
    {
        m_eliteRespawn = eliteSpawnTime + cro::Util::Random::value(0.1f, 2.3f);

        cro::Command cmd;
        cmd.targetFlags = CommandID::Elite;
        cmd.action = [this](cro::Entity entity, cro::Time)
        {
            auto& status = entity.getComponent<Npc>();
            if (!status.active)
            {
                status.active = true;
                status.elite.destination.x = cro::Util::Random::value(1.f, 4.f);
                status.elite.movementCount = cro::Util::Random::value(4, 8);
                status.elite.pauseTime = cro::Util::Random::value(1.2f, 2.2f);
                status.elite.firetime = cro::Util::Random::value(2.5f, 3.2f);
                status.health = eliteHealth;

                auto& tx = entity.getComponent<cro::Transform>();
                tx.setPosition({ 5.6f, cro::Util::Random::value(-2.f, 2.f) , zDepth });

                auto* msg = postMessage<NpcEvent>(MessageID::NpcMessage);
                msg->type = NpcEvent::HealthChanged;
                msg->npcType = status.type;
                msg->position = entity.getComponent<cro::Transform>().getWorldPosition();
                msg->entityID = entity.getIndex();
                msg->value = status.health;
            }
        };
        sendCommand(cmd);
    }

    m_choppaRespawn -= dtSec;
    if (m_choppaRespawn < 0)
    {
        m_choppaRespawn = choppaSpawnTime + cro::Util::Random::value(-2.f, 3.6439f);
        bool vertical = ((cro::Util::Random::value(0, 1) % 2) == 0);

        cro::Command cmd;
        cmd.targetFlags = CommandID::Choppa;
        cmd.action = [this, vertical](cro::Entity entity, cro::Time)
        {
            auto& status = entity.getComponent<Npc>();
            if (!status.active)
            {
                status.active = true;
                status.choppa.moveSpeed = cro::Util::Random::value(-8.3f, -7.8f);
                status.choppa.deathVelocity.x = status.choppa.moveSpeed;
                status.choppa.tableIndex = cro::Util::Random::value(0, 40); //hmm don't have table size here (see NpcSystem)
                status.health = choppaHealth;
                status.choppa.deathVelocity = {};

                //reset position
                auto& tx = entity.getComponent<cro::Transform>();

                if (vertical)
                {
                    tx.setPosition({ 7.f, -ChoppaNavigator::spacing + (status.choppa.ident * ChoppaNavigator::spacing), zDepth });                    
                }
                else
                {
                    auto yPos = cro::Util::Random::value(-1.9f, 1.9f);
                    tx.setPosition({ 7.f + (status.choppa.ident * ChoppaNavigator::spacing), yPos, zDepth });
                }
                tx.setRotation({ -cro::Util::Const::PI / 2.f, 0.f, 0.f });

                auto* msg = postMessage<NpcEvent>(MessageID::NpcMessage);
                msg->type = NpcEvent::HealthChanged;
                msg->npcType = status.type;
                msg->position = entity.getComponent<cro::Transform>().getWorldPosition();
                msg->entityID = entity.getIndex();
                msg->value = status.health;
            }
        };
        sendCommand(cmd);
    }

    m_speedrayRespawn -= dtSec;
    if(m_speedrayRespawn < 0)
    {
        cro::Command cmd;
        cmd.targetFlags = CommandID::Speedray;
        cmd.action = [this](cro::Entity entity, cro::Time)
        {
            auto& status = entity.getComponent<Npc>();
            if (!status.active)
            {
                status.active = true;
                status.health = speedrayHealth;

                //reset position
                auto& tx = entity.getComponent<cro::Transform>();
                tx.setPosition({ 7.f + static_cast<float>(status.speedray.ident) * SpeedrayNavigator::spacing, 0.f, zDepth });

                auto* msg = postMessage<NpcEvent>(MessageID::NpcMessage);
                msg->type = NpcEvent::HealthChanged;
                msg->npcType = status.type;
                msg->position = entity.getComponent<cro::Transform>().getWorldPosition();
                msg->entityID = entity.getIndex();
                msg->value = status.health;
            }
        };
        sendCommand(cmd);

        m_speedrayRespawn = choppaSpawnTime;
    }

    m_weaverRespawn -= dtSec;
    if (m_weaverRespawn < 0)
    {
        m_weaverRespawn = weaverSpawnTime;
        auto yPos = cro::Util::Random::value(-1.9f, 1.9f);

        cro::Command cmd;
        cmd.targetFlags = CommandID::Weaver;
        cmd.action = [this, yPos](cro::Entity entity, cro::Time) 
        {
            auto& status = entity.getComponent<Npc>();
            if (!status.active)
            {
                status.active = true;
                status.weaver.yPos = yPos;
                status.weaver.tableIndex = status.weaver.tableStartIndex;
                status.health = weaverHealth;
                status.weaver.dying = false;
                status.weaver.velocity = {};
                status.weaver.shootTime = cro::Util::Random::value(WeaverNavigator::nextShootTime - 0.5f, WeaverNavigator::nextShootTime + 2.f);

                auto& tx = entity.getComponent<cro::Transform>();
                tx.setPosition({ 7.f + (static_cast<float>(status.weaver.ident) * (WeaverNavigator::spacing * tx.getScale().x)), status.weaver.yPos, zDepth });

                auto* msg = postMessage<NpcEvent>(MessageID::NpcMessage);
                msg->type = NpcEvent::HealthChanged;
                msg->npcType = status.type;
                msg->position = entity.getComponent<cro::Transform>().getWorldPosition();
                msg->entityID = entity.getIndex();
                msg->value = status.health;
            }
        };
        sendCommand(cmd);
    }

    //TODO switch between turrets, and check player is not too close
    //when firing
    m_turretOrbTime -= dtSec;
    if (m_turretOrbTime < 0)
    {
        m_turretOrbTime = cro::Util::Random::value(turretOrbTime - 1.f, turretOrbTime + 1.f);
        cro::Command cmd;
        cmd.targetFlags = CommandID::TurretA | CommandID::TurretB;
        cmd.action = [this](cro::Entity entity, cro::Time)
        {
            if (entity.getComponent<cro::Model>().isVisible())
            {
                const auto& tx = entity.getComponent<cro::Transform>();
                float rotation = getScene().getEntity(tx.getChildIDs()[0]).getComponent<cro::Transform>().getRotation().z;

                for (auto i = 0u; i < 2; ++i)
                {
                    auto* msg = postMessage<NpcEvent>(MessageID::NpcMessage);
                    msg->entityID = entity.getIndex();
                    msg->npcType = Npc::Turret;
                    msg->type = NpcEvent::FiredWeapon;
                    msg->position = tx.getWorldPosition();
                    msg->position.z += 0.18f;
                    msg->velocity = glm::rotate(glm::vec3(0.f, 1.f, 0.f), (rotation - 0.2f) + (static_cast<float>(i) * 0.4f), glm::vec3(0.f, 0.f, 1.f));
                }
            }
        };
        sendCommand(cmd);
    }
}