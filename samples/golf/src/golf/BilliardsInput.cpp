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

#include "BilliardsInput.hpp"
#include "MessageIDs.hpp"
#include "BilliardsSystem.hpp"
#include "SharedStateData.hpp"

#include <crogine/core/GameController.hpp>
#include <crogine/core/MessageBus.hpp>
#include <crogine/core/Mouse.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Skeleton.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/gui/Gui.hpp>
#include <crogine/util/Constants.hpp>

namespace
{
    constexpr float MaxTilt = 10.f * cro::Util::Const::degToRad;
    constexpr float MinTilt = -30.f * cro::Util::Const::degToRad;
    constexpr float CamRotationSpeed = 2.f;
    constexpr float CamRotationSpeedFast = CamRotationSpeed * 2.f;
    constexpr float CueRotationSpeed = 1.f;
    constexpr float MaxCueRotation = cro::Util::Const::PI / 16.f;
    constexpr float PowerStep = 0.1f;
    constexpr float MinPower = PowerStep;
    constexpr float MaxSpin = cro::Util::Const::PI / 6.f;

    constexpr std::int16_t DeadZone = 8000;
    constexpr float AnalogueDeadZone = 0.2f;

    glm::vec2 mouseMove(0.f);
}

BilliardsInput::BilliardsInput(const SharedStateData& sd, cro::MessageBus& mb)
    : m_sharedData          (sd),
    m_messageBus            (mb),
    m_inputFlags            (0),
    m_prevFlags             (0),
    m_prevStick             (0),
    m_mouseMove             (0.f),
    m_prevMouseMove         (0.f),
    m_analogueAmountLeft    (1.f),
    m_analogueAmountRight   (1.f),
    m_active                (false),
    m_clampRotation         (true),
    m_maxRotation           (cro::Util::Const::PI / 2.f),
    m_power                 (0.5f),
    m_topSpin               (0.f),
    m_sideSpin              (0.f),
    m_spinOffset            (1.f, 0.f, 0.f, 0.f)
{
#ifdef CRO_DEBUG_
    /*registerWindow([&]()
        {
            if (ImGui::Begin("Controls"))
            {
                ImGui::Text("Mouse Move %3.8f, %3.8f", mouseMove.x, mouseMove.y);
                ImGui::Text("Spin %3.3f, %3.3f", m_topSpin, m_sideSpin);
            }
            ImGui::End();
        });*/
#endif
}

