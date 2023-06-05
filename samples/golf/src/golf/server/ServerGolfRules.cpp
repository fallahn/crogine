#include "../PacketIDs.hpp"

#include "ServerMessages.hpp"
#include "ServerGolfState.hpp"

using namespace sv;
void GolfState::handleDefaultRules(const GolfBallEvent& data)
{
    if (data.type == GolfBallEvent::TurnEnded)
    {
        //if match/skins play check if our score is even with anyone holed already and forfeit
        if (m_sharedData.scoreType != ScoreType::Stroke)
        {
            if (m_playerInfo[0].holeScore[m_currentHole] >= m_currentBest)
            {
                m_playerInfo[0].distanceToHole = 0;
                m_playerInfo[0].holeScore[m_currentHole]++;
            }
        }
    }
    else if (data.type == GolfBallEvent::Holed)
    {
        //if we're playing match play or skins then
                    //anyone who has a worse score has already lost
                    //so set them to finished.
        if (m_sharedData.scoreType != ScoreType::Stroke)
        {
            //eliminate anyone who can't beat this score
            for (auto i = 1u; i < m_playerInfo.size(); ++i)
            {
                if ((m_playerInfo[i].holeScore[m_currentHole]) >=
                    m_playerInfo[0].holeScore[m_currentHole])
                {
                    if (m_playerInfo[i].distanceToHole > 0) //not already holed
                    {
                        m_playerInfo[i].distanceToHole = 0.f;
                        m_playerInfo[i].holeScore[m_currentHole]++; //therefore they lose a stroke and don't draw
                    }
                }
            }

            //if this is the second hole and it has the same as the current best
            //force a draw by eliminating anyone who can't beat it
            if (m_playerInfo[0].holeScore[m_currentHole] == m_currentBest)
            {
                for (auto i = 1u; i < m_playerInfo.size(); ++i)
                {
                    if ((m_playerInfo[i].holeScore[m_currentHole] + 1) >=
                        m_currentBest)
                    {
                        if (m_playerInfo[i].distanceToHole > 0)
                        {
                            m_playerInfo[i].distanceToHole = 0.f;
                            m_playerInfo[i].holeScore[m_currentHole] = std::min(m_currentBest, std::uint8_t(m_playerInfo[i].holeScore[m_currentHole] + 1));
                        }
                    }
                }
            }
        }
    }
    else if (data.type == GolfBallEvent::Gimme)
    {
        //if match/skins play check if our score is even with anyone holed already and forfeit
        if (m_sharedData.scoreType != ScoreType::Stroke)
        {
            for (auto i = 1u; i < m_playerInfo.size(); ++i)
            {
                if (m_playerInfo[i].distanceToHole == 0
                    && m_playerInfo[i].holeScore[m_currentHole] < m_playerInfo[0].holeScore[m_currentHole])
                {
                    m_playerInfo[0].distanceToHole = 0;
                }
            }
        }
    }
}

bool GolfState::summariseDefaultRules()
{
    bool gameFinished = false;

    if (m_playerInfo.size() > 1)
    {
        auto sortData = m_playerInfo;
        std::sort(sortData.begin(), sortData.end(),
            [&](const PlayerStatus& a, const PlayerStatus& b)
            {
                return a.holeScore[m_currentHole] < b.holeScore[m_currentHole];
            });

        //only score if no player tied
        if (sortData[0].holeScore[m_currentHole] != sortData[1].holeScore[m_currentHole])
        {
            auto player = std::find_if(m_playerInfo.begin(), m_playerInfo.end(), [&sortData](const PlayerStatus& p)
                {
                    return p.client == sortData[0].client && p.player == sortData[0].player;
                });

            player->matchWins++;
            player->skins += m_skinsPot;
            m_skinsPot = 1;

            //check the match score and end the game if this is the mode we're in
            if (m_sharedData.scoreType == ScoreType::Match)
            {
                sortData[0].matchWins++;
                std::sort(sortData.begin(), sortData.end(),
                    [&](const PlayerStatus& a, const PlayerStatus& b)
                    {
                        return a.matchWins > b.matchWins;
                    });


                auto remainingHoles = static_cast<std::uint8_t>(m_holeData.size()) - (m_currentHole + 1);
                //if second place can't beat first even if they win all the holes it's game over
                if (sortData[1].matchWins + remainingHoles < sortData[0].matchWins)
                {
                    gameFinished = true;
                }
            }

            //send notification packet to clients that player won the hole
            std::uint16_t data = (player->client << 8) | player->player;
            m_sharedData.host.broadcastPacket(PacketID::HoleWon, data, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
        else
        {
            m_skinsPot++;

            std::uint16_t data = 0xff00 | m_skinsPot;
            m_sharedData.host.broadcastPacket(PacketID::HoleWon, data, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
    }

    return gameFinished;
}