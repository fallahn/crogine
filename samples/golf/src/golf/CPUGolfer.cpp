/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2025
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
#include "server/CPUStats.hpp"

#include <Social.hpp>

#include <crogine/gui/Gui.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

using namespace cl;

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
    constexpr std::int32_t MaxRetargets = 6;// 12;
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
    //std::int32_t previousFail = -1; //if the previous shot failed the client/player ID
    std::array<std::int32_t, ConstVal::MaxClients* ConstVal::MaxPlayers> failCounts = {};
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
    {CPUGolfer::Skill::Dynamic, 3.2f, 0.12f, 4, 6},
    {CPUGolfer::Skill::Dynamic, 3.f, 0.12f, 4, 6},
    {CPUGolfer::Skill::Dynamic, 2.8f, 0.1f, 3, 10},
    {CPUGolfer::Skill::Dynamic, 2.2f, 0.06f, 2, 14},
    {CPUGolfer::Skill::Dynamic, 1.f, 0.06f, 0, 50}
};

CPUGolfer::CPUGolfer(InputParser& ip, const ActivePlayer& ap, const CollisionMesh& cm)
    : m_inputParser     (ip),
    m_activePlayer      (ap),
    m_collisionMesh     (cm),
    m_fastCPU           (false),
    m_suspended         (false),
    m_puttFromTee       (false),
    m_distanceToPin     (1.f),
    m_target            (0.f),
    m_baseTarget        (0.f),
    m_fallbackTarget    (0.f),
    m_retargetCount     (0),
    m_predictionUpdated (false),
    m_wantsPrediction   (false),
    m_predictionResult  (0.f),
    m_predictionCount   (0),
    m_puttingPower      (0.f),
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
    std::fill(failCounts.begin(), failCounts.end(), 0);
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
        if (data.type == GolfEvent::BallLanded
            || data.type == GolfEvent::Gimme)
        {
            m_state = State::Inactive;

            if (data.terrain == TerrainID::Scrub
                || data.terrain == TerrainID::Water)
            {
                //switch to fallback target if possible
                failCounts[m_activePlayer.client * ConstVal::MaxPlayers + m_activePlayer.player]++;
            }
        }
    }
        break;
    }
}

