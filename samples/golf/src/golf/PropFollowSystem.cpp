/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2024
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

#include "PropFollowSystem.hpp"
#include "CollisionMesh.hpp"
#include "MessageIDs.hpp"

#include <crogine/ecs/components/Model.hpp>

#include <crogine/util/Maths.hpp>
#include <crogine/util/Random.hpp>

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

PropFollowSystem::PropFollowSystem(cro::MessageBus& mb, const CollisionMesh& cm)
    : cro::System   (mb, typeid(PropFollowSystem)),
    m_collisionMesh (cm),
    m_playerPosition(glm::vec3(0.f))
{
    requireComponent<PropFollower>();
    requireComponent<cro::Transform>();
}

//public
void PropFollowSystem::process(float dt)
{
    const auto& entities = getEntities();
    for (auto entity : entities)
    {
        if (!entity.getComponent<cro::Model>().isHidden())
        {
            auto& follower = entity.getComponent<PropFollower>();
            if (follower.state == PropFollower::Idle)
            {
                follower.stateTimer += dt;
                if (follower.stateTimer > follower.idleTime)
                {
                    follower.state = PropFollower::Follow;
                    follower.stateTimer = 0.f;
                }
            }
            else
            {
                auto& tx = entity.getComponent<cro::Transform>();

                //check proximity to player position so we don't bump into them :)
                static constexpr float MaxProximity = 5.f;
                static constexpr float MaxProximitySqr = MaxProximity * MaxProximity;
                auto proximitySpeed = glm::length2(tx.getWorldPosition() - m_playerPosition);
                if (proximitySpeed < MaxProximitySqr)
                {
                    proximitySpeed = glm::smoothstep(0.5f, 0.99f, std::sqrt(proximitySpeed) / MaxProximity);

                    if (proximitySpeed < 0.01f)
                    {
                        follower.waitTimeout -= dt;
                        if (follower.waitTimeout < 0)
                        {
                            follower.waitTimeout = PropFollower::WaitTime + cro::Util::Random::value(5, 10);

                            auto* msg = postMessage<CollisionEvent>(cl::MessageID::CollisionMessage);
                            msg->position = tx.getWorldPosition();
                            msg->terrain = CollisionEvent::Special::Timeout;
                            msg->type = CollisionEvent::Begin;
                        }
                    }
                }
                else
                {
                    proximitySpeed = 1.f;
                }


                //and the proximity to the target so we can slow down as we approach
                static constexpr float MaxDecelProximity = 15.f;
                static constexpr float MaxDecelProximitySqr = MaxDecelProximity * MaxDecelProximity;
                auto deceleration = glm::length2(follower.path[follower.target] - tx.getPosition());
                if (deceleration < MaxProximitySqr)
                {
                    proximitySpeed *= 0.85f + (0.15f * smoothstep(0.05f, 0.99f, std::sqrt(deceleration) / MaxProximity));
                }                
                

                //update our rotation
                if (follower.currentTurn < 1)
                {
                    follower.currentTurn = std::min(1.f, follower.currentTurn + (dt * follower.turnSpeed));
                    auto rotation = glm::slerp(follower.startRotation, follower.targetRotation, follower.currentTurn);
                    tx.setRotation(rotation);
                }

                //move along our forward vector
                const auto forward = glm::normalize(tx.getForwardVector());
                tx.move(forward * follower.moveSpeed * proximitySpeed * dt);

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
                    follower.target = (follower.target + 1) % follower.path.size();

                    if (follower.target == 0)
                    {
                        //if we loop carry on, else idle and reverse the path
                        if (!follower.loop)
                        {
                            follower.state = PropFollower::Idle;
                            std::reverse(follower.path.begin(), follower.path.end());
                        }
                    }

                    follower.startRotation = entity.getComponent<cro::Transform>().getRotation();
                    follower.targetRotation = getTargetRotation(follower.path[follower.target] - worldPos);
                    follower.currentTurn = 0.f;
                }

                //some paths might be aircraft, such as a blimp, so
                //only snap these below a threshold
                //or boats which need to be above water level
                auto pos = tx.getPosition();
                if (pos.y < 15.f)
                {
                    auto result = m_collisionMesh.getTerrain(pos);
                    if (result.height > WaterLevel)
                    {
                        pos.y = result.height;
                    }
                }

                tx.setPosition(pos);
            }
        }
    }
}

//private
void PropFollowSystem::onEntityAdded(cro::Entity entity)
{
    //e.getComponent<PropFollower>().initAxis(e);
    auto& follower = entity.getComponent<PropFollower>();
    CRO_ASSERT(follower.path.size() > 1, "Needs at least 2 points");

    entity.getComponent<cro::Transform>().setPosition(follower.path[0]);

    follower.targetRotation = getTargetRotation(follower.path[1] - follower.path[0]);
    follower.startRotation = follower.targetRotation;
    follower.currentTurn = 1.f;

    entity.getComponent<cro::Transform>().setRotation(follower.startRotation);
}