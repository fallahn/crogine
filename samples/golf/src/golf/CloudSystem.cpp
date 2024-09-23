/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2024
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

#include "CloudSystem.hpp"
#include "GameConsts.hpp"

#include <crogine/detail/glm/gtx/norm.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/util/Constants.hpp>

namespace
{
    constexpr float MaxRadius = MapSizeFloat.x / 2.f;// 380.f;
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

    auto camPos = getScene()->getActiveCamera().getComponent<cro::Transform>().getWorldPosition();


    //process clouds
    const auto& entities = getEntities();
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

        auto facing = camPos - tx.getPosition();// glm::vec2(camPos.x, camPos.z) - cloudPos;
        tx.setRotation(cro::Transform::Y_AXIS, std::atan2(-facing.z, facing.x) - (cro::Util::Const::degToRad * 90.f));
    }
}

void CloudSystem::setWindVector(glm::vec3 vector)
{
    m_targetWindSpeed = vector;
}