//public
void BilliardsInput::handleEvent(const cro::Event& evt)
{
    const auto& inputBinding = m_sharedData.inputBinding;
    if (evt.type == SDL_KEYDOWN)
    {
        if (evt.key.keysym.sym == inputBinding.keys[InputBinding::Up])
        {
            m_inputFlags |= InputFlag::Up;
        }
        else if (evt.key.keysym.sym == inputBinding.keys[InputBinding::Left])
        {
            m_inputFlags |= InputFlag::Left;
        }
        else if (evt.key.keysym.sym == inputBinding.keys[InputBinding::Right])
        {
            m_inputFlags |= InputFlag::Right;
        }
        else if (evt.key.keysym.sym == inputBinding.keys[InputBinding::Down])
        {
            m_inputFlags |= InputFlag::Down;
        }
        else if (evt.key.keysym.sym == inputBinding.keys[InputBinding::Action])
        {
            m_inputFlags |= InputFlag::Action;
        }
        else if (evt.key.keysym.sym == inputBinding.keys[InputBinding::NextClub])
        {
            m_inputFlags |= InputFlag::NextClub;
        }
        else if (evt.key.keysym.sym == inputBinding.keys[InputBinding::PrevClub])
        {
            m_inputFlags |= InputFlag::PrevClub;
        }
    }
    else if (evt.type == SDL_KEYUP)
    {
        if (evt.key.keysym.sym == inputBinding.keys[InputBinding::Up])
        {
            m_inputFlags &= ~InputFlag::Up;
        }
        else if (evt.key.keysym.sym == inputBinding.keys[InputBinding::Left])
        {
            m_inputFlags &= ~InputFlag::Left;
        }
        else if (evt.key.keysym.sym == inputBinding.keys[InputBinding::Right])
        {
            m_inputFlags &= ~InputFlag::Right;
        }
        else if (evt.key.keysym.sym == inputBinding.keys[InputBinding::Down])
        {
            m_inputFlags &= ~InputFlag::Down;
        }
        else if (evt.key.keysym.sym == inputBinding.keys[InputBinding::Action])
        {
            m_inputFlags &= ~InputFlag::Action;
        }
        else if (evt.key.keysym.sym == inputBinding.keys[InputBinding::NextClub])
        {
            m_inputFlags &= ~InputFlag::NextClub;
        }
        else if (evt.key.keysym.sym == inputBinding.keys[InputBinding::PrevClub])
        {
            m_inputFlags &= ~InputFlag::PrevClub;
        }
    }
    
    else if (evt.type == SDL_CONTROLLERBUTTONDOWN)
    {
        if (evt.cbutton.which == cro::GameController::deviceID(inputBinding.controllerID))
        {
            if (evt.cbutton.button == inputBinding.buttons[InputBinding::Action])
            {
                m_inputFlags |= InputFlag::Action;
            }
            else if (evt.cbutton.button == inputBinding.buttons[InputBinding::NextClub])
            {
                m_inputFlags |= InputFlag::NextClub;
            }
            else if (evt.cbutton.button == inputBinding.buttons[InputBinding::PrevClub])
            {
                m_inputFlags |= InputFlag::PrevClub;
            }
            else if (evt.cbutton.button == inputBinding.buttons[InputBinding::CamModifier])
            {
                m_inputFlags |= InputFlag::CamModifier;
            }

            else if (evt.cbutton.button == cro::GameController::DPadLeft)
            {
                m_inputFlags |= InputFlag::Left;
            }
            else if (evt.cbutton.button == cro::GameController::DPadRight)
            {
                m_inputFlags |= InputFlag::Right;
            }
            else if (evt.cbutton.button == cro::GameController::DPadUp)
            {
                m_inputFlags |= InputFlag::Up;
            }
            else if (evt.cbutton.button == cro::GameController::DPadDown)
            {
                m_inputFlags |= InputFlag::Down;
            }
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        if (evt.cbutton.which == cro::GameController::deviceID(inputBinding.controllerID))
        {
            if (evt.cbutton.button == inputBinding.buttons[InputBinding::Action])
            {
                m_inputFlags &= ~InputFlag::Action;
            }
            else if (evt.cbutton.button == inputBinding.buttons[InputBinding::NextClub])
            {
                m_inputFlags &= ~InputFlag::NextClub;
            }
            else if (evt.cbutton.button == inputBinding.buttons[InputBinding::PrevClub])
            {
                m_inputFlags &= ~InputFlag::PrevClub;
            }
            else if (evt.cbutton.button == inputBinding.buttons[InputBinding::CamModifier])
            {
                m_inputFlags &= ~InputFlag::CamModifier;
            }

            else if (evt.cbutton.button == cro::GameController::DPadLeft)
            {
                m_inputFlags &= ~InputFlag::Left;
            }
            else if (evt.cbutton.button == cro::GameController::DPadRight)
            {
                m_inputFlags &= ~InputFlag::Right;
            }
            else if (evt.cbutton.button == cro::GameController::DPadUp)
            {
                m_inputFlags &= ~InputFlag::Up;
            }
            else if (evt.cbutton.button == cro::GameController::DPadDown)
            {
                m_inputFlags &= ~InputFlag::Down;
            }
        }
    }

    else if (evt.type == SDL_MOUSEBUTTONDOWN)
    {
        switch (evt.button.button)
        {
        default: break;
        case SDL_BUTTON_LEFT:
            m_inputFlags |= InputFlag::Action;
            break;
        case SDL_BUTTON_RIGHT:
            m_inputFlags |= InputFlag::CamModifier;
            break;
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONUP)
    {
        switch (evt.button.button)
        {
        default: break;
        case SDL_BUTTON_LEFT:
            m_inputFlags &= ~InputFlag::Action;
            break;
        case SDL_BUTTON_RIGHT:
            m_inputFlags &= ~InputFlag::CamModifier;
            break;
        }
    }

    else if (evt.type == SDL_MOUSEWHEEL
        && m_active)
    {
        m_power = std::max(MinPower, std::min(MaxPower, m_power + (PowerStep * evt.wheel.y)));
    }
    else if (evt.type == SDL_MOUSEMOTION)
    {
        //normalise this to better match the controller input
        static constexpr float MaxMouseMove = 6.f;
        m_mouseMove.x = static_cast<float>(evt.motion.xrel) / MaxMouseMove;
        m_mouseMove.y = static_cast<float>(-evt.motion.yrel) / MaxMouseMove;
    }
}

void BilliardsInput::update(float dt)
{
    //if (m_active)
    {
        checkController(dt);

        if (m_state == Play)
        {
            updatePlay(dt);
        }
        else
        {
            updatePlaceBall(dt);
        }
    }

    m_prevFlags = m_inputFlags;
    
#ifdef CRO_DEBUG_
    mouseMove = m_mouseMove;
#endif
    m_prevMouseMove = m_mouseMove;
    m_mouseMove = glm::vec2(0.f);

    //player cam
    auto rotation = m_controlEntities.camera.getComponent<ControllerRotation>().rotation;
    m_controlEntities.camera.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation);

    rotation = m_controlEntities.cameraTilt.getComponent<ControllerRotation>().rotation;
    m_controlEntities.cameraTilt.getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, rotation);

    //cue
    rotation = m_controlEntities.cue.getComponent<ControllerRotation>().rotation;
    m_controlEntities.cue.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation);

    //spectate cam
    rotation = m_controlEntities.spectator.getComponent<ControllerRotation>().rotation;
    m_controlEntities.spectator.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation);
}

