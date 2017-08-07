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

#include "BuddySystem.hpp"
#include "Messages.hpp"
#include "ItemSystem.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/core/Clock.hpp>

#include <glm/gtx/norm.hpp>

namespace
{
    const float fireRate = 0.125f;
    const float lifespan = 20.f;
    const float maxEmitRate = 25.f;
}

BuddySystem::BuddySystem(cro::MessageBus& mb)
    : cro::System   (mb, typeid(BuddySystem)),
    m_requestSpawn  (false),
    m_requestDespawn(false)
{
    requireComponent<Buddy>();
    requireComponent<cro::Transform>();
}

//public
void BuddySystem::handleMessage(const cro::Message& msg)
{
    if (msg.id == MessageID::PlayerMessage)
    {
        const auto& data = msg.getData<PlayerEvent>();
        if (data.type == PlayerEvent::CollectedItem && data.itemID == CollectableItem::Buddy)
        {
            m_requestSpawn = true;
        }
        else if (data.type == PlayerEvent::Died)
        {
            m_requestDespawn = true;
        }
    }
}

void BuddySystem::process(cro::Time dt)
{
    float dtSec = dt.asSeconds();

    auto& entities = getEntities();
    for (auto& entity : entities)
    {
        switch (entity.getComponent<Buddy>().state)
        {
        default: break;
        case Buddy::Inactive:
            if (m_requestSpawn) 
            {
                entity.getComponent<Buddy>().state = Buddy::Initialising;
                entity.getComponent<Buddy>().lifespan = lifespan;
                m_requestSpawn = false;
                m_requestDespawn = false;
            }
            break;
        case Buddy::Initialising:
            processInit(dtSec, entity);
            break;
        case Buddy::Active:
            processActive(dtSec, entity);
            break;
        }
    }
}

//private
void BuddySystem::processInit(float dt, cro::Entity entity)
{
    //when reached final position transmit spawn message
    auto parent = entity.getComponent<cro::Transform>().getParentID();
    
    auto& tx = getScene()->getEntity(parent).getComponent<cro::Transform>();
    tx.move(-tx.getPosition() * dt * 5.f);

    if (glm::length2(tx.getPosition()) < 0.1f)
    {
        tx.setPosition({});
        entity.getComponent<Buddy>().state = Buddy::Active;
        entity.getComponent<Buddy>().lifespan = lifespan;
        entity.getComponent<cro::ParticleEmitter>().start();

        auto* msg = postMessage<BuddyEvent>(MessageID::BuddyMessage);
        msg->type = BuddyEvent::Spawned;
        msg->position = tx.getWorldPosition();
    }
}

void BuddySystem::processActive(float dt, cro::Entity entity)
{
    auto& buddy = entity.getComponent<Buddy>();
    
    //countdown life and despawn if despawn request or expired.
    buddy.lifespan -= dt;

    //increase particle effect
    entity.getComponent<cro::ParticleEmitter>().emitterSettings.emitRate = maxEmitRate * (1.f - (buddy.lifespan / lifespan));

    if (buddy.lifespan < 0 || m_requestDespawn)
    {
        m_requestDespawn = false;
        buddy.state = Buddy::Inactive;

        auto* msg = postMessage<BuddyEvent>(MessageID::BuddyMessage);
        msg->type = BuddyEvent::Died;
        msg->position = entity.getComponent<cro::Transform>().getWorldPosition();

        auto parent = entity.getComponent<cro::Transform>().getParentID();
        getScene()->getEntity(parent).getComponent<cro::Transform>().setPosition({ -25.f, 0.f, 0.f });

        //stop particle effect
        entity.getComponent<cro::ParticleEmitter>().stop();
    }

    //and shoot
    buddy.fireTime -= dt;
    if (buddy.fireTime < 0)
    {
        buddy.fireTime = fireRate;

        auto* msg = postMessage<BuddyEvent>(MessageID::BuddyMessage);
        msg->type = BuddyEvent::FiredWeapon;
        msg->position = entity.getComponent<cro::Transform>().getWorldPosition();
    }
}