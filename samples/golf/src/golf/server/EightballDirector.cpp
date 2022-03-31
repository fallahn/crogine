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

#include "EightballDirector.hpp"
#include "ServerMessages.hpp"
#include "../BilliardsSystem.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/util/Random.hpp>

namespace
{
    constexpr float BallHeight = 0.03f;
}

EightballDirector::EightballDirector()
    : m_cubeballPosition(0.f, BallHeight, 0.57f),
    m_currentPlayer     (cro::Util::Random::value(0,1)),
    m_firstCollision    (0)
{
    static constexpr glm::vec3 Offset(0.f, BallHeight, 0.0935f - 0.465f); //distance to 8 ball minus distance to spot

    const std::array IDs =
    {
        1,
        11, 5,
        2, 8, 10,
        9, 7, 14, 4,
        6, 15, 13,3,12
    };

    glm::vec3 basePos(0.f);
    const glm::vec3 Spacing(-0.027f, 0.f, -0.0468f);
    std::int32_t cols = 1;
    const std::int32_t rows = 5;

    std::int32_t i = 0;
    for (auto r = 0; r < rows; ++r)
    {
        for (auto c = 0; c < cols; ++c)
        {
            auto pos = basePos;
            pos.x += (c * (-Spacing.x * 2.f));
            m_ballLayout.emplace_back(pos + Offset, IDs[i++]);
        }
        cols++;
        basePos += Spacing;
    }

    std::fill(m_potCount.begin(), m_potCount.end(), 0);
}

//public
void EightballDirector::handleMessage(const cro::Message& msg)
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
            if (data.first == CueBall
                && m_firstCollision == CueBall)
            {
                m_firstCollision = data.second;
            }
        }
    }
}

const std::vector<BallInfo>& EightballDirector::getBallLayout() const
{
    return m_ballLayout;
}

glm::vec3 EightballDirector::getCueballPosition() const
{
    return m_cubeballPosition;
}

std::uint32_t EightballDirector::getTargetID() const
{
    const auto& balls = getScene().getSystem<BilliardsSystem>()->getEntities();
    for (const auto& ball : balls)
    {
        //hmmmm would be nice to get physically closest ball of this type
        if (getStatusType(ball.getComponent<BilliardBall>().id) == m_playerStatus[m_currentPlayer].target)
        {
            return ball.getIndex();
        }
    }

    return std::numeric_limits<std::uint32_t>::max();
}

//private
std::int32_t EightballDirector::getStatusType(std::int8_t ballID) const
{
    switch (ballID)
    {
    default: return PlayerStatus::None;
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
        return PlayerStatus::Stripes;
    case 8:
        return PlayerStatus::Eightball;
        break;
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
        return PlayerStatus::Spots;
    }
}

void EightballDirector::summariseTurn()
{
    bool foulTurn = false;
    bool forfeit = false; //potted the black too soon
    bool eightball = false;

    //look at what was first hit
    if (m_firstCollision == CueBall
        || getStatusType(m_firstCollision) != m_playerStatus[m_currentPlayer].target)
    {
        //either missed or hit the wrong ball first
        foulTurn = m_playerStatus[m_currentPlayer].target != PlayerStatus::None;
    }


    //look at what was pocketed
    for (auto id : m_pocketsThisTurn)
    {
        if (id == CueBall)
        {
            foulTurn = true;
            continue;
        }

        auto status = getStatusType(id);
        if (status != PlayerStatus::Eightball)
        {
            CRO_ASSERT(status != PlayerStatus::None, "");
            m_potCount[status]++;
        }
        else
        {
            eightball = true;
        }
    }

    //if this was the first pot assign the ball type to players
    if (m_playerStatus[m_currentPlayer].target == PlayerStatus::None
        && !m_pocketsThisTurn.empty())
    {
        if (m_potCount[PlayerStatus::Stripes] > m_potCount[PlayerStatus::Spots])
        {
            m_playerStatus[m_currentPlayer].target = PlayerStatus::Stripes;
            m_playerStatus[(m_currentPlayer + 1) % 2].target = PlayerStatus::Spots;
        }
        else
        {
            m_playerStatus[m_currentPlayer].target = PlayerStatus::Spots;
            m_playerStatus[(m_currentPlayer + 1) % 2].target = PlayerStatus::Stripes;
        }
    }
    //else check if the score bumps us up to eightball target
    else if (m_playerStatus[m_currentPlayer].target != PlayerStatus::Eightball)
    {
        if (m_potCount[m_playerStatus[m_currentPlayer].target] == 7)
        {
            m_playerStatus[m_currentPlayer].target = PlayerStatus::Eightball;
        }

        m_playerStatus[m_currentPlayer].score = m_potCount[m_playerStatus[m_currentPlayer].target];
    }

    //check if someone one or lost
    if (forfeit)
    {
        //set this to other player so handling game
        //end in server state reads correct winner ID
        m_currentPlayer = (m_currentPlayer + 1) % 2;

        auto* msg = postMessage<BilliardsEvent>(sv::MessageID::BilliardsMessage);
        msg->type = BilliardsEvent::GameEnded;
    }
    else if (eightball && !foulTurn)
    {
        //make sure 8 ball was last to win
        if (m_playerStatus[m_currentPlayer].target == PlayerStatus::Eightball
            && m_pocketsThisTurn.back() == 8)
        {
            //player wins!
            auto* msg = postMessage<BilliardsEvent>(sv::MessageID::BilliardsMessage);
            msg->type = BilliardsEvent::GameEnded;
        }
    }
    //else move to next turn
    else
    {
        //check if we should move to next player
        if (foulTurn || m_pocketsThisTurn.empty())
        {
            m_currentPlayer = (m_currentPlayer + 1) % 2;
        }

        //delete cueball if a foul so player resets
        if (foulTurn)
        {
            getScene().destroyEntity(getCueball());
        }

        //delay this slightly to allow ball status to
        //sync with clients
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
                msg->type = BilliardsEvent::PlayerSwitched;

                e.getComponent<cro::Callback>().active = false;
                getScene().destroyEntity(e);
            }
        };
    }
}