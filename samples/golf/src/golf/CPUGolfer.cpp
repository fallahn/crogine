/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2023
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

#include "CPUGolfer.hpp"
#include "InputParser.hpp"
#include "MessageIDs.hpp"
#include "Clubs.hpp"
#include "CollisionMesh.hpp"
#include "server/ServerPacketData.hpp"

#include <Social.hpp>

#include <crogine/gui/Gui.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

namespace
{
    const std::array StateStrings =
    {
        std::string("Inactive"),
        std::string("Distance Calc"),
        std::string("Picking Club"),
        std::string("Aiming"),
        std::string("Predicting"),
        std::string("Stroke"),
        std::string("Watching")
    };

    struct Debug final
    {
        float diff = 0.f;
        float windDot = 0.f;
        float windComp = 0.f;
        float slope = 0.f;
        float slopeComp = 0.f;
        float targetAngle = 0.f;
        float targetDot = 0.f;
    }debug;

    constexpr float MinSearchDistance = 10.f;
    constexpr float SearchIncrease = 10.f;
    constexpr float Epsilon = 0.005f;
    constexpr std::int32_t MaxRetargets = 12;
    constexpr std::int32_t RetargetsPerDirection = 3;
    constexpr std::int32_t MaxPredictions = 20;

    template <typename T>
    T* postMessage(std::int32_t id)
    {
        return cro::App::getInstance().getMessageBus().post<T>(id);
    };

    const cro::Time MaxAimTime = cro::seconds(5.f);
    const cro::Time MaxPredictTime = cro::seconds(6.f);

    constexpr std::array Deviance =
    {
        0.001f, 0.f, -0.001f, 0.f,
        0.003f, 0.f, -0.0022f, 0.f,
        0.004f, 0.002f, 0.0001f, 0.f
    };
    std::size_t devianceOffset = 0;
}

/*
result tolerance
result tolerance putting
stroke accuracy
mistake odds
*/
const std::array<CPUGolfer::SkillContext, 6> CPUGolfer::m_skills =
{
    CPUGolfer::SkillContext(CPUGolfer::Skill::Amateur, 0.f, 0.f, 4, 0),
    {CPUGolfer::Skill::Dynamic, 10.f, 0.12f, 6, 6},
    {CPUGolfer::Skill::Dynamic, 6.f, 0.12f, 5, 6},
    {CPUGolfer::Skill::Dynamic, 4.f, 0.9f, 4, 10},
    {CPUGolfer::Skill::Dynamic, 2.2f, 0.06f, 2, 14},
    {CPUGolfer::Skill::Dynamic, 2.f, 0.06f, 1, 50}
};

CPUGolfer::CPUGolfer(const InputParser& ip, const ActivePlayer& ap, const CollisionMesh& cm)
    : m_inputParser     (ip),
    m_activePlayer      (ap),
    m_collisionMesh     (cm),
    m_distanceToPin     (1.f),
    m_target            (0.f),
    m_baseTarget        (0.f),
    m_retargetCount     (0),
    m_predictionUpdated (false),
    m_wantsPrediction   (false),
    m_predictionResult  (0.f),
    m_predictionCount   (0),
    m_skillIndex        (0),
    m_clubID            (ClubID::Driver),
    m_prevClubID        (ClubID::Driver),
    m_searchDirection   (0),
    m_searchDistance    (MinSearchDistance),
    m_targetDistance    (0.f),
    m_aimDistance       (0.f),
    m_aimAngle          (0.f),
    m_targetAngle       (0.f),
    m_targetPower       (1.f),
    m_targetAccuracy    (0.f),
    m_prevPower         (0.f),
    m_prevAccuracy      (-1.f),
    m_thinking          (false),
    m_thinkTime         (0.f),
    m_offsetRotation    (cro::Util::Random::value(0, 1024))
{
#ifdef CRO_DEBUG_
    registerWindow([&]()
        {
            if (ImGui::Begin("CPU"))
            {
                ImGui::Text("State: %s", StateStrings[static_cast<std::int32_t>(m_state)].c_str());
                //ImGui::Text("Wind Dot: %3.2f", debug.windDot);
                //ImGui::Text("Target Diff: %3.2f", debug.diff);
                //ImGui::Text("Search Distance: %3.2f", m_searchDistance);
                //ImGui::Text("Target Distance: %3.3f", m_aimDistance);
                //ImGui::Text("Current Club: %s", Clubs[m_clubID].name.c_str());
                ImGui::Separator();
                //ImGui::Text("Wind Comp: %3.3f", debug.windComp);
                //ImGui::Text("Slope: %3.3f", debug.slope);
                //ImGui::Text("Slope Comp: %3.3f", debug.slopeComp);
                ImGui::Text("Start Angle: %3.3f", m_aimAngle);
                ImGui::Text("Target Angle: %3.3f", m_targetAngle);
                ImGui::Text("Target Dot: %3.3f", debug.targetDot);
                ImGui::Text("Current Angle: %3.3f", m_inputParser.getYaw());
                ImGui::Text("Target Power: %3.3f", m_targetPower);
                ImGui::Text("Target Accuracy: %3.3f", m_targetAccuracy);
                ImGui::Separator();
                auto target = m_target - m_activePlayer.position;
                ImGui::Text("Target %3.3f, %3.3f, %3.3f", target.x, target.y, target.z);
                target = m_predictionResult - m_activePlayer.position;
                ImGui::Text("Prediction %3.3f, %3.3f, %3.3f", target.x, target.y, target.z);
                float dist = glm::length(m_target - m_predictionResult);
                ImGui::Text("Distance to targ %3.3f", dist);
            }
            ImGui::End();
        }, true);
#endif
}

