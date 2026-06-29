/*-----------------------------------------------------------------------

Matt Marchant 2026
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

#pragma once

#include <crogine/ecs/Entity.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Skeleton.hpp>

#include <crogine/util/Random.hpp>

struct BehaviourSeagull final
{
    std::int32_t lastState = 0;
    float startHeight = 0.f;

    static constexpr glm::vec3 Elevation = glm::vec3(0.f, 0.5f, 0.f);

    cro::Scene& scene;

    enum class IdleState
    {
        Idle, Walk
    }idleState = IdleState::Idle;
    std::int32_t rotateDir = 1;
    float idleTime = 2.f;

    explicit BehaviourSeagull(cro::Scene& s) 
        : scene(s)
    {
        rotateDir = cro::Util::Random::value(0, 1) * 2 - 1;
    }


    void operator()(cro::Entity e, float dt)
    {
        const auto state = e.getComponent<cro::Callback>().getUserData<std::int32_t>();
        if (state != 0)
        {
            //fly mode
            if (state != lastState)
            {
                //we switched so change animation
                e.getComponent<cro::Skeleton>().play(e.getComponent<cro::Skeleton>().getAnimationIndex("Takeoff"), 2.f);
            }
            else if (e.getComponent<cro::Skeleton>().getState() == cro::Skeleton::Stopped)
            {
                e.getComponent<cro::Skeleton>().play(e.getComponent<cro::Skeleton>().getAnimationIndex("Flap"), 2.f);
            }

            auto& tx = e.getComponent<cro::Transform>();
            tx.move((-tx.getForwardVector() + Elevation) * dt * 6.f);

            if (tx.getPosition().y > (16.f + startHeight))
            {
                e.getComponent<cro::Callback>().active = false;
                scene.destroyEntity(e);
            }
        }
        else
        {
            startHeight = e.getComponent<cro::Transform>().getPosition().y;

            idleTime -= dt;
            if (idleState == IdleState::Idle)
            {
                if (idleTime < 0)
                {
                    idleTime += static_cast<float>(cro::Util::Random::value(1, 2)) / 4.5f;
                    idleState = IdleState::Walk;
                    rotateDir *= -1;
                }
            }
            else
            {
                e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 0.05f * rotateDir);
                if (idleTime < 0)
                {
                    idleTime += static_cast<float>(cro::Util::Random::value(6, 9));
                    idleState = IdleState::Idle;
                }
            }
        }

        lastState = state;
    }
};