void BilliardsInput::setActive(bool active, bool placeBall)
{
    m_active = active;
    m_clampRotation = placeBall;

    m_controlEntities.indicator.getComponent<cro::Model>().setHidden(!active);
    m_controlEntities.indicator.getComponent<cro::Callback>().active = active;

    if (active)
    {
        m_topSpin = 0.f;
        m_sideSpin = 0.f;
        m_power = 0.5f;
        m_spinOffset = glm::quat(1.f, 0.f, 0.f, 0.f);
    }

    if (placeBall)
    {
        m_state = PlaceBall;

        m_controlEntities.previewBall.getComponent<cro::Model>().setHidden(false);
        m_controlEntities.previewBall.getComponent<cro::Transform>().setPosition(m_controlEntities.camera.getComponent<cro::Transform>().getPosition());
    }
    else
    {
        m_state = Play;
        m_controlEntities.previewBall.getComponent<cro::Model>().setHidden(true);
        m_controlEntities.previewBall.getComponent<cro::Transform>().setPosition(glm::vec3(50.f)); //move out the way to prevent spurious collisions
    }
}

void BilliardsInput::setControlEntities(ControlEntities controlEntities)
{
    CRO_ASSERT(controlEntities.camera.hasComponent<ControllerRotation>(), "");
    CRO_ASSERT(controlEntities.cue.hasComponent<ControllerRotation>(), "");

    m_controlEntities = controlEntities;
}

std::pair<glm::vec3, glm::vec3> BilliardsInput::getImpulse() const
{
    const float TotalRotation = m_controlEntities.camera.getComponent<ControllerRotation>().rotation 
                                + m_controlEntities.cue.getComponent<ControllerRotation>().rotation;

    glm::vec4 direction(0.f, 0.f, -1.f, 0.f);
    auto rotation = glm::rotate(glm::mat4(1.f), TotalRotation, cro::Transform::Y_AXIS);

    direction = rotation * direction;

    glm::vec4 offset = rotation * glm::vec4(0.f, 0.f, 0.028f, 0.f); //ball radius(ish)
    offset = m_spinOffset * offset;

    return { glm::vec3(direction) * m_power, glm::vec3(offset) };
}

