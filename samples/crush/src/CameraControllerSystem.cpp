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

#include "CameraControllerSystem.hpp"
#include "PlayerSystem.hpp"
#include "GameConsts.hpp"

#include <crogine/ecs/components/Transform.hpp>

CameraControllerSystem::CameraControllerSystem(cro::MessageBus& mb, const std::array<cro::Entity, 4u>& avatars)
    : cro::System   (mb, typeid(CameraControllerSystem)),
    m_avatars       (avatars)
{
    requireComponent<CameraController>();
    requireComponent<cro::Transform>();
}

//public
void CameraControllerSystem::process(float dt)
{
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& controller = entity.getComponent<CameraController>();

        auto& player = controller.targetPlayer.getComponent<Player>();
        if (!m_avatars[player.cameraTargetIndex].isValid())
        {
            //skip to next valid index
            do
            {
                player.cameraTargetIndex = (player.cameraTargetIndex + 1) % 4;
            } while ((!m_avatars[player.cameraTargetIndex].isValid()));
        }
        auto targetPos = m_avatars[player.cameraTargetIndex].getComponent<cro::Transform>().getWorldPosition();

        //rotate the camera depending on the Z depth of the player
        static const float TotalDistance = LayerDepth * 2.f;
        auto position = targetPos.z;
        float currentDistance = LayerDepth - position;
        currentDistance /= TotalDistance;

        auto& tx = entity.getComponent<cro::Transform>();
        tx.setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI * currentDistance);

        //and follow the root node
        auto dir = targetPos - tx.getPosition();
        tx.move(dir * 10.f * dt);
    }
}