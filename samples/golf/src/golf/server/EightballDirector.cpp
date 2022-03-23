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

#include <crogine/util/Random.hpp>

namespace
{
    constexpr float BallHeight = 0.03f;
}

EightballDirector::EightballDirector()
    : m_cubeballPosition(0.f, BallHeight, 0.56f),
    m_currentPlayer     (cro::Util::Random::value(0,1))
{
    static constexpr glm::vec3 Offset(0.f, BallHeight, 0.0935f - 0.55f); //distance to 8 ball minus distance to spot

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
}

//public
void EightballDirector::handleMessage(const cro::Message& msg)
{
    if (msg.id == sv::MessageID::BilliardsMessage)
    {
        const auto& data = msg.getData<BilliardsEvent>();
        if (data.type == BilliardsEvent::TurnBegan)
        {
            //TODO start tracking ball events for current player
        }
        else if (data.type == BilliardsEvent::TurnEnded)
        {
            //TODO check foul status/pot status etc
            m_currentPlayer = (m_currentPlayer + 1) % 2;

            auto* msg2 = postMessage<BilliardsEvent>(sv::MessageID::BilliardsMessage);
            msg2->type = BilliardsEvent::PlayerSwitched;
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