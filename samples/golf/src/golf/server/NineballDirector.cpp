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

#include "NineballDirector.hpp"
#include "ServerMessages.hpp"

#include "../BilliardsSystem.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/util/Random.hpp>

NineballDirector::NineballDirector()
    : m_currentPlayer   (cro::Util::Random::value(0,1)),
    m_firstCollision    (CueBall),
    m_currentTarget     (1),
    m_turnFlags         (0)
{
    static constexpr glm::vec3 Offset(0.f, BallHeight, 0.0935f - 0.465f); //distance to 9 ball minus distance to spot

    const std::array IDs =
    {
        1,
        2, 3,
        4, 9, 5,
        6, 7,
        8
    };

    const std::array Cols =
    {
        1,2,3,2,1
    };

    glm::vec3 basePos(0.f);
    const glm::vec3 Spacing(-0.027f, 0.f, -0.0468f);
    const std::size_t rows = Cols.size();

    std::int32_t i = 0;
    for (auto r = 0u; r < rows; ++r)
    {
        basePos.x = Spacing.x * (Cols[r] - 1);
        
        for (auto c = 0; c < Cols[r]; ++c)
        {
            auto pos = basePos;
            pos.x += (c * (-Spacing.x * 2.f));
            m_ballLayout.emplace_back(pos + Offset, IDs[i++]);
        }

        basePos.z += Spacing.z;
    }

    std::fill(m_pocketCount.begin(), m_pocketCount.end(), 0);
}

//public
void NineballDirector::handleMessage(const cro::Message& msg)
{
    if (msg.id == sv::MessageID::BilliardsMessage)
    {
        const auto& data = msg.getData<BilliardsEvent>();
        if (data.type == BilliardsEvent::TurnBegan)
        {
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
                    m_firstCollision = data.second;
                }
                else if (data.second == CueBall)
                {
                    m_firstCollision = data.first;
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

                m_pocketCount[data.first]++;

                m_turnFlags |= TurnFlags::Foul;
            }
        }
    }
}

const std::vector<BallInfo>& NineballDirector::getBallLayout() const
{
    return m_ballLayout;
}

glm::vec3 NineballDirector::getCueballPosition() const
{
    return { 0.f, BallHeight, 0.57f };
}

std::uint32_t NineballDirector::getTargetID(glm::vec3) const
{
    const auto& balls = getScene().getSystem<BilliardsSystem>()->getEntities();
    const auto& result = std::find_if(balls.cbegin(), balls.cend(), 
        [&](const cro::Entity& e)
        {
            return e.getComponent<BilliardBall>().id == m_currentTarget; 
        });

    if (result != balls.cend())
    {
        return result->getIndex();
    }

    return 0;
}

//private
void NineballDirector::summariseTurn()
{
    std::int8_t foulType = 127;

    //if we didn't hit the current target first it's a foul
    if (m_firstCollision != m_currentTarget)
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

    //process pocketed balls
    for (auto ball : m_pocketsThisTurn)
    {
        if (ball == CueBall)
        {
            m_turnFlags |= TurnFlags::Foul;
            foulType = BilliardsEvent::CueBallPot;
        }

        if (ball == 9)
        {
            m_turnFlags |= TurnFlags::NineBallPot;
        }

        m_pocketCount[ball]++;
    }

    //update the target - if this doesn't find anything the game has ended anyway
    if (auto result = std::find(m_pocketCount.cbegin() + 1, m_pocketCount.cend(), 0);
        result != m_pocketCount.cend())
    {
        m_currentTarget = static_cast<std::int32_t>(std::distance(m_pocketCount.cbegin(), result));
    }


    if (m_turnFlags & TurnFlags::NineBallPot)
    {
        //if also foul other player wins
        if (m_turnFlags & TurnFlags::Foul)
        {
            //set this to other player so handling game
            //end in server state reads correct winner ID
            m_currentPlayer = (m_currentPlayer + 1) % 2;

            auto* msg = postMessage<BilliardsEvent>(sv::MessageID::BilliardsMessage);
            msg->type = BilliardsEvent::GameEnded;
            msg->first = BilliardsEvent::Forfeit;

            foulType = BilliardsEvent::Forfeit;
        }
        else //else current player wins
        {
            auto* msg = postMessage<BilliardsEvent>(sv::MessageID::BilliardsMessage);
            msg->type = BilliardsEvent::GameEnded;
        }
    }
    else
    {
        //move to next player if necessary
        if ((m_turnFlags & TurnFlags::Foul)
            || (m_pocketsThisTurn.empty()
                && (m_turnFlags & TurnFlags::FreeTable) == 0))
        {
            m_currentPlayer = (m_currentPlayer + 1) % 2;
        }


        if (m_turnFlags & TurnFlags::Foul)
        {
            //delete cueball to reset position
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

                //this should notify clients - in 9 ball
                //we get a free turn but it's technically not
                //a free table as we still have to hit the current
                //target... so it makes sense not to write 'free
                //table' on the client's screen.

                /*if (m_turnFlags == TurnFlags::FreeTable)
                {
                    auto* msg = postMessage<BilliardsEvent>(sv::MessageID::BilliardsMessage);
                    msg->type = BilliardsEvent::Foul;
                    msg->first = BilliardsEvent::FreeTable;
                }*/
            }
        };
    }


    std::uint8_t nextFlags = 0;
    if (m_turnFlags & TurnFlags::Foul)
    {
        auto* msg = postMessage<BilliardsEvent>(sv::MessageID::BilliardsMessage);
        msg->type = BilliardsEvent::Foul;
        msg->first = foulType;

        nextFlags = TurnFlags::FreeTable;
    }

    m_turnFlags = nextFlags;

    //do this last because we want to also check if anything was potted
    //when deciding if it's the next player's turn
    m_pocketsThisTurn.clear();
}