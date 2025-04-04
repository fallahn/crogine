/*-----------------------------------------------------------------------

Matt Marchant 2021
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include "InterpolationSystem.hpp"
#include "Messages.hpp"
#include "ActorSystem.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

namespace
{
    constexpr float MaxDistSqr = 40.f * 40.f; //if we're bigger than this go straight to dest to hide flickering
    constexpr float Latency = 0.03f;// 1.f / 20.f; //approx latency from interpolation. Used to extrapolate from velocity
}

InterpolationSystem::InterpolationSystem(cro::MessageBus& mb)
    : System(mb, typeid(InterpolationSystem))
{
    requireComponent<cro::Transform>();
    requireComponent<InterpolationComponent>();
}

//public
void InterpolationSystem::process(float dt)
{
    auto& entities = getEntities();
    for (auto& entity : entities)
    {
        auto& tx = entity.getComponent<cro::Transform>();
        auto& interp = entity.getComponent<InterpolationComponent>();
        
        //jump if a very large difference
        auto diff = (interp.m_targetPoint.position - interp.m_previousPoint.position);
        if (glm::length2(diff) > MaxDistSqr)
        {
            if (interp.m_enabled)
            {
                tx.setPosition(interp.m_targetPoint.position);
                tx.setRotation(interp.m_targetPoint.rotation);

                interp.applyNextTarget();
            }
            continue;
        }

        //previous position + diff * timePassed
        if (interp.m_enabled)
        {
            std::int32_t elapsed = interp.m_elapsedTimer.elapsed().asMilliseconds();
            float currTime = std::min(static_cast<float>(elapsed) / interp.m_timeDifference, 1.f);

            if (currTime < 1)
            {
                auto position = glm::mix(interp.m_previousPoint.position, interp.m_targetPoint.position, currTime);
                tx.setPosition(position);

                auto rotation = glm::slerp(interp.m_previousPoint.rotation, interp.m_targetPoint.rotation, currTime);
                tx.setRotation(rotation);


                /*interp.m_currentPoint.position = position;
                interp.m_currentPoint.rotation = rotation;
                interp.m_currentPoint.velocity = velocity;*/
            }
            else
            {
                //snap to target
                tx.setPosition(interp.m_targetPoint.position);
                tx.setRotation(interp.m_targetPoint.rotation);

                //shift interp target to next in buffer if available
                interp.applyNextTarget();
            }

            //check if we passed the timestamp where we ought to have died
            if (interp.m_previousPoint.timestamp + elapsed >= interp.m_removalTimestamp)
            {
                auto id = entity.getComponent<Actor>().id;

                auto* msg = postMessage<ActorEvent>(MessageID::ActorMessage);
                msg->id = id;
                msg->position = entity.getComponent<cro::Transform>().getPosition();
                msg->type = ActorEvent::Removed;


                getScene()->destroyEntity(entity);
            }
        }
    }
}

//private
