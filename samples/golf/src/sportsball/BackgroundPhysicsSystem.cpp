/*-----------------------------------------------------------------------

Matt Marchant 2025
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include "BackgroundPhysicsSystem.hpp"
#include "SBallConsts.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>

namespace
{
    constexpr glm::vec3 Gravity(0.f, -9.8f, 0.f);
}

BGPhysicsSystem::BGPhysicsSystem(cro::MessageBus&  mb)
    : cro::System(mb, typeid(BGPhysicsSystem))
{
    requireComponent<BGPhysics>();
    requireComponent<cro::Transform>();
}

//public
void BGPhysicsSystem::process(float dt)
{
    for (auto entity : getEntities())
    {
        auto& phys = entity.getComponent<BGPhysics>();
        phys.velocity += Gravity * dt;
        phys.lifetime -= dt;

        if (phys.lifetime < 0)
        {
            getScene()->destroyEntity(entity);
        }
        else
        {
            auto& tx = entity.getComponent<cro::Transform>();
            tx.move(phys.velocity * dt);

            const float rotation = glm::length(phys.velocity) / phys.radius;
            tx.rotate(cro::Transform::Z_AXIS, -rotation * dt);

            const auto newPos = tx.getPosition();
            if (newPos.y  + phys.radius < 0.f)
            {
                tx.setPosition({ newPos.x, phys.radius, newPos.y });
                phys.velocity = glm::reflect(phys.velocity, cro::Transform::Y_AXIS);
                phys.velocity *= 0.9f;

                auto* msg = postMessage<sb::CollisionEvent>(sb::MessageID::CollisionMessage);
                msg->ballID = phys.id;
                msg->type = sb::CollisionEvent::Background;
                msg->position = tx.getPosition();
            }
        }
    }
}