//public
void CPUGolfer::handleMessage(const cro::Message& msg)
{
    switch (msg.id)
    {
    default: break;
    case MessageID::GolfMessage:
    {
        const auto& data = msg.getData<GolfEvent>();
        if (data.type == GolfEvent::BallLanded)
        {
            m_state = State::Inactive;
            //LOG("CPU is now inactive", cro::Logger::Type::Info);
        }
    }
        break;
    }
}

void CPUGolfer::activate(glm::vec3 target)
{
    if (m_state == State::Inactive)
    {
        m_baseTarget = m_target = target;
        m_retargetCount = 0;
        m_state = State::CalcDistance;
        m_clubID = m_inputParser.getClub();
        m_prevClubID = m_clubID;
        m_wantsPrediction = false;
        m_predictionCount = 0;

        m_offsetRotation++; //causes the offset calc to pick a new number each time a player is selected

        //choose skill based on player's XP, increasing every 3 levels
        auto level = std::min(Social::getLevel(), 20);
        m_skillIndex = level / 4;

        startThinking(1.6f);

        //this just hides the icon cos I don't like it showing until the name animation is complete :)
        auto* msg = postMessage<AIEvent>(MessageID::AIMessage);
        msg->type = AIEvent::EndThink;

        //LOG("CPU is now active", cro::Logger::Type::Info);
    }
}

void CPUGolfer::update(float dt, glm::vec3 windVector, float distanceToPin)
{
    m_distanceToPin = distanceToPin;

    for (auto& evt : m_popEvents)
    {
        SDL_PushEvent(&evt);
    }
    m_popEvents.clear();

    if (m_skills[getSkillIndex()].skill == Skill::Dynamic)
    {
        switch (m_state)
        {
        default:
        case State::Inactive:
        case State::Watching:
            break;
        case State::CalcDistance:
            m_state = State::PickingClub;
            break;
        case State::PickingClub:
            pickClubDynamic(dt);
            break;
        case State::Aiming:
            aimDynamic(dt);
            break;
        case State::UpdatePrediction:
            updatePrediction(dt);
            break;
        case State::Stroke:
            stroke(dt);
            break;
        }
    }
    else
    {
        switch (m_state)
        {
        default:
        case State::Inactive:
        case State::Watching:
            break;
        case State::CalcDistance:
            calcDistance(dt, windVector);
            break;
        case State::PickingClub:
            pickClub(dt);
            break;
        case State::Aiming:
            aim(dt, windVector);
            break;
        case State::Stroke:
            stroke(dt);
            break;
        }
    }
}

