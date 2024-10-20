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
#include "BackgroundSystem.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/core/Console.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Constants.hpp>

#include <crogine/detail/glm/gtx/rotate_vector.hpp>
#include <crogine/detail/glm/gtx/quaternion.hpp>

namespace
{
    const float eliteSpawnTime = 24.f;
    const float choppaSpawnTime = 9.f;
    const float zDepth = -9.3f; //bums, this is replicated from NpcSystem.cpp
    const float weaverSpawnTime = 12.f;
    const float startX = 7.f;

    const float eliteHealth = 100.f;
    const float choppaHealth = 8.f;
    const float speedrayHealth = 2.f;
    const float weaverHealth = 4.f;

    const float turretOrbTime = 0.5f;

    std::size_t totalReleaseCount = 20;
    const float roundPauseTime = 3.f; //number of seconds after last NPC to wait before calling round change
}

NpcDirector::NpcDirector()
    : m_eliteRespawn    (eliteSpawnTime / 4.f),
    m_choppaRespawn     (choppaSpawnTime / 2.f),
    m_speedrayRespawn   (choppaSpawnTime),
    m_weaverRespawn     (6.f),
    m_turretOrbTime     (turretOrbTime),
    m_totalReleaseCount (0),
    m_releaseActive     (true),
    m_roundCount        (0),
    m_roundDelay        (roundPauseTime),
    m_npcCount          (0) //this counts individual parts for multipart NPCs
{
    m_eliteRespawn *= cro::Util::Random::value(0.1f, 1.f);
    m_choppaRespawn *= cro::Util::Random::value(0.1f, 1.f);
    m_speedrayRespawn *= cro::Util::Random::value(0.1f, 1.f);
    m_weaverRespawn *= cro::Util::Random::value(0.1f, 1.f);
    m_turretOrbTime *= cro::Util::Random::value(0.1f, 1.f);
}

