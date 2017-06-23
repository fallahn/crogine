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
#include <crogine/util/Random.hpp>

namespace
{
    const float eliteSpawnTime = 24.f;
    const float choppaSpawnTime = 9.f;
    const float zDepth = -9.3f; //bums, this is replicated from NpcSystem.cpp
    const float weaverSpawnTime = 6.f;

    const float eliteHealth = 15.f;
    const float choppaHealth = 8.f;
    const float speedrayHealth = 2.f;
    const float weaverHealth = 5.f;
}

NpcDirector::NpcDirector()
    : m_eliteRespawn(eliteSpawnTime / 4.f),
    m_choppaRespawn(choppaSpawnTime / 2.f),
    m_speedrayRespawn(choppaSpawnTime),
    m_weaverRespawn(1.f)
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
            cmd.targetFlags = CommandID::Turret;
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
        cmd.action = [](cro::Entity entity, cro::Time)
        {
            auto& status = entity.getComponent<Npc>();
            if (!status.active)
            {
                status.active = true;
                status.elite.destination.x = cro::Util::Random::value(1.f, 4.f);
                status.elite.movementCount = cro::Util::Random::value(4, 8);
                status.elite.pauseTime = cro::Util::Random::value(1.2f, 2.2f);
                status.health = eliteHealth;

                auto& tx = entity.getComponent<cro::Transform>();
                tx.setPosition({ 5.6f, cro::Util::Random::value(-2.f, 2.f) , zDepth });
            }
        };
        sendCommand(cmd);
    }

    m_choppaRespawn -= dtSec;
    if (m_choppaRespawn < 0)
    {
        m_choppaRespawn = choppaSpawnTime + cro::Util::Random::value(-2.f, 3.6439f);

        cro::Command cmd;
        cmd.targetFlags = CommandID::Choppa;
        cmd.action = [](cro::Entity entity, cro::Time)
        {
            auto& status = entity.getComponent<Npc>();
            if (!status.active)
            {
                status.active = true;
                status.choppa.moveSpeed = cro::Util::Random::value(-8.3f, -7.8f);
                status.choppa.deathVelocity.x = status.choppa.moveSpeed;
                status.choppa.tableIndex = cro::Util::Random::value(0, 40); //hmm don't have table size here (see NpcSystem)
                status.health = choppaHealth;

                //reset position
                auto& tx = entity.getComponent<cro::Transform>();
                tx.setPosition({ 7.f, -ChoppaNavigator::spacing + (status.choppa.ident * ChoppaNavigator::spacing), zDepth });
            }
        };
        sendCommand(cmd);
    }

    m_speedrayRespawn -= dtSec;
    if(m_speedrayRespawn < 0)
    {
        cro::Command cmd;
        cmd.targetFlags = CommandID::Speedray;
        cmd.action = [](cro::Entity entity, cro::Time)
        {
            auto& status = entity.getComponent<Npc>();
            if (!status.active)
            {
                status.active = true;
                status.health = speedrayHealth;

                //reset position
                auto& tx = entity.getComponent<cro::Transform>();
                tx.setPosition({ 7.f, 0.f, zDepth });
            }
        };
        sendCommand(cmd);

        m_speedrayRespawn = choppaSpawnTime;
    }

    m_weaverRespawn -= dtSec;
    if (m_weaverRespawn < 0)
    {
        m_weaverRespawn = weaverSpawnTime;
        auto yPos = cro::Util::Random::value(-1.2f, 1.2f);

        cro::Command cmd;
        cmd.targetFlags = CommandID::Weaver;
        cmd.action = [yPos](cro::Entity entity, cro::Time) 
        {
            auto& status = entity.getComponent<Npc>();
            if (!status.active)
            {
                status.active = true;
                status.weaver.yPos = yPos;
                status.weaver.tableIndex = status.weaver.tableStartIndex;
                status.health = weaverHealth;

                auto& tx = entity.getComponent<cro::Transform>();
                tx.setPosition({ 7.f + (static_cast<float>(status.weaver.ident) * (WeaverNavigator::spacing * tx.getScale().x)), status.weaver.yPos, zDepth });
            }
        };
        sendCommand(cmd);
    }
}