void CPUGolfer::setPredictionResult(glm::vec3 result, std::int32_t terrain)
{
    if (m_retargetCount < MaxRetargets &&
        (terrain == TerrainID::Water
        || terrain == TerrainID::Scrub))
    {
        //retarget
        const float searchDistance = m_activePlayer.terrain == TerrainID::Green ? 0.5f : 2.f;
        auto direction = m_retargetCount % 4;
        auto offset = (glm::normalize(m_baseTarget - m_activePlayer.position) * searchDistance) * static_cast<float>((m_retargetCount % RetargetsPerDirection) + 1);

        //TODO decide on some optimal priority for this?
        switch (direction)
        {
        default:
        case 0:
            //move away
            break;
        case 1:
            //move towards
            offset *= -1.f;
            break;
        case 2:
            //move right
            offset = { -offset.z, offset.y, offset.x };
            break;
        case 3:
            //move right
            offset = { offset.z, offset.y, -offset.x };
            break;
        }

        m_target = m_baseTarget + offset;
        m_state = State::CalcDistance;
        m_wantsPrediction = false;
        m_retargetCount++;
    }
    else
    {
        m_predictionResult = result + (glm::vec3(getOffsetValue(), 0.f, -getOffsetValue()) * 0.001f);
        m_predictionUpdated = true;
    }
}

//private
void CPUGolfer::startThinking(float duration)
{
    if (!m_thinking)
    {
        m_thinking = true;
        m_thinkTime = duration + cro::Util::Random::value(0.1f * duration, 0.8f * duration);

        auto* msg = postMessage<AIEvent>(MessageID::AIMessage);
        msg->type = AIEvent::BeginThink;
    }
}

void CPUGolfer::think(float dt)
{
    m_thinkTime -= dt;
    if (m_thinkTime < 0)
    {
        m_thinking = false;
    }
}

void CPUGolfer::calcDistance(float dt, glm::vec3 windVector)
{
    if (m_thinking)
    {
        think(dt);
    }
    else
    {
        //get distance to target
        auto targetDir = m_target - m_activePlayer.position;
        float targetDistance = glm::length(targetDir);
        float absDistance = targetDistance;

        //if we're on the green putter should be auto selected
        if (m_activePlayer.terrain == TerrainID::Green)
        {
            startThinking(1.f);
            m_state = State::Aiming;
            m_clubID = ClubID::Putter;
            m_prevClubID = m_clubID;

            m_aimDistance = targetDistance;
            m_aimAngle = m_inputParser.getYaw();
            m_aimTimer.restart();

            //LOG("CPU Entered Aiming Mode", cro::Logger::Type::Info);
            return;
        }

        //adjust the target distance depending on how the wind carries us
        if (m_activePlayer.terrain != TerrainID::Green)
        {
            float windDot = -(glm::dot(glm::normalize(glm::vec2(windVector.x, -windVector.z)), glm::normalize(glm::vec2(targetDir.x, -targetDir.z))));
            windDot *= windVector.y;
            windDot *= 0.1f; //magic number. This is the maximum amount of the current distance added to itself
            //windDot = std::max(0.f, windDot);
            targetDistance += (targetDistance * windDot);
        }

        //and hit further if we're off the fairway
        if (m_activePlayer.terrain == TerrainID::Rough
            || m_activePlayer.terrain == TerrainID::Bunker)
        {
            //we want to aim a bit further if we went
            //off the fairway, so we hit a bit harder.

            //but not too much if we're near the hole
            float multiplier = absDistance / Clubs[ClubID::PitchWedge].getTarget(m_distanceToPin);
            targetDistance += 20.f * multiplier; //TODO reduce this if we're close to the green
        }
        m_searchDistance = targetDistance;
        m_targetDistance = targetDistance;
        m_state = State::PickingClub;
    }
}