void CPUGolfer::activate(glm::vec3 target, glm::vec3 fallback, bool puttFromTee)
{
    if (/*!m_fastCPU &&*/
        m_state == State::Inactive)
    {
        const auto& Stat = CPUStats[m_cpuProfileIndices[m_activePlayer.client * ConstVal::MaxPlayers + m_activePlayer.player]];

        m_puttFromTee = puttFromTee;
        m_fallbackTarget = fallback;
        m_baseTarget = m_target = target;


        //check the pin target isn't around a corner on putting course
        //TODO we need to actually ASSERT this is the pin and not the fallback
        if (m_puttFromTee)
        {
            //add some offset to the fallback target to stop
            //players all aiming for exactly the same point
            glm::vec3 dir(0.f);
            switch (/*cro::Util::Random::value(0, 3)*/m_activePlayer.player % 4)
            {
            default:
            case 0:
                dir = glm::normalize(target - fallback);
                dir = glm::vec3(-dir.z, dir.y, dir.x);
                break;
            case 1:
                dir = glm::normalize(target - fallback);
                dir = glm::vec3(dir.z, dir.y, -dir.x);
                break;
            case 2:
                //perpendicular
                dir = glm::normalize(m_activePlayer.position - fallback);
                dir = glm::vec3(-dir.z, dir.y, dir.x);
                break;
            case 3:
                dir = glm::normalize(m_activePlayer.position - fallback);
                dir = glm::vec3(dir.z, dir.y, -dir.x);
                break;
            }
            float amount = static_cast<float>(cro::Util::Random::value(5 + (m_activePlayer.player % 4), 35) + (m_activePlayer.player % 3)) / 100.f;
            m_fallbackTarget += dir * amount;

            if (target != fallback)
            {
                auto fwd = m_baseTarget - m_activePlayer.position;
                auto alt = m_fallbackTarget - m_activePlayer.position;
                if (glm::dot(alt, fwd) > 0
                    && glm::dot(glm::normalize(alt), glm::normalize(fwd)) < 0.97f)
                {
                    if (glm::length2(alt) > 9.f
                        && glm::length2(alt / 2.f) < glm::length2(fwd))
                    {
                        m_target = m_baseTarget = m_fallbackTarget;
                        //LogI << "switched to fallback, pin around corner (activate)" << std::endl;
                    }
                }
            }
            else
            {
                //apply the updated fallback
                m_target = m_baseTarget = m_fallbackTarget;
            }
        }



        //if previous fail then the last shot was bad (stops meltdowns on doglegs)
        if (auto& count = failCounts[m_activePlayer.client * ConstVal::MaxPlayers + m_activePlayer.player]; count != 0)
        {
            //try moving to fallback if possible
            if (count == 1)
            {
                if (m_target != m_fallbackTarget)
                {
                    const auto moveDir = glm::normalize(m_fallbackTarget - m_target);
                    auto newDir = moveDir;

                    auto terrain = m_collisionMesh.getTerrain(m_baseTarget + newDir).terrain;
                    auto i = 0;
                    while ((terrain == TerrainID::Water
                        || terrain == TerrainID::Scrub
                        || terrain == TerrainID::Bunker)
                        && i++ < 3)
                    {
                        newDir += moveDir;
                        terrain = m_collisionMesh.getTerrain(m_baseTarget + newDir).terrain;
                    }
                    //LogI << "stepped towards fallback on activate" << std::endl;


                    if (terrain == TerrainID::Water
                        || terrain == TerrainID::Scrub)
                    {
                        //else snap to it if it's still in front
                        auto fwd = m_baseTarget - m_activePlayer.position;
                        auto alt = m_fallbackTarget - m_activePlayer.position;
                        if (glm::dot(alt, fwd) > 0)
                        {
                            m_baseTarget = m_target = fallback;
                            count = std::max(0, count - 1);
                            //LogI << "set to fallback on activate (couldn't find good alternative)" << std::endl;
                        }
                    }
                    else
                    {
                        m_baseTarget += newDir;
                        m_target = m_baseTarget;
                    }
                }
            }
            else
            {
                //else snap to it if it's still in front
                auto fwd = m_baseTarget - m_activePlayer.position;
                auto alt = m_fallbackTarget - m_activePlayer.position;
                if (glm::dot(alt, fwd) > 0)
                {
                    m_baseTarget = m_target = fallback;
                    //LogI << "set to fallback on activate" << std::endl;
                }
            }
        }
        //otherwise check if the target is in max range - if not shorten
        //the target (so we prefer the pin still, just fall short) unless
        //the shortened target is out of bounds, in which case use the fallback
        else if (auto len = glm::length(target - m_activePlayer.position);
                    len > Clubs[ClubID::Driver].getTargetAtLevel(Stat[CPUStat::Skill]))
        {
            glm::vec3 newDir(0.f);
            if (target != fallback)
            {
                len -= Clubs[ClubID::Driver].getTargetAtLevel(Stat[CPUStat::Skill]);
                const auto correctionDir = glm::normalize(fallback - target) * (len * 1.05f);
                newDir = target + correctionDir;
            }
            else
            {
                //just move back towards the player
                const float reduction = (Clubs[ClubID::Driver].getTargetAtLevel(Stat[CPUStat::Skill]) / len) * 0.95f;
                newDir = (target - m_activePlayer.position) * reduction;
                newDir += m_activePlayer.position;
            }

            const auto terrain = m_collisionMesh.getTerrain(newDir);
            switch (terrain.terrain)
            {
            default:
                m_baseTarget = m_target = newDir;
                break;
            case TerrainID::Water:
            case TerrainID::Scrub:
            case TerrainID::Stone:
                if (cro::Util::Random::value(0, 9) > Stat[CPUStat::MistakeLikelyhood])
                {
                    m_baseTarget = m_target = fallback;
                }
                break;
            }
        }
        else
        {
            //just check the target is in bounds
            const auto terrain = m_collisionMesh.getTerrain(m_target);
            switch (terrain.terrain)
            {
            default:
                break;
            case TerrainID::Water:
            case TerrainID::Scrub:
            case TerrainID::Bunker:
            case TerrainID::Stone:
                if (cro::Util::Random::value(0, 9) > Stat[CPUStat::MistakeLikelyhood])
                {
                    m_baseTarget = m_target = fallback;
                }
                break;
            }
        }

        m_retargetCount = 0;
        m_state = State::CalcDistance;
        m_clubID = m_inputParser.getClub();
        m_prevClubID = m_clubID;
        m_wantsPrediction = false;
        m_predictionCount = 0;

        m_offsetRotation++; //causes the offset calc to pick a new number each time a player is selected

        //choose skill based on player's XP, increasing every 3 levels
        auto level = std::min(Social::getLevel(), 20) + 2;
        m_skillIndex = level / 4;


        startThinking(1.6f);

        //this just hides the icon cos I don't like it showing until the name animation is complete :)
        auto* msg = postMessage<AIEvent>(MessageID::AIMessage);
        msg->type = AIEvent::EndThink;

        //LOG("CPU is now active", cro::Logger::Type::Info);
        /*if (m_target == m_fallbackTarget)
        {
            LogI << "Using fallback (activation)" << std::endl;
        }
        else
        {
            LogI << "Using target (activation)" << std::endl;
        }*/
    }
}

