#include "../PacketIDs.hpp"
#include "../BallSystem.hpp"
#include "../Clubs.hpp"
#include "../GameConsts.hpp"

#include "CPUStats.hpp"
#include "ServerMessages.hpp"
#include "ServerGolfState.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/util/Random.hpp>

namespace
{

}

using namespace sv;

void GolfState::handleRules(const GolfBallEvent& data)
{
    if (data.type == GolfBallEvent::TurnEnded)
    {
        //if match/skins play check if our score is even with anyone holed already and forfeit
        switch (m_sharedData.scoreType)
        {
        default: break;
        case ScoreType::Match:
        case ScoreType::Skins:
            if (m_playerInfo[0].holeScore[m_currentHole] >= m_currentBest)
            {
                m_playerInfo[0].distanceToHole = 0;
                m_playerInfo[0].holeScore[m_currentHole]++;
            }
            break;
        case ScoreType::NearestThePin:
            //we may be in the hole so make sure we dont sqrt(0)
            if (data.terrain < TerrainID::Water
                || data.terrain == TerrainID::Hole)
            {
                auto l2 = glm::length(data.position - m_holeData[m_currentHole].pin);
                if (l2 != 0)
                {
                    m_playerInfo[0].distanceScore[m_currentHole] = std::sqrt(l2);
                }
            }
            else
            {
                m_playerInfo[0].distanceScore[m_currentHole] = 666.f;
            }
            break;
        case ScoreType::LongestDrive:
            if (data.terrain == TerrainID::Fairway)
            {
                //rounding here saves on messing with string formatting
                auto d = glm::length(data.position - m_holeData[m_currentHole].tee);
                d = std::round(d * 10.f);
                d /= 10.f;

                m_playerInfo[0].distanceScore[m_currentHole] = d;
            }
            else
            {
                m_playerInfo[0].distanceScore[m_currentHole] = 0.f;
            }
            break;
        }
    }
    else if (data.type == GolfBallEvent::Holed)
    {
        //if we're playing match play or skins then
        //anyone who has a worse score has already lost
        //so set them to finished.
        switch (m_sharedData.scoreType)
        {
        default: break;
        case ScoreType::Match:
        case ScoreType::Skins:
            //if this is skins sudden death then make everyone else the loser
            if (m_skinsFinals)
            {
                for (auto i = 1u; i < m_playerInfo.size(); ++i)
                {
                    m_playerInfo[i].distanceToHole = 0.f;
                    m_playerInfo[i].holeScore[m_currentHole] = m_playerInfo[0].holeScore[m_currentHole] + 1;
                }
            }
            else
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
            break;
        case ScoreType::MultiTarget:
            if (!m_playerInfo[0].targetHit)
            {
                //forfeit the hole
                m_playerInfo[0].holeScore[m_currentHole] = m_scene.getSystem<BallSystem>()->getPuttFromTee() ? 6 : 12;
            }
            break;
        case ScoreType::BattleRoyale:
            //check if all other players already holed and eliminate remaining
            if (m_eliminationStarted)
            {
                auto sortData = m_playerInfo; //don't sort on the live data
                std::sort(sortData.begin(), sortData.end(), [](const PlayerStatus& a, const PlayerStatus& b)
                    {
                        if (!a.eliminated && !b.eliminated)
                        {
                            return a.distanceToHole > b.distanceToHole;
                        }

                        return !a.eliminated;
                    });

                if (sortData[1].distanceToHole == 0)
                {
                    auto eliminee = std::find_if(m_playerInfo.begin(), m_playerInfo.end(), 
                        [&](const PlayerStatus ps)
                        {
                            return ps.client == sortData[0].client && ps.player == sortData[0].player;                    
                        });
                    eliminee->eliminated = true;
                    eliminee->holeScore[m_currentHole] = m_holeData[m_currentHole].puttFromTee ? 6 : 12;
                    eliminee->distanceToHole = 0.f;

                    std::uint16_t packet = ((sortData[0].client << 8) | sortData[0].player);
                    m_sharedData.host.broadcastPacket(PacketID::Elimination, packet, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

                    //LogI << (int)sortData[0].player << " was eliminated" << std::endl;
                }
            }
            break;
        }
    }
    else if (data.type == GolfBallEvent::Gimme)
    {
        switch (m_sharedData.scoreType)
        {
        default: break;
        case ScoreType::Match:
        case ScoreType::Skins:
            //if this is skins sudden death then make everyone else the loser
            if (m_skinsFinals)
            {
                for (auto i = 1u; i < m_playerInfo.size(); ++i)
                {
                    m_playerInfo[i].distanceToHole = 0.f;
                    m_playerInfo[i].holeScore[m_currentHole] = m_playerInfo[0].holeScore[m_currentHole] + 1;
                }
            }
            else
            {
                //check if our score is even with anyone holed already and forfeit
                for (auto i = 1u; i < m_playerInfo.size(); ++i)
                {
                    if (m_playerInfo[i].distanceToHole == 0
                        && m_playerInfo[i].holeScore[m_currentHole] < m_playerInfo[0].holeScore[m_currentHole])
                    {
                        m_playerInfo[0].distanceToHole = 0;
                    }
                }
            }
            break;
        }
    }
}

bool GolfState::summariseRules()
{
    bool gameFinished = false;

    if (m_sharedData.scoreType == ScoreType::BattleRoyale)
    {
        if (m_playerInfo.size() == 1
            || m_playerInfo[1].eliminated)
        {
            //everyone quit or all but player 1 are eliminated
            return true;
        }
        return false;
    }

    if (m_playerInfo.size() > 1)
    {
        auto sortData = m_playerInfo;
        std::sort(sortData.begin(), sortData.end(),
            [&](const PlayerStatus& a, const PlayerStatus& b)
            {
                return a.holeScore[m_currentHole] < b.holeScore[m_currentHole];
            });


        //check if we tied the last hole in skins
        if (m_sharedData.scoreType == ScoreType::Skins
            && !m_skinsFinals
            && ((m_currentHole + 1) == m_holeData.size()))
        {
            if (sortData[0].holeScore[m_currentHole] == sortData[1].holeScore[m_currentHole])
            {
                //this is used to make sure we send the correct score update to the client when this function returns
                //and employ sudden death on the final hole
                m_skinsFinals = true;
                m_scene.getSystem<BallSystem>()->setGimmeRadius(0);

                for (auto& p : m_playerInfo)
                {
                    p.holeScore[m_currentHole] = 0;
                }

                if (m_currentHole)
                {
                    //we might be on a custom course with one
                    //hole in which case don't negate.
                    m_currentHole--;
                }
            }
        }


        //only score if no player tied
        if ((!m_skinsFinals && //we have to check this flag because if it was set m_currentHole was probably modified and the score check is the old hole.
            sortData[0].holeScore[m_currentHole] != sortData[1].holeScore[m_currentHole])
            || (m_skinsFinals && m_currentHole == m_holeData.size() - 1)) //this was the sudden death hole
        {
            auto player = std::find_if(m_playerInfo.begin(), m_playerInfo.end(), [&sortData](const PlayerStatus& p)
                {
                    return p.client == sortData[0].client && p.player == sortData[0].player;
                });

            player->matchWins++;
            player->skins += m_skinsPot;
            m_skinsPot = 1;

            sortData[0].matchWins++; //this is used to test to see if we won the majority of match points

            //send notification packet to clients that player won the hole
            std::uint16_t data = (player->client << 8) | player->player;
            m_sharedData.host.broadcastPacket(PacketID::HoleWon, data, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
        else //increase the skins pot, but only if not repeating the final hole
        {
            if (!m_skinsFinals)
            {
                m_skinsPot++;

                std::uint16_t data = 0xff00 | m_skinsPot;
                m_sharedData.host.broadcastPacket(PacketID::HoleWon, data, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
            }
        }

        //check the match score and end the game if this is the mode we're in
        if (m_sharedData.scoreType == ScoreType::Match)
        {
            std::sort(sortData.begin(), sortData.end(),
                [&](const PlayerStatus& a, const PlayerStatus& b)
                {
                    return a.matchWins > b.matchWins;
                });


            auto remainingHoles = static_cast<std::uint8_t>(m_holeData.size()) - (m_currentHole + 1);
            //if second place can't beat first even if they win all the remaining holes it's game over
            if (sortData[1].matchWins + remainingHoles < sortData[0].matchWins)
            {
                gameFinished = true;
            }
        }
    }

    return gameFinished;
}