//private
bool BilliardsInput::hasMouseMotion() const
{
    return (m_mouseMove.x + m_mouseMove.y) != 0.f;
}

void BilliardsInput::checkController(float dt)
{
    m_analogueAmountLeft = 1.f;
    m_analogueAmountRight = 1.f;

    const auto& inputBinding = m_sharedData.inputBinding;
    if (!cro::GameController::isConnected(inputBinding.controllerID))
    {
        return;
    }

    //triggers to set power
    float powerUp = static_cast<float>(cro::GameController::getAxisPosition(inputBinding.controllerID, cro::GameController::TriggerRight)) / cro::GameController::AxisMax;
    if (powerUp > AnalogueDeadZone)
    {
        m_power = std::min(MaxPower, m_power + (powerUp * dt));
    }

    float powerDown = static_cast<float>(cro::GameController::getAxisPosition(inputBinding.controllerID, cro::GameController::TriggerLeft)) / cro::GameController::AxisMax;
    if (powerDown > AnalogueDeadZone)
    {
        m_power = std::max(MinPower, m_power - (powerDown * dt));
    }

    //right stick to adjust spin - TODO make this move camera.
    /*auto startInput = m_inputFlags;
    float xPos = cro::GameController::getAxisPosition(inputBinding.controllerID, cro::GameController::AxisRightX);
    if (xPos < -DeadZone)
    {
        m_inputFlags |= InputFlag::Left;
    }
    else if (m_prevStick & InputFlag::Left)
    {
        m_inputFlags &= ~InputFlag::Left;
    }

    if (xPos > DeadZone)
    {
        m_inputFlags |= InputFlag::Right;
    }
    else if (m_prevStick & InputFlag::Right)
    {
        m_inputFlags &= ~InputFlag::Right;
    }

    float yPos = cro::GameController::getAxisPosition(inputBinding.controllerID, cro::GameController::AxisRightY);
    if (yPos > (DeadZone))
    {
        m_inputFlags |= InputFlag::Down;
        m_inputFlags &= ~InputFlag::Up;
    }
    else if (m_prevStick & InputFlag::Down)
    {
        m_inputFlags &= ~InputFlag::Down;
    }

    if (yPos < (-DeadZone))
    {
        m_inputFlags |= InputFlag::Up;
        m_inputFlags &= ~InputFlag::Down;
    }
    else if (m_prevStick & InputFlag::Up)
    {
        m_inputFlags &= ~InputFlag::Up;
    }*/

    /*float len2 = (xPos * xPos) + (yPos * yPos);
    static const float MinLen2 = (DeadZone * DeadZone);
    if (len2 > MinLen2)
    {
        m_analogueAmountRight = std::sqrt(len2) / (cro::GameController::AxisMax - DeadZone);
    }

    if (startInput ^ m_inputFlags)
    {
        m_prevStick = m_inputFlags;
    }*/


    //left stick emulates mouse movement (but shouldn't overwrite it)
    if (!hasMouseMotion())
    {
        float xMove = static_cast<float>(cro::GameController::getAxisPosition(inputBinding.controllerID, cro::GameController::AxisLeftX)) / cro::GameController::AxisMax;

        if (xMove > AnalogueDeadZone || xMove < -AnalogueDeadZone)
        {
            m_mouseMove.x = xMove;
        }

        float yMove = static_cast<float>(cro::GameController::getAxisPosition(inputBinding.controllerID, cro::GameController::AxisLeftY)) / cro::GameController::AxisMax;

        if (yMove > AnalogueDeadZone || yMove < -AnalogueDeadZone)
        {
            m_mouseMove.y = -yMove;
        }

        if (hasMouseMotion())
        {
            m_analogueAmountLeft = glm::length(m_mouseMove);
        }
    }
}

