/*-----------------------------------------------------------------------

Matt Marchant 2021
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include <crogine/ecs/components/Transform.hpp>

#include <crogine/detail/glm/gtx/norm.hpp>

namespace
{
    constexpr float MinBallDist = (HoleRadius * 1.2f) * (HoleRadius * 1.2f);
}

ClientCollisionSystem::ClientCollisionSystem(cro::MessageBus& mb, const std::vector<HoleData>& hd)
    : cro::System   (mb, typeid(ClientCollisionSystem)),
    m_holeData      (hd),
    m_holeIndex     (0)
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

        const auto notify = [&](CollisionEvent::Type type, glm::vec3 position)
        {
            auto* msg = postMessage<CollisionEvent>(MessageID::CollisionMessage);
            msg->type = type;
            msg->position = position;
            msg->terrain = readMap(m_currentMap, position.x, -position.z).first;

            if (msg->terrain == TerrainID::Green)
            {
                //check if we're in the hole
                auto len2 = glm::length2(m_holeData[m_holeIndex].pin - position);
                if (len2 < MinBallDist)
                {
                    msg->terrain = TerrainID::Hole;
                }
            }
        };

        static constexpr float CollisionLevel = 0.25f;
        auto position = tx.getPosition();
        std::int32_t direction = 0;
        if (position.y < collider.previousPosition.y)
        {
            direction = -1;
        }
        else if (position.y > collider.previousPosition.y)
        {
            direction = 1;
        }

        //yes it's an odd way of doing it, but we're floundering
        //around in the spongey world of interpolated actors.
        if (position.y < CollisionLevel)
        {
            if (direction > collider.previousDirection)
            {
                //started moving up
                notify(CollisionEvent::End, position);
            }
            else if (collider.previousPosition.y > CollisionLevel)
            {
                notify(CollisionEvent::Begin, position);
            }
            else if (position.y < -Ball::Radius
                && collider.previousPosition.y > -Ball::Radius)
            {
                //we're in the hole. Probably.
                notify(CollisionEvent::Begin, position);
            }
        }

        collider.previousDirection = direction;
        collider.previousPosition = position;
    }
}

void ClientCollisionSystem::setMap(std::uint32_t holeIndex)
{
    CRO_ASSERT(holeIndex < m_holeData.size(), "");
    CRO_ASSERT(!m_holeData[holeIndex].mapPath.empty(), "");

    m_holeIndex = holeIndex;
    m_currentMap.loadFromFile(m_holeData[holeIndex].mapPath);
}