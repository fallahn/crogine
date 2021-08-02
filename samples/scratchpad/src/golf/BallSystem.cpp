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
#include "HoleData.hpp"
#include "GameConsts.hpp"
#include "server/ServerMessages.hpp"

#include <crogine/ecs/components/Transform.hpp>

#include <crogine/graphics/Image.hpp>

#include <crogine/util/Random.hpp>
#include <crogine/util/Easings.hpp>

#include <crogine/detail/glm/gtx/norm.hpp>

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

    static constexpr float MinBallDistance = HoleRadius * HoleRadius;
}

BallSystem::BallSystem(cro::MessageBus& mb, const cro::Image& mapData)
    : cro::System           (mb, typeid(BallSystem)),
    m_mapData               (mapData),
    m_windDirTime           (cro::seconds(0.f)),
    m_windStrengthTime      (cro::seconds(1.f)),
    m_windInterpTime        (1.f),
    m_currentWindInterpTime (0.f),
    m_windDirection         (-1.f, 0.f, 0.f),
    m_windDirSrc            (m_windDirection),
    m_windDirTarget         (1.f, 0.f, 0.f),
    m_windStrength          (0.f),
    m_windStrengthSrc       (m_windStrength),
    m_windStrengthTarget    (0.1f),
    m_holeData              (nullptr)
{
    requireComponent<cro::Transform>();
    requireComponent<Ball>();

    updateWind();
}