void CPUGolfer::pickClub(float dt)
{
    if (m_thinking)
    {
        think(dt);
    }
    else
    {
        auto absDistance = glm::length(m_target - m_activePlayer.position);

        auto club = m_inputParser.getClub();
        float clubDistance = Clubs[club].getTarget(m_distanceToPin);

        const auto acceptClub = [&]()
        {
            startThinking(0.5f);
            m_state = State::Aiming;
            m_aimDistance = absDistance < 15.f ? absDistance : m_targetDistance; //hacky way to shorten distance near the green
            m_aimAngle = m_inputParser.getYaw();
            m_aimTimer.restart();
            //LOG("CPU Entered Aiming Mode", cro::Logger::Type::Info);
        };
        
        auto diff = m_searchDistance - clubDistance;
#ifdef CRO_DEBUG_
        debug.diff = diff;
        //debug.windDot = windDot;
#endif
        if (diff < 0) //club is further than search
        {
            m_prevClubID = club;
            m_clubID = club;

            acceptClub();
            return;
        }

        //if the new club has looped switch back and accept it
        if (m_searchDirection == 1 && clubDistance < Clubs[m_prevClubID].getTarget(m_distanceToPin))
        {
            sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::PrevClub]);
            m_clubID = m_prevClubID;

            acceptClub();
            return;
        }

        if (m_searchDirection == -1 && clubDistance > Clubs[m_prevClubID].getTarget(m_distanceToPin))
        {
            sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::NextClub]);
            m_clubID = m_prevClubID;

            acceptClub();
            return;
        }

        if (diff > 0)
        {
            //if we previously searched down decrease the search distance
            if (m_searchDirection == -1)
            {
                m_searchDistance -= SearchIncrease;
            }

            //increase club if needed
            sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::NextClub]);
            m_searchDirection = 1;
            startThinking(0.25f);
        }
        else
        {
            //if we previously searched up decrease the search distance
            if (m_searchDirection == 1)
            {
                m_searchDistance -= SearchIncrease;
            }

            //else decrease
            sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::PrevClub]);
            m_searchDirection = -1;
            startThinking(0.25f);
        }
        m_prevClubID = club;

        //else think for some time
        startThinking(1.f);
    }
}

void CPUGolfer::pickClubDynamic(float dt)
{
    if (m_thinking)
    {
        think(dt);
    }
    else
    {
        float targetDistance = glm::length(m_target - m_activePlayer.position);


        //if we're on the green putter should be auto selected
        if (m_activePlayer.terrain == TerrainID::Green)
        {
            startThinking(0.5f);
            m_state = State::Aiming;
            m_clubID = ClubID::Putter;
            m_prevClubID = m_clubID;

            m_aimAngle = m_inputParser.getYaw();
            m_aimTimer.restart();

            //TODO increase target power with slope
            const auto clubTarget = Clubs[ClubID::Putter].getTarget(m_distanceToPin);
            m_targetPower = std::min(1.f, targetDistance / clubTarget);
            if (clubTarget < Clubs[ClubID::Putter].getBaseTarget())
            {
                //this is a shortened putter
                m_targetPower += 0.2f;
            }

            return;
        }


        //find the first club whose target exceeds this distance
        //else use the most powerful club available
        auto club = m_inputParser.getClub();
        float clubDistance = Clubs[club].getTarget(m_distanceToPin);
        float diff = targetDistance - clubDistance;

        const auto acceptClub = [&]()
        {
            m_state = State::Aiming;
            m_aimAngle = m_inputParser.getYaw() + (getOffsetValue() * 0.002f);
            m_aimTimer.restart();

            //guestimate power based on club (this gets refined from predictions)
            m_targetPower = std::min(1.f, targetDistance / Clubs[m_clubID].getTarget(m_distanceToPin));
            m_targetPower = std::min(1.f, m_targetPower + (getOffsetValue() / 100.f));
        };

        if (diff < 0)
        {
            m_prevClubID = club;
            m_clubID = club;

            acceptClub();
            return;
        }


        //if the new club has looped switch back and accept it (it's the longest we have)
        if (m_searchDirection == 1 && clubDistance < Clubs[m_prevClubID].getTarget(m_distanceToPin))
        {
            sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::PrevClub]);
            m_clubID = m_prevClubID;

            acceptClub();
            return;
        }

        if (m_searchDirection == -1 && clubDistance > Clubs[m_prevClubID].getTarget(m_distanceToPin))
        {
            sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::NextClub]);
            m_clubID = m_prevClubID;

            acceptClub();
            return;
        }


        if (diff > 0)
        {
            //increase club if needed
            sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::NextClub]);
            m_searchDirection = 1;
            startThinking(0.25f);
        }
        else
        {
            //else decrease
            sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::PrevClub]);
            m_searchDirection = -1;
            startThinking(0.25f);
        }
        m_prevClubID = club;

        //else think for some time
        startThinking(1.f);
    }
}

