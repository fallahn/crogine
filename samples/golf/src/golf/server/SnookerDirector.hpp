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

#pragma once

#include "BilliardsDirector.hpp"

class SnookerDirector final : public BilliardsDirector
{
public:
    SnookerDirector();

    void handleMessage(const cro::Message&) override;

    const std::vector<BallInfo>& getBallLayout() const override;

    glm::vec3 getCueballPosition() const override;

    std::size_t getCurrentPlayer() const override { return m_currentPlayer; }

    std::uint32_t getTargetID(glm::vec3) const override;

    std::int32_t getScore(std::size_t player) const override { return m_scores[player]; }

private:

    std::vector<BallInfo> m_ballLayout;
    std::size_t m_currentPlayer;
    std::array<std::int32_t, 2u> m_scores = {};

    std::vector<std::int8_t> m_pocketsThisTurn;
    std::uint8_t m_firstCollision;

    struct TurnFlags final
    {
        enum
        {
            Foul = 0x01,
            Forfeit = 0x02
        };
    };
    std::uint8_t m_turnFlags;

    //unlike other games this can have multiple
    //targets so we flag the target bits
    static constexpr std::uint8_t TargetRed = 0x2;
    static constexpr std::uint8_t TargetColour = ~0x3;
    std::uint8_t m_currentTarget;

    std::int8_t m_redBallCount;
    std::int8_t m_lowestColour;
    std::vector<std::int8_t> m_replaceBalls;

    void summariseTurn();

};