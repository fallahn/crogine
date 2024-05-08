/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2024
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

#include "FpsCameraSystem.hpp"
#include "CollisionMesh.hpp"
#include "InputBinding.hpp"
#include "MessageIDs.hpp"

#include <crogine/core/GameController.hpp>
#include <crogine/core/Keyboard.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/util/Maths.hpp>

namespace
{
    constexpr float FlyMultiplier = 15.f;
    constexpr float MinHeight = 0.3f;

    constexpr std::int16_t MinTriggerMovement = 12000;
}

FpsCameraSystem::FpsCameraSystem(cro::MessageBus& mb, const CollisionMesh& cm, const InputBinding& ib)
    : cro::System       (mb, typeid(FpsCameraSystem)),
    m_collisionMesh     (cm),
    m_inputBinding      (ib),
    m_humanCount        (1),
    m_controllerID      (0),
    m_analogueMultiplier(1.f),
    m_inputAcceleration (0.f)
{
    requireComponent<FpsCamera>();
    requireComponent<cro::Transform>();
}

//public
void FpsCameraSystem::handleEvent(const cro::Event& evt)
{
    //hmmm what happens if we unplug this controller?
    //I guess keyboard is the default in this case
    const auto acceptInput = [&](std::int32_t joyID)
        {
            return m_humanCount == 1
                || joyID == m_controllerID;
        };

    switch (evt.type)
    {
    default: break;
    case SDL_KEYDOWN:
        if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Up])
        {
            //actually forward...
            m_input.buttonFlags |= Input::Forward;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Down])
        {
            //actually backward...
            m_input.buttonFlags |= Input::Backward;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Left])
        {
            m_input.buttonFlags |= Input::Left;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Right])
        {
            m_input.buttonFlags |= Input::Right;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Action])
        {
            //actually up...
            m_input.buttonFlags |= Input::Up;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::EmoteMenu])
        {
            //actually down
            m_input.buttonFlags |= Input::Down;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::NextClub])
        {
            m_input.buttonFlags |= Input::ZoomIn;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::PrevClub])
        {
            m_input.buttonFlags |= Input::ZoomOut;
        }
        else if (evt.key.keysym.sym == SDLK_LSHIFT)
        {
            m_input.buttonFlags |= Input::Sprint;
        }

        break;
    case SDL_KEYUP:
        if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Up])
        {
            //actually forward...
            m_input.buttonFlags &= ~Input::Forward;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Down])
        {
            //actually backward...
            m_input.buttonFlags &= ~Input::Backward;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Left])
        {
            m_input.buttonFlags &= ~Input::Left;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Right])
        {
            m_input.buttonFlags &= ~Input::Right;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Action])
        {
            //actually up...
            m_input.buttonFlags &= ~Input::Up;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::EmoteMenu])
        {
            //actually down
            m_input.buttonFlags &= ~Input::Down;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::NextClub])
        {
            m_input.buttonFlags &= ~Input::ZoomIn;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::PrevClub])
        {
            m_input.buttonFlags &= ~Input::ZoomOut;
        }
        else if (evt.key.keysym.sym == SDLK_LSHIFT)
        {
            m_input.buttonFlags &= ~Input::Sprint;
        }
        break;
    case SDL_MOUSEBUTTONDOWN:
        switch (evt.button.button)
        {
        default: break;
        case 0:
            Social::takeScreenshot();
            break;
        case 1:
            m_input.buttonFlags |= Input::Sprint;
            break;
        }
        break;
    case SDL_MOUSEBUTTONUP:
        switch (evt.button.button)
        {
        default: break;
        case 0:
            //m_input.buttonFlags &= ~Input::Sprint;
            break;
        case 1:
            m_input.buttonFlags &= ~Input::Sprint;
            break;
        }
        break;
    case SDL_MOUSEMOTION:
        m_input.xMove += evt.motion.xrel;
        m_input.yMove += evt.motion.yrel;
    break;


    //parse controller presses into corresponding inputs
    case SDL_CONTROLLERBUTTONDOWN:
    {
        if (acceptInput(evt.cbutton.which))
        {
            //CRO_ASSERT(controllerID < m_inputs.size(), "OUT OF RANGE");
            switch (evt.cbutton.button)
            {
            default: break;
            case cro::GameController::ButtonA:
                //m_input.buttonFlags |= Input::Flags::Up;
                Social::takeScreenshot();
                break;
            /*case cro::GameController::ButtonB:
                //this quits the mode
                m_input.buttonFlags |= Input::Flags::Down;
                break;*/
            case cro::GameController::ButtonX:
                m_input.buttonFlags |= Input::Flags::Sprint;
                break;
            case cro::GameController::ButtonY:
                m_input.buttonFlags |= Input::Flags::Walk;
                break;
            /*case cro::GameController::DPadUp:
                m_input.buttonFlags |= Input::Flags::Forward;
                break;
            case cro::GameController::DPadDown:
                m_input.buttonFlags |= Input::Flags::Backward;
                break;
            case cro::GameController::DPadLeft:
                m_input.buttonFlags |= Input::Flags::Left;
                break;
            case cro::GameController::DPadRight:
                m_input.buttonFlags |= Input::Flags::Right;
                break;*/
            case cro::GameController::ButtonRightShoulder:
                m_input.buttonFlags |= Input::Flags::ZoomIn;
                break;
            case cro::GameController::ButtonLeftShoulder:
                m_input.buttonFlags |= Input::Flags::ZoomOut;
                break;
            }
        }
    }
        break;
    case SDL_CONTROLLERBUTTONUP:
    {
        if (acceptInput(evt.cbutton.which))
        {
            switch (evt.cbutton.button)
            {
            default: break;
            /*case cro::GameController::ButtonA:
                m_input.buttonFlags &= ~Input::Flags::Up;
                break;
            case cro::GameController::ButtonB:
                m_input.buttonFlags &= ~Input::Flags::Down;
                break;*/
            case cro::GameController::ButtonX:
                m_input.buttonFlags &= ~Input::Flags::Sprint;
                break;
            case cro::GameController::ButtonY:
                m_input.buttonFlags &= ~Input::Flags::Walk;
                break;
            case cro::GameController::DPadDown:
            {
                auto* msg = postMessage<SceneEvent>(cl::MessageID::SceneMessage);
                msg->type = SceneEvent::RequestToggleFreecam;
                msg->data = evt.cbutton.button;
            }
                break;
            /*case cro::GameController::DPadUp:
                m_input.buttonFlags &= ~Input::Flags::Forward;
                break;
            case cro::GameController::DPadDown:
                m_input.buttonFlags &= ~Input::Flags::Backward;
                break;
            case cro::GameController::DPadLeft:
                m_input.buttonFlags &= ~Input::Flags::Left;
                break;
            case cro::GameController::DPadRight:
                m_input.buttonFlags &= ~Input::Flags::Right;
                break;*/
            case cro::GameController::ButtonRightShoulder:
                m_input.buttonFlags &= ~Input::Flags::ZoomIn;
                break;
            case cro::GameController::ButtonLeftShoulder:
                m_input.buttonFlags &= ~Input::Flags::ZoomOut;
                break;
            }
        }
    }
        break;
    case SDL_CONTROLLERAXISMOTION:
        if (acceptInput(evt.caxis.which))
        {
            switch (evt.caxis.axis)
            {
            default: break;
            case cro::GameController::TriggerLeft:
                if (evt.caxis.value > MinTriggerMovement)
                {
                    m_input.buttonFlags |= Input::Flags::Down;
                }
                else
                {
                    m_input.buttonFlags &= ~Input::Flags::Down;
                }
                break;
            case cro::GameController::TriggerRight:
                if (evt.caxis.value > MinTriggerMovement)
                {
                    m_input.buttonFlags |= Input::Flags::Up;
                }
                else
                {
                    m_input.buttonFlags &= ~Input::Flags::Up;
                }
                break;
            case cro::GameController::AxisLeftX:
            case cro::GameController::AxisLeftY:
            case cro::GameController::AxisRightX:
            case cro::GameController::AxisRightY:
                m_thumbsticks.setValue(evt.caxis.axis, evt.caxis.value);
                break;
            }
        }
        break;
    }
}