void CPUGolfer::aim(float dt, glm::vec3 windVector)
{
    if (m_thinking)
    {
        think(dt);
    }
    else
    {
        //check someone hasn't pressed the keyboard already
        //and started the power bar. In which case break from this
        //TODO could play avatar disappointed sound? :)
        if (m_inputParser.inProgress())
        {
            m_state = State::Watching;
            return;
        }

        //putting is a special case where wind effect is lower
        //but we also need to deal with the slope of the green
        float greenCompensation = 0.9f; //default reduction for driving (this was reduced from 1 because max rotation was increased to 0.13)
        float slopeCompensation = 0.f;

        auto targetDir = m_target - m_activePlayer.position;
        const float distanceToTarget = glm::length(targetDir);

        if (m_activePlayer.terrain == TerrainID::Green)
        {
            //no wind on the green
            greenCompensation = 0.f;// 0.016f; //reduce the wind compensation by this

//            //then calculate the slope by measuring two points either side of a point
//            //approx two thirds to the hole.
//            auto centrePoint = (m_target - m_activePlayer.position) * 0.75f;
//            float distanceReduction = std::min(0.86f, glm::length(centrePoint) / 3.f);
//
//            //reduce this as we get nearer the hole
//            auto distance = glm::normalize(centrePoint) * (0.1f + (0.9f * std::min(1.f, (distanceToTarget / 2.f))));
//            centrePoint += m_activePlayer.position;
//
//            //left point
//            distance = { distance.z, distance.y, -distance.x };
//            auto resultA = m_collisionMesh.getTerrain(centrePoint + distance);
//
//            //right point
//            distance *= -1.f;
//            auto resultB = m_collisionMesh.getTerrain(centrePoint + distance);
//
//            static constexpr float MaxSlope = 0.048f; //~48cm diff in slope
//#ifdef CRO_DEBUG_
//            debug.slope = resultA.height - resultB.height;
//#endif
//            float slope = (resultA.height - resultB.height) / MaxSlope;
//            slopeCompensation = m_inputParser.getMaxRotation() * slope;// *0.15f;
//            slopeCompensation *= distanceReduction; //reduce the effect nearer the hole
//            greenCompensation *= distanceReduction;
        }


        //wind is x, strength (0 - 1), z

        //create target angle based on wind strength / direction
        auto w = glm::normalize(glm::vec2(windVector.x, -windVector.z));
        auto t = glm::normalize(glm::vec2(targetDir.x, -targetDir.z));

        //max rotation (percent of InputParser::MaxRotation) to apply for wind.
        //rotation is good ~ 0.1 rads, so this values is 0.1/InputParser::MaxRotation
        const float MaxCompensation = 0.12f / m_inputParser.getMaxRotation();
        float dot = glm::dot(w, t);
        float windComp = (1.f - std::abs(dot)) * MaxCompensation;

        auto wAngle = std::atan2(w.y, w.x);
        auto tAngle = std::atan2(t.y, t.x);

        windComp *= cro::Util::Maths::sgn(cro::Util::Maths::shortestRotation(wAngle, tAngle));
        windComp *= windVector.y; //reduce with wind strength
        windComp *= greenCompensation;

        float targetAngle = m_aimAngle + (m_inputParser.getMaxRotation() * windComp);
        targetAngle += slopeCompensation;
        targetAngle = std::min(m_aimAngle + m_inputParser.getMaxRotation(), std::max(m_aimAngle - m_inputParser.getMaxRotation(), targetAngle));
        targetAngle *= 0.99f;

#ifdef CRO_DEBUG_
        debug.windComp = windComp;
        debug.slopeComp = slopeCompensation;
        debug.targetAngle = targetAngle;
#endif


        //hold rotate button if not within angle tolerance
        if (targetAngle < m_inputParser.getYaw())
        {
            sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::Right], false);
        }
        else
        {
            sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::Left], false);
        }

        //if angle correct then calc a target power
        if (std::abs(m_inputParser.getYaw() - targetAngle) < 0.01f
            || m_aimTimer.elapsed() > MaxAimTime) //get out clause if aim calculation is out of bounds.
        {
            //stop holding rotate
            sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::Right], true);
            sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::Left], true);

            //based on dot prod of aim angle and wind dir
            //multiplied by percent of selected club distance to target distance
            if (m_clubID < ClubID::PitchWedge)
            {
                //power is not actually linear - ie half the distance is not
                //half the power, so we need to pull back a little to stop
                //overshooting long drives
                m_targetPower = m_aimDistance / Clubs[m_clubID].getTarget(m_distanceToPin);
                //if (Clubs[m_clubID].target > m_aimDistance)
                {
                    //the further we try to drive the bigger the reduction
                    float amount = 1.f - (static_cast<float>(m_clubID) / ClubID::NineIron);
                    m_targetPower *= (1.f - (amount * 0.2f));
                }
            }
            else
            {
                m_targetPower = (distanceToTarget * 1.12f) / Clubs[m_clubID].getTarget(m_distanceToPin);
            }
            m_targetPower += ((0.06f * (-dot * windVector.y)) * greenCompensation) * m_targetPower;


            //add some random factor to target power and set to stroke mode
            m_targetPower = std::min(1.f, m_targetPower);
            m_targetPower += static_cast<float>(cro::Util::Random::value(-4, 4)) / 100.f;
            m_targetPower = std::max(0.06f, std::min(1.f, m_targetPower));

            if (m_activePlayer.terrain == TerrainID::Green)
            {
                //hackiness to compensate for putting shortfall
                float distRatio = 1.f - std::min(1.f, distanceToTarget / 3.5f); //applied within this radius
                float multiplier = (0.5f * distRatio) + 1.f;

                m_targetPower = std::min(1.f, m_targetPower * multiplier);
            }


            ////due to input lag 0.08 is actually ~0 ie perfectly accurate
            ////so this range lies ~0.04 either side of perfect
            //m_targetAccuracy = static_cast<float>(cro::Util::Random::value(4, 12)) / 100.f;

            ////occasionally make really inaccurate
            ////... or maybe even perfect? :)
            //if (cro::Util::Random::value(0, 8) == 0)
            //{
            //    m_targetAccuracy += static_cast<float>(cro::Util::Random::value(-8, 4)) / 100.f;
            //}

            calcAccuracy();

            m_state = State::Stroke;
            startThinking(0.4f);

            auto* msg = postMessage<AIEvent>(MessageID::AIMessage);
            msg->type = AIEvent::EndThink;

            //LOG("Started stroke", cro::Logger::Type::Info);
        }
    }
}