void CPUGolfer::update(float dt, glm::vec3 windVector, float distanceToPin)
{
    m_distanceToPin = distanceToPin;

    if (m_suspended)
    {
        return;
    }

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
            /*if (m_activePlayer.terrain == TerrainID::Green)
            {
                aim(dt, windVector);
            }
            else*/
            {
                aimDynamic(dt);
            }
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
    //check the pin target isn't around a corner on putting course
    if (m_puttFromTee
        && m_baseTarget != m_fallbackTarget)
    {
        auto fwd = m_baseTarget - m_activePlayer.position;
        auto alt = m_fallbackTarget - m_activePlayer.position;
        if (glm::dot(alt, fwd) > 0
            && glm::dot(glm::normalize(alt), glm::normalize(fwd)) < 0.97f)
        {
            m_baseTarget = m_fallbackTarget;
            //LogI << "switched to fallback, pin around corner (prediction)" << std::endl;
        }
    }


    //TODO should we be compensating for overshoot?
    if (auto& count = failCounts[m_activePlayer.client * ConstVal::MaxPlayers + m_activePlayer.player]; count != 0)
    {
        count = std::max(0, count - 1);

        auto fwd = m_baseTarget - m_activePlayer.position;
        auto alt = m_fallbackTarget - m_activePlayer.position;
        if (glm::dot(alt, fwd) > 0
            && glm::length2(alt) < glm::length2(fwd)) // has to be closer than the pin
        {
            //but not if it's like 3 feet in front...
            const float minDist = m_puttFromTee ? (9.f) : (2500.f);
            if (glm::length2(alt) > minDist)
            {
                m_baseTarget = m_fallbackTarget;
                m_target = m_baseTarget;
            }
        }
        m_state = State::CalcDistance;
        m_wantsPrediction = false;

        //LogI << "Falling back - fail count was " << (count + 1) << std::endl;
    }


    else if (m_retargetCount < (MaxRetargets - (MaxRetargets - getSkillIndex())) &&
        (terrain == TerrainID::Water
        || terrain == TerrainID::Scrub
            || terrain == TerrainID::Bunker))
    {
        const auto& Stat = CPUStats[m_cpuProfileIndices[m_activePlayer.client * ConstVal::MaxPlayers + m_activePlayer.player]];

        //retarget
        if ((cro::Util::Random::value(0, 9) > Stat[CPUStat::MistakeLikelyhood] && terrain != TerrainID::Bunker)
            /*|| (cro::Util::Random::value(0, 9) < Stat[CPUStat::MistakeLikelyhood] && terrain == TerrainID::Bunker)*/) //more likely to hit bunker 
        {
            if (m_retargetCount < 2)
            {
                //try moving towards the fallback from the pin
                if (m_baseTarget != m_fallbackTarget)
                {
                    const auto moveDir = /*glm::normalize*/(m_fallbackTarget - m_target) / 8.f;
                    auto newDir = moveDir;

                    //LogI << "stepped towards fallback on prediction result, would hit terrain " << TerrainStrings[terrain] << std::endl;
                    terrain = m_collisionMesh.getTerrain(m_baseTarget + newDir).terrain;

                    auto i = 0;
                    while ((terrain == TerrainID::Water
                        || terrain == TerrainID::Scrub
                        || terrain == TerrainID::Bunker)
                        && i++ < 3)
                    {
                        newDir += moveDir;
                        terrain = m_collisionMesh.getTerrain(m_baseTarget + newDir).terrain;
                    }

                    if (terrain == TerrainID::Water
                        || terrain == TerrainID::Scrub)
                    {
                        m_baseTarget = m_fallbackTarget;
                        //LogI << "Set to fall back target: failed to get out of " << TerrainStrings[terrain] << std::endl;
                    }
                    else
                    {
                        m_baseTarget += newDir;
                    }
                }
            }
            else if (terrain == TerrainID::Bunker) {}
            else
            {
                //fall back to default target if it's still in front
                auto fwd = m_baseTarget - m_activePlayer.position;
                auto alt = m_fallbackTarget - m_activePlayer.position;
                if (glm::dot(alt, fwd) > 0)
                {
                    m_baseTarget = m_fallbackTarget;
                    //LogI << "set to fallback target on prediction result, would hit terrain " << TerrainStrings[terrain] << std::endl;
                }
            }
            m_puttFromTee = false; //saves keep trying this until next activation
        }
        else
        {
            //LogI << "accepted we'll probably hit " << TerrainStrings[terrain] << std::endl;
            m_retargetCount = 100; // don't try any more, we accepted potential failure.

            m_predictionResult = result + (glm::vec3(getOffsetValue(), 0.f, -getOffsetValue()) * 0.001f);
            m_predictionUpdated = true;
            return;
        }

        m_target = m_baseTarget;
        m_state = State::CalcDistance;
        m_wantsPrediction = false;
        m_retargetCount++;
    }
    else
    {
        m_predictionResult = result + (glm::vec3(getOffsetValue(), 0.f, -getOffsetValue()) * 0.001f);
        m_predictionUpdated = true;
    }

    //if (m_target == m_fallbackTarget)
    //{
    //    LogI << "Using fallback (prediction)" << std::endl;
    //}
    //else
    //{
    //    LogI << "Using target (prediction)" << std::endl;
    //}
}

