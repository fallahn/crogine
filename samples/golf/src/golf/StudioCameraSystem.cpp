/*-----------------------------------------------------------------------

Matt Marchant - 2022
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

#include "StudioCameraSystem.hpp"

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>

#include <crogine/util/Maths.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    constexpr float CameraRadius = 0.8f;
    constexpr float CameraHeight = 0.4f;
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
        if (entity.getComponent<cro::Camera>().active)
        {
            auto& cam = entity.getComponent<StudioCamera>();
            cam.switchTime -= dt;

            if (cam.target.isValid())
            {
                //position cam always opposite the target
                auto targetPos = cam.target.getComponent<cro::Transform>().getPosition();

                auto sgn = cro::Util::Maths::sgn(targetPos.z);
                auto camPos = glm::normalize(glm::vec3((CameraRadius / 2.f) * sgn, CameraHeight, CameraRadius * -sgn));

                auto currentPos = entity.getComponent<cro::Transform>().getPosition();
                auto camDiff = camPos - currentPos;
                camPos = currentPos + (camDiff * dt);

                //interp the actual look-at point towards the target
                auto targetDiff = targetPos - cam.cameraTarget;
                cam.cameraTarget += targetDiff * (dt * 3.f);


                //set the final transform of the camera
                auto tx = glm::inverse(glm::lookAt(camPos, cam.cameraTarget, cro::Transform::Y_AXIS));
                entity.getComponent<cro::Transform>().setLocalTransform(tx);


                //stop following the target if it drops
                if (targetPos.y < 0)
                {
                    cam.target = {};
                }

                //TODO detect the target radius and test
                //for pockets to trigger a zoom
            }
        }
    }
}