void BilliardsInput::updatePlay(float dt)
{
    bool playerCamActive = *m_controlEntities.camera.getComponent<ControllerRotation>().activeCamera;
    bool spectateCamActive = *m_controlEntities.spectator.getComponent<ControllerRotation>().activeCamera;

    if (hasMouseMotion())
    {
        if (playerCamActive)
        {
            //this is the cue view
            if (m_inputFlags & InputFlag::CamModifier)
            {
                //move cam
                float direction = m_sharedData.invertX ? -m_sharedData.mouseSpeed : m_sharedData.mouseSpeed;
                auto rotation = m_controlEntities.camera.getComponent<ControllerRotation>().rotation - (CamRotationSpeed * direction * m_mouseMove.x * m_analogueAmountLeft * dt);
                
                if (m_clampRotation)
                {
                    //TODO this should be the rotation of the vector between the ball
                    //and the cue line
                    //static constexpr float MaxRotation = (cro::Util::Const::PI / 2.f) - MaxCueRotation;
                    rotation = std::max(-m_maxRotation, std::min(m_maxRotation, rotation));
                }
                m_controlEntities.camera.getComponent<ControllerRotation>().rotation = rotation;

                direction = m_sharedData.invertY ? -m_sharedData.mouseSpeed : m_sharedData.mouseSpeed;

                auto tilt = m_controlEntities.cameraTilt.getComponent<ControllerRotation>().rotation;
                tilt -= CamRotationSpeed * direction * -m_mouseMove.y * m_analogueAmountLeft * dt;
                tilt = std::min(MaxTilt, std::max(MinTilt, tilt));
                m_controlEntities.cameraTilt.getComponent<ControllerRotation>().rotation = tilt;
            }
            else
            {
                //move cue
                float direction = m_sharedData.invertX ? -m_sharedData.mouseSpeed : m_sharedData.mouseSpeed;

                auto rotation = m_controlEntities.cue.getComponent<ControllerRotation>().rotation - (CueRotationSpeed * direction * m_mouseMove.x * m_analogueAmountLeft * dt);
                rotation = std::max(-MaxCueRotation, std::min(MaxCueRotation, rotation));
                m_controlEntities.cue.getComponent<ControllerRotation>().rotation = rotation;
            }
        }
        else if (spectateCamActive)
        {
            //rotate spectator cam
            float direction = m_sharedData.invertX ? -m_sharedData.mouseSpeed : m_sharedData.mouseSpeed;
            m_controlEntities.spectator.getComponent<ControllerRotation>().rotation -= /*CamRotationSpeed **/ direction * m_mouseMove.x * m_analogueAmountLeft * dt;
        }
        else
        {
            //top down view so just rotate the camera (because the cue is attached)
            float direction = m_sharedData.invertX ? -m_sharedData.mouseSpeed : m_sharedData.mouseSpeed;
            m_controlEntities.camera.getComponent<ControllerRotation>().rotation += direction * CamRotationSpeed * m_mouseMove.x * m_analogueAmountLeft * dt;
        }
    }

    if (m_active)
    {
        //side/top/back spin
        if (m_inputFlags & InputFlag::Left)
        {
            m_sideSpin = std::max(-MaxSpin, m_sideSpin - (dt * m_analogueAmountRight));
        }
        if (m_inputFlags & InputFlag::Right)
        {
            m_sideSpin = std::min(MaxSpin, m_sideSpin + (dt * m_analogueAmountRight));
        }
        if (m_inputFlags & InputFlag::Up)
        {
            m_topSpin = std::max(-MaxSpin, m_topSpin - (dt * m_analogueAmountRight));
        }
        if (m_inputFlags & InputFlag::Down)
        {
            m_topSpin = std::min(MaxSpin, m_topSpin + (dt * m_analogueAmountRight));
        }
        if (m_inputFlags & (InputFlag::Up | InputFlag::Down | InputFlag::Left | InputFlag::Right))
        {
            m_spinOffset = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), m_topSpin, cro::Transform::X_AXIS);
            m_spinOffset = glm::rotate(m_spinOffset, m_sideSpin, cro::Transform::Y_AXIS);
        }

        //power
        if (m_inputFlags & InputFlag::PrevClub)
        {
            m_power = std::max(PowerStep, m_power - dt);
        }
        if (m_inputFlags & InputFlag::NextClub)
        {
            m_power = std::min(1.f, m_power + dt);
        }

        auto diff = m_prevFlags ^ m_inputFlags;
        if ((diff & m_inputFlags & InputFlag::Action) != 0
            /*&& playerCamActive*/)
        {
            m_controlEntities.cue.getComponent<cro::Skeleton>().play(1);
            setActive(false, false);

            auto* msg = m_messageBus.post<BilliardBallEvent>(MessageID::BilliardsMessage);
            msg->type = BilliardBallEvent::ShotTaken;
            msg->position = m_controlEntities.camera.getComponent<cro::Transform>().getPosition();
        }
    }
}