void FpsCameraSystem::process(float dt)
{
    checkControllerInput(dt);

    auto& entities = getEntities();
    for (auto entity : entities)
    {
        //TODO check zoom buttons - we need to do this first so wew can create a zoom speed multiplier for cam rotation


        auto& controller = entity.getComponent<FpsCamera>();
        auto& tx = entity.getComponent<cro::Transform>();

        //do mouselook if there's any recorded movement
        if (m_input.xMove + m_input.yMove != 0)
        {
            static constexpr float MoveScale = 0.004f;
            float pitchMove = static_cast<float>(-m_input.yMove) * MoveScale * controller.lookSensitivity;
            pitchMove *= controller.yInvert;

            float yawMove = static_cast<float>(-m_input.xMove) * MoveScale * controller.lookSensitivity;

            m_input.xMove = m_input.yMove = 0;

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

            auto rotation = tx.getRotation();
            rotation = yaw * rotation * pitch;
            tx.setRotation(rotation);
        }
        else
        {
            //see if we have any gamepad input
            auto controllerX = m_thumbsticks.getValue(cro::GameController::AxisRightX);
            auto controllerY = m_thumbsticks.getValue(cro::GameController::AxisRightY);
            glm::vec2 axisRotation = glm::vec2(0.f);

            if (std::abs(controllerX) > LeftThumbDeadZone)
            {
                //hmmm we want to read axis inversion from the settings...
                axisRotation.y = -(static_cast<float>(controllerX) / cro::GameController::AxisMax);
            }
            if (std::abs(controllerY) > LeftThumbDeadZone)
            {
                axisRotation.x = -(static_cast<float>(controllerY) / cro::GameController::AxisMax);
            }

            if (auto len2 = glm::length2(axisRotation); len2 != 0)
            {
                axisRotation = glm::normalize(axisRotation) * std::min(1.f, std::pow(std::sqrt(len2), 5.f));
            }

            auto invRotation = glm::inverse(tx.getRotation());
            auto up = invRotation * cro::Transform::Y_AXIS;

            //TODO implement zoom speed
            tx.rotate(up, axisRotation.y * /*zoomSpeed **/ dt);
            tx.rotate(cro::Transform::X_AXIS, axisRotation.x /** zoomSpeed*/ * dt);
        }


        //this only applies if we weren't 'flying' where the only rotation is around Y
        //rotation = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), controller.cameraYaw, glm::vec3(0.f, 1.f, 0.f));
        //glm::vec3 forwardVector = rotation * glm::vec3(0.f, 0.f, -1.f);
        //glm::vec3 rightVector = rotation * glm::vec3(1.f, 0.f, 0.f);


        //walking speed in metres per second
        float moveSpeed = controller.moveSpeed * dt;
        if (m_input.buttonFlags & Input::Flags::Sprint)
        {
            moveSpeed *= FlyMultiplier;
        }
        if (m_input.buttonFlags & Input::Flags::Walk)
        {
            moveSpeed *= 0.05f;
        }

        //move in the direction we're facing
        auto forwardVector = tx.getForwardVector();
        auto rightVector = tx.getRightVector();

        //TODO the forward and right vectors should
        //be combined into a single direction and normalised...
        if (m_input.buttonFlags & Input::Forward)
        {
            tx.move(forwardVector * moveSpeed * m_analogueMultiplier);
        }
        if (m_input.buttonFlags & Input::Backward)
        {
            tx.move(-forwardVector * moveSpeed * m_analogueMultiplier);
        }

        if (m_input.buttonFlags & Input::Left)
        {
            tx.move(-rightVector * moveSpeed * m_analogueMultiplier);
        }
        if (m_input.buttonFlags & Input::Right)
        {
            tx.move(rightVector * moveSpeed * m_analogueMultiplier);
        }

        //although... this is always up and down
        if (m_input.buttonFlags & Input::Up)
        {
            tx.move(glm::vec3(0.f, 1.f, 0.f) * moveSpeed);
        }
        if (m_input.buttonFlags & Input::Down)
        {
            tx.move(glm::vec3(0.f, -1.f, 0.f) * moveSpeed);
        }


        //clamp above the ground
        auto pos = tx.getPosition();
        auto result = m_collisionMesh.getTerrain(pos);
        if (auto diff = pos.y - result.height; diff < MinHeight)
        {
            tx.move({ 0.f, MinHeight - diff, 0.f });
        }
    }
}