void CPUGolfer::aimDynamic(float dt)
{
    if (m_thinking)
    {
        think(dt);
    }
    else
    {
        //pick inititial direction
        if (!m_wantsPrediction)
        {
            m_targetAngle = m_aimAngle;
            if (m_activePlayer.terrain != TerrainID::Green)
            {
                m_targetAngle += getOffsetValue() * 0.001f;
            }
            m_wantsPrediction = true;
        }
        //or refine based on prediction
        else
        {
            m_targetAngle = std::clamp(m_targetAngle + (Deviance[devianceOffset] * 0.1f), m_aimAngle - (m_inputParser.getMaxRotation() * 0.9f), m_aimAngle + (m_inputParser.getMaxRotation() * 0.9f));

            //update input parser
            if (auto diff = std::abs(m_inputParser.getYaw() - m_targetAngle); diff > 0.05f
                && m_aimTimer.elapsed() < MaxAimTime)
            {
                if (m_targetAngle < m_inputParser.getYaw())
                {
                    sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::Right], false);
                }
                else
                {
                    sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::Left], false);
                }
            }
            else
            {
                //stop holding rotate
                sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::Right], true);
                sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::Left], true);

                //request prediction and wait result.
                auto* msg = postMessage<AIEvent>(MessageID::AIMessage);
                msg->type = AIEvent::Predict;
                msg->power = m_targetPower;

                startThinking(0.1f);

                m_state = State::UpdatePrediction;
                m_predictTimer.restart();
            }
        }
    }
}

