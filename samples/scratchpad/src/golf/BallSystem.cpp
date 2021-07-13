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

#include <crogine/ecs/components/Transform.hpp>

namespace
{
    constexpr glm::vec3 Gravity(0.f, -9.f, 0.f);
}

BallSystem::BallSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(BallSystem))
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
        if (ball.state == Ball::State::Flight)
        {
            //add gravity
            ball.velocity += Gravity * dt;

            //add wind

            //add air friction?

            //move by velocity
            auto& tx = entity.getComponent<cro::Transform>();
            tx.move(ball.velocity * dt);

            //test collision
            if (auto y = entity.getComponent<cro::Transform>().getPosition().y; y < 0)
            {
                //ball.state = Ball::State::Idle;
                tx.move({ 0.f, -y, 0.f });
                ball.velocity = glm::reflect(ball.velocity, glm::vec3(0.f, 1.f, 0.f));
            }

            //if current surface is green, test distance to pin
        }
    }
}

//private
void BallSystem::doCollision(cro::Entity entity)
{
    //check height

    //if ball below radius, check terrain

    //apply dampening based on terrain (or splash)

    //check the surface map for normal vector for reflection

    //update ball velocity / state
}