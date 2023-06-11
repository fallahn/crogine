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
            entity.getComponent<cro::Callback>().setUserData<float>(1.f);
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
    skill = std::clamp(skill + offset, 0, 6);
    //skill += offset;

    //std::int32_t skill = m_playerInfo[0].player * 3;

    auto& ball = m_playerInfo[0].ballEntity.getComponent<Ball>();
    std::int32_t clubID = ClubID::Putter;

    //get longest range available
    if (ball.terrain != TerrainID::Green)
    {
        auto dist = glm::length(m_holeData[m_currentHole].tee - m_holeData[m_currentHole].pin);
        if (dist > 115.f) //forces a cut-off for pitch n putt
        {
            dist = 1000.f;
        }

        clubID = ClubID::SandWedge;
        while ((Clubs[clubID].getDefaultTarget() * 1.05f) < dist
            && clubID != ClubID::Driver)
        {
            do
            {
                clubID--;
            } while (clubID != ClubID::Driver);
        }
    }
    const float clubDist = Clubs[clubID].getTargetAtLevel(std::min(2, skill / 3));
    
    const auto pinDist = glm::length2(pinDir);
    const auto targetDist = glm::length2(targetDir);
    const float MinDist = m_scene.getSystem<BallSystem>()->getPuttFromTee() ? 9.f : 2500.f;

    const auto pickTarget = [&](float dp)
    {
        if (m_scene.getSystem<BallSystem>()->getPuttFromTee())
        {
            if (dp > 0.97f)
            {
                return m_holeData[m_currentHole].pin;
            }
            else
            {
                if (targetDist < MinDist
                    || pinDist < targetDist)
                {
                    return m_holeData[m_currentHole].pin;
                }

                return m_holeData[m_currentHole].target;
            }
        }
        else
        {
            return m_holeData[m_currentHole].pin;
        }
    };

    //if we're less than 20cm from the hole we'll assume
    //it's pretty much a dead cert going in
    if (pinDist > (0.2f * 0.2f))
    {
        //if both the pin and the target are in front of the player
        if (auto dp = glm::dot(glm::normalize(targetDir), glm::normalize(pinDir)); dp > 0.4)
        {
            //set the target depending on how close it is
            if (pinDist < targetDist)
            {
                //always target pin if its closer
                pos = pickTarget(dp);
            }

            //target the pin if its in range of our longest club
            //and CPU skill > something
            else if (skill > 2)
            {
                pos = pickTarget(dp);
            }

            else
            {
                //target the pin if the target is too close
                if (targetDist < MinDist) //remember this is len2
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
            pos = pickTarget(dp);
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
        if (ball.terrain == TerrainID::Green)
        {
            if (cro::Util::Random::value(0, 3 + (skill / 2)) == 0)
            {
                //add target offset
                pos += randomNormal() * cro::Util::Random::value(0.05f, 0.2f);
                CRO_ASSERT(!std::isnan(pos.x), "");
            }

            //add offset based on length of putt
            const auto len2 = glm::length2(pos - m_playerInfo[0].position);
            const auto length = std::min(1.f, len2 / (10.f * 10.f));
            auto accuracy = length * (1.f - (static_cast<float>(skill) / 10.f));
            accuracy /= 4.f;

            //lower skill has more chance of going off
            if (cro::Util::Random::value(0, 3 + skill) == 0)
            {
                accuracy *= 1.6f + (3 - (std::min(skill, 3)));
            }

            //higher skill has more chance of being accurate
            if (cro::Util::Random::value(0, 3 + skill) > 3)
            {
                accuracy /= 2.f;
            }

            pos += randomNormal() * accuracy;
            //LogI << "accuracy is " << accuracy << ", distance " << glm::length(pos - m_playerInfo[0].position) << std::endl;
        }
        else
        {
            //we're probably chipping if < 90m
            if (auto l2 = glm::length2(pos - m_playerInfo[0].position); l2 < (90.f * 90.f))
            {
                //if len > 20 almost always add offset so we don't hole the ball
                if (l2 > (20.f * 20.f))
                {
                    if (cro::Util::Random::value(0, 100 - (skill * 10)) != 0)
                    {
                        pos += randomNormal() * static_cast<float>(cro::Util::Random::value(3, 10)) / 10.f;
                    }
                }

                if (cro::Util::Random::value(0, 4 + skill) < 8)
                {
                    //add offset based on how close we are to the pin/target
                    pos += randomNormal() * (static_cast<float>(cro::Util::Random::value(6, 70)) / 10.f) * std::min(0.2f, std::max(1.f, std::sqrt(l2) / 100.f));
                    CRO_ASSERT(!std::isnan(pos.x), "");
                }

                //more likely to overshoot with distance and lower skill
                if (cro::Util::Random::value(0, 4 + skill) < 4)
                {
                    auto dir = pos - m_playerInfo[0].position;
                    dir *= 1.f + (0.065f * std::min(1.f, std::sqrt(l2) / 80.f));
                    pos = m_playerInfo[0].position + dir;
                }
            }
            else
            {
                //iron or driver so add some arbitrary offset
                //based on bounce and wind strength/dir and distance to pos

                //TODO we need a good bounce as a percentage value...
                constexpr float BouncePercent = 1.05f;
                pos = ((pos - m_playerInfo[0].position) * BouncePercent);

                auto step = pos / 5.f;
                pos = m_playerInfo[0].position + step;

                for (auto i = 0; i < 4; ++i)
                {
                    const auto wind = m_scene.getSystem<BallSystem>()->getWindDirection();
                    step += glm::vec3(wind.x, 0.f, wind.z) * wind.y * (0.1f * (7 - skill));
                    pos += step;
                }

                CRO_ASSERT(!std::isnan(pos.x), "");
            }

            //reduce the distance if we're in the rough based on skill
            if (ball.terrain == TerrainID::Rough
                || ball.terrain == TerrainID::Bunker)
            {
                auto dir = pos - m_playerInfo[0].position;

                float pc = 1.f - (static_cast<float>(std::max(0, 6 - skill)) / 100.f);
                dir *= pc;

                pos = m_playerInfo[0].position + dir;
            }
        }
    }
    else
    {
        pos = m_holeData[m_currentHole].pin;

        //if we're really daft maybe we'll hit it short
        if (cro::Util::Random::value(0, 2 + (skill * 2)) == 0)
        {
            pos = m_playerInfo[0].position + (pinDir / 2.f);
        }
    }
    
    
    
    
    pos.x = std::clamp(pos.x, 0.f, 320.f);
    pos.z = std::clamp(pos.z, -200.f, 0.f);

    //test terrain height and correct final position
    auto result = m_scene.getSystem<BallSystem>()->getTerrain(pos);

    //technically this means CPU players never make really bad shots
    //but otherwise they just get stuck in a loop
    switch (result.terrain)
    {
    case TerrainID::Water:
    case TerrainID::Stone:
    case TerrainID::Scrub:
    {
        std::int32_t tries = 300;
        auto dir = glm::normalize(pos - m_playerInfo[0].position);
        do
        {
            pos -= dir;
            result = m_scene.getSystem<BallSystem>()->getTerrain(pos);
        } while (tries-- 
            && (result.terrain == TerrainID::Water || result.terrain == TerrainID::Stone || result.terrain == TerrainID::Scrub)
            && glm::length2(pos) > 1);
    }
        break;
    default: break;
    }


    pos.y = result.intersection.y;

    CRO_ASSERT(!std::isnan(pos.x), "");
    CRO_ASSERT(!std::isnan(pos.y), "");
    CRO_ASSERT(!std::isnan(pos.z), "");

    m_playerInfo[0].ballEntity.getComponent<cro::Transform>().setPosition(pos);
    m_playerInfo[0].holeScore[m_currentHole]++;

    const auto velOffset = glm::normalize(pos - m_playerInfo[0].position) * 0.001f;

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
        ball.velocity = velOffset; //add a tiny bit of velocity to prevent div0/nan in BallSystem
        break;
    case TerrainID::Green:
    case TerrainID::Hole:
        ball.state = Ball::State::Putt;
        ball.velocity = velOffset;
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