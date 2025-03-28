/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include "BilliardsDirector.hpp"

class NineballDirector final : public BilliardsDirector
{
public:
    NineballDirector();

    void handleMessage(const cro::Message&) override;

    const std::vector<BallInfo>& getBallLayout() const override;

    glm::vec3 getCueballPosition() const override;

    std::size_t getCurrentPlayer() const override { return m_currentPlayer; }

    std::uint32_t getTargetID(glm::vec3) const override;

private:

    std::vector<BallInfo> m_ballLayout;
    std::size_t m_currentPlayer;
    std::int8_t m_firstCollision;

    std::int8_t m_currentTarget;
    std::vector<std::int8_t> m_pocketsThisTurn;
    std::array<std::int32_t, 10u> m_pocketCount = {};

    struct TurnFlags final
    {
        enum
        {
            Foul        = 0x1,
            NineBallPot = 0x2,
            FreeTable   = 0x4
        };
    };
    std::uint8_t m_turnFlags;

    void summariseTurn();
};
