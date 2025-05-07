/*-----------------------------------------------------------------------

Matt Marchant 2023
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

#include "BallAnimationSystem.hpp"
#include "InterpolationSystem.hpp"
#include "BallSystem.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

BallAnimationSystem::BallAnimationSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(BallAnimationSystem))
{
    requireComponent<BallAnimation>();
    requireComponent<cro::Transform>();
}

//public
void BallAnimationSystem::process(float dt)
{
    for (auto entity : getEntities())
    {
        const auto& animation = entity.getComponent<BallAnimation>();
        if (animation.parent.destroyed())
        {
            //we're waiting to be cleaned up so don't bother animating
            continue;
        }

        const auto& interp = animation.parent.getComponent<InterpolationComponent<InterpolationType::Linear>>();


        //as the dot product of these reaches +-1 it's not
        //possible to get a cross product (and probably causes the NaN case...)
        if (auto len2 = glm::length2(interp.getVelocity()); len2 != 0)
        {
            auto len = glm::sqrt(len2);
            auto forward = interp.getVelocity() / len;

            if (auto d = glm::dot(forward, cro::Transform::Y_AXIS);
                d < 0.9f && d > -0.9f)
            {
                auto rightVec = glm::normalize(glm::cross(cro::Transform::Y_AXIS, forward));
                CRO_ASSERT(!std::isnan(rightVec.x), "");

                rightVec = glm::inverse(glm::toMat3(animation.parent.getComponent<cro::Transform>().getRotation())) * rightVec;
                CRO_ASSERT(!std::isnan(rightVec.x), "NaN from parent rotation");

                rightVec = glm::inverse(entity.getComponent<cro::Transform>().getRotation()) * rightVec;
                CRO_ASSERT(!std::isnan(rightVec.x), "NaN from ball rotation");

                float rotation = (len / Ball::Radius);
                CRO_ASSERT(!std::isnan(rotation), "");

                entity.getComponent<cro::Transform>().rotate(rightVec, rotation * dt);
            }
        }
    }
}