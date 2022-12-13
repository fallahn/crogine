/*-----------------------------------------------------------------------

Matt Marchant - 2022
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

#include "PocketBallSystem.hpp"
#include "BilliardsSystem.hpp"
#include "MessageIDs.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>
#include <crogine/ecs/Scene.hpp>

namespace
{
    constexpr float MaxXPos = (-100.f - ((BilliardBall::Radius * 2.f) * 8.f) + BilliardBall::Radius);
    constexpr float Acceleration = 0.7f;
    constexpr std::int32_t MaxBalls = 16;
}

PocketBallSystem::PocketBallSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(PocketBallSystem))
{
    requireComponent<PocketBall>();
    requireComponent<cro::Transform>();
    requireComponent<cro::AudioEmitter>();
}

//public
void PocketBallSystem::process(float dt)
{
    static constexpr float MinDistance = (BilliardBall::Radius * 2.f);
    cro::Entity previousEntity;

    const auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& ball = entity.getComponent<PocketBall>();
        if (ball.awake)
        {
            ball.velocity += Acceleration * dt;
            float angularVel = ball.velocity / BilliardBall::Radius;

            auto& tx = entity.getComponent<cro::Transform>();
            tx.move({ -ball.velocity * dt, 0.f, 0.f });
            tx.rotate(cro::Transform::Z_AXIS, angularVel * dt);

            entity.getComponent<cro::AudioEmitter>().setPitch(0.3f + (0.1f * ball.velocity));

            //check neighbour
            if (previousEntity.isValid())
            {
                //as we only travel in X we can simplify this:
                float distance = tx.getPosition().x - previousEntity.getComponent<cro::Transform>().getPosition().x;

                if (distance < MinDistance)
                {
                    auto* msg = postMessage<BilliardBallEvent>(MessageID::BilliardsMessage);
                    msg->type = BilliardBallEvent::PocketEnd;
                    msg->volume = 0.28f * ball.velocity;

                    auto diff = MinDistance - distance;
                    tx.move({ diff, 0.f, 0.f });
                    ball.velocity = 0.f;
                    ball.awake = false;

                    entity.getComponent<cro::AudioEmitter>().stop();
                }
            }
            //else check we're at the bottom
            else if (tx.getPosition().x < MaxXPos)
            {
                auto* msg = postMessage<BilliardBallEvent>(MessageID::BilliardsMessage);
                msg->type = BilliardBallEvent::PocketEnd;
                msg->volume = 0.28f * ball.velocity;


                tx.setPosition({ MaxXPos, 0.f, 0.f });
                ball.awake = false;
                ball.velocity = 0.f;

                entity.getComponent<cro::AudioEmitter>().stop();
            }
        }
        else
        {
            //check if neighbour was removed and wake up
            if (previousEntity.isValid())
            {
                auto distance = entity.getComponent<cro::Transform>().getPosition().x - previousEntity.getComponent<cro::Transform>().getPosition().x;
                if (distance - MinDistance > 0.01f)
                {
                    entity.getComponent<PocketBall>().awake = true;
                    entity.getComponent<cro::AudioEmitter>().play();
                }
            }
        }
        previousEntity = entity;
    }
}

void PocketBallSystem::removeBall(std::int8_t id)
{
    auto& entities = getEntities();
    for (auto it = entities.crbegin(); it != entities.crend(); ++it)
    {
        if (it->getComponent<PocketBall>().id == id)
        {
            getScene()->destroyEntity(*it);
            break;
        }
    }
}

//private
void PocketBallSystem::onEntityAdded(cro::Entity)
{
    bool wasRemoved = false;

    const auto& entities = getEntities();
    for (auto entity : entities)
    {
        entity.getComponent<PocketBall>().totalCount++;
        if (entity.getComponent<PocketBall>().totalCount == MaxBalls)
        {
            getScene()->destroyEntity(entity);
            wasRemoved = true;
        }
    }

    if (wasRemoved)
    {
        for (auto entity : entities)
        {
            entity.getComponent<PocketBall>().awake = true;
            entity.getComponent<cro::AudioEmitter>().play();
        }
    }
}