/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include "SnookerDirector.hpp"
#include "ServerMessages.hpp"
#include "../BilliardsSystem.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/util/Random.hpp>

namespace
{
    //table length 3.265 / 1.6325
    constexpr float TableLength = 3.265f;
    constexpr float HalfLength = TableLength / 2.f;

    constexpr std::array<glm::vec3, SnookerID::Count> BallPositions =
    {
        glm::vec3(0.f, BallHeight,HalfLength - (TableLength * 0.15f)),
        glm::vec3(0.f),
        glm::vec3(0.3f, BallHeight, HalfLength - (TableLength * 0.2f)), //0.2 * table length
        glm::vec3(0.f, BallHeight, HalfLength - (TableLength * 0.2f)),
        glm::vec3(-0.3f, BallHeight, HalfLength - (TableLength * 0.2f)),
        glm::vec3(0.f, BallHeight, 0.f), //perfect centre
        glm::vec3(0.f, BallHeight, -HalfLength / 2.f), //0.25 table length
        glm::vec3(0.f, BallHeight, -(HalfLength - (TableLength * 0.09f))) //0.09 table length
    };
}

SnookerDirector::SnookerDirector()
    : m_currentPlayer   (cro::Util::Random::value(0, 1)),
    m_firstCollision    (0),
    m_turnFlags         (0),
    m_currentTarget     (TargetRed),
    m_redBallCount      (0),
    m_lowestColour      (SnookerID::Yellow)
{
    std::fill(m_scores.begin(), m_scores.end(), 0);

    //red balls
    static constexpr glm::vec3 Offset(0.f, BallHeight, -HalfLength * 0.55f); //distance to middle ball

    glm::vec3 basePos(0.f);
    const glm::vec3 Spacing(-0.027f, 0.f, -0.0468f);
    std::int32_t cols = 1;
#ifdef CRO_DEBUG_
    const std::int32_t rows = 2;
#else
    const std::int32_t rows = 5;
#endif

    std::int32_t i = 0;
    for (auto r = 0; r < rows; ++r)
    {
        for (auto c = 0; c < cols; ++c)
        {
            auto pos = basePos;
            pos.x += (c * (-Spacing.x * 2.f));
            m_ballLayout.emplace_back(pos + Offset, 1);
            m_redBallCount++;
        }
        cols++;
        basePos += Spacing;
    }

    for (i = 2; i < SnookerID::Count; ++i)
    {
        m_ballLayout.emplace_back(BallPositions[i], i);
    }
}

//public
void SnookerDirector::handleMessage(const cro::Message& msg)
{
    if (msg.id == sv::MessageID::BilliardsMessage)
    {
        const auto& data = msg.getData<BilliardsEvent>();
        if (data.type == BilliardsEvent::TurnBegan)
        {
            m_pocketsThisTurn.clear();
            m_firstCollision = CueBall;
        }
        else if (data.type == BilliardsEvent::TurnEnded)
        {
            summariseTurn();
        }
        else if (data.type == BilliardsEvent::Pocket)
        {
            m_pocketsThisTurn.push_back(data.first);
        }
        else if (data.type == BilliardsEvent::Collision)
        {

            if (m_firstCollision == CueBall)
            {
                if (data.first == CueBall)
                {
                    m_firstCollision = (1 << data.second);
                }
                else if (data.second == CueBall)
                {
                    m_firstCollision = (1 << data.first);
                }
            }
        }
        else if (data.type == BilliardsEvent::OutOfBounds)
        {
            if (data.second == 0)
            {
                //we weren't in a pocket so must have gone off the table

                auto* msg2 = postMessage<BilliardsEvent>(sv::MessageID::BilliardsMessage);
                msg2->type = BilliardsEvent::Foul;
                msg2->first = BilliardsEvent::OffTable;

                m_turnFlags |= TurnFlags::Foul;
                
                switch (data.first)
                {
                default: break;
                case 1:
                    m_redBallCount--;
                    break;
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                    if (m_redBallCount > 0)
                    {
                        m_replaceBalls.push_back(data.second);
                    }
                    break;
                }
            }
        }
    }
}

const std::vector<BallInfo>& SnookerDirector::getBallLayout() const
{
    return m_ballLayout;
}

glm::vec3 SnookerDirector::getCueballPosition() const
{
    return BallPositions[SnookerID::White];
}

std::uint32_t SnookerDirector::getTargetID(glm::vec3) const
{
    if (m_currentTarget == TargetColour)
    {
        return 0;
    }

    const auto& balls = getScene().getSystem<BilliardsSystem>()->getEntities();
    const auto& result = std::find_if(balls.cbegin(), balls.cend(),
        [&](const cro::Entity& e)
        {
            return (1 << e.getComponent<BilliardBall>().id) == m_currentTarget;
        });

    if (result != balls.cend())
    {
        return result->getIndex();
    }

    return 0;
}

