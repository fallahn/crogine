/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
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

//#define NO_WIND


using namespace cl;

namespace
{
    static constexpr float MinWindStrength = 0.1f;

    static constexpr float CupDepth = (Ball::Radius * 2.f) * 2.1f;

    static constexpr float MinBallDistance = HoleRadius * HoleRadius;
    static constexpr float FallRadius = Ball::Radius * 0.08f;// 0.25f;
    static constexpr float MinFallDistance = (HoleRadius - FallRadius) * (HoleRadius - FallRadius);
    static constexpr float AttractRadius = HoleRadius * 1.35f;// 1.24f; //1.2f;
    static constexpr float MinAttractRadius = AttractRadius * AttractRadius;
    static constexpr float AttractStrength = 30.f;
    static constexpr float Margin = 1.02f;
    static constexpr float BallHoleDistance = (HoleRadius * Margin) * (HoleRadius * Margin);
    static constexpr float BallTurnDelay = 2.5f; //how long to delay before stating turn ended
    static constexpr float AngularVelocity = 46.5f; //rad/s at 1m/s vel. Used for rolling animation.

    static constexpr float MinVelocitySqr = 0.001f;// 0.005f;//0.04f
    static constexpr float BallRollTimeout = -10.f;
    static constexpr float BallTimeoutVelocity = 0.04f;
    static constexpr float MinSpinPower = 0.05f; //min velocity to stop doesn't kick in if there's more than this much top/back spin to apply
    static constexpr float MinRollSlope = 0.995f; //ball won't stop rolling if the ground is steeper that this (smaller == stickier)

    static constexpr float MinRollVelocity = -0.15f;// -0.25f; //MUST be negative
    static constexpr float MaxStoneSlope = 0.95f; //dot prod with vertical - ball is OOB if less than this

    static constexpr float MaxRestitutionIncrease = 0.3f;// 0.05f; //depending on the angle of the bounce up to this much is added to restitution multiplier

    float getRestitution(glm::vec3 vel, glm::vec3 norm)
    {
        //TODO this needs to be a lookup table for the punch values for each club
        //however I don't think we *actually know* the club server side. Perhaps
        //we could use the length of velocity, but there's probably too much overlap
        //between the different clubs.
        const float StartAngle = 0.35f;
        const float l = glm::length(vel);

        const float d = glm::dot(-(vel / l), norm);
        //LogI << "Vel: " << l << ", dot: " << d << std::endl;
        const float amt = std::clamp(0.1f + (0.9f * glm::smoothstep(0.001f, StartAngle, d)), 0.f, 1.f);
        //LogI << amt << std::endl;

        /*
        When punching we get ~0 - 0.3
        Flops and drives are almost exclusively 0
        */

        return std::min((1.f - amt)/* * MaxRestitutionIncrease*/, 1.f - Restitution[TerrainID::Stone]);
    }

    static constexpr std::array<float, TerrainID::Count> Friction =
    {
        0.1f, 0.96f,
        0.986f, 0.1f,
        0.001f, 0.001f,
        0.958f,
        0.f
    };

    constexpr std::array GimmeRadii =
    {
        0.f, 0.65f * 0.65f, 1.f
    };

    struct SlopeData final
    {
        glm::vec3 direction = glm::vec3(0.f);
        float strength = 0.f;
    };

    SlopeData getSlope(glm::vec3 normal)
    {
        auto slopeStrength = (1.f - glm::dot(normal, cro::Transform::Y_AXIS)) * 500.f;
        //normal is not always perfectly normalised - so hack around this with some leighway
        if (slopeStrength > 0.001f)
        {
            auto tangent = glm::cross(normal, glm::normalize(glm::vec3(normal.x, 0.f, normal.z)));
            auto slope = glm::normalize(glm::cross(tangent, normal));
            return { slope, slopeStrength / 500.f };
        }
        return SlopeData();
    }

    //used when predicting outcome of swing
    GolfBallEvent predictionEvent;

    //these are multipliers
    constexpr std::array SpinDecay =
    {
        glm::vec2(0.f),           //idle
        glm::vec2(0.997f),        //flight,
        glm::vec2(0.7f, 0.98f),   //roll
        glm::vec2(0.2f, 0.955f),  //putt
        glm::vec2(0.f),           //paused
        glm::vec2(0.f)            //reset
    };
    constexpr float SideSpinInfluence = 6.f;
    constexpr float TopSpinInfluence = 1.f;

    constexpr float BallPenetrationAvg = 0.054f; //if the ball collision is greater than this it's set to 'buried' else 'sitting up'
}

const std::array<std::string, 5u> Ball::StateStrings = { "Idle", "Flight", "Putt", "Paused", "Reset" };