//public
void BallSystem::process(float dt)
{
    //interpolate current strength/direction
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
                doCollision(entity);
            }
        }
        break;
        case Ball::State::Putt:
            
            ball.delay -= dt;
            if (ball.delay < 0)
            {
                auto& tx = entity.getComponent<cro::Transform>();
                auto position = tx.getPosition();

                auto [terrain, normal] = getTerrain(tx.getPosition());

                //test distance to pin
                auto len2 = glm::length2(position - m_holeData->pin);
                if (len2 < MinBallDistance
                    || position.y > 0)
                {
                    //over hole or in the air

                    //this is some fudgy non-physics.
                    //if the ball falls low enough when
                    //over the hole we'll put it in.
                    ball.velocity += Gravity * dt;
                }
                else //we're on the green so roll
                {
                    //if the ball has registered some depth but is not over
                    //the hole reset the depth and slow it down as if it
                    //bumped the far edge
                    if (position.y < 0)
                    {
                        //these are all just a wild stab
                        //destined for some tweaking - basically puts the ball back along its vector
                        //towards the hole while maintaining gravity.
                        ball.velocity *= -1.f;
                        ball.velocity.y *= -1.f;
                    }

                    //TODO we could also test to see which side of the hole the ball
                    //currently is and add some 'side spin' to the velocity.



                    //add wind - adding less wind the morethe ball travels in the
                    //wind direction means we don't get blown forever
                    float windAmount = 1.f - glm::dot(m_windDirection, glm::normalize(ball.velocity));
                    ball.velocity += m_windDirection * m_windStrength * 0.04f * windAmount * dt;

                    //add slope from normal map
                    //glm::vec3 slope(normal.x, normal.y, 0.f); //TODO - calc this properly ;)
                    //float slopeAmount = 1.f - glm::dot(glm::normalize(slope), glm::normalize(ball.velocity));
                    //ball.velocity += slope * 0.04f * slopeAmount * dt;


                    //add friction
                    ball.velocity *= 0.985f;
                }

                //move by velocity
                tx.move(ball.velocity * dt);
                ball.terrain = terrain;

                //if we've slowed down or fallen more than the
                //ball's diameter (radius??) stop the ball
                if (glm::length2(ball.velocity) < 0.01f
                    || (position.y < -(Ball::Radius * 1.5f)))
                {
                    ball.velocity = glm::vec3(0.f);
                    
                    if (ball.terrain == TerrainID::Water
                        || ball.terrain == TerrainID::Scrub)
                    {
                        ball.state = Ball::State::Reset;
                    }
                    else
                    {
                        ball.state = Ball::State::Paused;
                    }

                    ball.delay = 2.f;
                }
            }
            break;
        case Ball::State::Reset:
        {
            ball.delay -= dt;
            if (ball.delay < 0)
            {
                //move towards hole util we find non-water
                auto& tx = entity.getComponent<cro::Transform>();

                std::uint8_t terrain = TerrainID::Water;
                auto ballPos = tx.getPosition();
                auto dir = glm::normalize(m_holeData->pin - ballPos);
                for (auto i = 0; i < 100; ++i) //max 100m
                {
                    ballPos += dir;
                    terrain = getTerrain(ballPos).first;

                    if (terrain != TerrainID::Water
                        && terrain != TerrainID::Scrub)
                    {
                        tx.setPosition(ballPos);
                        break;
                    }
                }

                //if for some reason we never got out the water, put the ball back at the start
                if (terrain == TerrainID::Water
                    || terrain == TerrainID::Scrub)
                {
                    terrain = TerrainID::Fairway;
                    tx.setPosition(m_holeData->tee);
                }

                //raise message to say player should be penalised
                auto* msg = postMessage<BallEvent>(sv::MessageID::BallMessage);
                msg->type = BallEvent::Foul;
                msg->terrain = terrain;
                msg->position = tx.getPosition();

                //set ball to reset / correct terrain
                ball.delay = 0.5f;
                ball.terrain = terrain;
                ball.state = Ball::State::Paused;
            }
        }
            break;
        case Ball::State::Paused:
        {
            ball.delay -= dt;
            if (ball.delay < 0)
            {
                //send message to report status
                auto* msg = postMessage<BallEvent>(sv::MessageID::BallMessage);
                msg->terrain = ball.terrain;
                msg->position = entity.getComponent<cro::Transform>().getPosition();

                if (msg->position.y < 0)
                {
                    //we're in the hole
                    msg->type = BallEvent::Holed;
                }
                else
                {
                    msg->type = BallEvent::TurnEnded;
                }


                ball.state = Ball::State::Idle;
                updateWind(); //is a bit less random but at least stops the wind
                //changing direction mid-stroke which is just annoying.
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

void BallSystem::setHoleData(const HoleData& holeData)
{
    m_holeData = &holeData;
}

//private
void BallSystem::doCollision(cro::Entity entity)
{
    //check height
    auto& tx = entity.getComponent<cro::Transform>();
    if (auto pos = tx.getPosition(); pos.y < 0)
    {
        pos.y = 0.f;
        tx.setPosition(pos);

        auto& ball = entity.getComponent<Ball>();

        auto [terrain, normal] = getTerrain(pos);

        //apply dampening based on terrain (or splash)
        switch (terrain)
        {
        default: break;
        case TerrainID::Bunker:
        case TerrainID::Water:
        case TerrainID::Scrub:
            ball.velocity = glm::vec3(0.f);
            break;
        case TerrainID::Fairway:
            ball.velocity *= 0.35f;
            ball.velocity = glm::reflect(ball.velocity, normal);
            break;
        case TerrainID::Green:
            ball.velocity *= 0.35f;

            //if low bounce start rolling
            if (ball.velocity.y > -0.001f)
            {
                ball.velocity.y = 0.f;
                ball.velocity *= 0.5f;
                ball.state = Ball::State::Putt;
            }
            else //bounce
            {
                ball.velocity = glm::reflect(ball.velocity, normal);
            }
            break;
        case TerrainID::Rough:
            ball.velocity *= 0.15f;
            ball.velocity = glm::reflect(ball.velocity, normal);
            break;
        }

        //stop the ball if velocity low enough
        if (glm::length2(ball.velocity) < 0.01f)
        {
            if (terrain == TerrainID::Water
                || terrain == TerrainID::Scrub)
            {
                ball.state = Ball::State::Reset;
            }
            else
            {
                ball.state = Ball::State::Paused;
            }
            ball.delay = 2.f;
            ball.terrain = terrain;
            ball.velocity = glm::vec3(0.f);
        }
    }
}

void BallSystem::updateWind()
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

        m_windStrengthTarget = static_cast<float>(cro::Util::Random::value(1, 10)) / 10.f;

        resetInterp();
    }
}

std::pair<std::uint8_t, glm::vec3> BallSystem::getTerrain(glm::vec3 pos) const
{
    auto size = glm::vec2(m_mapData.getSize());
    std::uint32_t x = static_cast<std::uint32_t>(std::max(0.f, std::min(size.x, std::floor(pos.x))));
    std::uint32_t y = static_cast<std::uint32_t>(std::max(0.f, std::min(size.y, std::floor(-pos.z))));

    CRO_ASSERT(m_mapData.getFormat() == cro::ImageFormat::RGBA, "expected RGBA format");

    auto index = ((y * static_cast<std::uint32_t>(size.x)) + x);

    //R is terrain * 10
    std::uint8_t terrain = m_mapData.getPixelData()[index * 4] / 10;
    terrain = std::min(static_cast<std::uint8_t>(TerrainID::Scrub), terrain);

    return std::make_pair(terrain, m_holeData->normalMap[index]);
}