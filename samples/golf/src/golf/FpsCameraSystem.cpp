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
#include "MessageIDs.hpp"
#include "SharedStateData.hpp"
#include "CameraFollowSystem.hpp"

#include <crogine/core/GameController.hpp>
#include <crogine/core/Keyboard.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/util/Maths.hpp>

namespace
{
    constexpr float TransitionSpeed = 5.f;

    constexpr float MaxSprintMultiplier = 15.f;
    constexpr float MinHeight = 0.3f;
    constexpr std::int32_t WheelZoomMultiplier = 4;

    constexpr std::int16_t MinTriggerMovement = 4000;

    struct RollingAvg final
    {
        static constexpr std::size_t AvgSize = 3;
        std::array<float, AvgSize> vals = {};

        std::int32_t idx = 0;
        float total = 0.f;

        RollingAvg()
        {
            std::fill(vals.begin(), vals.end(), 0.f);
        }

        float getAvg()
        {
            total += vals[idx];
            auto retVal = total / AvgSize;


            idx = (idx + 1) % AvgSize;
            total -= vals[idx];
            vals[idx] = 0;

            return retVal;
        }

        void addValue(std::int8_t v)
        {
            vals[idx] += v;
        }
    };
    RollingAvg xAvg;
    RollingAvg yAvg;
}

