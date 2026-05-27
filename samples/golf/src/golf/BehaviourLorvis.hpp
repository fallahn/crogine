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
#include "server/ServerPacketData.hpp"

struct BehaviourLorvis final
{
    const CollisionMesh& collisionMesh;
    const ActivePlayer& targetPlayer;

    enum State
    {
        Chase, Shout
    }state = Chase;

    enum AnimID
    {
        Gesture, Idle, Run,
        Fist, Yell
    };

    explicit BehaviourLorvis(const CollisionMesh& mesh, const ActivePlayer& target)
        : collisionMesh (mesh),
        targetPlayer    (target)
    {
        
    }

    void operator() (cro::Entity entity, float dt)
    {
        static constexpr float MinDist = 1.5f * 1.5f;

        if (state == Chase)
        {
            const auto dir = targetPlayer.position - entity.getComponent<cro::Transform>().getPosition();
            const auto currentDist = glm::length2(dir);
            if (currentDist > MinDist)
            {
                static constexpr float Velocity = 4.f;
                const auto movement = glm::normalize(dir) * Velocity;

                entity.getComponent<cro::Transform>().move(movement * dt);
                auto pos = entity.getComponent<cro::Transform>().getPosition();
                pos.y = collisionMesh.getTerrain(pos).height;
                entity.getComponent<cro::Transform>().setPosition(pos);

                const auto fwd = entity.getComponent<cro::Transform>().getForwardVector();
                const auto start = std::atan2(-fwd.z, fwd.x) - (cro::Util::Const::PI / 2.f);
                const auto end = std::atan2(-dir.z, dir.x) + (cro::Util::Const::PI / 2.f);

                const float rot = cro::Util::Maths::shortestRotation(start, end);
                const float rotAmt = rot * dt * 5.f;
                if (std::abs(rotAmt) < std::abs(rot))
                {
                    entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, rotAmt);
                }

                if (entity.hasComponent<cro::AudioEmitter>())
                {
                    entity.getComponent<cro::AudioEmitter>().setVelocity(movement);
                }
            }
            else
            {
                state = Shout;
                entity.getComponent<cro::Skeleton>().play(Yell);

                if (entity.hasComponent<cro::AudioEmitter>())
                {
                    entity.getComponent<cro::AudioEmitter>().setVelocity(glm::vec3(0.f));
                }
            }
        }
        else
        {
            const auto dir = targetPlayer.position - entity.getComponent<cro::Transform>().getPosition();
            const auto currentDist = glm::length2(dir);
            if (currentDist > MinDist
                && targetPlayer.terrain != TerrainID::Green)
            {
                //player moved
                state = Chase;
                entity.getComponent<cro::Skeleton>().play(Run);
            }
            else
            {
                if (entity.getComponent<cro::Skeleton>().getState() == cro::Skeleton::Stopped)
                {
                    auto idx = entity.getComponent<cro::Skeleton>().getActiveAnimations().first;
                    if (idx > AnimID::Run)
                    {
                        idx -= AnimID::Fist;
                        idx = (idx + 1) % 2;
                        idx += AnimID::Fist;
                    }
                    else
                    {
                        idx = AnimID::Yell;
                    }

                    entity.getComponent<cro::Skeleton>().play(idx);
                }
            }
        }
    }
};