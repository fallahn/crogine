/*-----------------------------------------------------------------------

Matt Marchant 2020
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

#include "FpsCameraSystem.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/util/Maths.hpp>

namespace
{
    const float FlyMultiplier = 15.f;
}

FpsCameraSystem::FpsCameraSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(FpsCameraSystem))
{
    requireComponent<FpsCamera>();
    requireComponent<cro::Transform>();
}

//public
void FpsCameraSystem::process(float dt)
{
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& controller = entity.getComponent<FpsCamera>();
        auto& input = controller.currentInput;

        const float moveScale = 0.004f;
        float pitchMove = static_cast<float>(-input.yMove) * moveScale;
        float yawMove = static_cast<float>(-input.xMove) * moveScale;

        //clamp pitch
        float newPitch = controller.cameraPitch + pitchMove;
        const float clamp = 1.4f;
        if (newPitch > clamp)
        {
            pitchMove -= (newPitch - clamp);
            controller.cameraPitch = clamp;
        }
        else if (newPitch < -clamp)
        {
            pitchMove -= (newPitch + clamp);
            controller.cameraPitch = -clamp;
        }
        else
        {
            controller.cameraPitch = newPitch;
        }

        //we need to clamp this to TAU (or ideally +- PI) else more than one rotation
        //introduces very visible jitter
        //player.cameraYaw = std::fmod(player.cameraYaw + yawMove, cro::Util::Const::TAU);
        controller.cameraYaw += yawMove;
        if (controller.cameraYaw < -cro::Util::Const::PI)
        {
            controller.cameraYaw += cro::Util::Const::TAU;
        }
        else if (controller.cameraYaw > cro::Util::Const::PI)
        {
            controller.cameraYaw -= cro::Util::Const::TAU;
        }

        glm::quat pitch = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), pitchMove, glm::vec3(1.f, 0.f, 0.f));
        glm::quat yaw = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), yawMove, glm::vec3(0.f, 1.f, 0.f));

        auto& tx = entity.getComponent<cro::Transform>();
        auto rotation = yaw * tx.getRotation() * pitch;
        tx.setRotation(rotation);


        //we only want to rotate around the yaw when walking
        rotation = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), controller.cameraYaw, glm::vec3(0.f, 1.f, 0.f));
        glm::vec3 forwardVector = rotation * glm::vec3(0.f, 0.f, -1.f);
        glm::vec3 rightVector = rotation * glm::vec3(1.f, 0.f, 0.f);


        //walking speed in metres per second
        float moveSpeed = controller.moveSpeed * dt;

        if (controller.flyMode)
        {
            moveSpeed *= FlyMultiplier;
        }

        if (input.buttonFlags & Input::Forward)
        {
            tx.move(forwardVector * moveSpeed);
        }
        if (input.buttonFlags & Input::Backward)
        {
            tx.move(-forwardVector * moveSpeed);
        }

        if (input.buttonFlags & Input::Left)
        {
            tx.move(-rightVector * moveSpeed);
        }
        if (input.buttonFlags & Input::Right)
        {
            tx.move(rightVector * moveSpeed);
        }

        if (controller.flyMode)
        {
            if (input.buttonFlags & Input::Jump)
            {
                tx.move(glm::vec3(0.f, 1.f, 0.f) * moveSpeed);
            }
            if (input.buttonFlags & Input::Crouch)
            {
                tx.move(glm::vec3(0.f, -1.f, 0.f) * moveSpeed);
            }
        }
        else
        {
            //add gravity / any jump impulse
        }
    }
}