/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

#include <crogine/core/GameController.hpp>
#include <crogine/core/Keyboard.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/util/Maths.hpp>

#include <crogine/gui/Gui.hpp>

namespace
{
    const float FlyMultiplier = 15.f;
}

VoxelFpsCameraSystem::VoxelFpsCameraSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(VoxelFpsCameraSystem))
{
    requireComponent<VoxelFpsCamera>();
    requireComponent<cro::Transform>();

    /*registerWindow([&]() 
        {
            if (ImGui::Begin("FPS Cam"))
            {
                const auto& input = m_inputs[0];
                ImGui::Text("Flags: %u", input.buttonFlags);

            }
            ImGui::End();
        });*/
}

//public
void VoxelFpsCameraSystem::handleEvent(const cro::Event& evt)
{
    //TODO add button mapping. Probably only map one
    //set of keys as it's unlikely people will play with
    //two mice, if that's even possible..?
    switch (evt.type)
    {
    default: break;
    case SDL_KEYDOWN:
        switch (evt.key.keysym.sym)
        {
        default:break;
        case SDLK_w:
            m_inputs[0].buttonFlags |= Input::Forward;
            break;
        case SDLK_s:
            m_inputs[0].buttonFlags |= Input::Backward;
            break;
        case SDLK_a:
            m_inputs[0].buttonFlags |= Input::Left;
            break;
        case SDLK_d:
            m_inputs[0].buttonFlags |= Input::Right;
            break;
        case SDLK_SPACE:
            m_inputs[0].buttonFlags |= Input::Jump;
            break;
        case SDLK_LCTRL:
            m_inputs[0].buttonFlags |= Input::Crouch;
            break;
        }
        break;
    case SDL_KEYUP:
        switch (evt.key.keysym.sym)
        {
        default:break;
        case SDLK_w:
            m_inputs[0].buttonFlags &= ~Input::Forward;
            break;
        case SDLK_s:
            m_inputs[0].buttonFlags &= ~Input::Backward;
            break;
        case SDLK_a:
            m_inputs[0].buttonFlags &= ~Input::Left;
            break;
        case SDLK_d:
            m_inputs[0].buttonFlags &= ~Input::Right;
            break;
        case SDLK_SPACE:
            m_inputs[0].buttonFlags &= ~Input::Jump;
            break;
        case SDLK_LCTRL:
            m_inputs[0].buttonFlags &= ~Input::Crouch;
            break;
        }
        break;
    case SDL_MOUSEBUTTONDOWN:
        switch (evt.button.button)
        {
        default: break;
        case SDL_BUTTON_LEFT:
            m_inputs[0].buttonFlags |= Input::LeftMouse;
            break;
        case SDL_BUTTON_MIDDLE:
            m_inputs[0].buttonFlags |= Input::RightMouse;
            break;
        }
        break;
    case SDL_MOUSEBUTTONUP:
        switch (evt.button.button)
        {
        default: break;
        case SDL_BUTTON_LEFT:
            m_inputs[0].buttonFlags &= ~Input::LeftMouse;
            break;
        case SDL_BUTTON_MIDDLE:
            m_inputs[0].buttonFlags &= ~Input::RightMouse;
            break;
        }
        break;
    case SDL_MOUSEMOTION:
        if (m_inputs[0].buttonFlags & Input::RightMouse)
        {
            m_inputs[0].xMove += evt.motion.xrel;
            m_inputs[0].yMove += evt.motion.yrel;
        }
    break;
    case SDL_MOUSEWHEEL:
        if (!cro::Keyboard::isKeyPressed(SDLK_LSHIFT))
        {
            m_inputs[0].forwardMotion = evt.wheel.y;
        }
        break;


    //parse controller presses into corresponding inputs
    case SDL_CONTROLLERBUTTONDOWN:
    {
        auto controllerID = cro::GameController::controllerID(evt.cbutton.which);
        if (controllerID != -1)
        {
            CRO_ASSERT(controllerID < m_inputs.size(), "OUT OF RANGE");
            switch (evt.cbutton.button)
            {
            default: break;
            case cro::GameController::ButtonA:
                m_inputs[controllerID].buttonFlags |= Input::Flags::Jump;
                break;
            case cro::GameController::ButtonB:
                m_inputs[controllerID].buttonFlags |= Input::Flags::Crouch;
                break;
            case cro::GameController::ButtonX:

                break;
            case cro::GameController::ButtonY:
                
                break;
            case cro::GameController::DPadUp:
                m_inputs[controllerID].buttonFlags |= Input::Flags::Forward;
                break;
            case cro::GameController::DPadDown:
                m_inputs[controllerID].buttonFlags |= Input::Flags::Backward;
                break;
            case cro::GameController::DPadLeft:
                m_inputs[controllerID].buttonFlags |= Input::Flags::Left;
                break;
            case cro::GameController::DPadRight:
                m_inputs[controllerID].buttonFlags |= Input::Flags::Right;
                break;
            }
        }
    }
        break;
    case SDL_CONTROLLERBUTTONUP:
    {
        auto controllerID = cro::GameController::controllerID(evt.cbutton.which);
        if (controllerID != -1)
        {
            CRO_ASSERT(controllerID < m_inputs.size(), "OUT OF RANGE");
            switch (evt.cbutton.button)
            {
            default: break;
            case cro::GameController::ButtonA:
                m_inputs[controllerID].buttonFlags &= ~Input::Flags::Jump;
                break;
            case cro::GameController::ButtonB:
                m_inputs[controllerID].buttonFlags &= ~Input::Flags::Crouch;
                break;
            case cro::GameController::ButtonX:

                break;
            case cro::GameController::ButtonY:

                break;
            case cro::GameController::DPadUp:
                m_inputs[controllerID].buttonFlags &= ~Input::Flags::Forward;
                break;
            case cro::GameController::DPadDown:
                m_inputs[controllerID].buttonFlags &= ~Input::Flags::Backward;
                break;
            case cro::GameController::DPadLeft:
                m_inputs[controllerID].buttonFlags &= ~Input::Flags::Left;
                break;
            case cro::GameController::DPadRight:
                m_inputs[controllerID].buttonFlags &= ~Input::Flags::Right;
                break;
            }
        }
    }
        break;
    }
}

void VoxelFpsCameraSystem::process(float dt)
{
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& controller = entity.getComponent<VoxelFpsCamera>();
        auto& input = m_inputs[controller.controllerIndex];

        static constexpr float MoveScale = 0.004f;
        float pitchMove = static_cast<float>(-input.yMove) * MoveScale * controller.lookSensitivity;
        pitchMove *= controller.yInvert;

        float yawMove = static_cast<float>(-input.xMove) * MoveScale * controller.lookSensitivity;
        
        input.xMove = input.yMove = 0;

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
        //rotation = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), controller.cameraYaw, glm::vec3(0.f, 1.f, 0.f));
        glm::vec3 forwardVector = tx.getForwardVector();// rotation* glm::vec3(0.f, 0.f, -1.f);
        glm::vec3 rightVector = tx.getRightVector();// rotation* glm::vec3(1.f, 0.f, 0.f);


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

        tx.move(forwardVector * moveSpeed * static_cast<float>(input.forwardMotion));
        input.forwardMotion = 0;


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

bool VoxelFpsCameraSystem::hasInput() const
{
    return m_inputs[0].buttonFlags != 0;
}

//private
void VoxelFpsCameraSystem::onEntityAdded(cro::Entity entity)
{
    entity.getComponent<VoxelFpsCamera>().resetOrientation(entity);
}