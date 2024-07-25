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

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/util/Maths.hpp>
#include <crogine/util/Random.hpp>

namespace
{
    constexpr float MinTargetRad = 0.5f;
    constexpr float MinTargetRadSqr = MinTargetRad * MinTargetRad;
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
                    follower.initAxis(entity);
                }
            }
            else
            {
                //check proximity to player position so we don't bump into them :)
                static constexpr float MaxProximity = 5.f;
                static constexpr float MaxProximitySqr = MaxProximity * MaxProximity;
                auto proximitySpeed = glm::length2(entity.getComponent<cro::Transform>().getWorldPosition() - m_playerPosition);
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
                            msg->position = entity.getComponent<cro::Transform>().getWorldPosition();
                            msg->terrain = CollisionEvent::Special::Timeout;
                            msg->type = CollisionEvent::Begin;
                        }
                    }
                }
                else
                {
                    proximitySpeed = 1.f;
                }

                for (auto& point : follower.axis)
                {
                    auto targetPos = follower.path.getPoint(point.target);
                    auto dir = targetPos - point.position;
                    auto len2 = glm::length2(glm::vec2(dir.x, dir.z));

                    point.position += glm::normalize(dir) * follower.speed * proximitySpeed * dt;
                    //point.position = cro::Util::Maths::smoothDamp(point.position, targetPos, point.velocity, 0.0125f, dt, follower.speed);

                    if (len2 < MinTargetRadSqr)
                    {
                        point.target = (point.target + 1) % follower.path.getPoints().size();

                        if (point.target == 0)
                        {
                            if (!follower.loop)
                            {
                                follower.path.reverse();
                            }
                            /*else
                            {
                                point.target = 1;
                            }*/

                            follower.state = PropFollower::Idle;
                        }
                    }
                }

                auto axis = (follower.axis[0].position - follower.axis[1].position) / 2.f;
                auto pos = axis + follower.axis[1].position;

                //some paths might be aircraft, such as a blimp, so
                //only snap these below a threshold
                //or boats which need to be above water level
                if (pos.y < 15.f)
                {
                    auto result = m_collisionMesh.getTerrain(pos);
                    if (result.height > WaterLevel)
                    {
                        pos.y = result.height;
                    }
                }

                entity.getComponent<cro::Transform>().setPosition(pos);


                follower.targetRotation = std::atan2(-axis.z, axis.x);
                follower.rotation += cro::Util::Maths::shortestRotation(follower.rotation, follower.targetRotation) * (dt * 6.f);
                entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, follower.rotation);
            }
        }
    }
}

//private
void PropFollowSystem::onEntityAdded(cro::Entity e)
{
    e.getComponent<PropFollower>().initAxis(e);
}

void PropFollower::initAxis(cro::Entity e)
{
    //this assumes the model is aligned along the X axis
    //to match atan2

    //we align the axis along the first segment of the path
    //and space them bounding box width

    auto bb = e.getComponent<cro::Model>().getAABB();

    //bigger models such as the blimp want their axes scaling
    //less as they're already big - so we'll approximate around
    //the blimp length
    const float scale = 4.f - (3.f * (bb[1].x / 38.f));

    auto& follower = e.getComponent<PropFollower>();
    follower.axis[0].position = follower.path.getPoint(0);
    auto dir = glm::normalize(follower.path.getPoint(1) - follower.path.getPoint(0));
    follower.axis[0].position += dir * bb[1].x * scale;

    follower.axis[1].position = follower.path.getPoint(0);
    follower.axis[1].position += dir * bb[0].x * scale;


    follower.axis[0].target = 1;
    follower.axis[1].target = 1;

    e.getComponent<cro::Transform>().setPosition(follower.path.getPoint(0));
}