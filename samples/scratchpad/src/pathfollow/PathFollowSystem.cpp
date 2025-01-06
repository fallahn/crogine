/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#include "PathFollowSystem.hpp"

#include <crogine/detail/glm/gtx/norm.hpp>
#include <crogine/util/Matrix.hpp>

namespace
{
    glm::quat getTargetRotation(glm::vec3 target)
    {
        glm::vec3 z_axis = glm::normalize(-target);
        glm::vec3 x_axis = glm::normalize(glm::cross(cro::Transform::Y_AXIS, z_axis));
        glm::vec3 y_axis = glm::normalize(glm::cross(z_axis, x_axis));

        glm::mat4 rotationMatrix(
            glm::vec4(x_axis, 0.f),
            glm::vec4(y_axis, 0.f),
            glm::vec4(z_axis, 0.f),
            glm::vec4(0.f, 0.f, 0.f, 1.f));

        return glm::quat_cast(rotationMatrix);
    }
}

PathFollowSystem::PathFollowSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(PathFollowSystem))
{
    requireComponent<PathFollower>();
    requireComponent<cro::Transform>();
}

//public
void PathFollowSystem::process(float dt)
{
    for (auto entity : getEntities())
    {
        auto& follower = entity.getComponent<PathFollower>();
        auto& tx = entity.getComponent<cro::Transform>();

        //update our rotation
        if (follower.currentTurn < 1)
        {
            follower.currentTurn = std::min(1.f, follower.currentTurn + (dt * follower.turnSpeed));
            auto rotation = glm::slerp(follower.startRotation, follower.targetRotation, follower.currentTurn);
            tx.setRotation(rotation);
        }

        //move along our forward vector
        const auto forward = glm::normalize(tx.getForwardVector());
        tx.move(forward * follower.moveSpeed * dt);

        //as we've moved the rotation will be out of date, so check
        //to see if we need to update it
        const auto worldPos = tx.getWorldPosition();
        if (glm::dot(forward, glm::normalize(follower.path[follower.target] - worldPos)) < 0.98f)
        {
            follower.startRotation = entity.getComponent<cro::Transform>().getRotation();
            follower.targetRotation = getTargetRotation(follower.path[follower.target] - worldPos);
            follower.currentTurn = 0.f;
        }


        //if we're in the radius of the target point
        //fetch the next target
        auto dist = glm::length2(entity.getComponent<cro::Transform>().getWorldPosition() - follower.path[follower.target]);
        if (dist < (follower.minRadius * follower.minRadius))
        {
            //TODO pause at end if required, and check if we're looped or ping pong

            follower.target = (follower.target + 1) % follower.path.size();

            follower.startRotation = entity.getComponent<cro::Transform>().getRotation();
            follower.targetRotation = getTargetRotation(follower.path[follower.target] - worldPos);
            follower.currentTurn = 0.f;
        }
    }
}

//private
void PathFollowSystem::onEntityAdded(cro::Entity entity)
{
    auto& follower = entity.getComponent<PathFollower>();
    CRO_ASSERT(follower.path.size() > 1, "Needs at least 2 points");

    entity.getComponent<cro::Transform>().setPosition(follower.path[0]);

    follower.targetRotation = getTargetRotation(follower.path[1] - follower.path[0]);
    follower.currentTurn = 0.f;
}