//private
void SnookerDirector::summariseTurn()
{
    std::int8_t foulType = 127;

    //if we didn't hit the current target first it's a foul
    if ((m_firstCollision & m_currentTarget) == 0)
    {
        m_turnFlags |= TurnFlags::Foul;
        foulType = BilliardsEvent::WrongBallHit;

        if (m_firstCollision == CueBall)
        {
            //no balls were hit
            foulType = BilliardsEvent::NoBallHit;
        }
    }
    m_firstCollision = CueBall;


    //check the first pocketed ball (if any) matches the target
    if (!m_pocketsThisTurn.empty())
    {
        std::uint8_t firstBall = (1 << m_pocketsThisTurn[0]);
        if ((firstBall & m_currentTarget) == 0)
        {
            m_turnFlags |= TurnFlags::Foul;
            foulType = m_pocketsThisTurn[0] == CueBall ? BilliardsEvent::CueBallPot : BilliardsEvent::WrongBallPot;
        }
        
        if (firstBall == TargetRed)
        {
            m_redBallCount--;

            if ((m_turnFlags & TurnFlags::Foul) == 0)
            {
                m_currentTarget = TargetColour;
                m_scores[m_currentPlayer] += SnookerID::Red;
            }
        }
        else
        {
            if ((m_turnFlags & TurnFlags::Foul) == 0)
            {
                //award colour point
                m_scores[m_currentPlayer] += m_pocketsThisTurn[0];
            }

            //mark for replacement if reds remain or not lowest colour
            if (m_redBallCount > 0
                && m_pocketsThisTurn[0] != CueBall)
            {
                m_replaceBalls.push_back(m_pocketsThisTurn[0]);
                m_currentTarget = TargetRed;
            }
            else
            {
                if (m_pocketsThisTurn[0] == m_lowestColour)
                {
                    m_lowestColour++;
                }
                else if (m_pocketsThisTurn[0] != CueBall)
                {
                    m_replaceBalls.push_back(m_pocketsThisTurn[0]);
                }
                m_currentTarget = (1 << m_lowestColour); //hm this will wrap around at the end of the game, is that ok?
            }
        }
    }

    //process remaining balls - once to check for fouls
    for (auto i = 1u; i < m_pocketsThisTurn.size(); ++i)
    {
        auto ball = m_pocketsThisTurn[i];

        if (ball == CueBall)
        {
            m_turnFlags |= TurnFlags::Foul;
            foulType = BilliardsEvent::CueBallPot;
        }
    }

    //then again to check scores
    for (auto i = 1u; i < m_pocketsThisTurn.size(); ++i)
    {
        auto ball = m_pocketsThisTurn[i];
        if ((1 << ball) == TargetRed)
        {
            m_redBallCount--;
            //m_currentTarget = TargetColour;

            if ((m_turnFlags & TurnFlags::Foul) == 0)
            {
                //award point
                m_scores[m_currentPlayer] += SnookerID::Red;
            }
        }
        else
        {
            if ((m_turnFlags & TurnFlags::Foul) == 0)
            {
                //award point
                m_scores[m_currentPlayer] += ball;
            }

            if (m_redBallCount != 0
                && ball != CueBall)
            {
                //mark ball to be replaced
                m_replaceBalls.push_back(ball);
                //m_currentTarget = TargetRed;
            }
            else
            {
                //set target to lowest remaining colour
                if (ball == m_lowestColour)
                {
                    m_lowestColour++;
                }
                else if (ball != CueBall)
                {
                    m_replaceBalls.push_back(ball);
                }
                m_currentTarget = (1 << m_lowestColour);
            }
        }
    }


    if ((m_turnFlags & TurnFlags::Foul)
        || m_pocketsThisTurn.empty())
    {
        //switch to next player
        m_currentPlayer = (m_currentPlayer + 1) % 2;

        if (m_redBallCount > 0)
        {
            m_currentTarget = TargetRed;
        }
        else
        {
            m_currentTarget = (1 << m_lowestColour);
        }
    }

    //end of game
    if (m_lowestColour == SnookerID::Count)
    {
        m_currentPlayer = m_scores[0] > m_scores[1] ? 0 : 1;


        //TODO does ending on a foul forfeit the game?
        m_turnFlags = 0;
    }


    //delay slightly to allow clients to sync
    auto entity = getScene().createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(1.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime -= dt;

        if (currTime <= 0)
        {
            auto* msg = postMessage<BilliardsEvent>(sv::MessageID::BilliardsMessage);
            msg->type = m_lowestColour == SnookerID::Count ? BilliardsEvent::GameEnded : BilliardsEvent::PlayerSwitched;


            for (auto b : m_replaceBalls)
            {
                //spawn balls
                msg = postMessage<BilliardsEvent>(sv::MessageID::BilliardsMessage);
                msg->type = BilliardsEvent::BallReplaced;
                msg->position = BallPositions[b];
                msg->first = b;
            }
            m_replaceBalls.clear();

            e.getComponent<cro::Callback>().active = false;
            getScene().destroyEntity(e);
        }
    };
    

    if (m_turnFlags & TurnFlags::Foul)
    {
        //resets cueball
        getScene().destroyEntity(getCueball());

        auto* msg = postMessage<BilliardsEvent>(sv::MessageID::BilliardsMessage);
        msg->type = BilliardsEvent::Foul;
        msg->first = foulType;
    }

    m_turnFlags = 0;
    m_pocketsThisTurn.clear();
}