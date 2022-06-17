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

#include "SpectatorSystem.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Skeleton.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Spline.hpp>

SpectatorSystem::SpectatorSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(SpectatorSystem))
{
    requireComponent<cro::Transform>();
    requireComponent<cro::Skeleton>();
    requireComponent<Spectator>();
}

//public
void SpectatorSystem::process(float dt)
{
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& spectator = entity.getComponent<Spectator>();
        if (spectator.path)
        {
            switch (spectator.state)
            {
            default: break;
            case Spectator::State::Pause:
                spectator.stateTime += dt;
                if (spectator.stateTime > spectator.pauseTime)
                {
                    spectator.stateTime = 0.f;
                    spectator.state = Spectator::State::Walk;

                    //TODO set speed based on movement speed(?)
                    entity.getComponent<cro::Skeleton>().play(spectator.anims[Spectator::AnimID::Walk], 1.f, 0.5f);
                }
                break;

            case Spectator::State::Walk:
            {
                //TODO make this target based (like the rotation)
                //then set target based on if we're turning or following the path.

                float speed = spectator.path->getLength() / spectator.walkSpeed;
                spectator.progress = std::min(1.f, std::max(0.f, spectator.progress + ((dt * spectator.direction) / speed)));

                auto next = std::min(1.f, std::max(0.f, spectator.progress + (0.01f * spectator.direction)));
                auto prev = std::min(1.f, std::max(0.f, spectator.progress - (0.01f * spectator.direction)));
                auto dir = spectator.path->getInterpolatedPoint(next) - spectator.path->getInterpolatedPoint(prev);
                spectator.targetRotation = std::atan2(dir.z * spectator.direction, dir.x) - ((cro::Util::Const::PI / 2.f) * spectator.direction);
                spectator.rotation += cro::Util::Maths::shortestRotation(spectator.rotation, spectator.targetRotation) * (dt * 3.f);

                entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, spectator.rotation * -spectator.direction);

                auto offset = glm::normalize(spectator.path->getPointAt(spectator.path->getSize() - 1) - spectator.path->getPointAt(0)) * spectator.direction;
                auto temp = offset.z;
                offset.z = -offset.x;
                offset.x = temp;
                entity.getComponent<cro::Transform>().setPosition(spectator.path->getInterpolatedPoint(spectator.progress) - (offset * 0.85f));


                if (spectator.progress == 0
                    || spectator.progress == 1)
                {
                    spectator.direction *= -1.f;
                    spectator.state = Spectator::State::Pause;

                    entity.getComponent<cro::Skeleton>().play(spectator.anims[Spectator::AnimID::Idle], 1.f, 0.5f);
                }
            }
                break;
            }


            //TODO query collision and set correct height?
        }
    }
}

//private
void SpectatorSystem::onEntityAdded(cro::Entity e)
{
    e.getComponent<Spectator>().pauseTime = 3.f + cro::Util::Random::value(0.f, 1.f);
}