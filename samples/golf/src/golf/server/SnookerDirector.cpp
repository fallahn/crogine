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

SnookerDirector::SnookerDirector()
    : m_currentPlayer   (0),
    m_firstCollision    (0),
    m_turnFlags         (0)
{
    //table length 3.265 / 1.6325
    constexpr float TableLength = 3.265f;
    constexpr float HalfLength = TableLength / 2.f;


    //red balls
    static constexpr glm::vec3 Offset(0.f, BallHeight, -HalfLength * 0.55f); //distance to middle ball

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
            m_ballLayout.emplace_back(pos + Offset, 1);
        }
        cols++;
        basePos += Spacing;
    }

    m_ballLayout.emplace_back(glm::vec3(0.3f, BallHeight, HalfLength - (TableLength * 0.2f)), 2); //0.2 * table length
    m_ballLayout.emplace_back(glm::vec3(0.f, BallHeight, HalfLength - (TableLength * 0.2f)), 3);
    m_ballLayout.emplace_back(glm::vec3(-0.3f, BallHeight, HalfLength - (TableLength * 0.2f)), 4);
    m_ballLayout.emplace_back(glm::vec3(0.f, BallHeight, 0.f), 5); //perfect centre
    m_ballLayout.emplace_back(glm::vec3(0.f, BallHeight, -HalfLength / 2.f), 6); //0.25 table length
    m_ballLayout.emplace_back(glm::vec3(0.f, BallHeight, -(HalfLength - (TableLength * 0.09f))), 7); //0.09 table length
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

                switch (data.first)
                {
                default:
                    m_turnFlags |= TurnFlags::Foul;
                    break;
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:

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
    return { 0.f, BallHeight, 0.87f };
}

std::uint32_t SnookerDirector::getTargetID(glm::vec3) const
{
    return 0;
}

//private
void SnookerDirector::summariseTurn()
{

}