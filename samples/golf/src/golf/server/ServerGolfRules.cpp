#include "../PacketIDs.hpp"
#include "../BallSystem.hpp"
#include "../Clubs.hpp"

#include "ServerMessages.hpp"
#include "ServerGolfState.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/util/Random.hpp>

namespace
{
    glm::vec3 randomNormal()
    {
        glm::vec2 v(
            static_cast<float>(cro::Util::Random::value(1, 10)) / 10.f,
            static_cast<float>(cro::Util::Random::value(1, 10)) / 10.f);

        v.x *= cro::Util::Random::value(0, 1) == 0 ? -1.f : 1.f;
        v.y *= cro::Util::Random::value(0, 1) == 0 ? -1.f : 1.f;
        v = glm::normalize(v);

        CRO_ASSERT(!std::isnan(v.x), "");
        return { v.x, 0.f, -v.y };
        //return { 1.f, 0.f, 0.f };
    }
}

using namespace sv;
void GolfState::makeCPUMove()
{
    if(m_sharedData.fastCPU
        && m_sharedData.clients[m_playerInfo[0].client].playerData[m_playerInfo[0].player].isCPU)
    {
        auto& ball = m_playerInfo[0].ballEntity.getComponent<Ball>();
        if (ball.state == Ball::State::Idle)
        {
            //wrap in an ent so we can add a small delay
            auto entity = m_scene.createEntity();
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().setUserData<float>(2.5f);
            entity.getComponent<cro::Callback>().function =
                [&](cro::Entity e, float dt)
            {
                auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
                currTime -= dt;

                if (currTime < 0)
                {
                    calcCPUPosition();

                    m_sharedData.host.broadcastPacket<std::uint8_t>(PacketID::CPUThink, 1, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                    e.getComponent<cro::Callback>().active = false;
                    m_scene.destroyEntity(e);
                }
            };

            m_sharedData.host.broadcastPacket<std::uint8_t>(PacketID::CPUThink, 0, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
    }
}

void GolfState::calcCPUPosition()
{
    auto targetDir = m_holeData[m_currentHole].target - m_playerInfo[0].position;
    auto pinDir = m_holeData[m_currentHole].pin - m_playerInfo[0].position;

    glm::vec3 pos = glm::vec3(0.f);
    std::int32_t skill = m_skillIndex;
    std::int32_t offset = m_playerInfo[0].player % 2;
    if (m_skillIndex > 2)
    {
        offset *= -1;
    }
    skill += offset;

    //get longest range available
    auto dist = glm::length(m_holeData[m_currentHole].tee - m_holeData[m_currentHole].pin);
    if (dist > 115.f) //forces a cut-off for pitch n putt
    {
        dist = 1000.f;
    }

    std::int32_t clubID = ClubID::SandWedge;
    while ((Clubs[clubID].getDefaultTarget() * 1.05f) < dist
        && clubID != ClubID::Driver)
    {
        do
        {
            clubID--;
        } while (clubID != ClubID::Driver);
    }
    const float clubDist = Clubs[clubID].getTargetAtLevel(std::min(2, skill / 3));


    //if both the pin and the target are in front of the player
    if (glm::dot(glm::normalize(targetDir), glm::normalize(pinDir)) > 0.4)
    {
        //set the target depending on how close it is
        const auto pinDist = glm::length2(pinDir);
        const auto targetDist = glm::length2(targetDir);
        if (pinDist < targetDist)
        {
            //always target pin if its closer
            pos = m_holeData[m_currentHole].pin;
        }

        //target the pin if its in range of our longest club
        //and CPU skill > something
        else if (/*pinDist < (clubDist * clubDist)
            &&*/ m_skillIndex > 2)
        {
            pos = m_holeData[m_currentHole].pin;
        }

        else
        {
            //target the pin if the target is too close
            const float MinDist = m_holeData[m_currentHole].puttFromTee ? 9.f : 2500.f;
            if (targetDist < MinDist) //remember this in len2
            {
                pos = m_holeData[m_currentHole].pin;
            }
            else
            {
                pos = m_holeData[m_currentHole].target;
            }
        }
    }
    else
    {
        //else set the pin as the target
        pos = m_holeData[m_currentHole].pin;
    }

    //reduce the target distance so that it's in range of our longest club
    if (auto len2 = glm::length2(pos - m_playerInfo[0].position); len2 >
        (clubDist * clubDist))
    {
        const float reduction = clubDist / std::sqrt(len2);
        pos = ((pos - m_playerInfo[0].position) * reduction) + m_playerInfo[0].position;
    }
    
    //make sure there's only a slim chance of getting it in the hole
    //if club is not a putter, and VERY slim chance if not a wedge
    auto& ball = m_playerInfo[0].ballEntity.getComponent<Ball>();
    if (ball.terrain == TerrainID::Green)
    {
        if (cro::Util::Random::value(0, 3 + skill) == 0)
        {
            //add target offset
            pos += randomNormal() * cro::Util::Random::value(0.05f, 0.2f);

            CRO_ASSERT(!std::isnan(pos.x), "");
        }
    }
    else
    {
        //we're probably chipping
        if (glm::length2(pos - m_playerInfo[0].position) < (90.f * 90.f))
        {
            if (cro::Util::Random::value(0, 4 + skill) < 8)
            {
                //add offset
                pos += randomNormal() * static_cast<float>(cro::Util::Random::value(6, 70)) / 10.f;

                CRO_ASSERT(!std::isnan(pos.x), "");
            }
        }
        else
        {
            //iron or driver so add some arbitrary offset
            //based on bounce and wind strength/dir and distance to pos

            //TODO we need a good bounce as a percentage value...
            constexpr float BouncePercent = 1.05f;
            pos = ((pos - m_playerInfo[0].position) * BouncePercent) + m_playerInfo[0].position;

            auto step = pos / 5.f;
            pos = m_playerInfo[0].position + step;

            for (auto i = 0; i < 4; ++i)
            {
                const auto wind = m_scene.getSystem<BallSystem>()->getWindDirection();
                step += glm::vec3(wind.x, 0.f, wind.z) * wind.y * (0.1f * (6 - skill));
                pos += step;
            }
            CRO_ASSERT(!std::isnan(pos.x), "");
        }
    }
    pos.x = std::clamp(pos.x, 0.f, 320.f);
    pos.z = std::clamp(pos.z, -200.f, 0.f);

    //test terrain height and correct final position
    auto result = m_scene.getSystem<BallSystem>()->getTerrain(pos);
    pos.y = result.intersection.y;

    CRO_ASSERT(!std::isnan(pos.x), "");
    CRO_ASSERT(!std::isnan(pos.y), "");
    CRO_ASSERT(!std::isnan(pos.z), "");

    m_playerInfo[0].ballEntity.getComponent<cro::Transform>().setPosition(pos);
    m_playerInfo[0].holeScore[m_currentHole]++;

    ball.terrain = result.terrain;
    switch (result.terrain)
    {
    default:
        ball.state = Ball::State::Paused;
        break;
    case TerrainID::Fairway:
    case TerrainID::Bunker:
    case TerrainID::Rough:
        ball.state = Ball::State::Flight;
        break;
    case TerrainID::Green:
    case TerrainID::Hole:
        ball.state = Ball::State::Putt;
        break;
    case TerrainID::Scrub:
    case TerrainID::Stone:
    case TerrainID::Water:
        ball.state = Ball::State::Reset;
        break;
    }
}

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