void CPUGolfer::updatePrediction(float dt)
{
    if (m_thinking)
    {
        think(dt);
    }
    else
    {
        auto devianceMultiplier = m_activePlayer.terrain - TerrainID::Green; //if green no deviance, rough then double deviance :)
        auto takeShot = [&]()
        {
            calcAccuracy();

            //we need to maintain a min power target incase we're on the lip of the hole
            m_targetPower = std::max(0.1f, std::min(m_targetPower + ((Deviance[devianceOffset] * 0.1f) * devianceMultiplier), 1.f));
            m_targetAccuracy -= (Deviance[devianceOffset] * 0.05f) * devianceMultiplier;

            m_state = State::Stroke;

            startThinking(0.4f);

            auto* msg = postMessage<AIEvent>(MessageID::AIMessage);
            msg->type = AIEvent::EndThink;
        };

        if (m_predictionUpdated)
        {
            //check aim and update target if necessary
            //then switch back to aiming
            auto targetDir = m_target - m_activePlayer.position;
            auto predictDir = m_predictionResult - m_activePlayer.position;

            auto dir = glm::normalize(targetDir);
            dir = { -dir.z, dir.y, dir.x }; //rotating this 90 deg means dot prod is +/- depending on if it's too far left or right

            auto resultDir = glm::normalize(predictDir);

            //adding the player ID just means that 2 players on a client won't make identical decisions
            const float tolerance = 0.05f + (getOffsetValue() / 100.f);
            float dot = glm::dot(resultDir, dir);
#ifdef CRO_DEBUG_
            debug.targetDot = dot;
#endif
            if (m_predictionCount++ < MaxPredictions &&
                ((dot < -tolerance
                && m_targetAngle < ((m_aimAngle + m_inputParser.getMaxRotation()) - Epsilon))
            || (dot > tolerance
                && m_targetAngle > ((m_aimAngle - m_inputParser.getMaxRotation()) + Epsilon))))
            {
                float skew = getOffsetValue() / 1000.f;
                skew += (Deviance[devianceOffset] * 0.06f) * devianceMultiplier;

                m_targetAngle = std::min(m_aimAngle + m_inputParser.getMaxRotation(), 
                    std::max(m_aimAngle - m_inputParser.getMaxRotation(),
                        m_targetAngle + ((cro::Util::Const::PI / 2.f) * dot) + skew));

                m_state = State::Aiming;
                m_aimTimer.restart();
            }
            //else check distance / current tolerance
            //and update power if necessary
            else
            {
                //calc this tolerance based on CPU difficulty / distance to hole
                float precision = m_skills[getSkillIndex()].resultTolerancePutt;
                if (m_activePlayer.terrain != TerrainID::Green)
                {
                    precision = m_skills[getSkillIndex()].resultTolerance + (getOffsetValue() * 0.5f);
                    precision = (precision / 2.f) + ((precision / 2.f) * std::min(1.f, glm::length(targetDir) / 200.f));
                }

                float precSqr = precision * precision;
                if (float resultPrecision = glm::length2(predictDir - targetDir);
                    resultPrecision > precSqr && m_targetPower < 1.f)
                {
                    float precDiff = std::sqrt(resultPrecision);
                    float change = (precDiff / Clubs[m_clubID].getTarget(m_distanceToPin)) / 2.f;
                    change += getOffsetValue() / 60.f;

                    if (glm::length2(predictDir) < glm::length2(targetDir))
                    {
                        //increase power by some amount
                        m_targetPower = std::min(1.f, m_targetPower + change);
                    }
                    else
                    {
                        //decrease power by some amount
                        m_targetPower = std::min(1.f, std::max(0.1f, m_targetPower - change));
                    }

                    //request new prediction
                    if (m_predictionCount++ < MaxPredictions)
                    {
                        auto* msg = postMessage<AIEvent>(MessageID::AIMessage);
                        msg->type = AIEvent::Predict;
                        msg->power = m_targetPower;

                        startThinking(0.05f);

                        m_predictTimer.restart();
                    }
                    else
                    {
                        //LogI << "Reached max predictions" << std::endl;
                        takeShot();
                    }
                }
                else
                {
                    //LogI << "result is " << resultPrecision << " and targ result is " << precSqr << std::endl;
                    //accept our settings
                    takeShot();
                }
            }

            m_predictionUpdated = false;
        }
        else if (m_predictTimer.elapsed() > MaxPredictTime)
        {
            //we timed out getting a response, so just take the shot
            //LogI << "Predict timer expired" << std::endl;
            takeShot();
        }
    }
}

void CPUGolfer::stroke(float dt)
{
    if (m_thinking)
    {
        think(dt);
    }
    else
    {
        if (!m_inputParser.inProgress())
        {
            //start the stroke
            sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::Action]);

            m_strokeState = StrokeState::Power;
            m_prevPower = 0.f;
            m_prevAccuracy = -1.f;

            devianceOffset = (devianceOffset + 1 + m_activePlayer.player) % Deviance.size();
            //TODO this actually happens twice because of the delay of the
            //event queue, how ever doesn't appear to be a problem as long
            //as the 'double-tap' prevention works
        }
        else
        {
            if (m_strokeState == StrokeState::Power)
            {
                auto power = m_inputParser.getPower();
                if ((m_targetPower - power) < 0.02f
                    && m_prevPower < power)
                {
                    sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::Action]);
                    m_strokeState = StrokeState::Accuracy;
                }
                m_prevPower = power;
            }
            else
            {
                auto accuracy = m_inputParser.getHook();
                if (accuracy < m_targetAccuracy
                    && m_prevAccuracy > accuracy)
                {
                    sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::Action]);
                    m_state = State::Watching;
                    return;
                }
                m_prevAccuracy = accuracy;
            }
        }
    }
}

