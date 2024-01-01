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

#include "ClientCollisionSystem.hpp"
#include "GameConsts.hpp"
#include "MessageIDs.hpp"
#include "BallSystem.hpp"
#include "CollisionMesh.hpp"
#include "BallSystem.hpp"
#include "Clubs.hpp"

#include <crogine/ecs/components/Transform.hpp>

#include <crogine/detail/glm/gtx/norm.hpp>

#include <Achievements.hpp>
#include <AchievementStrings.hpp>

using namespace cl;

namespace
{
    static constexpr float MinBallDist = (HoleRadius * 1.2f) * (HoleRadius * 1.2f);
    static constexpr float NearHoleDist = (0.1f * 0.1f); //10cm, used for near misses
}

ClientCollisionSystem::ClientCollisionSystem(cro::MessageBus& mb, const std::vector<HoleData>& hd, const CollisionMesh& cm)
    : cro::System   (mb, typeid(ClientCollisionSystem)),
    m_holeData      (hd),
    m_holeIndex     (0),
    m_collisionMesh (cm),
    m_club          (-1),
    m_pinPosition   (0.f),
    m_waterCollision(false)
{
    requireComponent<cro::Transform>();
    requireComponent<ClientCollider>();
}

//public
void ClientCollisionSystem::process(float)
{
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& collider = entity.getComponent<ClientCollider>();
        const auto& tx = entity.getComponent<cro::Transform>();
        auto position = tx.getPosition();

        //skip if not near the ground
        if (!collider.active
            || position.y > 30.f) //NOTE this assumes the ground is less than this, some courses are 27m high!
        {
            continue;
        }

        if (collider.state == static_cast<std::uint8_t>(Ball::State::Flight))
        {
            auto holePos = m_pinPosition;
            static constexpr float FlagRadius = 0.05f;
            static constexpr float CollisionRadius = FlagRadius + Ball::Radius;
            if (position.y - holePos.y < 1.9f) //flag is 2m tall
            {
                const glm::vec2 holeCollision = { holePos.x, -holePos.z };
                const glm::vec2 ballCollision = { position.x, -position.z };
                auto dir = ballCollision - holeCollision;
                if (auto l2 = glm::length2(dir); l2 < (CollisionRadius * CollisionRadius))
                {
                    auto* msg = postMessage<CollisionEvent>(MessageID::CollisionMessage);
                    msg->terrain = CollisionEvent::FlagPole;
                    msg->position = position;
                    msg->type = CollisionEvent::Begin;
                }
            }
        }




        auto result = m_collisionMesh.getTerrain(position);
        if (!result.wasRayHit)
        {
            //we've missed the geom so check the map to see if we're
            //in water or scrub
            result.terrain = readMap(m_mapImage, position.x, -position.z).first;
        }

        collider.terrain = result.terrain;

        if (collider.terrain == TerrainID::Green)
        {

            //check if we're in the hole
            glm::vec2 pin(m_holeData[m_holeIndex].pin.x, m_holeData[m_holeIndex].pin.z);
            glm::vec2 pos(position.x, position.z);
            
            auto len2 = glm::length2(pin - pos);

            auto oldNear = collider.nearHole;
            collider.nearHole = len2 < NearHoleDist;

            if (len2 <= MinBallDist
                && (result.height - position.y) > (Ball::Radius * 2.f))
            {
                collider.terrain = TerrainID::Hole;
                collider.nearHole = false;
            }

            //or a near miss
            else if (oldNear 
                && !collider.nearHole 
                //&& collider.terrain == TerrainID::Green
                && m_club == ClubID::Putter
                //&& position.y > m_holeData[m_holeIndex].pin.y
                /*&& result.height >= (position.y - (Ball::Radius / 2.f))*/)
            {
                auto* msg = postMessage<CollisionEvent>(MessageID::CollisionMessage);
                msg->type = CollisionEvent::NearMiss;
                msg->position = position;
                msg->terrain = TerrainID::Green;
                msg->clubID = m_club;
            }
        }
        else
        {
            collider.nearHole = false;
        }

        //tracks dolphin achievement - should happen only once per round
        if (collider.terrain == TerrainID::Water)
        {
            if (position.y < WaterLevel
                && collider.previousWorldHeight > WaterLevel)
            {
                if (!m_waterCollision)
                {
                    Achievements::incrementStat(StatStrings[StatID::WaterTrapRounds]);
                }
                m_waterCollision = true;
            }
        }

        const auto notify = [&](CollisionEvent::Type type, glm::vec3 position)
        {
            //ugh messy, but there are several edge cases (this is responsible for sound effects
            //and particle effects being raised)
            if (collider.state == static_cast<std::uint8_t>(Ball::State::Flight)
                || collider.state == static_cast<std::uint8_t>(Ball::State::Reset) //for scrub/water
                || collider.terrain == TerrainID::Hole
                || collider.terrain == TerrainID::Bunker)
            {
                auto* msg = postMessage<CollisionEvent>(MessageID::CollisionMessage);
                msg->type = type;
                msg->position = position;
                msg->terrain = collider.terrain;
                msg->clubID = m_club;
            }
        };

        static constexpr float CollisionLevel = 0.45f;
        float currentLevel = position.y - result.height;

        std::int32_t direction = 0;
        if (currentLevel < collider.previousHeight)
        {
            direction = -1;
        }
        else if (currentLevel > collider.previousHeight)
        {
            direction = 1;
        }

        //yes it's an odd way of doing it, but we're floundering
        //around in the spongey world of interpolated actors.
        if (currentLevel < CollisionLevel)
        {
            if (direction > collider.previousDirection)
            {
                //started moving up
                notify(CollisionEvent::End, position);
            }
            else if (collider.previousHeight > CollisionLevel)
            {
                notify(CollisionEvent::Begin, position);

                //really we only want to do this if the lie is 'buried' however
                //it's not been updated from the server yet...
                if (collider.terrain == TerrainID::Bunker)
                {
                    entity.getComponent<cro::Transform>().setOrigin({ 0.f, Ball::Radius * 1.2f, 0.f });
                }
                else
                {
                    entity.getComponent<cro::Transform>().setOrigin(glm::vec3(0.f));
                }
            }
            else if(collider.terrain == TerrainID::Hole
                &&(collider.previousHeight > result.height - (Ball::Radius * 2.f)
                    && currentLevel < result.height - (Ball::Radius * 2.f)))
            {
                //we're in the hole. Probably.
                notify(CollisionEvent::Begin, position);
            }
        }

        collider.previousDirection = direction;
        collider.previousHeight = currentLevel;
        collider.previousWorldHeight = position.y;
        collider.active = false; //forcibly reset this so it can only be explicitly set true by a server update
    }
}

void ClientCollisionSystem::setMap(std::uint32_t index)
{
    m_mapImage.loadFromFile(m_holeData[index].mapPath, true);
    m_holeIndex = index;
}