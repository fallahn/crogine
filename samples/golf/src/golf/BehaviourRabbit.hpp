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

#include "CollisionMesh.hpp"
#include "PoissonDisk.hpp"

#include <crogine/core/Log.hpp>
#include <crogine/ecs/Entity.hpp>
#include <crogine/util/Random.hpp>

struct BehaviourRabbit final
{
    std::vector<std::array<float, 2u>> targetPoints;
    const CollisionMesh& collisionMesh;
    std::int32_t targetIndex = 0;
    glm::vec3 currentTarget = glm::vec3(0.f);

    static constexpr float AreaSize = 3.f;

    BehaviourRabbit(const CollisionMesh& mesh, glm::vec3 basePoint)
        : collisionMesh(mesh)
    {
        //the actual mesh won't be loaded when the behaviour is
        //created, so we store a reference and query it at runtime
        const std::array<float, 2u> MinArea = { basePoint.x - AreaSize, -basePoint.z - AreaSize };
        const std::array<float, 2u> MaxArea = { basePoint.x + AreaSize, -basePoint.z + AreaSize };
        targetPoints = thinks::PoissonDiskSampling(2.f, MinArea, MaxArea, 30, static_cast<std::uint32_t>(std::time(nullptr)));
    }

    enum State
    {
        //NOTE the first two index the animations.
        Idle, Running,
        Inactive
    }state = Inactive;
    float stateTime = static_cast<float>(cro::Util::Random::value(6, 14));

    void operator() (cro::Entity e, float dt)
    {
        if (state == Inactive)
        {
            state = Idle;
            e.getComponent<cro::Skeleton>().play(Idle);

            glm::vec3 pos = { targetPoints[targetIndex][0], 0.f, -targetPoints[targetIndex][1] };
            pos.y = collisionMesh.getTerrain(pos).height;

            targetIndex = (targetIndex + 1) % targetPoints.size();
            e.getComponent<cro::Transform>().setPosition(pos);
        }

        else if (state == Idle)
        {
            stateTime -= dt;
            if (stateTime < 0)
            {
                stateTime += static_cast<float>(cro::Util::Random::value(8, 19));
                state = Running;
                e.getComponent<cro::Skeleton>().play(Running);

                currentTarget = { targetPoints[targetIndex][0], 0.f, -targetPoints[targetIndex][1] };
                currentTarget.y = collisionMesh.getTerrain(currentTarget).height;

                targetIndex = (targetIndex + 1) % targetPoints.size();
            }
        }

        else
        {
            static constexpr float MinDist = 0.25f * 0.25f;
            const auto dir = currentTarget - e.getComponent<cro::Transform>().getPosition();
            const auto currentDist = glm::length2(dir);
            if (currentDist > MinDist)
            {
                //9m/s =~ 20mph
                e.getComponent<cro::Transform>().move(glm::normalize(dir) * 4.f * dt);
                const auto fwd = e.getComponent<cro::Transform>().getForwardVector();
                const auto start = std::atan2(-fwd.z, fwd.x);
                const auto end = std::atan2(-dir.z, dir.x);

                if (std::abs(start - end) > 0.15f)
                {
                    e.getComponent<cro::Transform>().rotate(-cro::Transform::Y_AXIS, cro::Util::Maths::shortestRotation(start, end) * dt * 5.f);
                }
            }
            else
            {
                state = Idle;
                e.getComponent<cro::Skeleton>().play(Idle);
            }
        }
    }
};
