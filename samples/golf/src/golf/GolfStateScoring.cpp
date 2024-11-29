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

#include "GolfState.hpp"
#include "PacketIDs.hpp"
#include "MessageIDs.hpp"
#include "Clubs.hpp"

#include "XPAwardStrings.hpp"

using namespace cl;

void GolfState::updateHoleScore(std::uint16_t data)
{
    switch (m_sharedData.scoreType)
    {
    default: break;
    case ScoreType::NearestThePin:
    {
        auto client = (data & 0xff00) >> 8;
        auto player = (data & 0x00ff);

        client = std::clamp(client, 0, static_cast<std::int32_t>(ConstVal::MaxClients - 1));
        player = std::clamp(player, 0, static_cast<std::int32_t>(ConstVal::MaxPlayers - 1));

        //award XP
        if(m_sharedData.localConnectionData.connectionID == client
            && !m_sharedData.localConnectionData.playerData[player].isCPU)
        {
            auto active = Achievements::getActive();
            Achievements::setActive(m_allowAchievements);
            Social::awardXP(50, XPStringID::HoleWon);
            Achievements::setActive(active);
        }
        showNotification(m_sharedData.connectionData[client].playerData[player].name + " won the hole!");

        auto* msg = getContext().appInstance.getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
        msg->type = GolfEvent::HoleWon;
        msg->player = player;
        msg->client = client;
    }
        break;
    case ScoreType::Skins:
    case ScoreType::Match:
    {
        auto client = (data & 0xff00) >> 8;
        auto player = (data & 0x00ff);

        if (client == 255)
        {
            if (m_sharedData.scoreType == ScoreType::Skins)
            {
                showNotification("Skins pot increased to " + std::to_string(player));

                cro::Command cmd;
                cmd.targetFlags = CommandID::UI::ScoreTitle;
                cmd.action = [&, player](cro::Entity e, float)
                    {
                        auto str = m_courseTitle + " - Skins - Pot: " + std::to_string(player);
                        e.getComponent<cro::Text>().setString(str);
                        centreText(e);
                    };
                m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
            }
            else
            {
                showNotification("Hole was drawn");
            }

            auto* msg = getContext().appInstance.getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
            msg->type = GolfEvent::HoleDrawn;
        }
        else
        {
            client = std::clamp(client, 0, static_cast<std::int32_t>(ConstVal::MaxClients - 1));
            player = std::clamp(player, 0, static_cast<std::int32_t>(ConstVal::MaxPlayers - 1));

            auto txt = m_sharedData.connectionData[client].playerData[player].name;
            if (m_sharedData.scoreType == ScoreType::Match)
            {
                txt += " has won the hole!";
            }
            else
            {
                txt += " has won the pot!";
                
                cro::Command cmd;
                cmd.targetFlags = CommandID::UI::ScoreTitle;
                cmd.action = [&, player](cro::Entity e, float)
                    {
                        auto str = m_courseTitle + " - Skins - Pot: 1";
                        e.getComponent<cro::Text>().setString(str);
                        centreText(e);
                    };
                m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            }
            showNotification(txt);

            auto* msg = getContext().appInstance.getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
            msg->type = GolfEvent::HoleWon;
            msg->player = player;
            msg->client = client;
        }
    }
        break;
    }
}

