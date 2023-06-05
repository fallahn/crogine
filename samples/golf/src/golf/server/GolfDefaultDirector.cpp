/*-----------------------------------------------------------------------

Matt Marchant 2023
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

#include "GolfDefaultDirector.hpp"

GolfDefaultDirector::GolfDefaultDirector(sv::SharedData& sd, std::vector<PlayerStatus>& pi)
    : m_sharedData  (sd),
    m_playerInfo    (pi)
{

}

//public
void GolfDefaultDirector::handleMessage(const cro::Message& msg)
{

}

void GolfDefaultDirector::summariseHole()
{
    //if (m_playerInfo.size() > 1)
    //{
    //    auto sortData = m_playerInfo;
    //    std::sort(sortData.begin(), sortData.end(),
    //        [&](const PlayerStatus& a, const PlayerStatus& b)
    //        {
    //            return a.holeScore[m_currentHole] < b.holeScore[m_currentHole];
    //        });

    //    //only score if no player tied
    //    if (sortData[0].holeScore[m_currentHole] != sortData[1].holeScore[m_currentHole])
    //    {
    //        auto player = std::find_if(m_playerInfo.begin(), m_playerInfo.end(), [&sortData](const PlayerStatus& p)
    //            {
    //                return p.client == sortData[0].client && p.player == sortData[0].player;
    //            });

    //        player->matchWins++;
    //        player->skins += m_skinsPot;
    //        m_skinsPot = 1;

    //        //check the match score and end the game if this is the mode we're in
    //        if (m_sharedData.scoreType == ScoreType::Match)
    //        {
    //            sortData[0].matchWins++;
    //            std::sort(sortData.begin(), sortData.end(),
    //                [&](const PlayerStatus& a, const PlayerStatus& b)
    //                {
    //                    return a.matchWins > b.matchWins;
    //                });


    //            auto remainingHoles = static_cast<std::uint8_t>(m_holeData.size()) - (m_currentHole + 1);
    //            //if second place can't beat first even if they win all the holes it's game over
    //            if (sortData[1].matchWins + remainingHoles < sortData[0].matchWins)
    //            {
    //                gameFinished = true;
    //            }
    //        }

    //        //send notification packet to clients that player won the hole
    //        std::uint16_t data = (player->client << 8) | player->player;
    //        m_sharedData.host.broadcastPacket(PacketID::HoleWon, data, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
    //    }
    //    else
    //    {
    //        m_skinsPot++;

    //        std::uint16_t data = 0xff00 | m_skinsPot;
    //        m_sharedData.host.broadcastPacket(PacketID::HoleWon, data, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
    //    }
    //}
}