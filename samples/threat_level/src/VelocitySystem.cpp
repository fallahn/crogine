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

#include "VelocitySystem.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/ecs/components/Transform.hpp>

#include <crogine/detail/glm/gtx/norm.hpp>

VelocitySystem::VelocitySystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(VelocitySystem))
{
    requireComponent<cro::Transform>();
    requireComponent<Velocity>();
}

//public
void VelocitySystem::process(float dt)
{
    auto& entities = getEntities();
    for (auto& entity : entities)
    {
        auto& tx = entity.getComponent<cro::Transform>();
        auto& velocity = entity.getComponent<Velocity>();

        //reduce velocity with friction
        tx.move(velocity.velocity * dt);
        velocity.velocity *= (1.f - (velocity.friction * dt));

        //clamp to zero when below min speed to stop drifting
        if (glm::length2(velocity.velocity) < velocity.minSpeed)
        {
            velocity.velocity = {};
        }

        //reset rotation
        tx.setRotation(tx.getRotation() - (tx.getRotation() * (dt * 2.f)));
    }
}