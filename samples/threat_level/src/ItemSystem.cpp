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

#include "ItemSystem.hpp"
#include "Messages.hpp"
#include "ResourceIDs.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/core/App.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/PhysicsObject.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/util/Random.hpp>

#include <crogine/detail/glm/gtx/norm.hpp>

namespace
{
    const float minPlayerDistance = 4.4f; //squared dist
    const float magnetSpeed = 1.4f;
}

ItemSystem::ItemSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(ItemSystem))
{
    requireComponent<CollectableItem>();
    requireComponent<cro::Transform>();
    requireComponent<cro::Model>();
    requireComponent<cro::PhysicsObject>();
}

//public
void ItemSystem::process(cro::Time dt)
{
    const float dtSec = dt.asSeconds();

    auto& entities = getEntities();
    for (auto& entity : entities)
    {
        auto& status = entity.getComponent<CollectableItem>();
        if (status.active)
        {
            //move item
            auto& tx = entity.getComponent<cro::Transform>();
            tx.move({ status.speed * dtSec, 0.f, 0.f });

            tx.move(status.impulse * dtSec);
            status.impulse *= 0.94f;

            //if near the player get closer
            auto dir = m_playerPosition - tx.getWorldPosition();
            auto dist = glm::length2(dir);
            if (dist < minPlayerDistance)
            {
                float ratio = 1.f - (dist / minPlayerDistance);
                auto movement = glm::normalize(dir);
                tx.move(movement * magnetSpeed * ratio * dtSec);

                status.impulse += movement * 0.15f;
            }


            //if item out of view move back to beginning
            bool visible = entity.getComponent<cro::Model>().isVisible();
            if (!visible && status.wantsReset)
            {
                auto pos = tx.getWorldPosition();
                glm::vec3 dest(0.f);
                dest.x += 5.5f;
                dest.y = cro::Util::Random::value(-2.2f, 2.2f);
                dest.z = pos.z;
                tx.move(dest - pos); //keeps it in world coords
                status.wantsReset = false;
                status.active = false;
            }
            else if(visible)
            {
                //we've entered the visible area so will eventually want resetting
                status.wantsReset = true;
            }


            //check collision
            const auto& phys = entity.getComponent<cro::PhysicsObject>();
            const auto count = phys.getCollisionCount();
            for (auto i = 0u; i < count; ++i)
            {
                const auto& otherEnt = getScene()->getEntity(phys.getCollisionIDs()[i]);
                const auto& otherPhys = otherEnt.getComponent<cro::PhysicsObject>();

                //if laser move
                if (otherPhys.getCollisionGroups() & CollisionID::PlayerLaser)
                {
                    status.impulse.x += 3.f;
                }
                else if (otherPhys.getCollisionGroups() & CollisionID::Player)
                {
                    //if player, die
                    auto pos = tx.getWorldPosition();
                    glm::vec3 dest(0.f);
                    dest.x += 5.5f;
                    dest.y = cro::Util::Random::value(-2.2f, 2.2f);
                    dest.z = pos.z;
                    tx.move(dest - pos); //keeps it in world coords
                    status.wantsReset = false;
                    status.active = false;
                }
                //else nothing
            }
        }
    }
}

void ItemSystem::handleMessage(const cro::Message& msg)
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
        }
    }
}