void CPUGolfer::setPuttingPower(float power)
{
    m_puttingPower = power * 1.01f; 

    //measure slope in front and increase if necessary
    float slope = glm::dot(glm::normalize(m_target - m_activePlayer.position), glm::vec3(0.f, 1.f, 0.f));
    if (slope > 0.01f)
    {
        m_puttingPower = std::min(1.f, m_puttingPower * (1.f + slope));
    }
}

std::size_t CPUGolfer::getSkillIndex() const
{
    /*std::int32_t offset = ((m_activePlayer.player + 2) % 3) * 2;
    return std::clamp((static_cast<std::int32_t>(m_skillIndex) - offset), 0, 5);*/

    auto id = m_cpuProfileIndices[m_activePlayer.client * ConstVal::MaxPlayers + m_activePlayer.player];
    return std::clamp((static_cast<std::int32_t>(CPUStats.size()) - id) / 5, 0, 5);
}

std::int32_t CPUGolfer::getClubLevel() const
{
    return CPUStats[m_cpuProfileIndices[m_activePlayer.client * ConstVal::MaxPlayers + m_activePlayer.player]][CPUStat::Skill];
}

void CPUGolfer::setCPUCount(std::int32_t cpuCount, const SharedStateData& sharedData)
{
    std::fill(m_cpuProfileIndices.begin(), m_cpuProfileIndices.end(), -1);

    if (cpuCount)
    {
        std::int32_t baseCPUIndex = 0;
        //std::int32_t stride = 1;

        switch (/*Social::getClubLevel()*/sharedData.clubSet)
        {
        default: break;
        case 0:
            baseCPUIndex = 12;
            //stride = 16 / cpuCount; //even distribution through 16x level 0
            break;
        case 1:
            baseCPUIndex = 8;
            //stride = cpuCount > 4 ? 2 : 23 / cpuCount; //every other profile unless more than 4
            break;
        case 2:
            /*stride = cpuCount < 3 ? 1 :
                cpuCount < 8 ? 2 :
                27 / cpuCount;*/
            break;
        }


        //CRO_ASSERT(baseCPUIndex + (stride * (cpuCount - 1)) < CPUStats.size(), "");

        for (auto i = 0u; i < sharedData.connectionData.size(); ++i)
        {
            for (auto j = 0u; j < sharedData.connectionData[i].playerCount; ++j)
            {
                if (sharedData.connectionData[i].playerData[j].isCPU)
                {
                    CRO_ASSERT(baseCPUIndex < CPUStats.size(), "");

                    auto cpuIndex = i * ConstVal::MaxPlayers + j;
                    m_cpuProfileIndices[cpuIndex] = baseCPUIndex;
                    //baseCPUIndex += stride;
                    baseCPUIndex = (baseCPUIndex + cro::Util::Random::value(0, 2)) % CPUStats.size();
                }
            }
        }
    }
}

