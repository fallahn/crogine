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

#include "DrivingRangeDirector.hpp"
#include "MessageIDs.hpp"
#include "server/ServerMessages.hpp"
#include "CommandIDs.hpp"

#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>

#include <crogine/util/Random.hpp>
#include <crogine/util/Easings.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

DrivingRangeDirector::DrivingRangeDirector(std::vector<HoleData>& hd)
    : m_holeData	(hd),
    m_holeCount		(18),
    m_totalHoleCount(18)
{

}

//public
void DrivingRangeDirector::handleMessage(const cro::Message& msg)
{
    switch (msg.id)
    {
    default: break;
    case MessageID::GolfMessage:
    {
        const auto& data = msg.getData<GolfEvent>();
        if (data.type == GolfEvent::HitBall)
        {
            //actual ball whacking is done in the driving state
            //as it needs to read the input parser
        }
    }
        break;
    case sv::MessageID::BallMessage:
    {
        const auto& data = msg.getData<BallEvent>();
        if (data.type == BallEvent::TurnEnded
            || data.type == BallEvent::Foul)
        {
            auto hole = getCurrentHole();
            auto idx = m_totalHoleCount - m_holeCount;

            static constexpr float MinDistance = 20.f;

            float distance = MinDistance;
            //allow some leighway for longer shots
            float difficulty = std::min(1.f, glm::length(PlayerPosition - data.position) / 250.f);
            difficulty *= 2.f;
            difficulty -= 1.f;

            distance += 10.f * difficulty;

            float score = 1.f - std::min(1.f, glm::length(data.position - m_holeData[hole].pin) / distance);
            m_scores[idx] = cro::Util::Easing::easeOutQuad(score) * 100.f; //grade on a curve

            m_holeCount--;
        }
    }
        break;
    }
}

void DrivingRangeDirector::process(float)
{

}

void DrivingRangeDirector::setHoleCount(std::int32_t count)
{
    m_totalHoleCount = m_holeCount = std::min(count, MaxHoles);
    std::shuffle(m_holeData.begin(), m_holeData.end(), cro::Util::Random::rndEngine);

    std::fill(m_scores.begin(), m_scores.end(), 0.f);

    
}

std::int32_t DrivingRangeDirector::getCurrentHole() const
{
    return m_holeCount % m_holeData.size();
}