/*-----------------------------------------------------------------------

Matt Marchant 2020
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

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

namespace
{
    const float MaxDistSqr = 460.f * 460.f; //if we're bigger than this go straight to dest to hide flickering

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
            float currTime = std::min(static_cast<float>(interp.m_elapsedTimer.elapsed().asMilliseconds()) / interp.m_timeDifference, 1.f);

            if (currTime < 1)
            {
                tx.setRotation(glm::slerp(interp.m_previousPoint.rotation, interp.m_targetPoint.rotation, currTime));
                tx.setPosition(interp.m_previousPoint.position + (diff * currTime));
            }
            else
            {
                //snap to target
                tx.setPosition(interp.m_targetPoint.position);
                tx.setRotation(interp.m_targetPoint.rotation);

                //shift interp target to next in buffer if available
                interp.applyNextTarget();
            }
        }
    }
}

//private