void CPUGolfer::suspend(bool suspend)
{
    //can't suspend during stroke
    //else we'll mess up the CPUs shot
    if (m_state != State::Stroke)
    {
        m_suspended = suspend;
    }
    else
    {
        m_suspended = false;
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
            startThinking(0.1f);
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
            const auto& Stat = CPUStats[m_cpuProfileIndices[m_activePlayer.client * ConstVal::MaxPlayers + m_activePlayer.player]];

            //but not too much if we're near the hole
            float multiplier = absDistance / Clubs[ClubID::PitchWedge].getTargetAtLevel(Stat[CPUStat::Skill]);
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

        auto club = m_inputParser.getClub(); //this should never really be the putter here?
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
        const auto& Stat = CPUStats[m_cpuProfileIndices[m_activePlayer.client * ConstVal::MaxPlayers + m_activePlayer.player]];

        auto club = m_inputParser.getClub();
        float clubDistance = Clubs[club].getTargetAtLevel(Stat[CPUStat::Skill]);
        float diff = targetDistance - clubDistance;

        const auto acceptClub = [&]()
        {
            m_state = State::Aiming;
            m_aimAngle = m_inputParser.getYaw() + (getOffsetValue() * 0.002f);
            m_aimTimer.restart();

            //guestimate power based on club (this gets refined from predictions)
            m_targetPower = std::min(1.f, targetDistance / Clubs[m_clubID].getTargetAtLevel(Stat[CPUStat::Skill]));
            //m_targetPower = std::min(1.f, m_targetPower + (getOffsetValue() / 100.f));
        };

        if (diff < 0)
        {
            m_prevClubID = club;
            m_clubID = club;

            acceptClub();
            return;
        }


        //if the new club has looped switch back and accept it (it's the longest we have)
        if (m_searchDirection == 1 && clubDistance < Clubs[m_prevClubID].getTargetAtLevel(Stat[CPUStat::Skill]))
        {
            sendKeystroke(m_inputParser.getInputBinding().keys[InputBinding::PrevClub]);
            m_clubID = m_prevClubID;

            acceptClub();
            return;
        }

        if (m_searchDirection == -1 && clubDistance > Clubs[m_prevClubID].getTargetAtLevel(Stat[CPUStat::Skill]))
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
        /*if (m_inputParser.inProgress())
        {
            m_state = State::Watching;
            return;
        }*/

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

        const auto& Stat = CPUStats[m_cpuProfileIndices[m_activePlayer.client * ConstVal::MaxPlayers + m_activePlayer.player]];

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
                m_targetPower = m_aimDistance / Clubs[m_clubID].getTarget(m_distanceToPin); //these aren't particularly extended anyway so leave this
                //if (Clubs[m_clubID].target > m_aimDistance)
                {
                    //the further we try to drive the bigger the reduction
                    float amount = 1.f - (static_cast<float>(m_clubID) / ClubID::NineIron);
                    m_targetPower *= (1.f - (amount * 0.2f));
                }
            }
            else
            {
                m_targetPower = (distanceToTarget * 1.12f) / Clubs[m_clubID].getTargetAtLevel(Stat[CPUStat::Skill]);
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
                m_targetPower = std::min(m_targetPower, m_puttingPower);
            }

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


            if (m_activePlayer.terrain == TerrainID::Green
                && (!m_puttFromTee || getSkillIndex() < 4))
            {
                //don't predict when putting, assume we already face the hole
                m_state = State::UpdatePrediction;
            }
            else
            {
                m_targetAngle += getOffsetValue() * 0.001f;
                m_wantsPrediction = true;
            }
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

            if (m_activePlayer.terrain == TerrainID::Green)
            {
                //see if the flag indicator has a better suggestion :)
                m_targetPower = std::min(m_targetPower, m_puttingPower * (1.f + (static_cast<float>(getSkillIndex()) * 0.01f)));
                //auto cpuID = m_cpuProfileIndices[m_activePlayer.client * ConstVal::MaxPlayers + m_activePlayer.player];
                //m_targetPower = std::min(m_targetPower, m_puttingPower * (1.f + (static_cast<float>((CPUStats.size() - cpuID) / 2) * 0.01f)));
            }

            m_targetAccuracy -= (Deviance[devianceOffset] * 0.05f) * devianceMultiplier;

            m_state = State::Stroke;

            startThinking(0.4f);

            auto* msg = postMessage<AIEvent>(MessageID::AIMessage);
            msg->type = AIEvent::EndThink;
        };

        if (m_activePlayer.terrain == TerrainID::Green
            && (getSkillIndex() < 4 || !m_puttFromTee))
        {
            takeShot();
            return;
        }

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
                    const auto& Stat = CPUStats[m_cpuProfileIndices[m_activePlayer.client * ConstVal::MaxPlayers + m_activePlayer.player]];

                    float precDiff = std::sqrt(resultPrecision);
                    float change = (precDiff / Clubs[m_clubID].getTargetAtLevel(Stat[CPUStat::Skill])) / 2.f;
                    change += getOffsetValue() / 50.f;

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
        if (m_fastCPU)
        {
            //just apply our values and raise a stroke message
            m_inputParser.doFastStroke(m_targetAccuracy, m_targetPower);
            m_state = State::Watching;
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
}