void CPUGolfer::calcAccuracy()
{
    //due to input lag 0.08 is actually ~0 ie perfectly accurate
    //so this range lies ~0.04 either side of perfect
    //m_targetAccuracy = static_cast<float>(cro::Util::Random::value(4, 12)) / 100.f;

    m_targetAccuracy = 0.08f;
    if (m_skills[getSkillIndex()].strokeAccuracy != 0)
    {
        m_targetAccuracy += static_cast<float>(-m_skills[getSkillIndex()].strokeAccuracy, m_skills[m_skillIndex].strokeAccuracy) / 100.f;
    }

    //occasionally make really inaccurate
    //... or maybe even perfect? :)
    if (m_skills[getSkillIndex()].mistakeOdds != 0)
    {
        if (cro::Util::Random::value(0, m_skills[getSkillIndex()].mistakeOdds) == 0)
        {
            m_targetAccuracy += static_cast<float>(cro::Util::Random::value(-16, 16)) / 100.f;
        }
    }

    //to prevent multiple players making the same decision offset the accuracy a small amount
    //based on their client and player number
    auto offset = (m_offsetRotation % 4) * 10;
    m_targetAccuracy += (static_cast<float>(cro::Util::Random::value(-(offset / 2), (offset / 2) + 1)) / 500.f) * getOffsetValue();

    if (m_clubID != ClubID::Putter)
    {
        m_targetPower = std::min(1.f, m_targetPower + (1 - (cro::Util::Random::value(0, 1) * 2)) * (static_cast<float>(m_offsetRotation % 4) / 50.f));

        if (m_skills[getSkillIndex()].mistakeOdds != 0)
        {
            if (cro::Util::Random::value(0, m_skills[getSkillIndex()].mistakeOdds) == 0)
            {
                m_targetPower += static_cast<float>(cro::Util::Random::value(-6, 6)) / 1000.f;
            }
        }
        m_targetPower = std::min(1.f, m_targetPower);
    }
    else
    {
        //hack to make the ball putt a little further
        m_targetPower = std::min(1.f, m_targetPower * 0.98f /*+ (static_cast<float>(m_skillIndex) * 0.01f)*/);
    }
}

float CPUGolfer::getOffsetValue() const
{
    float multiplier = m_activePlayer.terrain == TerrainID::Green ? smoothstep(0.2f, 0.95f, glm::length(m_target - m_activePlayer.position) / Clubs[ClubID::Putter].getTarget(m_distanceToPin)) : 1.f;

    return static_cast<float>(1 - ((m_offsetRotation % 2) * 2))
        * static_cast<float>((m_offsetRotation % (m_skills.size() - getSkillIndex())))
        * multiplier;
}

std::size_t CPUGolfer::getSkillIndex() const
{
    std::int32_t offset = m_activePlayer.player % 2;
    if (m_skillIndex > 2)
    {
        offset *= -1;
    }

    return std::min(static_cast<std::int32_t>(m_skills.size() - 1), static_cast<std::int32_t>(m_skillIndex) + offset);
}

void CPUGolfer::sendKeystroke(std::int32_t key, bool autoRelease)
{
    SDL_Event evt;
    evt.type = SDL_KEYDOWN;
    evt.key.keysym.mod = 0; //must zero out else we get phantom keypresses
    evt.key.keysym.sym = key;
    evt.key.keysym.scancode = SDL_GetScancodeFromKey(key);
    evt.key.timestamp = 0;
    evt.key.repeat = 0;
    evt.key.windowID = InputParser::CPU_ID;
    evt.key.state = SDL_PRESSED;

    SDL_PushEvent(&evt);

    if (autoRelease)
    {
        //stash the counter-event
        evt.type = SDL_KEYUP;
        evt.key.state = SDL_RELEASED;
        m_popEvents.push_back(evt);
    }
};