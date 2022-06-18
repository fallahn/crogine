/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include "PropFollowSystem.hpp"
#include "CollisionMesh.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/util/Maths.hpp>
#include <crogine/util/Random.hpp>

PropFollowSystem::PropFollowSystem(cro::MessageBus& mb, const CollisionMesh& cm)
    : cro::System   (mb, typeid(PropFollowSystem)),
    m_collisionMesh (cm)
{
    requireComponent<PropFollower>();
    requireComponent<cro::Transform>();
}

//public
void PropFollowSystem::process(float dt)
{
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        if (!entity.getComponent<cro::Model>().isHidden())
        {
            auto& follower = entity.getComponent<PropFollower>();
            if (follower.state == PropFollower::Idle)
            {
                follower.idleTime -= dt;
                if (follower.idleTime < 0)
                {
                    follower.state = PropFollower::Follow;

                    entity.getComponent<cro::Transform>().setPosition(follower.path.getPoint(0));
                    follower.target = 1;
                }
            }
            else
            {
                auto targetPos = follower.path.getPoint(follower.target);
                float speed = follower.speed;// follower.path.getSpeedMultiplier(follower.target - 1)* (follower.speed / follower.path.getLength());

                auto& tx = entity.getComponent<cro::Transform>();

                auto dir = targetPos - tx.getPosition();

                auto len2 = glm::length2(glm::vec2(dir.x, dir.z));

                auto tangent = glm::normalize(dir);
                auto movement = tangent * speed;
                tx.move(movement * dt);



                follower.targetRotation = std::atan2(-movement.z, movement.x);
                follower.rotation += cro::Util::Maths::shortestRotation(follower.rotation, follower.targetRotation) * (dt * 6.f);
                tx.setRotation(cro::Transform::Y_AXIS, follower.rotation);

                auto pos = tx.getPosition();
                auto result = m_collisionMesh.getTerrain(pos);
                pos.y = result.height;

                tx.setPosition(pos);

                /*auto bitan = glm::normalize(glm::cross(tangent, result.normal));
                tangent = glm::normalize(glm::cross(bitan, result.normal));
                glm::mat3 mat(tangent, result.normal, bitan);
                tx.setRotation(glm::quat(mat));*/


                if (len2 < (0.5f * 0.5f))
                {
                    //TODO check the follower to see if we ping-pong because loop is false
                    follower.target = (follower.target + 1) % follower.path.getPoints().size();

                    if (follower.target == 0)
                    {
                        follower.state = PropFollower::Idle;
                        follower.idleTime = 4.f + cro::Util::Random::value(0.f, 3.f);
                    }
                }
            }
        }
    }
}