void CPUGolfer::calcAccuracy()
{
    //due to input lag 0.08 is actually ~0 ie perfectly accurate

    m_targetAccuracy = 0.08f;

    //old ver
    if (m_skills[getSkillIndex()].strokeAccuracy != 0)
    {
        m_targetAccuracy += static_cast<float>(-m_skills[getSkillIndex()].strokeAccuracy) / 100.f;
    }

    //new ver
    const auto& Stat = CPUStats[m_cpuProfileIndices[m_activePlayer.client * ConstVal::MaxPlayers + m_activePlayer.player]];
    //if (m_puttFromTee)
    //{
    //    m_targetAccuracy += cstat::getOffset(cstat::AccuracyOffsets, Stat[CPUStat::StrokeAccuracy]) * 0.00375f; //scales max value to 0.06
    //}

    //occasionally make really inaccurate
    //... or maybe even perfect? :)
    //old version
    if (m_puttFromTee)
    {
        if (m_skills[getSkillIndex()].mistakeOdds != 0)
        {
            if (cro::Util::Random::value(0, m_skills[getSkillIndex()].mistakeOdds) == 0)
            {
                m_targetAccuracy += static_cast<float>(cro::Util::Random::value(-16, 16)) / 100.f;
            }
        }
    }
    else
    {
        //new version
        std::int32_t puttingOdds = 0;
        /*if (m_activePlayer.terrain == TerrainID::Green)
        {
            float odds = std::min(1.f, glm::length(m_target - m_activePlayer.position) / 12.f) * 2.f;
            puttingOdds = static_cast<std::int32_t>(std::round(odds));
        }*/

        if (cro::Util::Random::value(0, 9) < Stat[CPUStat::MistakeLikelyhood] + puttingOdds)
        {
            m_targetAccuracy += cstat::getOffset(cstat::AccuracyOffsets, Stat[CPUStat::StrokeAccuracy]) / 100.f;
        }
    }


    //to prevent multiple players making the same decision offset the accuracy a small amount
    //based on their client and player number
    //old version
    auto offset = (m_offsetRotation % 4) * 10;
    m_targetAccuracy += (static_cast<float>(cro::Util::Random::value(-(offset / 2), (offset / 2) + 1)) / 500.f) * getOffsetValue();

    if (m_clubID != ClubID::Putter)
    {
        m_targetPower = std::min(1.f, m_targetPower + (1 - (cro::Util::Random::value(0, 1) * 2)) * (static_cast<float>(m_offsetRotation % 4) / 50.f));

        //old ver
        if (m_skills[getSkillIndex()].mistakeOdds != 0)
        {
            if (cro::Util::Random::value(0, m_skills[getSkillIndex()].mistakeOdds) == 0)
            {
                m_targetPower += static_cast<float>(cro::Util::Random::value(-6, 6)) / 1000.f;
            }
        }

        //new ver
        /*if (cro::Util::Random::value(0, 9) < Stat[CPUStat::MistakeLikelyhood])
        {
            m_targetPower += cstat::getOffset(cstat::PowerOffsets, Stat[CPUStat::PowerAccuracy]) * 0.00001f;
        }*/

        m_targetPower = std::min(1.f, m_targetPower);
    }
    else
    {
        //hack to make the ball putt a little further
        m_targetPower = std::min(1.f, m_targetPower * 0.98f /*+ (static_cast<float>(m_skillIndex) * 0.01f)*/);

        //uhhh this actually shortens the power?? leaving this here though because it apparently works...
    }
}

