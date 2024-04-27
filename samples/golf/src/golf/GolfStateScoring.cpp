/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
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

using namespace cl;

void GolfState::updateHoleScore(std::uint16_t data)
{
    switch (m_sharedData.scoreType)
    {
    default: break;
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
            client = std::clamp(client, 0, static_cast<std::int32_t>(ConstVal::MaxClients));
            player = std::clamp(player, 0, static_cast<std::int32_t>(ConstVal::MaxPlayers));

            auto txt = m_sharedData.connectionData[client].playerData[player].name;
            if (m_sharedData.scoreType == ScoreType::Match)
            {
                txt += " has won the hole!";
            }
            else
            {
                txt += " has won the pot!";
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