void FpsCameraSystem::setControllerID(std::int32_t p)
{
    m_controllerID = p;
    m_thumbsticks = {};
    m_input.buttonFlags = 0;
}

//private
void FpsCameraSystem::checkControllerInput(float dt)
{
    static constexpr float MinAcceleration = 0.5f;
    if (m_input.buttonFlags & (Input::Left | Input::Right | Input::Forward | Input::Backward))
    {
        m_inputAcceleration = std::min(1.f, m_inputAcceleration + dt);
    }
    else
    {
        m_inputAcceleration = 0.f;
    }

    //default amount from keyboard input, is overwritten
    //by controller if there is any controller input.
    m_analogueMultiplier = MinAcceleration + ((1.f - MinAcceleration) * m_inputAcceleration);

    //left stick
    auto startInput = m_input.buttonFlags;
    float xPos = m_thumbsticks.getValue(cro::GameController::AxisLeftX);

    if (xPos < -LeftThumbDeadZone)
    {
        m_input.buttonFlags |= Input::Left;
    }
    else if (m_input.prevStick & Input::Left)
    {
        m_input.buttonFlags &= ~Input::Left;
    }

    if (xPos > LeftThumbDeadZone)
    {
        m_input.buttonFlags |= Input::Right;
    }
    else if (m_input.prevStick & Input::Right)
    {
        m_input.buttonFlags &= ~Input::Right;
    }

    float yPos = m_thumbsticks.getValue(cro::GameController::AxisLeftY);

    
    if (yPos < -LeftThumbDeadZone)
    {
        m_input.buttonFlags |= Input::Forward;
    }
    else if (m_input.prevStick & Input::Forward)
    {
        m_input.buttonFlags &= ~Input::Forward;
    }

    if (yPos > LeftThumbDeadZone)
    {
        m_input.buttonFlags |= Input::Backward;
    }
    else if (m_input.prevStick & Input::Backward)
    {
        m_input.buttonFlags &= ~Input::Backward;
    }


    //calc multiplier from magnitude
    float len2 = (xPos * xPos) + (yPos * yPos);
    static const float MinLen2 = static_cast<float>(LeftThumbDeadZone * LeftThumbDeadZone);
    if (len2 > MinLen2)
    {
        m_analogueMultiplier = std::min(1.f, std::pow(std::sqrt(len2) / (cro::GameController::AxisMax), 5.f));
    }

    if (startInput ^ m_input.buttonFlags)
    {
        m_input.prevStick = m_input.buttonFlags;
    }
}

void FpsCameraSystem::onEntityAdded(cro::Entity entity)
{
    entity.getComponent<FpsCamera>().resetOrientation(entity);
}