FpsCameraSystem::FpsCameraSystem(cro::MessageBus& mb, const CollisionMesh& cm, const SharedStateData& sd)
    : cro::System       (mb, typeid(FpsCameraSystem)),
    m_collisionMesh     (cm),
    m_sharedData        (sd),
    m_humanCount        (1),
    m_controllerID      (0),
    m_triggerAmount     (0),
    m_sprintMultiplier  (1.f),
    m_analogueMultiplier(1.f),
    m_inputAcceleration (0.f),
    m_forwardAmount     (1.f),
    m_sideAmount        (1.f),
    m_upAmount          (1.f)
{
    requireComponent<FpsCamera>();
    requireComponent<cro::Transform>();

    //registerWindow([&]() 
    //    {
    //        if (ImGui::Begin("FPS Cam"))
    //        {
    //            //ImGui::Text("Multiplier %3.5f", m_analogueMultiplier);
    //            //ImGui::Text("Forward %3.5f", fwd);
    //            //ImGui::Text("Side %3.5f", swd);

    //            ImGui::Text("X Avg: %3.3f", xAvg.total / RollingAvg::AvgSize);
    //            ImGui::Text("X Move: %d", xDebug);
    //        }
    //        ImGui::End();
    //    });
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
        if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Up])
        {
            //actually forward...
            m_input.buttonFlags |= Input::Forward;
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Down])
        {
            //actually backward...
            m_input.buttonFlags |= Input::Backward;
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Left])
        {
            m_input.buttonFlags |= Input::Left;
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Right])
        {
            m_input.buttonFlags |= Input::Right;
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Action])
        {
            //actually up...
            m_input.buttonFlags |= Input::Up;
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::EmoteMenu])
        {
            //actually down
            m_input.buttonFlags |= Input::Down;
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::NextClub])
        {
            m_input.buttonFlags |= Input::ZoomIn;
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::PrevClub])
        {
            m_input.buttonFlags |= Input::ZoomOut;
        }
        else if (evt.key.keysym.sym == SDLK_LSHIFT)
        {
            //we toggle this in keyup now
            //m_input.buttonFlags |= Input::Sprint;
        }

        break;
    case SDL_KEYUP:
        if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Up])
        {
            //actually forward...
            m_input.buttonFlags &= ~Input::Forward;
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Down])
        {
            //actually backward...
            m_input.buttonFlags &= ~Input::Backward;
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Left])
        {
            m_input.buttonFlags &= ~Input::Left;
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Right])
        {
            m_input.buttonFlags &= ~Input::Right;
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Action])
        {
            //actually up...
            m_input.buttonFlags &= ~Input::Up;
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::EmoteMenu])
        {
            //actually down
            m_input.buttonFlags &= ~Input::Down;
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::NextClub])
        {
            m_input.buttonFlags &= ~Input::ZoomIn;
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::PrevClub])
        {
            m_input.buttonFlags &= ~Input::ZoomOut;
        }
        else if (evt.key.keysym.sym == SDLK_LSHIFT)
        {
            if (m_input.buttonFlags & Input::Sprint)
            {
                m_input.buttonFlags &= ~Input::Sprint;
            }
            else
            {
                m_input.buttonFlags |= Input::Sprint;
            }
        }
        break;
    case SDL_MOUSEBUTTONDOWN:
        switch (evt.button.button)
        {
        default: break;
        case SDL_BUTTON_RIGHT:
            //m_input.buttonFlags |= Input::Sprint;
            break;
        case SDL_BUTTON_LEFT:
            Social::takeScreenshot(m_screenshotLocation, m_sharedData.courseIndex);
            break;
        }
        break;
    case SDL_MOUSEBUTTONUP:
        switch (evt.button.button)
        {
        default: break;
        case SDL_BUTTON_RIGHT:
            if (m_input.buttonFlags & Input::Sprint)
            {
                m_input.buttonFlags &= ~Input::Sprint;
            }
            else
            {
                m_input.buttonFlags |= Input::Sprint;
            }
            break;
        case SDL_BUTTON_LEFT:
            //m_input.buttonFlags &= ~Input::Sprint;
            break;
        }
        break;
    case SDL_MOUSEMOTION:
        m_input.xMove += evt.motion.xrel;
        m_input.yMove += evt.motion.yrel;

        xAvg.addValue(evt.motion.xrel);
        yAvg.addValue(evt.motion.yrel);
    break;
    case SDL_MOUSEWHEEL:
        m_input.wheel = evt.wheel.y * WheelZoomMultiplier;
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
                Social::takeScreenshot(m_screenshotLocation, m_sharedData.courseIndex);
                break;
            /*case cro::GameController::ButtonB:
                //this quits the mode
                m_input.buttonFlags |= Input::Flags::Down;
                break;*/
            case cro::GameController::ButtonLeftStick:
                //m_input.buttonFlags |= Input::Flags::Sprint;
                break;
            case cro::GameController::ButtonX:
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
            case cro::GameController::ButtonLeftStick:
                //m_input.buttonFlags &= ~Input::Flags::Sprint;
                if (m_input.buttonFlags & Input::Sprint)
                {
                    m_input.buttonFlags &= ~Input::Sprint;
                }
                else
                {
                    m_input.buttonFlags |= Input::Sprint;
                }
                break;
            case cro::GameController::ButtonX:
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
                    m_triggerAmount = evt.caxis.value;
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
                    m_triggerAmount = evt.caxis.value;
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
        auto& controller = entity.getComponent<FpsCamera>();

        if (controller.state == FpsCamera::State::Enter)
        {
            enterAnim(entity, dt);
        }
        else if (controller.state == FpsCamera::State::Exit)
        {
            exitAnim(entity, dt);
        }
        else
        {
            //check zoom buttons - we need to do this first so we can create a zoom speed multiplier for cam rotation
            float zoom = 0.f;
            if ((m_input.buttonFlags & Input::ZoomIn)
                || m_input.wheel > 0)
            {
                zoom = dt * std::max(1, m_input.wheel);
            }
            else if ((m_input.buttonFlags & Input::ZoomOut)
                || m_input.wheel < 0)
            {
                zoom = -dt * std::max(1, std::abs(m_input.wheel));
            }
            m_input.wheel = 0;

            if (zoom)
            {
                controller.zoomProgress = std::clamp(controller.zoomProgress + zoom, 0.f, 1.f);
                controller.fov = glm::mix(FpsCamera::MinZoom, FpsCamera::MaxZoom, cro::Util::Easing::easeOutExpo(cro::Util::Easing::easeInQuad(controller.zoomProgress)));
                entity.getComponent<cro::Camera>().resizeCallback(entity.getComponent<cro::Camera>());
            }
            float zoomSpeed = 1.f - controller.zoomProgress;
            zoomSpeed = 0.2f + (0.8f * zoomSpeed);

            auto& tx = entity.getComponent<cro::Transform>();

            //do mouselook if there's any recorded movement
            auto yMove = -yAvg.getAvg(); //this makes sure to reset and increment the current frame
            auto xMove = -xAvg.getAvg();

            if (m_input.xMove + m_input.yMove != 0)
            {
                static constexpr float MoveScale = 0.002f;
                float pitchMove = yMove * MoveScale * m_sharedData.mouseSpeed * zoomSpeed;
                pitchMove *= m_sharedData.invertY ? -1.f : 1.f;

                float yawMove = xMove * MoveScale * m_sharedData.mouseSpeed * zoomSpeed;
                yawMove *= m_sharedData.invertX ? -1.f : 1.f;

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
                    axisRotation.y = -(static_cast<float>(controllerX) / cro::GameController::AxisMax);
                    axisRotation.y *= m_sharedData.invertX ? -1.f : 1.f;
                    axisRotation.y *= m_sharedData.mouseSpeed;
                }
                if (std::abs(controllerY) > LeftThumbDeadZone)
                {
                    axisRotation.x = -(static_cast<float>(controllerY) / cro::GameController::AxisMax);
                    axisRotation.x *= m_sharedData.invertY ? -1.f : 1.f;
                    axisRotation.x *= m_sharedData.mouseSpeed;
                }

                if (auto len2 = glm::length2(axisRotation); len2 != 0)
                {
                    axisRotation = glm::normalize(axisRotation) * std::min(1.f, std::pow(std::sqrt(len2), 5.f));
                }

                auto invRotation = glm::inverse(tx.getRotation());
                auto up = invRotation * cro::Transform::Y_AXIS;

                tx.rotate(up, axisRotation.y * zoomSpeed * dt);
                tx.rotate(cro::Transform::X_AXIS, axisRotation.x * zoomSpeed * dt);
            }



            //walking speed in metres per second
            float moveSpeed = controller.moveSpeed * dt;

            if (m_input.buttonFlags & Input::Flags::Walk)
            {
                moveSpeed *= 0.05f;
            }



            //move in the direction we're facing
            auto forwardVector = tx.getForwardVector() * m_forwardAmount;
            auto rightVector = tx.getRightVector() * m_sideAmount * zoomSpeed;
            glm::vec3 movement(0.f);

            if (m_input.buttonFlags & Input::Forward)
            {
                movement += forwardVector;
            }
            if (m_input.buttonFlags & Input::Backward)
            {
                movement -= forwardVector;
            }

            if (m_input.buttonFlags & Input::Left)
            {
                movement -= rightVector;
            }
            if (m_input.buttonFlags & Input::Right)
            {
                movement += rightVector;
            }


            auto vMovement = glm::vec3(0.f);
            if (m_input.buttonFlags & Input::Up)
            {
                vMovement += cro::Transform::Y_AXIS;
            }
            if (m_input.buttonFlags & Input::Down)
            {
                vMovement -= cro::Transform::Y_AXIS;
            }

            //apply acceleration if *any* movement
            if (glm::length2(movement + vMovement) != 0)
            {
                static constexpr float SprintAcceleration = 25.f;
                //smooth sprint multiplier
                if (m_input.buttonFlags & Input::Sprint)
                {
                    m_sprintMultiplier = std::min(MaxSprintMultiplier, m_sprintMultiplier + (dt * SprintAcceleration));
                }
                else
                {
                    m_sprintMultiplier = std::max(1.f, m_sprintMultiplier - ((dt * SprintAcceleration) * 3.f));
                }
            }
            else
            {
                m_sprintMultiplier = 1.f;
            }


            if (glm::length2(movement) != 0)
            {
                movement = glm::normalize(movement) * moveSpeed * m_analogueMultiplier * m_sprintMultiplier;
                tx.move(movement);
            }

            if (glm::length2(vMovement) != 0)
            {
                tx.move(vMovement * moveSpeed * m_upAmount * m_sprintMultiplier);
            }

            //clamp above the ground
            auto pos = tx.getPosition();
            auto result = m_collisionMesh.getTerrain(pos);
            if (auto diff = pos.y - std::max(WaterLevel, result.height); diff < MinHeight)
            {
                tx.move({ 0.f, MinHeight - diff, 0.f });
                pos = tx.getPosition();
            }
            
            //and within the map
            pos.x = std::clamp(pos.x, 0.f, static_cast<float>(MapSize.x));
            pos.y = std::min(pos.y, 80.f);
            pos.z = std::clamp(pos.z, -static_cast<float>(MapSize.y), 0.f);
            tx.setPosition(pos);


            glm::vec3 intersection(0.f);
            if (entity.getComponent<TargetInfo>().waterPlane.isValid()
                && planeIntersect(tx.getWorldTransform(), intersection))
            {
                intersection.y = WaterLevel;
                entity.getComponent<TargetInfo>().waterPlane.getComponent<cro::Callback>().setUserData<glm::vec3>(intersection);
            }
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
        m_forwardAmount = 1.f;
        m_sideAmount = 1.f;
    }
    else
    {
        m_inputAcceleration = 0.f;
    }

    if (m_input.buttonFlags & (Input::Up | Input::Down))
    {
        m_upAmount = 1.f;
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
        m_forwardAmount = std::abs(yPos / cro::GameController::AxisMax);
        m_sideAmount = std::abs(xPos / cro::GameController::AxisMax);
    }

    if (m_triggerAmount)
    {
        m_upAmount = static_cast<float>(m_triggerAmount) / cro::GameController::AxisMax;
    }

    if (startInput ^ m_input.buttonFlags)
    {
        m_input.prevStick = m_input.buttonFlags;
    }
}

