/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2024
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
    constexpr float NTPPenalty = 10.f; //approx radius of green m_holeData[m_currentHole].distanceToPin;
}

using namespace sv;

void GolfState::handleRules(std::int32_t groupID, const GolfBallEvent& data)
{
    if (m_playerInfo[groupID].playerInfo.empty())
    {
        return;
    }


    //const auto getAllData =
    //    [&]()
    //    {
    //        //concat all the player info and do a single sort/compare on the total results
    //        std::vector<PlayerStatus> allData;
    //        allData.reserve(2 * ConstVal::MaxPlayers);

    //        for (auto& group : m_playerInfo)
    //        {
    //            if (!group.playerInfo.empty())
    //            {
    //                //this is an intentional copy
    //                allData.insert(allData.end(), group.playerInfo.begin(), group.playerInfo.end());
    //            }
    //        }
    //        return allData;
    //    };

    //auto playerFromInfo = 
    //    [&](const PlayerStatus& info) mutable
    //{
    //        auto& pi = m_playerInfo[m_groupAssignments[info.client]].playerInfo;
    //        auto res = std::find_if(pi.begin(), pi.end(), [info](const PlayerStatus& ps) {return ps.player == info.player; });
    //        return res; //ugh this assumes we didn't get pi.end();
    //};

    if (data.type == GolfBallEvent::TurnEnded)
    {
        //if match/skins play check if our score is even with anyone holed already and forfeit
        switch (m_sharedData.scoreType)
        {
        default: break;
        case ScoreType::Elimination:
            if (data.terrain != TerrainID::Hole)
            {
                if (m_playerInfo[groupID].playerInfo[0].holeScore[m_currentHole] >= m_holeData[m_currentHole].par -1) //never going to finish under par
                {
                    m_playerInfo[groupID].playerInfo[0].skins--;
                    std::uint16_t packet = ((m_playerInfo[groupID].playerInfo[0].client << 8) | m_playerInfo[groupID].playerInfo[0].player);
                    auto packetID = PacketID::LifeLost;

                    //if no lives left, eliminate
                    if (m_playerInfo[groupID].playerInfo[0].skins == 0)
                    {
                        m_playerInfo[groupID].playerInfo[0].eliminated = true;
                        packetID = PacketID::Elimination;
                    }
                    m_sharedData.host.broadcastPacket(packetID, packet, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                    
                    //forfeit the rest of the hole
                    m_playerInfo[groupID].playerInfo[0].holeScore[m_currentHole]++;
                    m_playerInfo[groupID].playerInfo[0].position = m_holeData[m_currentHole].pin;
                    m_playerInfo[groupID].playerInfo[0].distanceToHole = 0;
                    m_playerInfo[groupID].playerInfo[0].matchWins = 1; //flags this is a life lost
                }
            }
            break;
        case ScoreType::Match:
        case ScoreType::Skins:
            //forfeit this hole if we can't beat the best score
            if (m_playerInfo[groupID].playerInfo[0].holeScore[m_currentHole] >= m_currentBest)
            {
                m_playerInfo[groupID].playerInfo[0].distanceToHole = 0;
                m_playerInfo[groupID].playerInfo[0].holeScore[m_currentHole]++;
            }
            break;
        case ScoreType::NearestThePin:
            //we may be in the hole so make sure we dont sqrt(0)
            if ((data.terrain < TerrainID::Water
                && data.terrain != TerrainID::Hole)
                || data.terrain == TerrainID::Stone)
            {
                auto l2 = glm::length2(data.position - m_holeData[m_currentHole].pin);
                if (l2 != 0)
                {
                    m_playerInfo[groupID].playerInfo[0].distanceScore[m_currentHole] = std::sqrt(l2);
                }
            }
            else
            {
                m_playerInfo[groupID].playerInfo[0].distanceScore[m_currentHole] = NTPPenalty;
            }
            break;
        case ScoreType::LongestDrive:
            if (data.terrain == TerrainID::Fairway)
            {
                //rounding here saves on messing with string formatting
                auto d = glm::length(data.position - m_holeData[m_currentHole].tee);
                d = std::round(d * 10.f);
                d /= 10.f;

                m_playerInfo[groupID].playerInfo[0].distanceScore[m_currentHole] = d;
            }
            else
            {
                m_playerInfo[groupID].playerInfo[0].distanceScore[m_currentHole] = 0.f;
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
        {
            //auto allData = getAllData();
            auto& playerInfo = m_playerInfo[groupID].playerInfo;
            //if this is skins sudden death then make everyone else the loser
            if (m_skinsFinals)
            {
                //skins and match play are always in a single group
                //so we don't consider other players here
                const auto& currPlayer = playerInfo[0];
                for (auto i = 1; i < playerInfo.size(); ++i)
                {
                    playerInfo[i].distanceToHole = 0.f;
                    playerInfo[i].holeScore[m_currentHole] = currPlayer.holeScore[m_currentHole] + 1;
                }
                /*for (const auto& d : allData)
                {
                    if (d != currPlayer)
                    {
                        auto player = playerFromInfo(d);
                        player->distanceToHole = 0.f;
                        player->holeScore[m_currentHole] = currPlayer.holeScore[m_currentHole] + 1;
                    }
                }*/
            }
            else
            {
                //eliminate anyone who can't beat this score
                //again - assume all players exist in the dame group
                const auto& currPlayer = playerInfo[0];
                for (auto i = 1; i < playerInfo.size(); ++i)
                {
                    if (playerInfo[i].holeScore[m_currentHole] >= currPlayer.holeScore[m_currentHole])
                    {
                        if (playerInfo[i].distanceToHole > 0) //not already holed
                        {
                            playerInfo[i].distanceToHole = 0.f;
                            playerInfo[i].holeScore[m_currentHole]++;
                        }
                    }
                }

                //for (auto& d : allData)
                //{
                //    if (d != currPlayer &&
                //        d.holeScore[m_currentHole] >= currPlayer.holeScore[m_currentHole])
                //    {
                //        if (d.distanceToHole > 0) //not already holed
                //        {
                //            auto player = playerFromInfo(d);
                //            player->distanceToHole = 0.f;
                //            player->holeScore[m_currentHole]++;

                //            //also update the sorted data as we rely on the results below
                //            d.distanceToHole = 0.f;
                //            d.holeScore[m_currentHole]++; //therefore they lose a stroke and don't draw
                //        }
                //    }
                //}

                //if this is the second hole and it has the same as the current best
                //force a draw by eliminating anyone who can't beat it
                if (currPlayer.holeScore[m_currentHole] == m_currentBest)
                {
                    for (auto i = 1; i < playerInfo.size(); ++i)
                    {
                        if (playerInfo[i].holeScore[m_currentHole] + 1 >= m_currentBest)
                        {
                            playerInfo[i].distanceToHole = 0.f;
                            playerInfo[i].holeScore[m_currentHole] = std::min(m_currentBest, std::uint8_t(playerInfo[i].holeScore[m_currentHole] + 1));
                        }
                    }

                    /*for (auto& d : allData)
                    {
                        if (d != currPlayer &&
                            d.holeScore[m_currentHole] + 1 >= m_currentBest)
                        {
                            if (d.distanceToHole > 0)
                            {
                                auto player = playerFromInfo(d);
                                player->distanceToHole = 0.f;
                                player->holeScore[m_currentHole] = std::min(m_currentBest, std::uint8_t(d.holeScore[m_currentHole] + 1));
                            }
                        }
                    }*/
                }
            }
        }
            break;
        case ScoreType::MultiTarget:
            if (!m_playerInfo[groupID].playerInfo[0].targetHit)
            {
                //forfeit the hole
                m_playerInfo[groupID].playerInfo[0].holeScore[m_currentHole] = m_scene.getSystem<BallSystem>()->getPuttFromTee() ? 6 : 12;
            }
            break;
        case ScoreType::Elimination:
            //check player score and update lives if necessary
            if (m_playerInfo[groupID].playerInfo[0].holeScore[m_currentHole] >= m_holeData[m_currentHole].par)
            {
                m_playerInfo[groupID].playerInfo[0].skins--;
                std::uint16_t packet = ((m_playerInfo[groupID].playerInfo[0].client << 8) | m_playerInfo[groupID].playerInfo[0].player);
                auto packetID = PacketID::LifeLost;

                //if no lives left, eliminate
                if (m_playerInfo[groupID].playerInfo[0].skins == 0)
                {
                    m_playerInfo[groupID].playerInfo[0].eliminated = true;
                    packetID = PacketID::Elimination;
                }
                m_sharedData.host.broadcastPacket(packetID, packet, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

                m_playerInfo[groupID].playerInfo[0].matchWins = 1; //marks player as having just lost a life
            }
            else if (m_playerInfo[groupID].playerInfo[0].holeScore[m_currentHole] < m_holeData[m_currentHole].par - 1)
            {
                m_playerInfo[groupID].playerInfo[0].skins++;
                std::uint16_t packet = ((m_playerInfo[groupID].playerInfo[0].client << 8) | m_playerInfo[groupID].playerInfo[0].player);
                m_sharedData.host.broadcastPacket(PacketID::LifeGained, packet, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
            }

            break;
        case ScoreType::NearestThePin:
            m_playerInfo[groupID].playerInfo[0].distanceScore[m_currentHole] = NTPPenalty;// m_holeData[m_currentHole].distanceToPin / 2.f;
            m_playerInfo[groupID].playerInfo[0].holeScore[m_currentHole] = MaxNTPStrokes + 1;
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
        {
            //auto allData = getAllData();
            auto& playerInfo = m_playerInfo[groupID].playerInfo;

            //if this is skins sudden death then make everyone else the loser
            //ACTUALLY gimmes should never occur on sudden death rounds
            if (m_skinsFinals)
            {
                //skins / match play should always be in the same group
                const auto& currPlayer = m_playerInfo[groupID].playerInfo[0];
                for (auto i = 1; i < playerInfo.size(); ++i)
                {
                    playerInfo[i].distanceToHole = 0.f;
                    playerInfo[i].holeScore[m_currentHole] = currPlayer.holeScore[m_currentHole] + 1;
                }

                /*for (auto& d : allData)
                {
                    if (d != currPlayer)
                    {
                        auto player = playerFromInfo(d);
                        player->distanceToHole = 0.f;
                        player->holeScore[m_currentHole] = currPlayer.holeScore[m_currentHole] + 1;
                    }
                }*/
            }
            else
            {
                //check if our score is even with anyone holed already and forfeit
                auto& currPlayer = m_playerInfo[groupID].playerInfo[0];
                for (auto i = 1; i < playerInfo.size(); ++i)
                {
                    if (playerInfo[i].distanceToHole == 0
                        && playerInfo[i].holeScore[m_currentHole] < currPlayer.holeScore[m_currentHole])
                    {
                        currPlayer.distanceToHole = 0.f;
                    }
                }
                /*for (auto& d : allData)
                {
                    if (d != currPlayer
                        && d.distanceToHole == 0
                        && d.holeScore[m_currentHole] < currPlayer.holeScore[m_currentHole])
                    {
                        m_playerInfo[groupID].playerInfo[0].distanceToHole = 0.f;
                    }
                }*/
            }
            break;
        }
        }
    }
}

bool GolfState::summariseRules()
{
    //concat all the player info and do a single sort/compare on the total results
    std::vector<PlayerStatus> sortData;
    sortData.reserve(2 * ConstVal::MaxPlayers);

    for (auto& group : m_playerInfo)
    {
        if (!group.playerInfo.empty())
        {
            //this is an intentional copy
            sortData.insert(sortData.end(), group.playerInfo.begin(), group.playerInfo.end());
        }
    }    
    
    
    //check sort data to see if we have 1 remaining player
    if (m_sharedData.scoreType == ScoreType::Elimination)
    {
        std::sort(sortData.begin(), sortData.end(),
            [](const PlayerStatus& a, const PlayerStatus& b)
            {
                return !a.eliminated;
            });

        if (sortData.size() == 1
            || sortData[1].eliminated)
        {
            //everyone quit or all but player 1 are eliminated
            return true;
        }
        return false;
    }

    bool gameFinished = false;

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

            for (auto& group : m_playerInfo)
            {
                for (auto& p : group.playerInfo)
                {
                    p.holeScore[m_currentHole] = 0;
                }
            }

            if (m_currentHole)
            {
                //we might be on a custom course with one
                //hole in which case don't negate.
                m_currentHole--;
            }
        }
    }

    //hmm this used to apply to ALL score types before 
    //elimination mode shanghaid some of the playerInfo fields
    //so... are these the only two score types relevant here to
    //do we need to allow all but exclude elimination (and maybe NTP)?
    if (m_sharedData.scoreType == ScoreType::Skins
        || m_sharedData.scoreType == ScoreType::Match)
    {
        //only score if no player tied
        if ((!m_skinsFinals && //we have to check this flag because if it was set m_currentHole was probably modified and the score check is the old hole.
            sortData[0].holeScore[m_currentHole] != sortData[1].holeScore[m_currentHole])
            || (m_skinsFinals && m_currentHole == m_holeData.size() - 1)) //this was the sudden death hole
        {
            for (auto& group : m_playerInfo)
            {
                auto player = std::find_if(group.playerInfo.begin(), group.playerInfo.end(),
                    [&sortData](const PlayerStatus& p)
                    {
                        return p.client == sortData[0].client && p.player == sortData[0].player;
                    });

                if (player != group.playerInfo.end())
                {
                    player->matchWins++;
                    player->skins += m_skinsPot;
                    m_skinsPot = 1;

                    sortData[0].matchWins++; //this is used to test to see if we won the majority of match points

                    //send notification packet to clients that player won the hole
                    std::uint16_t data = (player->client << 8) | player->player;
                    m_sharedData.host.broadcastPacket(PacketID::HoleWon, data, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

                    break;
                }
            }
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
    
    return gameFinished;
}