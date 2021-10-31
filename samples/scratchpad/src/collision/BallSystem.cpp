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

#include "BallSystem.hpp"

#include <crogine/core/Console.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

#include <btBulletCollisionCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>

namespace
{
    constexpr glm::vec3 Gravity = glm::vec3(0.f, -0.7f, 0.f);
    const btVector3 RayLength = btVector3(0.f, 1.f, 0.f);
    constexpr float Friction = 0.99f;

    btVector3 fromGLM(glm::vec3 v)
    {
        return { v.x, v.y, v.z };
    }

    glm::vec3 toGLM(btVector3 v)
    {
        return { v.x(), v.y(), v.z() };
    }
}

BallSystem::BallSystem(cro::MessageBus& mb, std::unique_ptr<btCollisionWorld>& cw)
    : cro::System       (mb, typeid(BallSystem)),
    m_collisionWorld    (cw)
{
    requireComponent<cro::Transform>();
    requireComponent<Ball>();
}

//public
void BallSystem::process(float dt)
{
    auto& entities = getEntities();

    for (auto entity : entities)
    {
        auto& ball = entity.getComponent<Ball>();
        auto& tx = entity.getComponent<cro::Transform>();

        ball.prevPosition = tx.getPosition();
        ball.prevVelocity = ball.velocity;

        if (ball.state == Ball::State::Awake)
        {
            ball.velocity += Gravity;
            ball.velocity *= Friction;

            tx.move(ball.velocity * dt);

            auto worldPos = fromGLM(tx.getPosition());
            btCollisionWorld::ClosestRayResultCallback res(worldPos, worldPos + RayLength);
            m_collisionWorld->rayTest(worldPos, worldPos + RayLength, res);

            if (res.hasHit())
            {
                //as we're effectively moving back up the arc to the collision
                //point we need to recalc the velocity as it would have been, 
                //through interpolation.
                auto targetPos = toGLM(res.m_hitPointWorld);
                auto travel = glm::length(tx.getPosition() - ball.prevPosition);
                auto targetTravel = glm::length(targetPos - ball.prevPosition);
                float interp = std::min(1.f, travel / targetTravel); 

                ball.velocity = ball.prevVelocity + (Gravity * interp);
                ball.velocity *= Friction;

                tx.setPosition(targetPos);

                ball.velocity = glm::reflect(ball.velocity, -toGLM(res.m_hitNormalWorld));
                ball.velocity *= 0.35f; //TODO this should be variable based on the terrain
            }

            if (glm::length2(ball.velocity) < 0.1f)
            {
                //we might be at the top of an arc, so test
                //if we're near the ground first
                auto rayStart = worldPos + (RayLength / 50.f);
                auto rayEnd = worldPos - (RayLength / 25.f);
                res = { rayStart, rayEnd };
                m_collisionWorld->rayTest(rayStart, rayEnd, res);

                if (res.hasHit())
                {
                    ball.velocity = glm::vec3(0.f);
                    ball.state = Ball::State::Sleep;
                }
            }

            if (tx.getPosition().y < -2.f)
            {
                ball.velocity = Gravity;
                tx.setPosition(glm::vec3(cro::Util::Random::value(-0.2f, 0.3f), 5.f, cro::Util::Random::value(-0.2f, 0.3f)));
            }
        }
    }
}