void GolfState::updateLeaderboardScore(bool& personalBest, cro::String& bestString)
{
    if (m_sharedData.gameMode == GameMode::Tutorial)
    {
        bestString = "Tutorial Complete";
        return;
    }

    else if (m_sharedData.scoreType == ScoreType::Stroke
        && Social::getLeaderboardsEnabled())
    {
        const auto& connectionData = m_sharedData.connectionData[m_sharedData.clientConnection.connectionID];
        for (auto k = 0u; k < connectionData.playerCount; ++k)
        {
            if (!connectionData.playerData[k].isCPU)
            {
                std::int32_t score = connectionData.playerData[k].score;
                auto best = Social::getPersonalBest(m_sharedData.mapDirectory, m_sharedData.holeCount);

                if (score < best
                    || best == 0)
                {
                    personalBest = true;
                }

                if (!personalBest)
                {
                    //see if we at least got a new monthly score
                    best = Social::getMonthlyBest(m_sharedData.mapDirectory, m_sharedData.holeCount);

                    if (score < best
                        || best == 0)
                    {
                        personalBest = true;
                        bestString = "MONTHLY BEST!";
                    }
                }

                //calculate the player's stableford score
                std::int32_t stableford = 0;
                for (auto i = 0u; i < m_holeData.size(); ++i)
                {
                    std::int32_t holeScore = static_cast<std::int32_t>(connectionData.playerData[k].holeScores[i]) - m_holeData[i].par;
                    stableford += std::max(0, 2 - holeScore);
                }

                Social::insertScore(m_sharedData.mapDirectory, m_sharedData.holeCount, score, stableford);
                break;
            }
        }
    }
    //else
    //{
    //    cro::Logger::log("LEADERBOARD did not insert score: Score Type is not Stroke.\n", cro::Logger::Type::Info, cro::Logger::Output::File);
    //}
}

