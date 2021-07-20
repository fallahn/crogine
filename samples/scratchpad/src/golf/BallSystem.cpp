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
#include "Terrain.hpp"
#include "server/ServerMessages.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Easings.hpp>

namespace
{
    constexpr glm::vec3 Gravity(0.f, -9.8f, 0.f);

    glm::vec3 interpolate(glm::vec3 a, glm::vec3 b, float t)
    {
        auto diff = b - a;
        return a + (diff * cro::Util::Easing::easeOutElastic(t));
    }

    float interpolate(float a, float b, float t)
    {
        auto diff = b - a;
        return a + (diff * t);
    }
}

BallSystem::BallSystem(cro::MessageBus& mb)
    : cro::System           (mb, typeid(BallSystem)),
    m_windDirTime           (cro::seconds(0.f)),
    m_windStrengthTime      (cro::seconds(1.f)),
    m_windInterpTime        (1.f),
    m_currentWindInterpTime (0.f),
    m_windDirection         (-1.f, 0.f, 0.f),
    m_windDirSrc            (m_windDirection),
    m_windDirTarget         (1.f, 0.f, 0.f),
    m_windStrength          (0.f),
    m_windStrengthSrc       (m_windStrength),
    m_windStrengthTarget    (3.f)
{
    requireComponent<cro::Transform>();
    requireComponent<Ball>();
}

//public
void BallSystem::process(float dt)
{
    auto resetInterp =
        [&]()
    {
        m_windDirSrc = m_windDirection;
        m_windStrengthSrc = m_windStrength;

        m_currentWindInterpTime = 0.f;
        m_windInterpTime = static_cast<float>(cro::Util::Random::value(50, 75)) / 10.f;
    };

    //update wind direction
    if (m_windDirClock.elapsed() > m_windDirTime)
    {
        m_windDirClock.restart();
        m_windDirTime = cro::seconds(static_cast<float>(cro::Util::Random::value(100, 220)) / 10.f);

        //create new direction
        m_windDirTarget.x = static_cast<float>(cro::Util::Random::value(-10, 10)) / 10.f;
        m_windDirTarget.z = static_cast<float>(cro::Util::Random::value(-10, 10)) / 10.f;

        m_windDirTarget = glm::normalize(m_windDirTarget);

        resetInterp();
    }
    

    //update wind strength
    if (m_windStrengthClock.elapsed() > m_windStrengthTime)
    {
        m_windStrengthClock.restart();
        m_windStrengthTime = cro::seconds(static_cast<float>(cro::Util::Random::value(80, 180)) / 10.f);

        m_windStrengthTarget = static_cast<float>(cro::Util::Random::value(1, 40)) / 10.f;

        resetInterp();
    }

    //interpolate current strength/direction
    //TODO - only do this when balls are idle?
    m_currentWindInterpTime = std::min(m_windInterpTime, m_currentWindInterpTime + dt);
    float interp = m_currentWindInterpTime / m_windInterpTime;
    m_windDirection = interpolate(m_windDirSrc, m_windDirTarget, interp);
    m_windStrength = interpolate(m_windStrengthSrc, m_windStrengthTarget, interp);

    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& ball = entity.getComponent<Ball>();
        switch (ball.state)
        {
        default: break;
        case Ball::State::Flight:
        {
            ball.delay -= dt;
            if (ball.delay < 0)
            {
                //add gravity
                ball.velocity += Gravity * dt;

                //add wind
                ball.velocity += m_windDirection * m_windStrength * dt;

                //add air friction?

                //move by velocity
                auto& tx = entity.getComponent<cro::Transform>();
                tx.move(ball.velocity * dt);

                //test collision
                if (auto y = entity.getComponent<cro::Transform>().getPosition().y; y < 0)
                {
                    //ball.state = Ball::State::Idle;
                    tx.move({ 0.f, -y, 0.f });
                    /*ball.velocity = glm::reflect(ball.velocity, glm::vec3(0.f, 1.f, 0.f));
                    ball.velocity *= 0.5f;*/
                    ball.state = Ball::State::Paused;
                    ball.delay = 2.f;
                }
            }
        }
        break;
        case Ball::State::Putt:
            //if current surface is green, test distance to pin
            break;
        case Ball::State::Paused:
        {
            ball.delay -= dt;
            if (ball.delay < 0)
            {
                //send message to report status
                auto* msg = postMessage<BallEvent>(sv::MessageID::BallMessage);
                msg->type = BallEvent::Landed;
                msg->terrain = TerrainID::Fairway;
                msg->position = entity.getComponent<cro::Transform>().getPosition();

                ball.state = Ball::State::Idle;
            }
        }
            break;
        }
    }
}

glm::vec3 BallSystem::getWindDirection() const
{
    //the Y value is unused so we pack the strength in here
    //(it's only for vis on the client anyhoo)
    return { m_windDirection.x, m_windStrength, m_windDirection.z };
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