/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

#include "BallSystem.hpp"
#include "Terrain.hpp"
#include "HoleData.hpp"
#include "GameConsts.hpp"
#include "MessageIDs.hpp"
#include "../ErrorCheck.hpp"
#include "server/ServerMessages.hpp"

#include <crogine/ecs/components/Transform.hpp>

#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/MeshBuilder.hpp>

#include <crogine/util/Random.hpp>
#include <crogine/util/Easings.hpp>

#include <crogine/detail/ModelBinary.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

namespace
{
    glm::vec3 btToGlm(btVector3 v)
    {
        return { v.getX(), v.getY(), v.getZ() };
    }

    static constexpr glm::vec3 Gravity(0.f, -9.8f, 0.f);

    static constexpr float MinBallDistance = HoleRadius * HoleRadius;
    static constexpr float FallRadius = Ball::Radius * 0.25f;
    static constexpr float MinFallDistance = (HoleRadius - FallRadius) * (HoleRadius - FallRadius);
    static constexpr float AttractRadius = HoleRadius * 1.25f;
    static constexpr float MinAttachRadius = AttractRadius * AttractRadius;
    static constexpr float Margin = 1.02f;
    static constexpr float BallHoleDistance = (HoleRadius * Margin) * (HoleRadius * Margin);
    static constexpr float BallTurnDelay = 2.5f; //how long to delay before stating turn ended
    static constexpr float AngularVelocity = 46.5f; //rad/s at 1m/s vel. Used for rolling animation.

    static constexpr float MinVelocitySqr = 0.005f;//0.04f

    static constexpr std::array<float, TerrainID::Count> Friction =
    {
        0.1f, 0.9f,
        0.985f, 0.1f,
        0.001f, 0.001f,
        0.98f,
        0.f
    };

    constexpr std::array GimmeRadii =
    {
        0.f, 0.65f * 0.65f, 1.f
    };

    struct CollisionGroup final
    {
        enum
        {
            Ball = 1,
            Terrain = 2
        };
    };

    struct SlopeData final
    {
        glm::vec3 direction = glm::vec3(0.f);
        float strength = 0.f;
    };

    SlopeData getSlope(glm::vec3 normal)
    {
        auto slopeStrength = 1.f - glm::dot(normal, cro::Transform::Y_AXIS);
        //normal is not always perfectly normalised - so hack around this with some leighway
        if (slopeStrength > 0.001f)
        {
            auto tangent = glm::cross(normal, glm::normalize(glm::vec3(normal.x, 0.f, normal.z)));
            auto slope = glm::normalize(glm::cross(tangent, normal));
            return { slope, slopeStrength };
        }
        return SlopeData();
    }
}

const std::array<std::string, 5u> Ball::StateStrings = { "Idle", "Flight", "Putt", "Paused", "Reset" };

BallSystem::BallSystem(cro::MessageBus& mb, bool drawDebug)
    : cro::System           (mb, typeid(BallSystem)),
    m_windDirTime           (cro::seconds(0.f)),
    m_windStrengthTime      (cro::seconds(1.f)),
    m_windDirection         (-1.f, 0.f, 0.f),
    m_windDirSrc            (m_windDirection),
    m_windDirTarget         (1.f, 0.f, 0.f),
    m_windStrength          (0.f),
    m_windStrengthSrc       (m_windStrength),
    m_windStrengthTarget    (0.1f),
    m_windInterpTime        (1.f),
    m_currentWindInterpTime (0.f),
    m_holeData              (nullptr),
    m_puttFromTee           (false),
    m_gimmeRadius           (0)
{
    requireComponent<cro::Transform>();
    requireComponent<Ball>();

    m_windDirTarget.x = static_cast<float>(cro::Util::Random::value(-10, 10));
    m_windDirTarget.z = static_cast<float>(cro::Util::Random::value(-10, 10));
    m_windDirTarget.x += 0.5f; //deliberately not a whole number because we might end up with cases where we add 1 to -1 and end up back at zero...
    m_windDirTarget.z -= 0.5f;

    m_windDirTarget = glm::normalize(m_windDirTarget);
    CRO_ASSERT(!std::isnan(m_windDirTarget.x), "");
    CRO_ASSERT(!std::isnan(m_windDirTarget.z), "");

    m_windStrengthTarget = static_cast<float>(cro::Util::Random::value(1, 10)) / 10.f;

    updateWind();

    initCollisionWorld(drawDebug);
}