void BilliardsInput::updatePlaceBall(float dt)
{
    //kludge to stop this in anything other than
    //player view - ideally we want to allow this but
    //ofc the top down or side views have a rotated
    //X/Z axis relative to the camera so placing the
    //ball just feels... weird in these views because
    //it doesn't move in the way the player expects...

    if (*m_controlEntities.camera.getComponent<ControllerRotation>().activeCamera)
    {
        glm::vec3 movement(0.f);

        if (hasMouseMotion())
        {
            movement = glm::normalize(glm::vec3(m_mouseMove.x, 0.f, -m_mouseMove.y)) * m_analogueAmountLeft;
        }
        else
        {
            if (m_inputFlags & InputFlag::Left)
            {
                movement.x = -1.f;
            }
            if (m_inputFlags & InputFlag::Right)
            {
                movement.x += 1.f;
            }
            if (m_inputFlags & InputFlag::Up)
            {
                movement.z = -1.f;
            }
            if (m_inputFlags & InputFlag::Down)
            {
                movement.z += 1.f;
            }
            if (glm::length2(movement) > 1)
            {
                movement = glm::normalize(movement);
            }
            movement *= m_analogueAmountRight;
        }

        m_controlEntities.previewBall.getComponent<cro::Transform>().move(movement * 0.3f * dt);
        auto newPos = m_controlEntities.previewBall.getComponent<cro::Transform>().getPosition();

        //clamp to spawn
        CRO_ASSERT(m_spawnArea.width > 0 && m_spawnArea.height > 0, "");
        newPos.x = std::max(m_spawnArea.left, std::min(newPos.x, m_spawnArea.left + m_spawnArea.width));
        newPos.z = std::max(-m_spawnArea.bottom - m_spawnArea.height, std::min(newPos.z, -m_spawnArea.bottom));

        m_controlEntities.previewBall.getComponent<cro::Transform>().setPosition(newPos);
        m_controlEntities.camera.getComponent<cro::Transform>().setPosition(newPos);

        auto flagDiff = m_prevFlags ^ m_inputFlags;
        if ((flagDiff & m_inputFlags & InputFlag::Action) != 0)
        {
            //don't place if overlapping an existing ball
            const auto& ball = m_controlEntities.previewBall.getComponent<BilliardBall>();
            if (ball.getContact() < 1)
            {
                auto* msg = m_messageBus.post<BilliardBallEvent>(MessageID::BilliardsMessage);
                msg->type = BilliardBallEvent::BallPlaced;
                msg->position = m_controlEntities.previewBall.getComponent<cro::Transform>().getPosition();

                m_state = Play;
                m_controlEntities.previewBall.getComponent<cro::Model>().setHidden(true);
                m_controlEntities.previewBall.getComponent<cro::Transform>().setPosition(glm::vec3(50.f));


                //calc the max rotation so we can't shoot behind the line
                glm::vec2 ballVec(msg->position.x, msg->position.z);
                glm::vec2 lineVec(m_spawnArea.left + m_spawnArea.width, -m_spawnArea.bottom - m_spawnArea.height);
                glm::vec2 direction = lineVec - ballVec;

                m_maxRotation = (cro::Util::Const::PI / 2.f) + std::atan2(direction.y, direction.x) - MaxCueRotation;
            }
        }
    }
}