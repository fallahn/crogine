/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include "CloudSystem.hpp"

#include <crogine/detail/glm/gtx/norm.hpp>
#include <crogine/ecs/components/Transform.hpp>

namespace
{
    constexpr float MaxRadius = 380.f;
    constexpr float MaxDistance = MaxRadius * MaxRadius;
    constexpr float ResetDistance = (MaxRadius * 2.f);
}

CloudSystem::CloudSystem(cro::MessageBus& mb, glm::vec3 worldCentre)
    : cro::System       (mb, typeid(CloudSystem)),
    m_worldCentre       (worldCentre),
    m_currentWindSpeed  (0.f),
    m_targetWindSpeed   (0.f)
{
    requireComponent<Cloud>();
    requireComponent<cro::Transform>();
}

//public
void CloudSystem::process(float dt)
{
    //interp wind speed
    //x/z is dir, y is strength
    m_currentWindSpeed += (m_targetWindSpeed - m_currentWindSpeed) * dt;

    glm::vec3 velocity(m_currentWindSpeed.x, 0.f, m_currentWindSpeed.z);
    velocity *= 10.f * m_currentWindSpeed.y;

    //process clouds
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& cloud = entity.getComponent<Cloud>();
        auto& tx = entity.getComponent<cro::Transform>();
        tx.move(velocity * cloud.speedMultiplier * dt);

        glm::vec2 cloudPos(tx.getPosition().x - m_worldCentre.x, tx.getPosition().z - m_worldCentre.z);
        auto cloudDist = glm::length2(cloudPos);

        if (cloudDist > MaxDistance
            && glm::dot(velocity, tx.getPosition()) > 0)
        {
            auto movement = glm::normalize(velocity) * ResetDistance;
            tx.move(-movement);
        }
    }
}

void CloudSystem::setWindVector(glm::vec3 vector)
{
    m_targetWindSpeed = vector;
}