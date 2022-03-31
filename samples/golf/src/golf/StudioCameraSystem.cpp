/*-----------------------------------------------------------------------

Matt Marchant - 2022
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

#include "StudioCameraSystem.hpp"

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    constexpr float CameraRadius = 1.3f;
    constexpr float CameraHeight = 0.6f;
}

StudioCameraSystem::StudioCameraSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(StudioCameraSystem))
{
    requireComponent<StudioCamera>();
    requireComponent<cro::Transform>();
    requireComponent<cro::Camera>();
}

//public
void StudioCameraSystem::process(float dt)
{
    for (auto entity : getEntities())
    {
        //if (entity.getComponent<cro::Camera>().active)
        {
            auto& cam = entity.getComponent<StudioCamera>();
            if (cam.target.isValid())
            {
                //use the opposite vector of the target to table centre
                //to position cam always opposite
                auto targetPos = cam.target.getComponent<cro::Transform>().getPosition();
                auto camPos = glm::normalize(glm::vec3(-targetPos.x, 0.f, -targetPos.y)) * CameraRadius;
                camPos.y = CameraHeight;


                //interp the actual look-at point towards the target
                auto targetDiff = targetPos - cam.cameraTarget;
                cam.cameraTarget += targetDiff * (dt * 4.f);


                //set the final transform of the camera
                auto tx = glm::inverse(glm::lookAt(camPos, cam.cameraTarget, cro::Transform::Y_AXIS));
                entity.getComponent<cro::Transform>().setLocalTransform(tx);


                //TODO detect the target radius and test
                //for pockets to trigger a zoom
            }
        }
    }
}