//private
void NpcDirector::handleEvent(const cro::Event& evt)
{
    if (evt.type == SDL_KEYUP)
    {
        if (evt.key.keysym.sym == SDLK_l)
        {
            m_totalReleaseCount = 100;
        }
    }
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
            cmd.action = [data](cro::Entity entity, float)
            {
                auto& tx = entity.getComponent<cro::Transform>();
                if (entity.getComponent<Family>().parent.getIndex() == data.entityID)
                {
                    //this our man. Or woman. Turret.
                    tx.setPosition(glm::vec3(data.position, 0.f));
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
                cmd.action = [partID](cro::Entity entity, float)
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
                cmd.action = [](cro::Entity entity, float)
                {
                    entity.getComponent<cro::ParticleEmitter>().stop();
                };
                sendCommand(cmd);
            }
            else if (data.npcType == Npc::Choppa)
            {
                getScene().getEntity(data.entityID).getComponent<cro::ParticleEmitter>().start();
            }

            if(m_npcCount > 0 && data.npcType != Npc::Turret) m_npcCount--;
            break;
        case NpcEvent::FiredWeapon:
            //if a weaver fired reset all the other times to prevent more than one
            //part of the weaver firing at once
            if (data.npcType == Npc::Weaver)
            {
                cro::Command cmd;
                cmd.targetFlags = CommandID::Weaver;
                cmd.action = [](cro::Entity entity, float)
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
                cmd.action = [health](cro::Entity entity, float)
                {
                    float amount = (1.f - (health / 100.f));
                    if (amount > 0)
                    {
                        entity.getComponent<cro::ParticleEmitter>().settings.emitRate = amount * entity.getComponent<Npc>().elite.maxEmitRate;
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
            if (m_npcCount > 0 && data.npcType != Npc::Turret) m_npcCount--;
            break;
        }
    }
    else if (msg.id == MessageID::GameMessage)
    {
        const auto& data = msg.getData<GameEvent>();
        switch (data.type)
        {
        default: break;
        case GameEvent::RoundStart:
            m_releaseActive = true;
            break;
        case GameEvent::GameOver:
        case GameEvent::RoundEnd:
        case GameEvent::BossStart:
            m_releaseActive = false;
            {
                //remove the turrets
                cro::Command cmd;
                cmd.targetFlags = CommandID::TurretA | CommandID::TurretB;
                cmd.action = [&](cro::Entity entity, float)
                {
                    auto& tx = entity.getComponent<cro::Transform>();
                    auto oldPos = tx.getWorldPosition();
                    tx.setPosition({ -1000.f, -200.f, 0.f });

                    //raise a message so we get explodies
                    auto* deathMsg = postMessage<NpcEvent>(MessageID::NpcMessage);
                    deathMsg->npcType = Npc::Turret;
                    deathMsg->type = NpcEvent::Died;
                    deathMsg->position = oldPos;
                    deathMsg->entityID = entity.getComponent<Family>().child.getIndex();
                };
                sendCommand(cmd);

            }
            break;
        }

        //remember to reset times else everything spawns at once!
        m_eliteRespawn = eliteSpawnTime + cro::Util::Random::value(0.1f, 2.3f);
        m_choppaRespawn = choppaSpawnTime + cro::Util::Random::value(-2.f, 3.6439f);
        m_speedrayRespawn = choppaSpawnTime;
        m_weaverRespawn = weaverSpawnTime;
        m_turretOrbTime = cro::Util::Random::value(turretOrbTime - 1.f, turretOrbTime + 1.f);

        m_eliteRespawn *= cro::Util::Random::value(0.1f, 1.f);
        m_choppaRespawn *= cro::Util::Random::value(0.1f, 1.f);
        m_speedrayRespawn *= cro::Util::Random::value(0.1f, 1.f);
        m_weaverRespawn *= cro::Util::Random::value(0.1f, 1.f);
        m_turretOrbTime *= cro::Util::Random::value(0.1f, 1.f);
    }
}

void NpcDirector::process(float dt)
{   
    DPRINT("NPC Count", std::to_string(m_npcCount));

    //track how many enemies spawned / died
    //then switch to boss mode when needed
    if (m_totalReleaseCount > totalReleaseCount)
    {
        m_releaseActive = false;

        //wait until all NPCs died
        if (m_npcCount == 0)
        {
            m_roundDelay -= dt;
            getScene().getSystem<BackgroundSystem>()->setColourAngle(-50.f);

            if (m_roundDelay < 0)
            {
                m_roundCount++;
                m_totalReleaseCount = 0;
                m_roundDelay = roundPauseTime;

                //create boss mode message
                //or end of round message if first round
                if (m_roundCount == 1)
                {
                    //EOR
                    auto* msg = postMessage<GameEvent>(MessageID::GameMessage);
                    msg->type = GameEvent::RoundEnd;
                }
                else
                {
                    //Boss fight
                    auto* msg = postMessage<GameEvent>(MessageID::GameMessage);
                    msg->type = GameEvent::BossStart;
                }
            }
            //temp - DELETE!
            //m_roundCount = 0;
        }
    }
    if (!m_releaseActive) return;

    m_eliteRespawn -= dt;
    if (m_eliteRespawn < 0)
    {
        m_eliteRespawn = eliteSpawnTime + cro::Util::Random::value(0.1f, 2.3f);

        cro::Command cmd;
        cmd.targetFlags = CommandID::Elite;
        cmd.action = [this](cro::Entity entity, float)
        {
            if (!m_releaseActive) return;
            
            auto& status = entity.getComponent<Npc>();
            if (!status.active)
            {
                status.active = true;
                status.elite.destination.x = cro::Util::Random::value(1.f, 4.f);
                status.elite.movementCount = cro::Util::Random::value(4, 8);
                status.elite.pauseTime = cro::Util::Random::value(1.2f, 2.2f);
                status.elite.firetime = cro::Util::Random::value(2.5f, 3.2f);
                status.elite.dying = false;
                status.elite.deathVelocity = { -4.f, 0.f, 0.f };
                status.health = eliteHealth;

                auto& tx = entity.getComponent<cro::Transform>();
                tx.setPosition({ 5.6f, cro::Util::Random::value(-2.f, 2.f) , zDepth });
                tx.setRotation(glm::quat(1.f, 0.f, 0.f, 0.f));

                auto* msg = postMessage<NpcEvent>(MessageID::NpcMessage);
                msg->type = NpcEvent::HealthChanged;
                msg->npcType = status.type;
                msg->position = entity.getComponent<cro::Transform>().getWorldPosition();
                msg->entityID = entity.getIndex();
                msg->value = status.health;

                m_npcCount++;
            }
        };
        sendCommand(cmd);
        m_totalReleaseCount++;      
    }

    m_choppaRespawn -= dt;
    if (m_choppaRespawn < 0)
    {
        m_choppaRespawn = choppaSpawnTime + cro::Util::Random::value(-2.f, 3.6439f);
        bool vertical = ((cro::Util::Random::value(0, 1) % 2) == 0);

        cro::Command cmd;
        cmd.targetFlags = CommandID::Choppa;
        cmd.action = [this, vertical](cro::Entity entity, float)
        {
            if (!m_releaseActive) return;
            
            auto& status = entity.getComponent<Npc>();
            if (!status.active)
            {
                status.active = true;
                status.choppa.moveSpeed = cro::Util::Random::value(-8.3f, -7.8f);
                status.choppa.deathVelocity.x = status.choppa.moveSpeed;
                status.choppa.tableIndex = cro::Util::Random::value(0, 40); //hmm don't have table size here (see NpcSystem)
                status.health = choppaHealth;
                status.choppa.deathVelocity = { status.choppa.moveSpeed, 0.f, 0.f };

                //reset position
                auto& tx = entity.getComponent<cro::Transform>();

                if (vertical)
                {
                    tx.setPosition({ startX, -ChoppaNavigator::spacing + (status.choppa.ident * ChoppaNavigator::spacing), zDepth });
                }
                else
                {
                    auto yPos = cro::Util::Random::value(-1.9f, 1.9f);
                    tx.setPosition({ startX + (status.choppa.ident * ChoppaNavigator::spacing), yPos, zDepth });
                }
                tx.setRotation(glm::quat(1.f, 0.f, 0.f, 0.f));

                auto* msg = postMessage<NpcEvent>(MessageID::NpcMessage);
                msg->type = NpcEvent::HealthChanged;
                msg->npcType = status.type;
                msg->position = entity.getComponent<cro::Transform>().getWorldPosition();
                msg->entityID = entity.getIndex();
                msg->value = status.health;

                m_npcCount++;
            }
        };
        sendCommand(cmd);
        m_totalReleaseCount++;
    }

    m_speedrayRespawn -= dt;
    if(m_speedrayRespawn < 0)
    {
        cro::Command cmd;
        cmd.targetFlags = CommandID::Speedray;
        cmd.action = [this](cro::Entity entity, float)
        {
            if (!m_releaseActive) return;

            auto& status = entity.getComponent<Npc>();
            if (!status.active)
            {
                status.active = true;
                status.health = speedrayHealth;

                //reset position
                auto& tx = entity.getComponent<cro::Transform>();
                tx.setPosition({ startX + static_cast<float>(status.speedray.ident) * SpeedrayNavigator::spacing, 0.f, zDepth });

                auto* msg = postMessage<NpcEvent>(MessageID::NpcMessage);
                msg->type = NpcEvent::HealthChanged;
                msg->npcType = status.type;
                msg->position = entity.getComponent<cro::Transform>().getWorldPosition();
                msg->entityID = entity.getIndex();
                msg->value = status.health;

                m_npcCount++;
            }
        };
        sendCommand(cmd);

        m_speedrayRespawn = choppaSpawnTime;
        m_totalReleaseCount++;
    }

    m_weaverRespawn -= dt;
    if (m_weaverRespawn < 0)
    {
        m_weaverRespawn = weaverSpawnTime;
        auto yPos = cro::Util::Random::value(-1.9f, 1.9f);

        cro::Command cmd;
        cmd.targetFlags = CommandID::Weaver;
        cmd.action = [this, yPos](cro::Entity entity, float) 
        {
            if (!m_releaseActive) return;
            
            auto& status = entity.getComponent<Npc>();
            //if (!status.active) //we want to reset all parts, regardless
            {
                if (!status.active) m_npcCount++;

                status.active = true;
                status.weaver.yPos = yPos;
                status.weaver.tableIndex = status.weaver.tableStartIndex;
                status.health = weaverHealth;
                status.weaver.dying = false;
                status.weaver.velocity = {};
                status.weaver.shootTime = cro::Util::Random::value(WeaverNavigator::nextShootTime - 0.5f, WeaverNavigator::nextShootTime + 2.f);

                auto& tx = entity.getComponent<cro::Transform>();
                tx.setPosition({ startX + (static_cast<float>(status.weaver.ident) * (WeaverNavigator::spacing * tx.getScale().x)), status.weaver.yPos, zDepth });

                auto* msg = postMessage<NpcEvent>(MessageID::NpcMessage);
                msg->type = NpcEvent::HealthChanged;
                msg->npcType = status.type;
                msg->position = entity.getComponent<cro::Transform>().getWorldPosition();
                msg->entityID = entity.getIndex();
                msg->value = status.health;
            }
        };
        sendCommand(cmd);
        m_totalReleaseCount++;
    }

    //TODO switch between turrets, and check player is not too close
    //when firing
    m_turretOrbTime -= dt;
    if (m_turretOrbTime < 0)
    {
        m_turretOrbTime = cro::Util::Random::value(turretOrbTime - 1.f, turretOrbTime + 1.f);
        cro::Command cmd;
        cmd.targetFlags = CommandID::TurretA | CommandID::TurretB;
        cmd.action = [this](cro::Entity entity, float)
        {
            if (!m_releaseActive) return;
            
            if (entity.getComponent<cro::Model>().isVisible())
            {
                const auto& tx = entity.getComponent<cro::Transform>();
                float rotation = glm::eulerAngles(entity.getComponent<Family>().child.getComponent<cro::Transform>().getRotation()).z;

                for (auto i = 0u; i < 2; ++i)
                {
                    auto* msg = postMessage<NpcEvent>(MessageID::NpcMessage);
                    msg->entityID = entity.getIndex();
                    msg->npcType = Npc::Turret;
                    msg->type = NpcEvent::FiredWeapon;
                    msg->position = tx.getWorldPosition();
                    msg->position.z += 0.18f;
                    msg->velocity = glm::rotate(cro::Transform::Y_AXIS, (rotation - 0.2f) + (static_cast<float>(i) * 0.4f), cro::Transform::Z_AXIS);
                }
            }
        };
        sendCommand(cmd);
    }
}