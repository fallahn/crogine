/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2022
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

#include <crogine/core/App.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

namespace
{
    static constexpr float MaxDistSqr = 4.f * 4.f; //if we're bigger than this go straight to dest to hide flickering
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
        CRO_ASSERT(!std::isnan(diff.x), "");

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
            interp.m_currentTime += interp.m_elapsedTimer.restart().asMilliseconds();
            if (interp.m_currentTime > interp.m_timeDifference)
            {
                //shift interp target to next in buffer if available
                interp.applyNextTarget();
            }

            float currTime = std::max(0.f, std::min(1.f, static_cast<float>(interp.m_currentTime) / (static_cast<float>(interp.m_timeDifference))));

            tx.setRotation(glm::slerp(interp.m_previousPoint.rotation, interp.m_targetPoint.rotation, currTime));
            tx.setPosition(interp.m_previousPoint.position + (diff * currTime));
        }
    }
}

//private