void FpsCameraSystem::enterAnim(cro::Entity entity, float dt)
{
    auto& fpsCam = entity.getComponent<FpsCamera>();

    fpsCam.transition.progress = std::min(1.f, fpsCam.transition.progress + (dt * TransitionSpeed));
    
    const auto t = cro::Util::Easing::easeOutExpo(fpsCam.transition.progress);
    auto pos = glm::mix(fpsCam.transition.startPosition, fpsCam.transition.endPosition, t);
    auto rot = glm::slerp(fpsCam.transition.startRotation, fpsCam.transition.endRotation, t);

    auto& tx = entity.getComponent<cro::Transform>();
    tx.setPosition(pos);
    tx.setRotation(rot);

    if (fpsCam.fov != fpsCam.transition.endFov)
    {
        fpsCam.fov = glm::mix(fpsCam.transition.startFov, fpsCam.transition.endFov, t);
        entity.getComponent<cro::Camera>().resizeCallback(entity.getComponent<cro::Camera>());
    }

    if (fpsCam.transition.progress == 1)
    {
        fpsCam.state = FpsCamera::State::Active;
    }
}

void FpsCameraSystem::exitAnim(cro::Entity entity, float dt)
{
    auto& fpsCam = entity.getComponent<FpsCamera>();

    fpsCam.transition.progress = std::max(0.f, fpsCam.transition.progress - (dt * TransitionSpeed));

    const auto t = cro::Util::Easing::easeInQuint(fpsCam.transition.progress);
    auto pos = glm::mix(fpsCam.transition.startPosition, fpsCam.transition.endPosition, t);
    auto rot = glm::slerp(fpsCam.transition.startRotation, fpsCam.transition.endRotation, t);

    auto& tx = entity.getComponent<cro::Transform>();
    tx.setPosition(pos);
    tx.setRotation(rot);

    if (fpsCam.fov != fpsCam.transition.startFov)
    {
        fpsCam.fov = glm::mix(fpsCam.transition.startFov, fpsCam.transition.endFov, t);
        entity.getComponent<cro::Camera>().resizeCallback(entity.getComponent<cro::Camera>());
    }

    if (fpsCam.transition.progress == 0)
    {
        fpsCam.transition.completionCallback();
    }
}