BallSystem::~BallSystem()
{
    clearCollisionObjects();
}

//public
void BallSystem::process(float dt)
{
    auto& entities = getEntities();
    
    //interpolate current strength/direction
    m_currentWindInterpTime = std::min(m_windInterpTime, m_currentWindInterpTime + dt);
    float interp = std::min(1.f, std::max(0.f, m_currentWindInterpTime / m_windInterpTime));
    m_windDirection = interpolate(m_windDirSrc, m_windDirTarget, interp);
    m_windStrength = interpolate(m_windStrengthSrc, m_windStrengthTarget, interp);

    CRO_ASSERT(!std::isnan(m_windDirection.x), "");
    CRO_ASSERT(!std::isnan(m_windDirTarget.x), "");

    for (auto entity : entities)
    {
        auto& ball = entity.getComponent<Ball>();

        //*sigh* isnan bug
        CRO_ASSERT(!std::isnan(ball.velocity.x), "");

        switch (ball.state)
        {
        default: break;
        case Ball::State::Idle:
            ball.hadAir = false;

            //used this to test smoothness of client side interpolation
            /*if (ball.terrain == TerrainID::Green)
            {
                static float accum = 0.f;
                accum += dt;

                auto holePos = m_holeData->pin;
                holePos.x += std::sin(accum * 2.f);
                holePos.z += std::cos(accum * 2.f);
                entity.getComponent<cro::Transform>().setPosition(holePos);
            }*/

            break;
        case Ball::State::Flight:
        {
            ball.hadAir = false;
            ball.delay -= dt;
            if (ball.delay < 0)
            {
                //add gravity
                ball.velocity += Gravity * dt;

                //add wind
                ball.velocity += m_windDirection * m_windStrength * dt;

                //TODO add air friction?

                //move by velocity
                auto& tx = entity.getComponent<cro::Transform>();
                tx.move(ball.velocity * dt);

                //test collision
                doCollision(entity);

                CRO_ASSERT(!std::isnan(tx.getPosition().x), "");
                CRO_ASSERT(!std::isnan(ball.velocity.x), "");
            }
        }
        break;
        case Ball::State::Putt:
            
            ball.delay -= dt;
            if (ball.delay < 0)
            {
                auto& tx = entity.getComponent<cro::Transform>();
                auto position = tx.getPosition();

                //attempts to trap an obscure NaN bug
                CRO_ASSERT(!std::isnan(position.x), "");

                auto terrainContact = getTerrain(position);

                //test distance to pin
                auto pinDir = m_holeData->pin - position;
                auto len2 = glm::length2(glm::vec2(pinDir.x, pinDir.z));

                if (len2 < AttractRadius)
                {
                    auto attraction = pinDir;
                    attraction.y = 0.f;
                    ball.velocity += attraction * dt;
                }

                if (len2 < MinBallDistance)
                {
                    //over hole or in the air

                    //apply more gravity/push the closer we are to the pin
                    float forceAffect = 1.f - smoothstep(MinFallDistance, MinBallDistance, len2);


                    //gravity
                    static constexpr float MinFallVelocity = 2.1f;
                    float gravityAmount = 1.f - std::min(1.f, glm::length2(ball.velocity) / MinFallVelocity);

                    //this is some fudgy non-physics.
                    //if the ball falls low enough when
                    //over the hole we'll put it in.
                    ball.velocity += (gravityAmount * Gravity * forceAffect) * dt;


                    //this draws the ball to the pin a little bit to make sure the ball
                    //falls entirely within the radius
                    pinDir.y = 0.f;
                    ball.velocity += pinDir * forceAffect;// dt;

                    ball.hadAir = true;
                    CRO_ASSERT(!std::isnan(ball.velocity.x), "");
                }
                else //we're on the green so roll
                {
                    //if the ball has registered some air but is not over
                    //the hole reset the depth and slow it down as if it
                    //bumped the far edge
                    if (ball.hadAir)
                    {
                        //these are all just a wild stab
                        //destined for some tweaking - basically puts the ball back along its vector
                        //towards the hole while maintaining gravity. As if it bounced off the inside of the hole
                        if (terrainContact.penetration > (Ball::Radius * 0.5f))
                        {
                            ball.velocity *= -1.f;
                            ball.velocity.y *= -1.f;
                        }
                        else
                        {
                            //lets the ball continue travelling, ie overshoot
                            auto bounceVel = glm::length(ball.velocity) * 0.4f;// 0.2f;
                            ball.velocity *= 0.65f;// 0.15f;
                            ball.velocity.y = bounceVel;

                            //position.y += terrainContact.penetration;
                            position.y = terrainContact.intersection.y;
                            tx.setPosition(position);
                        }

                        CRO_ASSERT(!std::isnan(position.x), "");
                        CRO_ASSERT(!std::isnan(ball.velocity.x), "");
                    }
                    else
                    {
                        if(terrainContact.penetration < 0) //above ground
                        {
                            //we had a bump so add gravity
                            ball.velocity += Gravity * dt;
                        }
                        else if (terrainContact.penetration > 0)
                        {
                            //we've sunk into the ground so correct
                            ball.velocity.y = 0.f;

                            //position.y += terrainContact.penetration;
                            position.y = terrainContact.intersection.y;
                            tx.setPosition(position);
                        }
                        CRO_ASSERT(!std::isnan(position.x), "");
                        CRO_ASSERT(!std::isnan(ball.velocity.x), "");
                    }
                    ball.hadAir = false;

                   

                    //add wind - adding less wind the more the ball travels in the
                    //wind direction means we don't get blown forever
                    auto velLength = glm::length(ball.velocity);
                    float windAmount = 1.f - glm::dot(m_windDirection, ball.velocity / velLength);
                    ball.velocity += m_windDirection * m_windStrength * 0.06f * windAmount * dt;

                    if (!m_puttFromTee)
                    {
                        //slope strength (arcade version)
                        glm::vec3 slope = glm::vec3(terrainContact.normal.x, 0.f, terrainContact.normal.z) * 0.95f * smoothstep(0.35f, 4.5f, velLength);
                        ball.velocity += slope;

                        //add friction
                        ball.velocity *= Friction[ball.terrain];
                    }
                    else
                    {
                        //add wind again - this is intentional
                        ball.velocity += m_windDirection * m_windStrength * 0.06f * windAmount * dt;
                        
                        //only use this when we're mini putting
                        //it's more accurate (ball runs back down a slope it went up)
                        //but is really boring to play on the full size courses
                        auto [slope, slopeStrength] = getSlope(terrainContact.normal);

                        //add friction
                        ball.velocity *= Friction[ball.terrain] + (slopeStrength * 0.05f);

                        //move by slope from surface normal
                        ball.velocity += slope * slopeStrength;
                        CRO_ASSERT(!std::isnan(ball.velocity.x), "");
                    }
                }
                

                //move by velocity
                tx.move(ball.velocity * dt);

                auto newPos = tx.getPosition();
                terrainContact = getTerrain(newPos);
                ball.terrain = terrainContact.terrain;

                //spin based on velocity
                auto vel2 = glm::length2(ball.velocity);
                static constexpr float MaxVel = 2.f; //some arbitrary number. Actual max is ~20.f so smaller is faster spin
                tx.rotate(cro::Transform::Y_AXIS, cro::Util::Const::TAU * (vel2 / MaxVel) * ball.spin * dt);


                //if we've slowed down or fallen more than the
                //ball's diameter (radius??) stop the ball
                if (vel2 < MinVelocitySqr
                    || (terrainContact.penetration > (Ball::Radius * 2.5f)))
                {
                    ball.velocity = glm::vec3(0.f);
                    
                    if (ball.terrain == TerrainID::Water
                        || ball.terrain == TerrainID::Scrub)
                    {
                        ball.state = Ball::State::Reset;
                    }
                    else if (ball.terrain == TerrainID::Stone)
                    {
                        ball.state = Ball::State::Reset;
                        ball.terrain = TerrainID::Scrub;
                    }
                    else
                    {
                        ball.state = Ball::State::Paused;
                    }

                    ball.delay = BallTurnDelay;

                    //make sure to update the position
                    position = tx.getPosition();
                    len2 = glm::length2(glm::vec2(position.x, position.z) - glm::vec2(m_holeData->pin.x, m_holeData->pin.z));

                    auto* msg = postMessage<GolfBallEvent>(sv::MessageID::GolfMessage);
                    msg->type = GolfBallEvent::Landed;
                    msg->terrain = ((terrainContact.penetration > Ball::Radius)||(len2 < MinBallDistance)) ? TerrainID::Hole : ball.terrain;

                    if (msg->terrain == TerrainID::Hole)
                    {
                        position.x = m_holeData->pin.x;
                        position.z = m_holeData->pin.z;
                        tx.setPosition(position);
                    }
                    else if(len2 < GimmeRadii[m_gimmeRadius])
                    {
                        auto* msg2 = postMessage<GolfBallEvent>(sv::MessageID::GolfMessage);
                        msg2->type = GolfBallEvent::Gimme;
                    }

                    msg->position = position;

                    CRO_ASSERT(!std::isnan(position.x), "");
                    CRO_ASSERT(!std::isnan(ball.velocity.x), "");
                }
                //makes the ball go bat shit crazy for some reason.
                /*else if (terrainContact.penetration < 0.1f)
                {
                    ball.state = Ball::State::Flight;
                    ball.delay = 0.f;
                }*/
            }
            break;
        case Ball::State::Reset:
        {
            ball.delay -= dt;

            auto& tx = entity.getComponent<cro::Transform>();
            auto ballPos = tx.getPosition();
            
            //hold ball under water until reset
            if (ball.terrain == TerrainID::Water
                || ball.terrain == TerrainID::Scrub) //collision actually resets this to scrub even with water
            {
                ballPos.y = -0.5f;
                tx.setPosition(ballPos);
            }

            if (ball.delay < 0)
            {
                //move towards player until we find non-water
                std::uint8_t terrain = TerrainID::Water;

                //make sure ball height is level with target
                //else moving it may cause the collision test
                //to miss if the terrain is much higher than
                //the water level.

                glm::vec3 dir = ball.startPoint - ballPos;
                ballPos.y = ball.startPoint.y;

                auto length = glm::length(dir);
                dir /= length;
                std::int32_t maxDist = static_cast<std::int32_t>(length /*- 10.f*/);

                //if we're on a putting course take smaller steps for better accuracy
                if (m_puttFromTee)
                {
                    dir /= 4.f;
                    maxDist *= 4;
                }

                for (auto i = 0; i < maxDist; ++i)
                {
                    ballPos += dir;
                    auto res = getTerrain(ballPos);
                    terrain = res.terrain;

                    if (terrain != TerrainID::Water
                        && terrain != TerrainID::Scrub
                        && terrain != TerrainID::Stone)
                    {
                        //move the ball a bit closer so we're not balancing on the edge
                        //but only if we're not on the green else we might get placed in the hole :)
                        if (res.terrain != TerrainID::Green)
                        {
                            ballPos += dir * 1.5f;
                            res = getTerrain(ballPos);
                        }

                        ballPos = res.intersection;
                        tx.setPosition(ballPos);
                        break;
                    }
                }

                //if for some reason we never got out the water, put the ball back at the start
                if (terrain == TerrainID::Water
                    || terrain == TerrainID::Scrub)
                {
                    //this is important else we'll end up trying to drive
                    //down a putting course :facepalm:
                    terrain = m_puttFromTee ? TerrainID::Green : TerrainID::Fairway;
                    tx.setPosition(m_holeData->tee);
                }

                //raise message to say player should be penalised
                auto* msg = postMessage<GolfBallEvent>(sv::MessageID::GolfMessage);
                msg->type = GolfBallEvent::Foul;
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
                auto position = entity.getComponent<cro::Transform>().getPosition();
                auto len2 = glm::length2(glm::vec2(position.x, position.z) - glm::vec2(m_holeData->pin.x, m_holeData->pin.z));

                //send message to report status
                auto* msg = postMessage<GolfBallEvent>(sv::MessageID::GolfMessage);
                msg->terrain = ball.terrain;
                msg->position = position;

                if (ball.terrain == TerrainID::Hole
                        || (len2 <= (BallHoleDistance + GimmeRadii[m_gimmeRadius])))
                {
                    //we're in the hole
                    msg->type = GolfBallEvent::Holed;
                    //LogI << "Ball Holed" << std::endl;
                }
                else
                {
                    msg->type = GolfBallEvent::TurnEnded;
                    //LogI << "Turn Ended" << std::endl;
                }
                //LogI << "Distance: " << len2 << ", terrain: " << TerrainStrings[ball.terrain] << std::endl;

                ball.lastStrokeDistance = glm::length(ball.startPoint - position);
                msg->distance = ball.lastStrokeDistance;
                ball.state = Ball::State::Idle;


                //changed this so we force update wind change when hole changes.
                //updateWind(); //is a bit less random but at least stops the wind
                //changing direction mid-stroke which is just annoying.
            }
            //else if (m_puttFromTee &&
            //    ball.terrain == TerrainID::Green)
            //{
            //    //add slope and start moving if vel > min vel
            //    auto terrainContact = getTerrain(entity.getComponent<cro::Transform>().getPosition());
            //    auto [slope, slopeStrength] = getSlope(terrainContact.normal);

            //    ball.velocity += slope * slopeStrength;
            //    ball.velocity *= Friction[TerrainID::Green] + 0.001f;

            //    if (glm::length2(ball.velocity / 1.1f) > MinVelocitySqr)
            //    {
            //        ball.delay = 0.f;
            //        ball.state = Ball::State::Putt;
            //    }
            //}
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

void BallSystem::forceWindChange()
{
    m_windDirTime = cro::seconds(0.f);
    m_windStrengthTime = cro::seconds(0.f);
    updateWind();
}

bool BallSystem::setHoleData(const HoleData& holeData, bool rebuildMesh)
{
    m_holeData = &holeData;

    return rebuildMesh ? updateCollisionMesh(holeData.modelPath) : true;
}

void BallSystem::setGimmeRadius(std::uint8_t rad)
{
    m_gimmeRadius = std::min(std::uint8_t(2), rad);
}

BallSystem::TerrainResult BallSystem::getTerrain(glm::vec3 pos) const
{
    TerrainResult retVal;

    //casts a vertical ray 10m above/below the ball
    static const btVector3 RayLength = { 0.f, -20.f, 0.f };
    btVector3 rayStart = { pos.x, pos.y, pos.z };
    rayStart -= (RayLength / 2.f);
    auto rayEnd = rayStart + RayLength;

    RayResultCallback res(rayStart, rayEnd);
    //btCollisionWorld::ClosestRayResultCallback res(rayStart, rayEnd);
    m_collisionWorld->rayTest(rayStart, rayEnd, res);
    if (res.hasHit())
    {
        retVal.terrain = static_cast<std::uint8_t>(res.m_collisionObject->getUserIndex());
        retVal.normal = { res.m_hitNormalWorld.x(), res.m_hitNormalWorld.y(), res.m_hitNormalWorld.z() };
        retVal.intersection = { res.m_hitPointWorld.x(), res.m_hitPointWorld.y(), res.m_hitPointWorld.z() };
        retVal.penetration = res.m_hitPointWorld.y() - pos.y;
    }

    return retVal;
}

#ifdef CRO_DEBUG_
void BallSystem::renderDebug(const glm::mat4& mat, glm::uvec2 targetSize)
{
    CRO_ASSERT(m_debugDraw, "");
    m_debugDraw->render(mat, targetSize);
}

void BallSystem::setDebugFlags(std::int32_t flags)
{
    CRO_ASSERT(m_debugDraw, "");
    m_debugDraw->setDebugMode(flags);
    m_collisionWorld->debugDrawWorld();
}
#endif

//private
void BallSystem::doCollision(cro::Entity entity)
{
    /*
    This function raises collision messages, although
    they are only effective when the system is used locally, such
    as in the driving range. Server instances ignore
    collision messages.
    */

    //check height
    auto& tx = entity.getComponent<cro::Transform>();
    auto pos = tx.getPosition();
    CRO_ASSERT(!std::isnan(pos.x), "");

    const auto resetBall = [&](Ball& ball, Ball::State state, std::uint8_t terrain)
    {
        ball.velocity = glm::vec3(0.f);
        ball.state = state;
        ball.delay = BallTurnDelay;
        ball.terrain = terrain;

        auto* msg = postMessage<GolfBallEvent>(sv::MessageID::GolfMessage);
        msg->type = GolfBallEvent::Landed;
        msg->terrain = ball.terrain;
        msg->position = tx.getPosition();
    };

    auto terrainResult = getTerrain(pos);

    if (terrainResult.penetration > 0)
    {
        //TODO reduce the velocity based on interpolation (see scratchpad)
        //TODO this should move back along ball velocity, not the normal.
        pos = terrainResult.intersection;
        tx.setPosition(pos);

        auto& ball = entity.getComponent<Ball>();
        CRO_ASSERT(!std::isnan(pos.x), "");
        CRO_ASSERT(!std::isnan(ball.velocity.x), "");

        //apply dampening based on terrain (or splash)
        switch (terrainResult.terrain)
        {
        default: break;
        case TerrainID::Water:
            pos.y = WaterLevel - (Ball::Radius * 2.f);
            tx.setPosition(pos);
            [[fallthrough]];
        case TerrainID::Scrub:
        case TerrainID::Bunker:
            ball.velocity *= Restitution[terrainResult.terrain];
            break;
        case TerrainID::Fairway:
        case TerrainID::Stone: //bouncy :)
        case TerrainID::Rough:
            ball.velocity *= Restitution[terrainResult.terrain];
            ball.velocity = glm::reflect(ball.velocity, terrainResult.normal);
            break;
        case TerrainID::Green:
            ball.velocity *= 0.26f;

            //if low bounce start rolling
            if (ball.velocity.y > -0.05f)
            {
                float momentum = 1.f - glm::dot(-cro::Transform::Y_AXIS, glm::normalize(ball.velocity));
                static constexpr float MaxMomentum = 20.f;
                momentum *= MaxMomentum;
                CRO_ASSERT(!std::isnan(ball.velocity.x), "");

                auto len = glm::length(ball.velocity);
                ball.velocity.y = 0.f;
                ball.velocity = glm::normalize(ball.velocity) * len * momentum; //fake physics to simulate momentum
                ball.state = Ball::State::Putt;
                ball.delay = 0.f;

                auto [slope, slopeStrength] = getSlope(terrainResult.normal);
                ball.velocity += slope * slopeStrength;

                CRO_ASSERT(!std::isnan(ball.velocity.x), "");

                return;
            }
            else //bounce
            {
                ball.velocity = glm::reflect(ball.velocity, terrainResult.normal);
                CRO_ASSERT(!std::isnan(ball.velocity.x), "");
            }
            break;
        }

        //stop the ball if velocity low enough
        auto len2 = glm::length2(ball.velocity);
        if (len2 < 0.01f)
        {
            if (terrainResult.terrain == TerrainID::Water
                || terrainResult.terrain == TerrainID::Scrub)
            {
                resetBall(ball, Ball::State::Reset, terrainResult.terrain);
            }
            else if (terrainResult.terrain == TerrainID::Stone)
            {
                resetBall(ball, Ball::State::Reset, TerrainID::Scrub);
            }
            else
            {
                resetBall(ball, Ball::State::Paused, terrainResult.terrain);
            }
        }

        //these are only used for sound on client side
        //usage - ie driving range, so don't raise them
        //if the velocity is too low.
        if (len2 > 0.05f
            || terrainResult.terrain == TerrainID::Scrub) //vel will be 0 in this case
        {
            auto* msg = postMessage<CollisionEvent>(MessageID::CollisionMessage);
            msg->terrain = terrainResult.terrain;
            msg->position = pos;
            msg->type = CollisionEvent::Begin;
        }
    }
    else if (pos.y < WaterLevel)
    {
        //must have missed all geometry and so are in scrub or water
        auto& ball = entity.getComponent<Ball>();
        resetBall(ball, Ball::State::Reset, TerrainID::Scrub);

        auto* msg = postMessage<CollisionEvent>(MessageID::CollisionMessage);
        msg->terrain = TerrainID::Water;
        msg->position = pos;
        msg->type = CollisionEvent::Begin;
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
        m_windDirTarget.x = static_cast<float>(cro::Util::Random::value(-10, 10));
        m_windDirTarget.z = static_cast<float>(cro::Util::Random::value(-10, 10));

        //on rare occasions both of the above might have a value of 0 - in which
        //case attempting to normalise below causes a NaN which cascades through
        //ball velocity and eventually ball position culminating in a cluster fk.
        m_windDirTarget.x += 0.5f;
        m_windDirTarget.z -= 0.5f;

        m_windDirTarget = glm::normalize(m_windDirTarget);
        CRO_ASSERT(!std::isnan(m_windDirTarget.x), "");
        CRO_ASSERT(!std::isnan(m_windDirTarget.z), "");

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

void BallSystem::initCollisionWorld(bool drawDebug)
{
    m_collisionCfg = std::make_unique<btDefaultCollisionConfiguration>();
    m_collisionDispatcher = std::make_unique<btCollisionDispatcher>(m_collisionCfg.get());

    m_broadphaseInterface = std::make_unique<btDbvtBroadphase>();
    m_collisionWorld = std::make_unique<btCollisionWorld>(m_collisionDispatcher.get(), m_broadphaseInterface.get(), m_collisionCfg.get());

#ifdef CRO_DEBUG_
    if (drawDebug)
    {
        m_debugDraw = std::make_unique<BulletDebug>();
        m_collisionWorld->setDebugDrawer(m_debugDraw.get());
    }
#endif
}

void BallSystem::clearCollisionObjects()
{
    for (auto& obj : m_groundObjects)
    {
        m_collisionWorld->removeCollisionObject(obj.get());
    }

    m_groundObjects.clear();
    m_groundShapes.clear();
    m_groundVertices.clear();

    m_vertexData.clear();
    m_indexData.clear();
}

bool BallSystem::updateCollisionMesh(const std::string& modelPath)
{
    clearCollisionObjects();

    cro::Mesh::Data meshData = cro::Detail::ModelBinary::read(modelPath, m_vertexData, m_indexData);

    if ((meshData.attributeFlags & cro::VertexProperty::Colour) == 0)
    {
        LogE << "No colour property found in collision mesh" << std::endl;
        return false;
    }



    std::int32_t colourOffset = 0;
    for (auto i = 0; i < cro::Mesh::Attribute::Colour; ++i)
    {
        colourOffset += meshData.attributes[i];
    }

    //we have to create a specific object for each sub mesh
    //to be able to tag it with a different terrain...
    for (auto i = 0u; i < m_indexData.size(); ++i)
    {
        btIndexedMesh groundMesh;
        groundMesh.m_vertexBase = reinterpret_cast<std::uint8_t*>(m_vertexData.data());
        groundMesh.m_numVertices = meshData.vertexCount;
        groundMesh.m_vertexStride = meshData.vertexSize;

        groundMesh.m_numTriangles = meshData.indexData[i].indexCount / 3;
        groundMesh.m_triangleIndexBase = reinterpret_cast<std::uint8_t*>(m_indexData[i].data());
        groundMesh.m_triangleIndexStride = 3 * sizeof(std::uint32_t);


        float terrain = std::min(1.f, std::max(0.f, m_vertexData[(m_indexData[i][0] * (meshData.vertexSize / sizeof(float))) + colourOffset])) * 255.f;
        terrain = std::floor(terrain / 10.f);

        if (terrain >= TerrainID::Hole)
        {
            terrain = TerrainID::Scrub;
        }

        m_groundVertices.emplace_back(std::make_unique<btTriangleIndexVertexArray>())->addIndexedMesh(groundMesh);
        m_groundShapes.emplace_back(std::make_unique<btBvhTriangleMeshShape>(m_groundVertices.back().get(), false));
        m_groundObjects.emplace_back(std::make_unique<btPairCachingGhostObject>())->setCollisionShape(m_groundShapes.back().get());
        m_groundObjects.back()->setUserIndex(static_cast<std::int32_t>(terrain)); // set the terrain type
        m_collisionWorld->addCollisionObject(m_groundObjects.back().get(), CollisionGroup::Terrain, CollisionGroup::Ball);
    }


    m_puttFromTee = getTerrain(m_holeData->tee).terrain == TerrainID::Green;


    return true;
}


//custom callback to return proper face normal (I wish we could cache these...)
RayResultCallback::RayResultCallback(const btVector3& rayFromWorld, const btVector3& rayToWorld)
    : ClosestRayResultCallback(rayFromWorld, rayToWorld)
{
}

btScalar RayResultCallback::addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace)
{
    //caller already does the filter on the m_closestHitFraction
    btAssert(rayResult.m_hitFraction <= m_closestHitFraction);

    m_closestHitFraction = rayResult.m_hitFraction;
    m_collisionObject = rayResult.m_collisionObject;
    if (rayResult.m_collisionObject->getCollisionFlags() == CollisionGroup::Ball)
    {
        m_hitNormalWorld = getFaceNormal(rayResult);
        m_hitPointWorld.setInterpolate3(m_rayFromWorld, m_rayToWorld, rayResult.m_hitFraction);
    }
    else
    {
        rayResult.m_collisionObject = nullptr; //remove the ball objects then hasHit() returns false
    }
    return rayResult.m_hitFraction;
}

btVector3 RayResultCallback::getFaceNormal(const btCollisionWorld::LocalRayResult& rayResult) const
{
    /*
    Respect to https://pybullet.org/Bullet/phpBB3/viewtopic.php?t=12826
    */

    const unsigned char* vertices = nullptr;
    int numVertices = 0;
    int vertexStride = 0;
    PHY_ScalarType verticesType;

    const unsigned char* indices = nullptr;
    int indicesStride = 0;
    int numFaces = 0;
    PHY_ScalarType indicesType;

    auto vertex = [&](int vertexIndex)
    {
        const auto* data = reinterpret_cast<const btScalar*>(vertices + vertexIndex * vertexStride);
        return btVector3(*data, *(data + 1), *(data + 2));
    };

    const auto* triangleShape = static_cast<const btBvhTriangleMeshShape*>(rayResult.m_collisionObject->getCollisionShape());
    const auto* triangleMesh = static_cast<const btTriangleIndexVertexArray*>(triangleShape->getMeshInterface());

    triangleMesh->getLockedReadOnlyVertexIndexBase(
        &vertices, numVertices, verticesType, vertexStride, &indices, indicesStride, numFaces, indicesType, rayResult.m_localShapeInfo->m_shapePart
    );

    const auto* index = reinterpret_cast<const int*>(indices + rayResult.m_localShapeInfo->m_triangleIndex * indicesStride);
    btVector3 va = vertex(*index);
    btVector3 vb = vertex(*(index + 1));
    btVector3 vc = vertex(*(index + 2));
    btVector3 normal = (vb - va).cross(vc - va).normalized();

    triangleMesh->unLockReadOnlyVertexBase(rayResult.m_localShapeInfo->m_shapePart);

    return normal;
}