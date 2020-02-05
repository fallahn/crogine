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

#include "ExplosionSystem.hpp"
#include "Messages.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/util/Random.hpp>

namespace
{
    const float backgroundMultiplier = 21.3f;
}

ExplosionSystem::ExplosionSystem(cro::MessageBus& mb)
    : cro::System       (mb, typeid(ExplosionSystem)),
    m_aliveCount        (0),
    m_deadCount         (0),
    m_backgroundSpeed   (0.f)
{
    requireComponent<Explosion>();
    requireComponent<cro::SpriteAnimation>();
}

//public
void ExplosionSystem::handleMessage(const cro::Message& msg)
{
    switch (msg.id)
    {
    default: break;
    case MessageID::NpcMessage:
        {
            const auto& data = msg.getData<NpcEvent>();
            if (data.type == NpcEvent::Died ||
                data.type == NpcEvent::HealthChanged)
            {
                spawnExplosion(data.position);
            }
        }
        break;
    case MessageID::PlayerMessage:
        {
            const auto& data = msg.getData<PlayerEvent>();
            if (data.type == PlayerEvent::Died)
            {
                spawnExplosion(data.position);
            }
        }
        break;
    case MessageID::BackgroundSystem:
        {
            const auto& data = msg.getData<BackgroundEvent>();
            if (data.type == BackgroundEvent::SpeedChange)
            {
                m_backgroundSpeed = data.value * backgroundMultiplier;
            }
        }
        break;
    case MessageID::BuddyMessage:
    {
        const auto& data = msg.getData<BuddyEvent>();
        if (data.type == BuddyEvent::Died)
        {
            spawnExplosion(data.position);
        }
    }
        break;
    }
}

void ExplosionSystem::process(float dt)
{
    for (auto i = 0u; i < m_aliveCount; ++i)
    {
        auto entity = getScene()->getEntity(m_aliveExplosions[i]);
        entity.getComponent<cro::Transform>().move({ -m_backgroundSpeed * dt, 0.f, 0.f });
        if (!entity.getComponent<cro::SpriteAnimation>().playing)
        {
            //animation ended, move to dead list and off screen
            entity.getComponent<cro::Transform>().setPosition({ 0.f, 16.f, 9.5f });

            //it died, remove it
            m_deadExplosions[m_deadCount] = m_aliveExplosions[i];
            m_deadCount++;

            //and remove from alive list
            m_aliveCount--;
            m_aliveExplosions[i] = m_aliveExplosions[m_aliveCount];
            i--; //decrement so we process the swapped in ID

            //LOG("Explosion Died", cro::Logger::Type::Info);
        }
    }
}

//private
void ExplosionSystem::onEntityAdded(cro::Entity entity)
{
    m_deadExplosions.push_back(entity.getIndex());
    m_deadCount++;

    m_aliveExplosions.push_back(-1);

    std::shuffle(m_deadExplosions.begin(), m_deadExplosions.end(), cro::Util::Random::rndEngine);
}

void ExplosionSystem::spawnExplosion(glm::vec3 position)
{
    if (m_deadCount > 0)
    {
        m_deadCount--;
        auto entity = getScene()->getEntity(m_deadExplosions[m_deadCount]);
        m_aliveExplosions[m_aliveCount] = m_deadExplosions[m_deadCount];
        m_aliveCount++;

        entity.getComponent<cro::Transform>().setPosition(position);
        entity.getComponent<cro::SpriteAnimation>().play(0);
        //LOG("Spawned explosion: " + std::to_string(position.x) + ", " + std::to_string(position.y) + ", " + std::to_string(position.z), cro::Logger::Type::Info);
    }
}