BallSystem::BallSystem(cro::MessageBus& mb, bool drawDebug)
    : cro::System           (mb, typeid(BallSystem)),
    m_useRandomWind         (false),
    m_windDirTime           (cro::seconds(0.f)),
    m_windStrengthTime      (cro::seconds(1.f)),
    m_windRandomTime        (cro::seconds(30.f)),
    m_windDirection         (-1.f, 0.f, 0.f),
    m_windDirSrc            (m_windDirection),
    m_windDirTarget         (1.f, 0.f, 0.f),
    m_windStrength          (0.f),
    m_windStrengthSrc       (m_windStrength),
    m_windStrengthTarget    (0.1f),
    m_maxStrengthMultiplier (1.f),
    m_windInterpTime        (1.f),
    m_currentWindInterpTime (0.f),
    m_noiseIndex            (0),
    m_holeData              (nullptr),
    m_puttFromTee           (false),
    m_gimmeRadius           (0),
    m_activeGimme           (0),
    m_processFlags          (0)
{
    requireComponent<cro::Transform>();
    requireComponent<Ball>();

    //generate a white noise table for wind strength
    static constexpr float NoiseScale = 2.f / 0xffffffff;
    std::int32_t x1 = 0x67452301;
    std::int32_t x2 = 0xefcdab89;

    //arbitrary size
    static constexpr std::int32_t NoiseSampleCount = 240;
    for (auto i = 0; i < NoiseSampleCount; ++i)
    {
        x1 ^= x2;
        m_noiseBuffer.push_back(((x2 * NoiseScale) + 1.f) / 2.f);
        x2 += x1;
    }
    m_noiseIndex = cro::Util::Random::value(0, NoiseSampleCount - 1);
    m_windStrengthTarget = MinWindStrength + (m_noiseBuffer[cro::Util::Random::value(0, NoiseSampleCount - 1)] - MinWindStrength);
    m_windStrengthTarget *= m_maxStrengthMultiplier;

    m_windDirTarget.x = static_cast<float>(cro::Util::Random::value(-10, 10));
    m_windDirTarget.z = static_cast<float>(cro::Util::Random::value(-10, 10));
    m_windDirTarget.x += 0.5f; //deliberately not a whole number because we might end up with cases where we add 1 to -1 and end up back at zero...
    m_windDirTarget.z -= 0.5f;

    m_windDirTarget = glm::normalize(m_windDirTarget);
    CRO_ASSERT(!std::isnan(m_windDirTarget.x), "");
    CRO_ASSERT(!std::isnan(m_windDirTarget.z), "");


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
    const auto& entities = getEntities();
    
    //interpolate current strength/direction
    m_currentWindInterpTime = std::min(m_windInterpTime, m_currentWindInterpTime + dt);
    float interp = std::min(1.f, std::max(0.f, m_currentWindInterpTime / m_windInterpTime));
    m_windDirection = interpolate(m_windDirSrc, m_windDirTarget, interp);

#ifdef NO_WIND
    m_windStrength = 0.f;
#else
    m_windStrength = interpolate(m_windStrengthSrc, m_windStrengthTarget, interp);
#endif

    if (m_useRandomWind)
    {
        if (m_windRandomClock.elapsed() > m_windRandomTime)
        {
            forceWindChange();
            m_windRandomClock.restart();
            m_windRandomTime = cro::seconds(cro::Util::Random::value(30, 90));
        }
    }

    CRO_ASSERT(!std::isnan(m_windDirection.x), "");
    CRO_ASSERT(!std::isnan(m_windDirTarget.x), "");

    for (auto entity : entities)
    {
        processEntity(entity, dt);
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

bool BallSystem::setHoleData(HoleData& holeData, bool rebuildMesh)
{
    m_bullsEye.position = glm::vec3(0.f);
    m_bullsEye.diametre = 0.f;

    m_holeData = &holeData;

    //updating the collision mesh sets m_puttFromTee too
    auto result = rebuildMesh ? updateCollisionMesh(holeData.modelPath) : true;
    holeData.pin.y = getTerrain(holeData.pin).intersection.y;
    holeData.target.y = getTerrain(holeData.target).intersection.y;
    holeData.tee.y = getTerrain(holeData.tee).intersection.y;
    holeData.puttFromTee = m_puttFromTee;

    for (auto entity : getEntities())
    {
        entity.getComponent<cro::Transform>().setPosition(holeData.tee);
    }

    return result;
}

void BallSystem::setGimmeRadius(std::uint8_t rad)
{
    m_gimmeRadius = std::min(std::uint8_t(2), rad);
    m_activeGimme = m_gimmeRadius;
}

const BullsEye& BallSystem::spawnBullsEye()
{
    //TODO how do we decide on a radius?
    m_bullsEye.diametre = static_cast<float>(cro::Util::Random::value(MinBullDiametre, MaxBullDiametre));
    if (m_puttFromTee)
    {
        m_bullsEye.diametre *= 0.032f;
    }
        
    m_bullsEye.position = m_holeData->target;
    m_bullsEye.spawn = true;
    return m_bullsEye;
}

BallSystem::TerrainResult BallSystem::getTerrain(glm::vec3 pos, glm::vec3 forward, float rayLength) const
{
    CRO_ASSERT(glm::length2(forward) != 0, "");
    //TODO how do we assert forward is a normal vec without normalising?

    TerrainResult retVal;

    //casts a ray in front/behind the ball
    //static constexpr float RayLength = 20.f;
    const auto f = btVector3(forward.x, forward.y, forward.z) * rayLength;

    btVector3 rayStart = { pos.x, pos.y, pos.z };
    rayStart -= (f / 2.f);
    auto rayEnd = rayStart + f;

    RayResultCallback res(rayStart, rayEnd);

    m_collisionWorld->rayTest(rayStart, rayEnd, res);
    if (res.hasHit())
    {
        retVal.terrain = (res.m_collisionType >> 24);
        retVal.trigger = ((res.m_collisionType & 0x00ff0000) >> 16);
        retVal.normal = { res.m_hitNormalWorld.x(), res.m_hitNormalWorld.y(), res.m_hitNormalWorld.z() };
        retVal.intersection = { res.m_hitPointWorld.x(), res.m_hitPointWorld.y(), res.m_hitPointWorld.z() };
        retVal.penetration = res.m_hitPointWorld.y() - pos.y;
    }

    return retVal;
}

void BallSystem::runPrediction(cro::Entity entity, float accuracy)
{
    CRO_ASSERT(entity.hasComponent<cro::Transform>(), "");
    CRO_ASSERT(entity.hasComponent<Ball>(), "");

    m_processFlags = ProcessFlags::Predicting;

    fastProcess(entity, accuracy);

    m_processFlags = 0;
}

void BallSystem::fastForward(cro::Entity entity)
{
    CRO_ASSERT(entity.hasComponent<cro::Transform>(), "");
    CRO_ASSERT(entity.hasComponent<Ball>(), "");

    m_processFlags = ProcessFlags::FastForward;
    fastProcess(entity, 1.f/60.f);
    m_processFlags = 0;

    //still have to raise the final event...
    auto* msg = postMessage<GolfBallEvent>(sv::MessageID::GolfMessage);
    *msg = predictionEvent;
    msg->client = entity.getComponent<Ball>().client;
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
void BallSystem::fastProcess(cro::Entity entity, float dt)
{
    std::int32_t maxTries = 600;
    predictionEvent = {};
    do
    {
        processEntity(entity, dt);
    } while (predictionEvent.type == GolfBallEvent::None && --maxTries);
}

GolfBallEvent* BallSystem::postEvent() const
{
    if (m_processFlags != 0)
    {
        //TODO we might actually need to queue this if there
        //are more than one per prediction frame...
        predictionEvent = {};
        return &predictionEvent;
    }
    return postMessage<GolfBallEvent>(sv::MessageID::GolfMessage);
}

void BallSystem::processEntity(cro::Entity entity, float dt)
{
    auto& ball = entity.getComponent<Ball>();
    ball.spin.x *= SpinDecay[static_cast<std::int32_t>(ball.state)].x;
    ball.windEffect = 0.f;

    //*sigh* isnan bug
    CRO_ASSERT(!std::isnan(ball.velocity.x), "");

    switch (ball.state)
    {
    default: break;
    case Ball::State::Idle:
        ball.hadAir = false;
        break;
    case Ball::State::Flight:
    {
        ball.hadAir = false;
        ball.delay -= dt;
        if (ball.delay < 0)
        {
            auto& tx = entity.getComponent<cro::Transform>();

            //helps prevent tunnelling through cliffs/flag pole
            //TODO this is mostly wasted when we're high up, so we could make the iteration count dynamic
            static constexpr std::int32_t Iterations = 3;
            dt /= Iterations;

            for (auto f = 0; f < Iterations; ++f)
            {
                //add gravity
                ball.velocity += Gravity * dt;

                const auto t = getTerrain(tx.getPosition(), glm::vec3(0.f, -1.f, 0.f), 60.f);
                const auto height = std::clamp(-t.penetration, 0.f, 60.f);

                //add wind
                const auto multiplier = getWindMultiplier(/*tx.getPosition().y - ball.startPoint.y*/height, glm::length(m_holeData->pin - tx.getPosition())) * 1.36f;
                ball.velocity += m_windDirection * m_windStrength * multiplier * dt;
                ball.windEffect = m_windStrength * multiplier;

                //add spin
                ball.velocity += ball.initialSideVector * ball.spin.x * SideSpinInfluence * dt;


                //move by velocity
                tx.move(ball.velocity * dt);

                //rotate based on velocity
                auto vel2 = glm::length2(ball.velocity);
                static constexpr float MaxVel = 20.f; //some arbitrary number. Actual max is ~20.f so smaller is faster spin
                static constexpr float MaxRotation = 5.f;
                float r = cro::Util::Const::TAU * (vel2 / MaxVel) * ball.rotation;// *ball.spin.x;
                r = std::clamp(r, -MaxRotation, MaxRotation);


                tx.rotate(cro::Transform::Y_AXIS, r * dt);

                //test collision
                doCollision(entity);
                //doBallCollision(entity);

                if (ball.state != Ball::State::Flight)
                {
                    //TODO this might skip 1 Fth of dt here
                    //is this a problem when switching states?
                    //it ought to be added to the iteration
                    //time of whichever state we switched to...
                    break;
                }
            }
            CRO_ASSERT(!std::isnan(tx.getPosition().x), "");
            CRO_ASSERT(!std::isnan(ball.velocity.x), "");
        }
    }
    break;
    case Ball::State::Roll:
    {
        ball.delay -= dt;

        //doBallCollision(entity);

        auto& tx = entity.getComponent<cro::Transform>();
        auto position = tx.getPosition();
        auto terrainContact = getTerrain(position);

        ball.velocity += ball.spin.y * ball.initialForwardVector * SpinAddition[terrainContact.terrain] * TopSpinInfluence;
        ball.spin.y *= SpinDecay[static_cast<std::int32_t>(ball.state)].y;

        if (terrainContact.penetration < 0) //above ground
        {
            //we had a bump so add gravity
            ball.velocity += Gravity * dt;
        }
        else if (terrainContact.penetration > 0)
        {
            //we've sunk into the ground so correct
            ball.velocity.y = 0.f;

            position.y = terrainContact.intersection.y;
            tx.setPosition(position);
        }

        auto [slope, slopeStrength] = getSlope(terrainContact.normal);
        const float friction = std::min(1.f, Friction[ball.terrain] + (slopeStrength * 0.05f));

        //as a timeout device to stop the ball rolling forever reduce the slope strength
        const float timeoutStrength = std::clamp(((BallRollTimeout - ball.delay) / -3.f), 0.f, 1.f);

        ball.velocity += slope * slopeStrength * timeoutStrength;
        ball.velocity *= friction;

        //move by velocity
        tx.move(ball.velocity * dt);

        //and check we haven't sunk again
        auto newPos = tx.getPosition();
        terrainContact = getTerrain(newPos);
        ball.terrain = terrainContact.terrain;

        if (terrainContact.penetration > 0)
        {
            newPos.y = terrainContact.intersection.y;
            tx.setPosition(newPos);
        }
        doBullsEyeCollision(tx.getPosition(), ball.client);


        const auto resetBall = [&](Ball::State state, std::uint8_t terrain)
        {
            ball.velocity = glm::vec3(0.f);
            ball.state = state;
            ball.delay = BallTurnDelay;
            ball.terrain = terrain;

            auto* msg = postEvent();
            msg->type = GolfBallEvent::Landed;
            msg->terrain = ball.terrain;
            msg->position = newPos;
            msg->client = ball.client;
        };

        //check if we rolled onto the green
        //or one of the other terrains
        switch (terrainContact.terrain)
        {
        default: break;
        case TerrainID::Rough:
        case TerrainID::Bunker:
            resetBall(Ball::State::Paused, terrainContact.terrain);
            return; //we want to skip the velocity check, below
        case TerrainID::Scrub:
        case TerrainID::Water:
            resetBall(Ball::State::Reset, terrainContact.terrain);
            return;
        case TerrainID::Green:
            ball.state = Ball::State::Putt;
            ball.delay = 0.f;
            return;
        }

        //finally check to see if we're slow enough to stop
        constexpr float TimeOut = (BallRollTimeout / 2.f);
        auto len2 = glm::length2(ball.velocity);
        if ((len2 < MinVelocitySqr 
            && std::abs(ball.spin.y) < MinSpinPower
            && glm::dot(cro::Transform::Y_AXIS, terrainContact.normal) > MinRollSlope)

            || ((ball.delay < TimeOut) && (len2 < BallTimeoutVelocity))
            || (ball.delay < (TimeOut * 2.f)))
        {
            switch (terrainContact.terrain)
            {
            default:
                resetBall(Ball::State::Paused, terrainContact.terrain);
                break;
                //clause above should make sure we never reach these cases
            /*case TerrainID::Water:
            case TerrainID::Scrub:
                resetBall(Ball::State::Reset, terrainContact.terrain);
                break;*/
            case TerrainID::Stone:
                if (glm::dot(terrainContact.normal, cro::Transform::Y_AXIS) > MaxStoneSlope)
                {
                    resetBall(Ball::State::Paused, terrainContact.terrain);
                }
                else
                {
                    resetBall(Ball::State::Reset, TerrainID::Scrub);
                }
                break;
            }
        }
    }
        break;
    case Ball::State::Putt:

        ball.delay -= dt;
        if (ball.delay < 0)
        {
            
            //doBallCollision(entity);

            auto& tx = entity.getComponent<cro::Transform>();
            auto position = tx.getPosition();

            //attempts to trap NaN bug caused by invalid wind values
            CRO_ASSERT(!std::isnan(position.x), "");

            auto terrainContact = getTerrain(position);

            ball.velocity += ball.spin.y * ball.initialForwardVector * SpinAddition[terrainContact.terrain] * TopSpinInfluence;
            ball.spin.y *= SpinDecay[static_cast<std::int32_t>(ball.state)].y;

            //test distance to pin
            auto pinDir = m_holeData->pin - position;
            auto len2 = glm::length2(glm::vec2(pinDir.x, pinDir.z));

            if (len2 < MinAttractRadius)
            {
                auto attraction = pinDir; //* 0.5f
                attraction.y = 0.f;

                const float attractMultiplier = 0.05f + std::min(glm::length2(ball.velocity), 0.95f);
                attraction *= (AttractStrength * attractMultiplier);
                //LogI << glm::length(attraction) << std::endl;
                ball.velocity += attraction * dt;
            }

            if (len2 < MinBallDistance)
            {
                //over hole or in the air

                //apply more gravity/push the closer we are to the pin
                const float forceAffect = 1.f - smoothstep(MinFallDistance, MinBallDistance, len2);


                //gravity
                static constexpr float MinFallVelocity = 4.f;// 2.f;
                float gravityAmount = 1.f - std::min(1.f, glm::length2(ball.velocity) / MinFallVelocity);

                //this is some fudgy non-physics.
                //if the ball falls low enough when
                //over the hole we'll put it in.
                ball.velocity += (gravityAmount * Gravity * forceAffect) * dt;
                ball.velocity *= glm::vec3(0.995f, 1.f, 0.995f);

                //this draws the ball to the pin a little bit to make sure the ball
                //falls entirely within the radius
                pinDir.y = -0.05f;
                ball.velocity += pinDir * (AttractStrength * 0.15f) * forceAffect;// dt;

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
                    if (terrainContact.penetration > (Ball::Radius * 0.25f))
                    {
                        ball.velocity *= -1.f;
                        ball.velocity.y *= -1.f;
                    }
                    else
                    {
                        //lets the ball continue travelling, ie overshoot
                        auto bounceVel = glm::length(ball.velocity) * 0.5f;// 0.2f;
                        ball.velocity *= 0.3f;// 0.45f;// 0.65f;// 0.15f;
                        ball.velocity.y = bounceVel;

                        position.y = terrainContact.intersection.y;
                        tx.setPosition(position);
                    }

                    CRO_ASSERT(!std::isnan(position.x), "");
                    CRO_ASSERT(!std::isnan(ball.velocity.x), "");
                }
                else
                {
                    if (terrainContact.penetration < 0) //above ground
                    {
                        //we had a bump so add gravity
                        ball.velocity += Gravity * dt;
                    }
                    else if (terrainContact.penetration > 0)
                    {
                        //we've sunk into the ground so correct
                        ball.velocity.y = 0.f;

                        position.y = terrainContact.intersection.y;
                        tx.setPosition(position);
                    }
                    CRO_ASSERT(!std::isnan(position.x), "");
                    CRO_ASSERT(!std::isnan(ball.velocity.x), "");
                }
                ball.hadAir = false;



                //add wind - adding less wind the more the ball travels in the
                //wind direction means we don't get blown forever
                const auto velLength = glm::length(ball.velocity);
                const float windAmount = (1.f - glm::dot(m_windDirection, ball.velocity / velLength)) * 0.1f;
                ball.velocity += (m_windDirection * m_windStrength * 0.07f * windAmount * dt);


                auto [slope, slopeStrength] = getSlope(terrainContact.normal);
                const float friction = std::min(1.f, Friction[ball.terrain] + (slopeStrength * 0.05f));

                //as a timeout device to stop the ball rolling forever reduce the slope strength
                const float timeoutStrength = std::clamp(((BallRollTimeout - ball.delay) / -3.f), 0.f, 1.f);

                //and reduce the slope effect near the hole because it's just painful
                const float holeStrength = 0.4f + (0.6f * std::clamp(len2, 0.f, 1.f));

                //move by slope from surface normal
                ball.velocity += slope * slopeStrength * timeoutStrength * holeStrength;
                ball.velocity *= friction;

                CRO_ASSERT(!std::isnan(ball.velocity.x), "");
            }


            //move by velocity
            tx.move(ball.velocity * dt);


            //check for wall collision
            /*if (auto l2 = glm::length2(ball.velocity); l2 != 0
                && glm::dot(ball.velocity, cro::Transform::Y_AXIS) > 0)
            {
                //the problem with this is that the ray *shouldn't* be cast from behind the ball in this case.
                auto wallResult = getTerrain(tx.getPosition(), ball.velocity / std::sqrt(l2) * (Ball::Radius * 4.f));
                if (wallResult.penetration > 0)
                {
                    tx.move(wallResult.normal * wallResult.penetration);
                    ball.velocity = glm::reflect(ball.velocity, wallResult.normal);
                    LogI << "Bounced off wall" << std::endl;
                }
            }*/



            auto newPos = tx.getPosition();
            terrainContact = getTerrain(newPos);
            ball.terrain = terrainContact.terrain;

            //one final correction to stop jitter
            pinDir = m_holeData->pin - newPos;
            len2 = glm::length2(glm::vec2(pinDir.x, pinDir.z));
            if (len2 > MinBallDistance
                && terrainContact.penetration > 0
                && terrainContact.penetration < Ball::Radius * 2.4f)
            {
                newPos.y = terrainContact.intersection.y;
                tx.setPosition(newPos);
            }

            //if we rolled onto the fairway switch state
            if (terrainContact.terrain == TerrainID::Fairway)
            {
                ball.state = Ball::State::Roll;
                ball.terrain = TerrainID::Fairway;
                //ball.delay = 0.f;
                return;
            }
            //if we went OOB in a putting course we
            //want to quit immediately, not wait for the velocity to stop
            else if (ball.terrain == TerrainID::Water
                || ball.terrain == TerrainID::Scrub)
            {
                ball.state = Ball::State::Reset;
                ball.delay = BallTurnDelay;
                ball.velocity = glm::vec3(0.f);

                auto* msg = postEvent();
                msg->type = GolfBallEvent::Landed;
                msg->terrain = ball.terrain;
                msg->client = ball.client;
                return;
            }

            //rotate based on velocity
            auto vel2 = glm::length2(ball.velocity);
            static constexpr float MaxVel = 2.f; //some arbitrary number. Actual max is ~20.f so smaller is faster spin
            tx.rotate(cro::Transform::Y_AXIS, cro::Util::Const::TAU * (vel2 / MaxVel) * ball.rotation * dt);

            //check for target
            if (vel2 != 0)
            {
                doBullsEyeCollision(tx.getPosition(), ball.client);
            }

            //if we've slowed down or fallen more than the
            //ball's diameter (radius??) stop the ball
            if ((vel2 < MinVelocitySqr 
                && std::abs(ball.spin.y) < MinSpinPower //this might be true when there's still spin to be applied
                && glm::dot(cro::Transform::Y_AXIS, terrainContact.normal) > MinRollSlope) //and we don't want to stop on a slope                
                
                || (terrainContact.penetration > CupDepth) 
                || ((ball.delay < BallRollTimeout * 1.5f) && (vel2 < BallTimeoutVelocity))
                || (ball.delay < (BallRollTimeout * 2.f)))
            {
                ball.velocity = glm::vec3(0.f);

                if (ball.terrain == TerrainID::Water //TODO this should never be true because of the above check
                    || ball.terrain == TerrainID::Scrub)
                {
                    ball.state = Ball::State::Reset;
                }
                /*else if (ball.terrain == TerrainID::Stone)
                {
                    ball.state = Ball::State::Reset;
                    ball.terrain = TerrainID::Scrub;
                }*/
                else
                {
                    ball.state = Ball::State::Paused;
                }

                ball.delay = BallTurnDelay;

                //make sure to update the position
                position = tx.getPosition();
                len2 = glm::length2(glm::vec2(position.x, position.z) - glm::vec2(m_holeData->pin.x, m_holeData->pin.z));

                auto* msg = postEvent();
                msg->type = GolfBallEvent::Landed;
                msg->terrain = ((terrainContact.penetration > Ball::Radius) || (len2 < MinBallDistance)) ? TerrainID::Hole : ball.terrain;
                msg->client = ball.client;

                if (msg->terrain == TerrainID::Hole)
                {
                    position.x = m_holeData->pin.x;
                    position.z = m_holeData->pin.z;
                    tx.setPosition(position);

                    ball.terrain = TerrainID::Hole;
                }
                else if (len2 < GimmeRadii[m_gimmeRadius]
                    && ball.terrain == TerrainID::Green) //this might be OOB on a putting course
                {
                //    moved to post-pause delay (below)
                //    auto* msg2 = postEvent();
                //    msg2->type = GolfBallEvent::Gimme;

                //    position.x = m_holeData->pin.x;
                //    position.y = m_holeData->pin.y - (Ball::Radius * 2.5f);
                //    position.z = m_holeData->pin.z;
                //    tx.setPosition(position);

                //    ball.terrain = TerrainID::Hole; //let the ball reset know to raise a holed message
                    ball.checkGimme = true;
                }

                msg->position = position;

                CRO_ASSERT(!std::isnan(position.x), "");
                CRO_ASSERT(!std::isnan(ball.velocity.x), "");
            }
            //makes the ball go bat shit crazy for some reason,
            //when I expect it to actually get some air
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
            ball.spin = { 0.f,0.f };
            
            std::uint8_t terrain = TerrainID::Water;
            if (m_puttFromTee)
            {
                //just place at the nearest tee or target

                terrain = TerrainID::Green;
                if (glm::length2(tx.getPosition() - m_holeData->tee) <
                    glm::length2(tx.getPosition() - m_holeData->target))
                {
                    tx.setPosition(m_holeData->tee);
                }
                else
                {
                    tx.setPosition(m_holeData->target);

                    //this might be multi-target mode so we don't want to sit right on top of the target
                    //that we also want to aim at...

                    auto d = glm::normalize(m_holeData->pin - m_holeData->target) * 0.5f;
                    tx.move(d);

                }
                auto pos = tx.getPosition();
                pos.y = getTerrain(pos).intersection.y;
                tx.setPosition(pos);
            }
            //else if (ball.resetCount == 1)
            //{
            //    //this is probably a CPU player melting down
            //    //so move to the nearest target
            //    auto pos = tx.getPosition();

            //    //if target is closer than tee check if target
            //    if (glm::length2(pos - m_holeData->target) < glm::length2(pos - m_holeData->tee))
            //    {
            //        //or sub-target is closer (default sub-target is miles away)
            //        if (glm::length2(pos - m_holeData->subtarget) < glm::length2(pos - m_holeData->target))
            //        {
            //            tx.setPosition(m_holeData->subtarget);
            //        }
            //        else
            //        {
            //            //we also have to move away in case we're in multi-target mode
            //            tx.setPosition(m_holeData->target);

            //            //TODO we probably only want to do this in multi-target mode
            //            //but unfortunately we don't know what the game mode is here...
            //            constexpr float Radius = static_cast<float>(MaxBullDiametre / 2) + 0.5f;
            //            glm::vec3 testDir = glm::normalize(m_holeData->tee - m_holeData->target) * Radius;
            //            auto testTerrain = getTerrain(m_holeData->target + testDir);
            //            if (testTerrain.terrain == TerrainID::Scrub
            //                || testTerrain.terrain == TerrainID::Water)
            //            {
            //                //move the other way
            //                testDir = glm::normalize(m_holeData->pin - m_holeData->target) * Radius;

            //                //hmm this is unlikely, but if we're still in the water
            //                //we probably want to fall back to the tee?
            //            }
            //            tx.move(testDir);
            //        }
            //    }
            //    //else move to tee
            //    else
            //    {
            //        tx.setPosition(m_holeData->tee);
            //    }

            //    pos = tx.getPosition();
            //    const auto terrainInf = getTerrain(pos);
            //    terrain = terrainInf.terrain;
            //    pos.y = terrainInf.intersection.y;
            //    tx.setPosition(pos);

            //    ball.resetCount = 0;
            //}
            else
            {

                //move towards player until we find non-water

                //make sure ball height is level with target
                //else moving it may cause the collision test
                //to miss if the terrain is much higher than
                //the water level.

                //ACTUALLY the terrain may rise up between the two points
                //so we need to set this at least as high as the
                //tallest terrain point (can we use the mesh AABB?)

                //static constexpr float SearchHeight = 15.f;

                const auto start = ball.startPoint;
                const auto ballLandingPos = ballPos; //used if we get reset to where we started, below

                //start.y = SearchHeight;
                ballPos.y = start.y;// SearchHeight;// ball.startPoint.y;
                glm::vec3 dir = start - ballPos;

                const auto length = glm::length(dir);
                dir /= length;
                std::int32_t maxDist = static_cast<std::int32_t>(length /*- 10.f*/);

                for (auto i = 0; i < maxDist; ++i)
                {
                    ballPos += dir;
                    auto res = getTerrain(ballPos, -cro::Transform::Y_AXIS, 60.f); //+/- 30.f above below ball pos
                    terrain = res.terrain;

                    const auto slope = dot(res.normal, glm::vec3(0.f, 1.f, 0.f));

                    if ((terrain == TerrainID::Rough
                        || terrain == TerrainID::Fairway
                        || terrain == TerrainID::Green)
                        && slope > 0.996f)
                    {
                        //move the ball a bit closer so we're not balancing on the edge
                        //but only if we're not on the green else we might get placed in the hole :)
                        if (res.terrain != TerrainID::Green)
                        {
                            ballPos += dir * 1.5f;
                            res = getTerrain(ballPos, -cro::Transform::Y_AXIS, 60.f);
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
                    terrain = TerrainID::Fairway;
                    tx.setPosition(m_holeData->tee);
                }

                //only count this if we reset to the same (or near to) the position we started
                else if (glm::length2(tx.getPosition() - ball.startPoint) < 64.f)
                {
                    //this is probably a CPU player melting down
                    //so move to the nearest target


                    //if target is closer than tee check if target
                    if (glm::length2(ballLandingPos - m_holeData->target) < glm::length2(ballLandingPos - m_holeData->tee))
                    {
                        //or sub-target is closer (default sub-target is miles away)
                        if (glm::length2(ballLandingPos - m_holeData->subtarget) < glm::length2(ballLandingPos - m_holeData->target))
                        {
                            tx.setPosition(m_holeData->subtarget);
                        }
                        else
                        {
                            //we also have to move away in case we're in multi-target mode
                            tx.setPosition(m_holeData->target);

                            //TODO we probably only want to do this in multi-target mode
                            //but unfortunately we don't know what the game mode is here...
                            constexpr float Radius = static_cast<float>(MaxBullDiametre / 2) + 0.5f;
                            glm::vec3 testDir = glm::normalize(m_holeData->tee - m_holeData->target) * Radius;
                            auto testTerrain = getTerrain(m_holeData->target + testDir);
                            if (testTerrain.terrain == TerrainID::Scrub
                                || testTerrain.terrain == TerrainID::Water)
                            {
                                //move the other way
                                testDir = glm::normalize(m_holeData->pin - m_holeData->target) * Radius;

                                //hmm this is unlikely, but if we're still in the water
                                //we probably want to fall back to the tee?
                            }
                            tx.move(testDir);
                        }
                    }
                    //else move to tee
                    else
                    {
                        tx.setPosition(m_holeData->tee);
                    }

                    auto pos = tx.getPosition();
                    const auto terrainInf = getTerrain(pos);
                    terrain = terrainInf.terrain;
                    pos.y = terrainInf.intersection.y;
                    tx.setPosition(pos);

                    //TODO we could do one final check to see if we're in water and replace at tee anyway
                }
            }


            //raise message to say player should be penalised
            auto* msg = postEvent();
            msg->type = GolfBallEvent::Foul;
            msg->terrain = terrain;
            msg->position = tx.getPosition();
            msg->client = ball.client;

            //set ball to reset / correct terrain
            ball.delay = 0.5f;
            ball.terrain = terrain;
            ball.state = Ball::State::Paused;
        }
    }
    break;
    case Ball::State::Paused:
    {
        //do the ball collision here to separate any balls which were overlapping when they came to rest
        //we do this outside the time out, else the server won't send the updated position until the next turn
        doBallCollision(entity);

        ball.delay -= dt;

        if (ball.checkGimme && ball.delay < (BallTurnDelay / 2.f))
        {
            auto position = entity.getComponent<cro::Transform>().getPosition();

            //check for gimme first
            const auto pinDir = m_holeData->pin - position;
            const auto len2 = glm::length2(glm::vec2(pinDir.x, pinDir.z));

            if (len2 < GimmeRadii[m_gimmeRadius]
                && ball.terrain == TerrainID::Green) //this might be OOB on a putting course
            {
                auto* msg2 = postEvent();
                msg2->type = GolfBallEvent::Gimme;
                msg2->client = ball.client;

                position.x = m_holeData->pin.x;
                position.y = m_holeData->pin.y - CupDepth;
                position.z = m_holeData->pin.z;
                entity.getComponent<cro::Transform>().setPosition(position);

                ball.terrain = TerrainID::Hole; //let the ball reset know to raise a holed message
                ball.checkGimme = false;

                ball.delay += (BallTurnDelay / 2.f);
            }
        }

        if (ball.delay < 0)
        {
            auto position = entity.getComponent<cro::Transform>().getPosition();

            ball.spin = { 0.f, 0.f };
            ball.initialForwardVector = { 0.f, 0.f, 0.f };
            ball.initialSideVector = { 0.f, 0.f, 0.f };


            //send message to report status
            auto* msg = postEvent();
            msg->terrain = ball.terrain;
            msg->client = ball.client;

            if (ball.terrain == TerrainID::Hole)
            {
                //we're in the hole
                msg->type = GolfBallEvent::Holed;
                msg->position = m_holeData->pin;
                //LogI << "Ball Holed" << std::endl;
            }
            else
            {
                msg->position = position;
                msg->type = GolfBallEvent::TurnEnded;
                //LogI << "Turn Ended" << std::endl;
            }
            //LogI << "Distance: " << len2 << ", terrain: " << TerrainStrings[ball.terrain] << std::endl;

            ball.lastStrokeDistance = glm::length(ball.startPoint - position);
            msg->distance = ball.lastStrokeDistance;
            ball.state = Ball::State::Idle;

            //changed this so we force update wind change when hole changes.
            if (m_processFlags != ProcessFlags::Predicting)
            {
                m_windStrengthTarget = std::min(cro::Util::Random::value(0.97f, 1.025f) * m_windStrengthTarget, m_maxStrengthMultiplier);
            }
        }
    }
    break;
    }
}

void BallSystem::doCollision(cro::Entity entity)
{
    //check height
    auto& tx = entity.getComponent<cro::Transform>();
    auto pos = tx.getPosition();
    CRO_ASSERT(!std::isnan(pos.x), "");


    //fudge in some psuedo flag collision - this assumes 
    //that the collision is only done when ball is in flight
    auto holePos = m_holeData->pin;
    static constexpr float FlagRadius = 0.01f;
    static constexpr float CollisionRadius = FlagRadius + Ball::Radius;
    if (auto ballHeight = pos.y - holePos.y; ballHeight < 1.9f) //flag is 2m tall
    {
        const glm::vec2 holeCollision = { holePos.x, -holePos.z };
        const glm::vec2 ballCollision = { pos.x, -pos.z };
        auto dir = ballCollision - holeCollision;
        if (auto l2 = glm::length2(dir); l2 < (CollisionRadius * CollisionRadius))
        {
            const auto len = std::sqrt(l2);
            const auto overlap = (CollisionRadius - len) + Ball::Radius;

            dir /= len;

            glm::vec3 worldDir(dir.x, 0.f, -dir.y);
            tx.move(worldDir * overlap);

            //check if the collision is on the right or left
            //of the velocity vector and impart more spin the
            //greater the velocity and the steeper the angle
            auto& ball = entity.getComponent<Ball>();
            const glm::vec3 rightVec(ball.velocity.z, ball.velocity.y, -ball.velocity.x); //yeah, yeah...
            const float spinOffset = glm::dot(glm::normalize(rightVec), -worldDir);
            ball.spin.x += spinOffset * std::clamp(glm::length2(ball.velocity) / 2500.f, 0.f, 1.f) * 10.f;
            ball.spin.y += std::pow(cro::Util::Random::value(-1.f, 1.f), 5.f);

            ball.velocity = glm::reflect(ball.velocity, worldDir);
            ball.velocity *= (0.5f + static_cast<float>(cro::Util::Random::value(0, 1)) / 10.f);

            //reduce the velocity more nearer the top as the flag is bendier (??)
            ball.velocity *= (0.5f + (0.2f * (1.f - (ballHeight / 1.9f))));

            ball.lastTerrain = TriggerID::FlagStick;

            auto* msg = postMessage<CollisionEvent>(MessageID::CollisionMessage);
            msg->terrain = CollisionEvent::FlagPole;
            msg->position = pos;
            msg->type = CollisionEvent::Begin;
            msg->client = ball.client;
        }
    }



    const auto resetBall = [&](Ball& ball, Ball::State state, std::uint8_t terrain)
    {
        ball.velocity = glm::vec3(0.f);
        ball.state = state;
        ball.delay = BallTurnDelay;
        ball.terrain = terrain;

        auto* msg = postEvent();
        msg->type = GolfBallEvent::Landed;
        msg->terrain = ball.terrain;
        msg->position = tx.getPosition();
        msg->client = ball.client;
    };
    const auto startRoll = [](Ball::State state, Ball& ball)
    {
        auto len = glm::length(ball.velocity);
        ball.velocity.y = 0.f;
        ball.velocity = glm::normalize(ball.velocity) * len * 3.f; //fake physics to simulate momentum
        ball.state = state;
        ball.delay = 0.f;
    };

    auto terrainResult = getTerrain(pos);

    if (terrainResult.penetration > 0)
    {
        //TODO reduce the velocity based on interpolation (see scratchpad)
        //TODO this should move back along ball velocity, not the normal.
        pos = terrainResult.intersection;
        tx.setPosition(pos);

        auto& ball = entity.getComponent<Ball>();
        ball.lie = terrainResult.penetration > BallPenetrationAvg ? 0 : 1;
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

            if (m_processFlags == 0)
            {
                auto* msg2 = postMessage<TriggerEvent>(sv::MessageID::TriggerMessage);
                msg2->triggerID = terrainResult.trigger;
                msg2->client = ball.client;
            }

            break;
        case TerrainID::Fairway:
        case TerrainID::Stone: //bouncy :)
            if (ball.velocity.y > MinRollVelocity)
            {
                //start rolling
                startRoll(Ball::State::Roll, ball);
                break;
            }
            //else bounce
            [[fallthrough]];
        case TerrainID::Rough:
            doBullsEyeCollision(tx.getPosition(), ball.client);

            ball.velocity *= Restitution[terrainResult.terrain] + getRestitution(ball.velocity, terrainResult.normal);
            ball.velocity = glm::reflect(ball.velocity, terrainResult.normal);
            //ball.velocity += ball.spin.y * ball.initialForwardVector * SpinAddition[terrainResult.terrain];
            ball.spin *= SpinReduction[terrainResult.terrain];
            break;
        case TerrainID::Green:
            //if low bounce start rolling
            if (ball.velocity.y > MinRollVelocity) // the sooner we start rolling the more velocity we have left to roll :)
            {
                CRO_ASSERT(!std::isnan(ball.velocity.x), "");
                startRoll(Ball::State::Putt, ball);
                CRO_ASSERT(!std::isnan(ball.velocity.x), "");

                return;
            }
            else //bounce
            {
                ball.velocity *= Restitution[terrainResult.terrain] + getRestitution(ball.velocity, terrainResult.normal);
                ball.velocity = glm::reflect(ball.velocity, terrainResult.normal);
                //ball.velocity += ball.spin.y * ball.initialForwardVector * SpinAddition[terrainResult.terrain];
                ball.spin *= SpinReduction[terrainResult.terrain];
                CRO_ASSERT(!std::isnan(ball.velocity.x), "");
            }
            break;
        }

        //stop the ball if velocity low enough
        auto len2 = glm::length2(ball.velocity);
        if (len2 < 0.01f)
        {
            switch (terrainResult.terrain)
            {
            default: 
                resetBall(ball, Ball::State::Paused, terrainResult.terrain);
                break;
            case TerrainID::Green:
                //we need to check if we're in hole/gimme rad so switch to rolling state
                //and let that deal with stopping
                ball.state = Ball::State::Putt;
                ball.delay = 0.f;
                break;
            case TerrainID::Water:
            case TerrainID::Scrub:
                resetBall(ball, Ball::State::Reset, terrainResult.terrain);
                break;
            case TerrainID::Stone:
                //assume flat surfaces are OK to stop on
                if (glm::dot(terrainResult.normal, cro::Transform::Y_AXIS) > MaxStoneSlope)
                {
                    resetBall(ball, Ball::State::Paused, terrainResult.terrain);
                }
                else
                {
                    resetBall(ball, Ball::State::Reset, TerrainID::Scrub);
                }
                break;
            }
        }

        //this stops repeated events if the ball is moving slowly
        //so eg we don't raise a lot of sound requests
        if (len2 > 0.05f
            || terrainResult.terrain == TerrainID::Scrub
            || terrainResult.terrain == TerrainID::Water) //vel will be 0 in this case
        {
            auto* msg = postMessage<CollisionEvent>(MessageID::CollisionMessage);
            msg->terrain = terrainResult.terrain;
            msg->position = pos;
            msg->type = CollisionEvent::Begin;
            msg->velocity = glm::length2(ball.velocity);

            ball.lastTerrain = terrainResult.terrain;

            //this might raise an achievement for example
            //so don't do it during CPU prediction, or fast forwarding, cos that kinda cheats
            //or at least means the player misses out on seeing it happen
            if (m_processFlags == 0)
            {
                auto* msg2 = postMessage<TriggerEvent>(sv::MessageID::TriggerMessage);
                msg2->triggerID = terrainResult.trigger;
                msg2->client = ball.client;
            }
        }
    }
    else if (pos.y < WaterLevel)
    {
        //must have missed all geometry and so are in scrub or water
        auto& ball = entity.getComponent<Ball>();
        resetBall(ball, Ball::State::Reset, TerrainID::Scrub);
        ball.lastTerrain = TerrainID::Water;

        auto* msg = postMessage<CollisionEvent>(MessageID::CollisionMessage);
        msg->terrain = TerrainID::Water;
        msg->position = pos;
        msg->type = CollisionEvent::Begin;
    }
}

void BallSystem::doBallCollision(cro::Entity entity)
{
    //even with max 16 balls this should be negligable to do
    //but we can always place them in some sort of grid if we need to

    //this is crude collision which acts like other balls are
    //solid, but it prevents balls overlapping and accidentally
    //knocking other balls into the hole which would need
    //complicated rule making... :3

    auto& tx = entity.getComponent<cro::Transform>();
    auto& ball = entity.getComponent<Ball>();
    //static constexpr float MinDist = 5.f * 5.f;// 0.5f * 0.5f;// (Ball::Radius * 10.f)* (Ball::Radius * 10.f);
    static constexpr float CollisionDist = (Ball::Radius * 1.7f) * (Ball::Radius * 1.7f);

    //don't collide until we moved from our start position
    if (ball.terrain != TerrainID::Hole &&
        /*glm::length2(ball.startPoint - tx.getPosition()) > MinDist &&*/
        glm::length2(m_holeData->pin - tx.getPosition()) > MinAttractRadius)
    {
        //ball centre is actually pos.y + radius
        glm::vec3 ballPos = entity.getComponent<cro::Transform>().getPosition();
        ballPos.y += Ball::Radius;
        const auto& others = getEntities();
        for (auto other : others)
        {
            if (other != entity
                && other.getComponent<Ball>().terrain != TerrainID::Hole)
            {
                auto otherPos = other.getComponent<cro::Transform>().getPosition();
                otherPos.y += Ball::Radius;

                auto otherDir = ballPos - otherPos;
                if (float testDist = glm::length2(otherDir); testDist < CollisionDist && testDist != 0)
                {
                    float actualDist = std::sqrt(testDist);
                    auto norm = otherDir / actualDist;
                    float penetration = (Ball::Radius * 2.f) - actualDist;

                    tx.move(norm * penetration);
                    ball.velocity = glm::reflect(ball.velocity, norm) * 0.4f;

                    CRO_ASSERT(!std::isnan(tx.getPosition().x), "");
                    CRO_ASSERT(!std::isnan(ball.velocity.x), "");
                }
                else if (testDist == 0)
                {
                    //by some chance we're directly on top of each other
                    //(this might happen in fast CPU play)
                    tx.move({ Ball::Radius, 0.f, 0.f });
                    ball.velocity = glm::vec3(0.f);
                }
            }
        }
    }
}

void BallSystem::doBullsEyeCollision(glm::vec3 ballPos, std::uint8_t client)
{
    if (m_bullsEye.spawn && m_processFlags != ProcessFlags::Predicting)
    {
        const glm::vec2 p1(ballPos.x, -ballPos.z);
        const glm::vec2 p2(m_bullsEye.position.x, -m_bullsEye.position.z);

        const float BullRad = m_bullsEye.diametre / 2.f;

        if (auto len2 = glm::length2(p2 - p1); len2 < (BullRad * BullRad))
        {
            auto* msg = postMessage<BullsEyeEvent>(sv::MessageID::BullsEyeMessage);
            //unlikely, but will upset sqrt
            if (len2 == 0)
            {
                //len2 = 0.000001f;
                msg->accuracy = 1.f;
            }
            else
            {
                msg->accuracy = std::clamp(1.f - (std::sqrt(len2) / BullRad), 0.f, 1.f);
            }
            msg->position = ballPos;
            msg->client = client;
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
        m_windInterpTime = static_cast<float>(cro::Util::Random::value(10, 25)) / 10.f;
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

        m_windStrengthTarget = MinWindStrength + (m_noiseBuffer[m_noiseIndex] - MinWindStrength);
        m_noiseIndex = (m_noiseIndex + 1) % m_noiseBuffer.size();

        m_windStrengthTarget *= m_maxStrengthMultiplier;
        resetInterp();
    }
    //m_windStrengthTarget = 0.f;
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
        colourOffset += static_cast<std::int32_t>(meshData.attributes[i]);
    }

    //we have to create a specific object for each sub mesh
    //to be able to tag it with a different terrain...

    //Later note: now we have per-triangle terrain detection this probably isn't true now.
    //for (auto i = 0u; i < m_indexData.size(); ++i)
    for(const auto& id : m_indexData)
    {
        btIndexedMesh groundMesh;
        groundMesh.m_vertexBase = reinterpret_cast<std::uint8_t*>(m_vertexData.data());
        groundMesh.m_numVertices = static_cast<int>(meshData.vertexCount);
        groundMesh.m_vertexStride = static_cast<int>(meshData.vertexSize);

        
        groundMesh.m_numTriangles = /*meshData.indexData[i].indexCount*/id.size() / 3;
        groundMesh.m_triangleIndexBase = reinterpret_cast<const std::uint8_t*>(id.data());
        groundMesh.m_triangleIndexStride = 3 * sizeof(std::uint32_t);


        m_groundVertices.emplace_back(std::make_unique<btTriangleIndexVertexArray>())->addIndexedMesh(groundMesh);
        m_groundShapes.emplace_back(std::make_unique<btBvhTriangleMeshShape>(m_groundVertices.back().get(), false));
        m_groundObjects.emplace_back(std::make_unique<btPairCachingGhostObject>())->setCollisionShape(m_groundShapes.back().get());
        m_groundObjects.back()->setUserIndex(colourOffset); //use to read the terrain type in RayResult
        m_collisionWorld->addCollisionObject(m_groundObjects.back().get(), CollisionGroup::Terrain, CollisionGroup::Ball);
    }


    m_puttFromTee = getTerrain(m_holeData->tee).terrain == TerrainID::Green;

    return true;
}
