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

#pragma once

#include "CommonConsts.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/detail/glm/vec3.hpp>
#include <crogine/gui/GuiClient.hpp>

#include <vector>
#include <array>

namespace cro
{
    class Message;
}

class InputParser;
class CollisionMesh;
struct ActivePlayer;
class CPUGolfer final : public cro::GuiClient
{
public:

    CPUGolfer(InputParser&, const ActivePlayer&, const CollisionMesh&);

    void handleMessage(const cro::Message&);
    void activate(glm::vec3 target, glm::vec3 fallbackTarget, bool puttFromTee);
    void update(float, glm::vec3, float distanceToPin);
    bool thinking() const { return m_thinking; }
    void setPredictionResult(glm::vec3, std::int32_t);
    void setPuttingPower(float p);
    glm::vec3 getTarget() const { return m_target; }

    std::size_t getSkillIndex() const;
    std::int32_t getClubLevel() const; //remember we have to set the active player FIRST

    void setFastCPU(bool fast) { m_fastCPU = fast; }
    void setCPUCount(std::int32_t, const struct SharedStateData&);
    void suspend(bool);
private:

    InputParser& m_inputParser;
    const ActivePlayer& m_activePlayer;
    const CollisionMesh& m_collisionMesh;
    bool m_fastCPU;
    bool m_suspended;
    bool m_puttFromTee;
    float m_distanceToPin;
    glm::vec3 m_target;
    glm::vec3 m_baseTarget; //this is what was originally set before retargetting potentially updates m_target
    glm::vec3 m_fallbackTarget; //we try this to see if its still in front if current prediction goes OOB
    std::int32_t m_retargetCount;

    bool m_predictionUpdated;
    bool m_wantsPrediction;
    glm::vec3 m_predictionResult;
    std::int32_t m_predictionCount;
    std::int32_t m_OOBCount; //number of times prediction returned OOB
    float m_puttingPower; //how much power is predicted by the power bar flag

    std::array<std::int32_t, ConstVal::MaxClients * ConstVal::MaxPlayers> m_cpuProfileIndices = {};

    enum class State
    {
        Inactive,
        CalcDistance, //assess distance to pin from player
        PickingClub, //tries to find suitable club for chosen distance
        Aiming, //account for wind speed and direction
        UpdatePrediction, //dynamic mode correcting for prediction
        Stroke, //power bar is active
        Watching, //ball is mid-flight but turn not yet ended
    }m_state = State::Inactive;


    enum class Skill
    {
        Amateur, //uses the 'old' style
        Dynamic  //adjusts accuracy based on some metric (player XP?)
    };

    struct SkillContext final
    {
        Skill skill = Skill::Amateur;
        float resultTolerance = 2.f; //!< how close to the target the prediction will be before we accept
        float resultTolerancePutt = 0.05f;
        std::int32_t strokeAccuracy = 0; //!< the bigger this number the more likely the accuracy will be off, with 0 being perfect
        std::uint32_t mistakeOdds = 0; //!< the larger the number the smaller the odds of making a 'mistake'

        SkillContext(Skill s, float rt, float rp, std::int32_t sa, std::uint32_t mo)
            : skill(s), resultTolerance(rt), resultTolerancePutt(rp), strokeAccuracy(sa), mistakeOdds(mo) {}
    };
    std::size_t m_skillIndex;
    static const std::array<SkillContext, 6u> m_skills;

    std::int32_t m_clubID;
    std::int32_t m_prevClubID;
    std::int32_t m_searchDirection; //which way we search for a club
    float m_searchDistance;
    float m_targetDistance;

    float m_aimDistance;
    float m_aimAngle; //this is the initial aim angle when the player is placed

    float m_targetAngle;
    float m_targetPower;
    float m_targetAccuracy;

    float m_prevPower;
    float m_prevAccuracy;
    enum class StrokeState
    {
        Power, Accuracy
    }m_strokeState = StrokeState::Power;

    cro::Clock m_aimTimer;
    cro::Clock m_predictTimer;

    bool m_thinking; //not a state per se, rather used to pause/idle while in specific states
    float m_thinkTime;
    void startThinking(float);
    void think(float);

    void calcDistance(float, glm::vec3);
    void pickClub(float);
    void pickClubDynamic(float);
    void aim(float, glm::vec3);
    void aimDynamic(float);
    void updatePrediction(float);
    void stroke(float);

    std::int32_t m_offsetRotation;
    void calcAccuracy();
    float getOffsetValue() const;

    //for each pressed event we need a release event the next frame
    std::vector<cro::Event> m_popEvents;
    void sendKeystroke(std::int32_t, bool autoRelease = true);

    glm::vec3 getRandomOffset(glm::vec3) const;
};
