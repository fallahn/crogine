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

    std::int32_t getClub(float dist)
    {
        if (dist > 115.f) //forces a cut-off for pitch n putt
        {
            dist = 1000.f;
        }

        std::int32_t clubID = ClubID::SandWedge;
        while ((Clubs[clubID].getDefaultTarget() * 1.05f) < dist
            && clubID != ClubID::Driver)
        {
            clubID--;
        }
        return clubID;
    }
}

using namespace sv;
using namespace cstat;
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
            entity.getComponent<cro::Callback>().setUserData<float>(1.2f);
            entity.getComponent<cro::Callback>().function =
                [&](cro::Entity e, float dt)
            {
                auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
                currTime -= dt;

                if (currTime < 0)
                {
                    auto& ball = m_playerInfo[0].ballEntity.getComponent<Ball>();
                    auto animID = ball.terrain == TerrainID::Green ? AnimationID::Putt : AnimationID::Celebrate;
                 
                    //this is just separated out so we can swap calcs more easily
                    auto pos = calcCPUPosition();


                    //test terrain height and correct final position
                    auto result = m_scene.getSystem<BallSystem>()->getTerrain(pos);

                    //technically this means CPU players never make really bad shots
                    //but otherwise they just get stuck in a loop
                    switch (result.terrain)
                    {
                    case TerrainID::Water:
                        //use mistake odds to occasionally hit the water
                    {
                        const auto cpuID = m_cpuProfileIndices[m_playerInfo[0].client * ConstVal::MaxPlayers + m_playerInfo[0].player];
                        if (cro::Util::Random::value(0, 9) < CPUStats[cpuID][CPUStat::MistakeLikelyhood])
                        {
                            break;
                        }
                    }
                        [[fallthrough]];
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

                    //TODO this case should never happen...
                    auto velOffset = pos - m_playerInfo[0].position;
                    if (glm::length2(velOffset) == 0)
                    {
                        velOffset.x = 0.0001f;
                    }
                    velOffset = glm::normalize(velOffset) * 0.001f;

                    //const auto velOffset = glm::normalize(pos - m_playerInfo[0].position) * 0.001f;
                    
                    //LogI << velOffset << std::endl;
                    ball.terrain = result.terrain;
                    switch (result.terrain)
                    {
                    default:
                        ball.state = Ball::State::Paused;
                        break;
                    case TerrainID::Bunker:
                    case TerrainID::Rough:
                        animID = AnimationID::Disappoint;
                        [[fallthrough]];
                    case TerrainID::Fairway:
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
                        animID = AnimationID::Disappoint;
                        break;
                    }

                    m_sharedData.host.broadcastPacket(PacketID::ActorAnimation, std::uint8_t(animID), net::NetFlag::Reliable, ConstVal::NetChannelReliable);



                    m_sharedData.host.broadcastPacket<std::uint8_t>(PacketID::CPUThink, 1, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                    e.getComponent<cro::Callback>().active = false;
                    m_scene.destroyEntity(e);
                }
            };

            m_sharedData.host.broadcastPacket<std::uint8_t>(PacketID::CPUThink, 0, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
    }
}

glm::vec3 GolfState::calcCPUPosition() const
{
    const auto targetDir = m_holeData[m_currentHole].target - m_playerInfo[0].position;
    const auto pinDir = m_holeData[m_currentHole].pin - m_playerInfo[0].position;
    auto pos = m_holeData[m_currentHole].pin; //always prefer the pin as the target unless blocked for some reason
    
    const auto cpuID = m_cpuProfileIndices[m_playerInfo[0].client * ConstVal::MaxPlayers + m_playerInfo[0].player];
    CRO_ASSERT(cpuID != -1, "");

    const std::int32_t skill = CPUStats[cpuID][CPUStat::Skill];

    auto& ball = m_playerInfo[0].ballEntity.getComponent<Ball>();
    std::int32_t clubID = ClubID::Putter;

    //get longest range available
    if (ball.terrain != TerrainID::Green)
    {
        if (ball.terrain == TerrainID::Bunker)
        {
            clubID = ClubID::PitchWedge;
        }
        else
        {
            auto dist = glm::length(pinDir);
            clubID = getClub(dist);
        }

        //check to see if the club range can hit the ball into a valid area,
        //by reducing pos to max range
        const float clubDist = Clubs[clubID].getTargetAtLevel(skill);
        if (auto len2 = glm::length2(pinDir); 
            len2 > (clubDist * clubDist))
        {
            const float reduction = clubDist / std::sqrt(len2);
            pos = (pinDir * reduction) + m_playerInfo[0].position;
        }

        auto result = m_scene.getSystem<BallSystem>()->getTerrain(pos);
        switch (result.terrain)
        {
        default: break;
        case TerrainID::Water:
            //use the mistake odds to sometimes accept this target
            if (cro::Util::Random::value(0, 9) < CPUStats[cpuID][CPUStat::MistakeLikelyhood])
            {
                break;
            }
            [[fallthrough]];
        case TerrainID::Stone:
        case TerrainID::Scrub:
            //else use the target point instead of the pin
            pos = m_holeData[m_currentHole].target;
            break;
        }
    }
    else 
    {
        //else if we're on a mini-putt course see if there's a dog-leg
        if (m_scene.getSystem<BallSystem>()->getPuttFromTee())
        {
            if (auto dp = glm::dot(glm::normalize(targetDir), glm::normalize(pinDir));
                dp > 0.4 && dp < 0.98f) //target in front, but not the same dir as pin
            {
                //don't use if too close
                if (glm::length2(targetDir) > (3.f * 3.f)
                    && glm::length2(pinDir) > glm::length2(targetDir))
                {
                    pos = m_holeData[m_currentHole].target;
                }
            }
        }
        else
        {
            //regular putting - assume we go in the hole under 20cm
            if (glm::length2(pinDir) < (0.2f * 0.2f))
            {
                return m_holeData[m_currentHole].pin;
            }
        }
    }


    const auto playerName = [&]()
    {
        return m_sharedData.clients[m_playerInfo[0].client].playerData[m_playerInfo[0].player].name.toAnsiString();
    };


    auto stepDir = pos - m_playerInfo[0].position;
    const auto totalDist = glm::length(stepDir);
    std::int32_t stepCount = std::max(1, static_cast<std::int32_t>(totalDist));
    
    //use smaller steps when putting
    if (ball.terrain == TerrainID::Green)
    {
        stepCount *= 2;
    }
    stepDir /= stepCount;

    //using the CPU stats calculate some sort of offset from the target.
    float windEffect = 0.f;
    if (clubID < ClubID::SandWedge)
    {
        windEffect = getOffset(WindOffsets, CPUStats[cpuID][CPUStat::WindAccuracy]);
        windEffect *= Clubs[clubID].getBaseTarget() / Clubs[ClubID::Driver].getBaseTarget();

        //LogI << "Wind effect " << windEffect << std::endl;
    }
    auto wind = m_scene.getSystem<BallSystem>()->getWindDirection();
    wind = (glm::vec3(wind.x, 0.f, wind.z) * wind.y * windEffect);
    wind /= stepCount;

    const auto clubMultiplier = (Clubs[clubID].getTarget(totalDist) / Clubs[ClubID::Driver].getTargetAtLevel(CPUStats[cpuID][CPUStat::Skill]));
    auto dirNorm = glm::normalize(stepDir);

    const float overShoot = getOffset(PowerOffsets, CPUStats[cpuID][CPUStat::PowerAccuracy]);
    auto overShootDir = dirNorm * overShoot * (clubMultiplier * clubMultiplier);
    overShootDir /= stepCount;

    dirNorm = { -dirNorm.z, dirNorm.y, dirNorm.x }; //perpendicular
    const float strokeAccuracy = getOffset(AccuracyOffsets, CPUStats[cpuID][CPUStat::StrokeAccuracy]);
    auto accuracyDir = dirNorm * strokeAccuracy * clubMultiplier;
    accuracyDir /= stepCount;

    //TODO include offset for rough or bunker terrain - this should probably be another stat
    //for how well CPU compensates


    //calculate mistake odds - increase this with distance when putting
    bool perfect = false;
    std::int32_t puttingOdds = 0;
    if (ball.terrain == TerrainID::Green)
    {
        float odds = std::pow(std::min(1.f, totalDist / 5.f), 2.f);

        accuracyDir *= (1.f + odds);
        odds *= 2.f;

        puttingOdds = static_cast<std::int32_t>(std::round(odds));
    }

    if (cro::Util::Random::value(0, 14) < CPUStats[cpuID][CPUStat::MistakeLikelyhood] + puttingOdds)
    {
        //TODO check these values so we don't accidentally
        //undo existing power/accuracy and improve them...
        LogI << " Made a mistake!" << std::endl;
        switch (cro::Util::Random::value(0, 2))
        {
        default: 
        case 0:
            LogI << playerName() << " Fluffed power" << std::endl;
            overShootDir *= 2.56f;
            break;
        case 1:
            LogI << playerName() << " Fluffed accuracy" << std::endl;
            accuracyDir *= 1.78f;
            break;
        case 2:
            LogI << playerName() << " Fluffed power and accuracy" << std::endl;
            accuracyDir *= 1.92f;
            overShootDir *= 1.98f;
            break;
        }

        if (cro::Util::Random::value(0, 29) < CPUStats[cpuID][CPUStat::MistakeLikelyhood] + (puttingOdds * 2))
        {
            LogI << playerName() << " made uber mistake" << std::endl;
            stepDir /= 2.f;

            stepDir += (stepDir * getOffset(MistakeCorrection, CPUStats[cpuID][CPUStat::MistakeAccuracy]));
        }
    }

    //calculate perfection odds only if we didn't make a mistake
    else if (cro::Util::Random::value(0, 99) < CPUStats[cpuID][CPUStat::PerfectionLikelyhood])
    {
        if (totalDist < 20.f)
        {
            perfect = cro::Util::Random::value(1, 10) == CPUStats[cpuID][CPUStat::PerfectionLikelyhood] / 10;
        }
        else if (totalDist < 100.f)
        {
            perfect = cro::Util::Random::value(1, 50) == CPUStats[cpuID][CPUStat::PerfectionLikelyhood] / 2;
        }
        else
        {
            perfect = cro::Util::Random::value(1, 100) == CPUStats[cpuID][CPUStat::PerfectionLikelyhood];
        }




        if(perfect && 
            totalDist < Clubs[clubID].getBaseTarget())
        {
            LogI << playerName() << " Got a PERFECT shot" << std::endl;
            pos = m_holeData[m_currentHole].pin;
        }
        else
        {
            perfect = false; //need to do the calc below if we're not in range
        }
    }

    CRO_ASSERT(glm::length2(stepDir), "");
    //CRO_ASSERT(glm::length2(wind), "");
    CRO_ASSERT(glm::length2(overShootDir), "");
    CRO_ASSERT(glm::length2(accuracyDir), "");
    CRO_ASSERT(stepCount != 0, "");

    //if not perfect
    if (!perfect)
    {
        pos = m_playerInfo[0].position;
        for (auto i = 0; i < stepCount; ++i)
        {
            pos += stepDir;
            pos += wind;
            pos += overShootDir;
            pos += accuracyDir;

            //TODO check each step for terrain to see if we
            //accidentally cut corners
        }

        //if we're really close to the hole plop it in based on stroke accuracy
        if (glm::length2(pos - m_holeData[m_currentHole].pin) < (0.15f * 0.15f) &&
            cro::Util::Random::value(1, 100) > CPUStats[cpuID][CPUStat::StrokeAccuracy] * 10)
        {
            pos = m_holeData[m_currentHole].pin;
        }
    }

    return pos;
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