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
    constexpr glm::vec3 Gravity = glm::vec3(0.f, -0.9f, 0.f);
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
    const std::int32_t StepCount = 2;
    float stepTime = dt / StepCount;

    auto& entities = getEntities();

    for (auto step = 0; step < StepCount; ++step)
    {
        for (auto entity : entities)
        {
            auto& ball = entity.getComponent<Ball>();
            if (ball.state == Ball::State::Awake)
            {
                auto& tx = entity.getComponent<cro::Transform>();


                ball.velocity += Gravity / static_cast<float>(StepCount);

                if(ball.contactCount != 0)
                {
                    if (step == 0)
                    {
                        ball.velocity *= 0.99999f;
                    }

                    if (glm::length2(ball.velocity) < 0.001f)
                    {
                        ball.state = Ball::State::Sleep;
                    }
                }
                ball.contactCount = 0;
                tx.move(ball.velocity * stepTime);

                if (tx.getPosition().y < -2.f)
                {
                    ball.velocity = Gravity;
                    tx.setPosition(glm::vec3(cro::Util::Random::value(-0.2f, 0.3f), 8.f, cro::Util::Random::value(-0.2f, 0.3f)));
                }

                auto rot = tx.getRotation();
                auto pos = tx.getWorldPosition();
                btTransform btXf(btQuaternion(rot.x, rot.y, rot.z, rot.w), btVector3(pos.x, pos.y, pos.z));
                ball.collisionObject->setWorldTransform(btXf);


            }
        }


        m_collisionWorld->performDiscreteCollisionDetection();

        auto manifoldCount = m_collisionWorld->getDispatcher()->getNumManifolds();

        for (auto i = 0; i < manifoldCount; ++i)
        {
            auto manifold = m_collisionWorld->getDispatcher()->getManifoldByIndexInternal(i);
            auto body0 = manifold->getBody0();
            auto body1 = manifold->getBody1();
            Ball* ball = nullptr;
            glm::vec3 sumNormal = glm::vec3(0.f);

            auto contactCount = manifold->getNumContacts();
            for (auto j = 0; j < contactCount; ++j)
            {
                const auto& point = manifold->getContactPoint(j);
                auto normal = point.m_normalWorldOnB;
                auto penetration = point.getDistance();


                if (body0->getUserPointer() != nullptr)
                {
                    ball = static_cast<Ball*>(body0->getUserPointer());

                    if (ball->state == Ball::State::Awake)
                    {
                        sumNormal += glm::vec3(normal.x(), normal.y(), normal.z());

                        auto& tx = ball->entity.getComponent<cro::Transform>();
                        normal *= -penetration;
                        tx.move({ normal.x(), normal.y(), normal.z() });
                    }
                }
                else if (body1->getUserPointer() != nullptr)
                {
                    ball = static_cast<Ball*>(body0->getUserPointer());

                    if (ball->state == Ball::State::Awake)
                    {
                        sumNormal += glm::vec3(-normal.x(), -normal.y(), -normal.z());

                        auto& tx = ball->entity.getComponent<cro::Transform>();
                        normal *= penetration;
                        tx.move({ normal.x(), normal.y(), normal.z() });
                    }
                }
            }

            if (ball &&
                contactCount)
            {
                sumNormal /= contactCount;
                ball->velocity = glm::reflect(ball->velocity, sumNormal);
                ball->velocity *= 0.7f;
                ball->contactCount = contactCount;
            }
        }
    }
}