void GolfState::updateTournament(bool playerWon)
{
    //TODO we might want to throw all this into an async func
    //as long as it completes before the loading screen for menu
    //finished (hold quitting state until future returns?)
    if (m_sharedData.gameMode == GameMode::Tournament)
    {
        ScoreCalculator scoreCalc(Club::getClubLevel());
        std::vector<bool> overPar;
        for (auto i = 0u; i < m_holeData.size(); ++i)
        {
            overPar.push_back(m_sharedData.connectionData[0].playerData[0].holeScores[i] > m_holeData[i].par);
        }

        const auto& league = League(LeagueRoundID::Club, m_sharedData);
        auto& tournament = m_sharedData.tournaments[m_sharedData.activeTournament];

        const auto updateCPUScores =
            [&](std::int32_t* tierIn, std::size_t playerCount, std::int32_t* tierOut, std::int32_t tierOffset)
            {
                std::int32_t k = 0;
                for (auto i = 0u; i < playerCount; i += 2)
                {
                    //skip ourself and current opponent
                    if (tierOffset > -1 && //assuming we're even in the tier
                        (tierIn[i] == -1
                        || tierIn[i] == tournament.opponentStats.nameIndex))
                    {
                        //insert the winner into the output tier
                        tierOut[k++] = playerWon ? -1 : tournament.opponentStats.nameIndex;
                    }
                    else
                    {
                        const auto& player0 = league.getPlayer(tierIn[i]);
                        const auto& player1 = league.getPlayer(tierIn[i + 1]);

                        HoleScores scores0 = {};
                        HoleScores scores1 = {};

                        std::fill(scores0.begin(), scores0.end(), 0);
                        std::fill(scores1.begin(), scores1.end(), 0);

                        std::int32_t total0 = 0;
                        std::int32_t total1 = 0;

                        const auto holeCount = tournament.round < 3 ? 9 : 18;

                        for (auto j = 0; j < holeCount; ++j)
                        {
                            //check if we're front or back 9
                            const auto cpuOffset = i < (playerCount / 2) ? 0 : 1;

                            if (cpuOffset == tierOffset)
                            {
                                //we're in the same half of the tier as the player
                                scoreCalc.calculate(player0, j, m_holeData[j].par, overPar[j], scores0);
                                scoreCalc.calculate(player1, j, m_holeData[j].par, overPar[j], scores1);

                                total0 += scores0[j];
                                total1 += scores1[j];
                            }
                            //else we read the par values from the tables
                            else
                            {
                                const auto hole = j + (cpuOffset * 9);
                                const auto par = TierPars[m_sharedData.activeTournament][tournament.round][hole];
                                scoreCalc.calculate(player0, hole, par, false, scores0);
                                scoreCalc.calculate(player1, hole, par, false, scores1);

                                total0 += scores0[hole];
                                total1 += scores1[hole];
                            }
                        }

                        //LogI << "Totals: " << total0 << ", " << total1 << std::endl;
                        if (total0 < total1)
                        {
                            //LogI << "CPU 0 Won (player " << player0.nameIndex << ")" << std::endl;
                            tierOut[k++] = player0.nameIndex;
                        }
                        else
                        {
                            //let's not care about draws
                            //LogI << "CPU 1 Won (player " << player1.nameIndex << ")" << std::endl;
                            tierOut[k++] = player1.nameIndex;
                        }
                    }
                }
                tournament.round++;
            };


        //iterate all CPU for current round to set next round standing
        const auto iterateScores = [&](std::int32_t round)
            {
                switch (round)
                {
                default:
                    LogE << "Tournament round " << round << ": invalid round!" << std::endl;
                    return;
                case 0:
                {
                    auto& tierIn = tournament.tier0;
                    auto& tierOut = tournament.tier1;
                    auto tierOffset = -1; //-1 if player not in this tier (ie lost a round)
                    if (auto pos = std::find(tierIn.begin(), tierIn.end(), -1); pos != tierIn.end())
                    {
                        tierOffset = std::distance(tierIn.begin(), pos) < (tierIn.size() / 2u) ? 0 : 1;
                    }

                    updateCPUScores(tierIn.data(), tierIn.size(), tierOut.data(), tierOffset);
                }
                break;
                case 1:
                {
                    auto& tierIn = tournament.tier1;
                    auto& tierOut = tournament.tier2;
                    auto tierOffset = -1;
                    if (auto pos = std::find(tierIn.begin(), tierIn.end(), -1); pos != tierIn.end())
                    {
                        tierOffset = std::distance(tierIn.begin(), pos) < (tierIn.size() / 2u) ? 0 : 1;
                    }

                    updateCPUScores(tierIn.data(), tierIn.size(), tierOut.data(), tierOffset);
                }
                break;
                case 2:
                {
                    auto& tierIn = tournament.tier2;
                    auto& tierOut = tournament.tier3;
                    auto tierOffset = -1;
                    if (auto pos = std::find(tierIn.begin(), tierIn.end(), -1); pos != tierIn.end())
                    {
                        tierOffset = std::distance(tierIn.begin(), pos) < (tierIn.size() / 2u) ? 0 : 1;
                    }

                    updateCPUScores(tierIn.data(), tierIn.size(), tierOut.data(), tierOffset);
                }
                break;
                case 3:
                {
                    const auto tierOffset = (tournament.tier3[0] == -1 || tournament.tier3[1] == -1) ? 0 : -1;
                    updateCPUScores(tournament.tier3.data(), 2, &tournament.winner, tierOffset);

                    //see if we came first or second
                    if (tierOffset == 0)
                    {
                        tournament.currentBest = tournament.winner == -1 ? 1 : 2;
                    }
                }
                break;
                }
            };

        iterateScores(tournament.round);

        if (!playerWon)
        {
            //iterate the CPU for all remaining round to find out who won!
            //iterateScores() has, at this point, incremented the round number already
            for (auto i = tournament.round; i < 4; ++i)
            {
                iterateScores(i);
                //LogI << "Iterate round " << i << std::endl;
            }

            //LogI << "Player lost tournament round" << std::endl;
        }
        /*else
        {
            LogI << "player won tournament round" << std::endl;
        }*/

        //save tournament
        std::fill(tournament.scores.begin(), tournament.scores.end(), 0);
        std::fill(tournament.opponentScores.begin(), tournament.opponentScores.end(), 0);
        tournament.mulliganCount = 1;
        writeTournamentData(tournament);
    }
    else
    {
        LogW << "Not a tournament" << std::endl;
    }
}