float CPUGolfer::getOffsetValue() const
{
    float multiplier = m_activePlayer.terrain == TerrainID::Green ? smoothstep(0.2f, 0.95f, glm::length(m_target - m_activePlayer.position) / Clubs[ClubID::Putter].getTarget(m_distanceToPin)) : 1.f;

    return static_cast<float>(1 - ((m_offsetRotation % 2) * 2))
        //* static_cast<float>((m_offsetRotation % (m_skills.size() - getSkillIndex())))
        * multiplier;
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

glm::vec3 CPUGolfer::getRandomOffset(glm::vec3 baseDir) const
{
    return glm::vec3(0.f);

    //auto indexOffset = m_cpuProfileIndices[m_activePlayer.client * ConstVal::MaxPlayers + m_activePlayer.player];

    //const auto baseLength = glm::length(baseDir);
    //auto normDir = baseDir / baseLength;

    //const auto& cpuStat = CPUStats[indexOffset];
    //const float maxDist = Clubs[ClubID::Driver].getTargetAtLevel(cpuStat[CPUStat::Skill]);
    //const float resultMultiplier = std::max(1.f, baseLength / maxDist);

    //const float powerOffset = cstat::getOffset(cstat::PowerOffsets, cpuStat[CPUStat::PowerAccuracy]);
    //auto retVal = normDir * powerOffset * (resultMultiplier * resultMultiplier);

    //normDir = { -normDir.z, normDir.y, normDir.x };
    //const float accuracyOffset = cstat::getOffset(cstat::AccuracyOffsets, cpuStat[CPUStat::StrokeAccuracy]);
    //retVal += normDir * accuracyOffset * resultMultiplier;

    //return retVal * (static_cast<float>(indexOffset) / CPUStats.size()) * 0.05f;
}