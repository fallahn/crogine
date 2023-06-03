/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
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
    m_holeCount		(0),
    m_totalHoleCount(18),
    m_holeIndex     (-1)
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
    case sv::MessageID::GolfMessage:
    {
        const auto& data = msg.getData<GolfBallEvent>();
        if (data.type == GolfBallEvent::TurnEnded
            || data.type == GolfBallEvent::Holed)
        {
            auto hole = getCurrentHole();
            auto idx = m_totalHoleCount - m_holeCount;


            if (data.type == GolfBallEvent::TurnEnded)
            {
                static constexpr float MinDistance = 20.f;

                float distance = MinDistance;
                //allow some leighway for longer shots
                float difficulty = std::min(1.f, glm::length(PlayerPosition - data.position) / 250.f);
                difficulty *= 2.f;
                difficulty -= 1.f;

                distance += 10.f * difficulty;
                distance = std::max(0.0001f, distance);

                float score = 1.f - std::max(0.f, std::min(1.f, glm::length(data.position - m_holeData[hole].pin) / distance));
                m_scores[idx] = cro::Util::Easing::easeOutQuad(score) * 100.f; //grade on a curve
            }
            else
            {
                m_scores[idx] = 100.f;
            }
            m_holeCount--;
        }
    }
        break;
    }
}

void DrivingRangeDirector::process(float)
{

}

void DrivingRangeDirector::setHoleCount(std::int32_t count, std::int32_t holeIndex)
{
    m_totalHoleCount = m_holeCount = std::min(count, MaxHoles);

    m_holeIndex = std::min(holeIndex, static_cast<std::int32_t>(m_holeData.size()) - 1);

    //check for random holes, or if we want to play the same over
    if (holeIndex == -1)
    {
        std::shuffle(m_holeData.begin(), m_holeData.end(), cro::Util::Random::rndEngine);
    }
    else
    {
        std::sort(m_holeData.begin(), m_holeData.end(), [](const HoleData& a, const HoleData& b)
            {
                return a.par < b.par;
            });
    }
    std::fill(m_scores.begin(), m_scores.end(), 0.f);
}

std::int32_t DrivingRangeDirector::getCurrentHole() const
{
    return m_holeIndex == -1 ? (m_holeCount % m_holeData.size()) : m_holeIndex;
}