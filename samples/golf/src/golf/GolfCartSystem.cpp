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

#include "GolfCartSystem.hpp"
#include "GameConsts.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>

#include <crogine/util/Random.hpp>
#include <crogine/util/Maths.hpp>

namespace
{
    static constexpr std::array Path01 = { glm::vec3(-70.f, 0.01f, 5.4f),glm::vec3(60.f, 0.01f, 5.4f),  glm::vec3(70.f, 0.01f, 5.4f),  glm::vec3(77.f, 0.01f, 5.4f) };
    static constexpr std::array Path03 = { glm::vec3(76.f, 0.01f, 9.5f), glm::vec3(8.f, 0.01f, 9.5f), glm::vec3(-30.f, 0.01f, 9.5f),glm::vec3(-70.f, 0.01f, 9.5f) };
    static constexpr std::array Path02 = { glm::vec3(-7.f, 0.01f, 70.f),glm::vec3(-7.f, 0.01f, 11.f), glm::vec3(-9.f, 0.01f, 9.5f), glm::vec3(-70.f, 0.01f, 9.5f) };

    static constexpr std::array Paths =
    {
        Path01, Path02, Path03
    };
}

GolfCartSystem::GolfCartSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(GolfCartSystem)),
    m_nextFreePath(0),
    m_maxPitch(1.f)
{
    requireComponent<GolfCart>();
    requireComponent<cro::Transform>();
}

//public
void GolfCartSystem::process(float dt)
{
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& cart = entity.getComponent<GolfCart>();
        if (cart.state == GolfCart::Wait)
        {
            cart.currentTime = std::max(0.f, cart.currentTime - dt);
            if (cart.currentTime == 0)
            {
                cart.state = GolfCart::Driving;
            }
        }
        else if (cart.state == GolfCart::Driving)
        {
            auto target = Paths[cart.pathIndex][cart.pointIndex];
            auto& tx = entity.getComponent<cro::Transform>();

            auto currPos = tx.getPosition();
            auto dir = target - currPos;

            auto length = glm::length(dir);
            if (length < 0.5f)
            {
                //fetch next point
                cart.pointIndex++;

                if (cart.pointIndex == Paths[cart.pathIndex].size())
                {
                    //we're at the end of a path
                    cart.pointIndex = 0;
                    cart.pathIndex = m_nextFreePath;
                    cart.state = GolfCart::Wait;
                    cart.currentTime = cro::Util::Random::value(3.f, 5.f);

                    m_nextFreePath = (m_nextFreePath + 1) % Paths.size();

                    tx.setPosition(Paths[cart.pathIndex][cart.pointIndex]);

                    auto newDir = Paths[cart.pathIndex][1] - Paths[cart.pathIndex][0];
                    cart.rotation = std::atan2(-newDir.z, newDir.x);
                    tx.setRotation(cro::Transform::Y_AXIS, cart.rotation);
                }
            }
            else
            {
                static constexpr float BrakingDistance = 4.f;
                static constexpr float MinAcceleration = 0.6f;

                //move
                static constexpr float Speed = 5.f;
                float accel = std::min(1.f, length / BrakingDistance); //slow down / speed up within this length
                accel = MinAcceleration + (accel * (1.f - MinAcceleration));

                if (cart.pointIndex > 0)
                {
                    //check prev point too...
                    auto prevPoint = Paths[cart.pathIndex][cart.pointIndex - 1];
                    auto length2 = glm::length(prevPoint - currPos);
                    auto accel2 = std::min(1.f, length2 / BrakingDistance);
                    accel2 = MinAcceleration + (accel2 * (1.f - MinAcceleration));

                    //we want to see how much these overlap by then
                    //interpolate the value based on how far along
                    //the overlap we are so there are no 'pops' in speed
                    float overlap = (BrakingDistance * 2.f) - glm::length(prevPoint - target);
                    float travel = length - (BrakingDistance - overlap);
                    travel = std::max(0.f, std::min(1.f, travel / overlap));

                    accel = interpolate(accel, accel2, travel);
                }                


                //if we're in braking distance and there's a 
                //further point then look at that to turn to
                if (length < BrakingDistance / 2.f)
                {
                    if (cart.pointIndex < (Paths[cart.pathIndex].size() - 1))
                    {
                        dir = Paths[cart.pathIndex][cart.pointIndex + 1] - currPos;
                    }
                }

                static constexpr float RotationSpeed = 2.f;

                auto rotation = cro::Util::Maths::shortestRotation(cart.rotation, std::atan2(-dir.z, dir.x));
                cart.rotation += rotation * RotationSpeed * accel * dt;
                tx.setRotation(cro::Transform::Y_AXIS, cart.rotation);

                tx.move(tx.getRightVector() * Speed * accel * dt);

                //this just affects the doppler amount as the velocity is between 0 - 1 (assuming no scaling on the transform)
                static constexpr float Exaggeration = 50.f;
                entity.getComponent<cro::AudioEmitter>().setVelocity(tx.getForwardVector() * accel * Exaggeration);
                entity.getComponent<cro::AudioEmitter>().setPitch(accel * m_maxPitch);
            }
        }
    }
}

//private
void GolfCartSystem::onEntityAdded(cro::Entity entity)
{
    auto pathIndex = m_nextFreePath;
    m_nextFreePath = (m_nextFreePath + 1) % Paths.size();
    entity.getComponent<GolfCart>().pathIndex = pathIndex;
    entity.getComponent<GolfCart>().currentTime = cro::Util::Random::value(1.f, 2.5f);
    entity.getComponent<cro::Transform>().setPosition(Paths[pathIndex][0]);

    auto dir = Paths[pathIndex][1] - Paths[pathIndex][0];
    entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, std::atan2(-dir.z, dir.x));

    //technically this is set to whichever entity is added last
    //but they *should* all be the same...
    m_maxPitch = entity.getComponent<cro::AudioEmitter>().getPitch();
}