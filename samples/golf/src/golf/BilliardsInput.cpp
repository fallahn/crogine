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

#include <crogine/core/GameController.hpp>
#include <crogine/core/MessageBus.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Skeleton.hpp>
#include <crogine/util/Constants.hpp>

namespace
{
    constexpr float CamRotationSpeed = 0.05f;
    constexpr float CamRotationSpeedFast = CamRotationSpeed * 5.f;
    constexpr float CueRotationSpeed = 0.025f;
    constexpr float MaxCueRotation = cro::Util::Const::PI / 16.f;
    constexpr float PowerStep = 0.1f;
    constexpr float MinPower = PowerStep;
    constexpr float MaxSpin = cro::Util::Const::PI / 6.f;

    constexpr std::int16_t DeadZone = 8000;
    constexpr float AnalogueDeadZone = 0.2f;
}

BilliardsInput::BilliardsInput(const InputBinding& ip, cro::MessageBus& mb)
    : m_inputBinding(ip),
    m_messageBus    (mb),
    m_inputFlags    (0),
    m_prevFlags     (0),
    m_prevStick     (0),
    m_mouseMove     (0.f),
    m_prevMouseMove (0.f),
    m_analogueAmount(1.f),
    m_active        (true),
    m_power         (0.5f),
    m_topSpin       (0.f),
    m_sideSpin      (0.f),
    m_spinOffset    (1.f, 0.f, 0.f, 0.f)
{

}

//public
void BilliardsInput::handleEvent(const cro::Event& evt)
{
    if (evt.type == SDL_KEYDOWN)
    {
        if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Up])
        {
            m_inputFlags |= InputFlag::Up;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Left])
        {
            m_inputFlags |= InputFlag::Left;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Right])
        {
            m_inputFlags |= InputFlag::Right;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Down])
        {
            m_inputFlags |= InputFlag::Down;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Action])
        {
            m_inputFlags |= InputFlag::Action;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::NextClub])
        {
            //m_inputFlags |= InputFlag::NextClub;
            m_power = std::min(MaxPower, m_power + PowerStep);
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::PrevClub])
        {
            //m_inputFlags |= InputFlag::PrevClub;
            m_power = std::max(MinPower, m_power - PowerStep);
        }
    }
    else if (evt.type == SDL_KEYUP)
    {
        if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Up])
        {
            m_inputFlags &= ~InputFlag::Up;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Left])
        {
            m_inputFlags &= ~InputFlag::Left;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Right])
        {
            m_inputFlags &= ~InputFlag::Right;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Down])
        {
            m_inputFlags &= ~InputFlag::Down;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Action])
        {
            m_inputFlags &= ~InputFlag::Action;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::NextClub])
        {
            m_inputFlags &= ~InputFlag::NextClub;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::PrevClub])
        {
            m_inputFlags &= ~InputFlag::PrevClub;
        }
    }
    
    else if (evt.type == SDL_CONTROLLERBUTTONDOWN)
    {
        if (evt.cbutton.which == cro::GameController::deviceID(m_inputBinding.controllerID))
        {
            if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::Action])
            {
                m_inputFlags |= InputFlag::Action;
            }
            else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::NextClub])
            {
                m_inputFlags |= InputFlag::NextClub;
            }
            else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::PrevClub])
            {
                m_inputFlags |= InputFlag::PrevClub;
            }
            else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::CamModifier])
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
        if (evt.cbutton.which == cro::GameController::deviceID(m_inputBinding.controllerID))
        {
            if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::Action])
            {
                m_inputFlags &= ~InputFlag::Action;
            }
            else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::NextClub])
            {
                m_inputFlags &= ~InputFlag::NextClub;
            }
            else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::PrevClub])
            {
                m_inputFlags &= ~InputFlag::PrevClub;
            }
            else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::CamModifier])
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

    else if (evt.type == SDL_MOUSEWHEEL)
    {
        m_power = std::max(MinPower, std::min(MaxPower, m_power + (PowerStep * evt.wheel.y)));
    }
    else if (evt.type == SDL_MOUSEMOTION)
    {
        m_mouseMove = static_cast<float>(evt.motion.xrel);
    }
}

void BilliardsInput::update(float dt)
{
    if (m_active)
    {
        checkController(dt);

        if (m_mouseMove)
        {
            if (*m_camEntity.getComponent<ControllerRotation>().activeCamera)
            {
                //this is the cue view
                if (m_inputFlags & InputFlag::CamModifier)
                {
                    //move cam
                    m_camEntity.getComponent<ControllerRotation>().rotation += CamRotationSpeed * m_mouseMove * dt;
                    m_camEntity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, m_camEntity.getComponent<ControllerRotation>().rotation);
                }
                else
                {
                    //move cue
                    auto rotation = m_cueEntity.getComponent<ControllerRotation>().rotation + CueRotationSpeed * m_mouseMove * dt;
                    rotation = std::max(-MaxCueRotation, std::min(MaxCueRotation, rotation));
                    m_cueEntity.getComponent<ControllerRotation>().rotation = rotation;
                    m_cueEntity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation);
                }
            }
            else
            {
                //some other view so just rotate the camera - TODO use the angle to the mouse cursor, but how to do with controller?
                m_camEntity.getComponent<ControllerRotation>().rotation += CamRotationSpeedFast * m_mouseMove * dt;
                m_camEntity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, m_camEntity.getComponent<ControllerRotation>().rotation);
            }
        }

        if (m_inputFlags & InputFlag::Left)
        {
            m_sideSpin = std::max(-MaxSpin, m_sideSpin - (dt * m_analogueAmount));
        }
        if (m_inputFlags & InputFlag::Right)
        {
            m_sideSpin = std::min(MaxSpin, m_sideSpin + (dt * m_analogueAmount));
        }
        if (m_inputFlags & InputFlag::Up)
        {
            m_topSpin = std::max(-MaxSpin, m_topSpin - (dt * m_analogueAmount));
        }
        if (m_inputFlags & InputFlag::Down)
        {
            m_topSpin = std::min(MaxSpin, m_topSpin + (dt * m_analogueAmount));
        }
        if (m_inputFlags & (InputFlag::Up | InputFlag::Down | InputFlag::Left | InputFlag::Right))
        {
            m_spinOffset = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), m_topSpin, cro::Transform::X_AXIS);
            m_spinOffset = glm::rotate(m_spinOffset, m_sideSpin, cro::Transform::Y_AXIS);
        }

        auto diff = m_prevFlags ^ m_inputFlags;
        if ((diff & m_inputFlags & InputFlag::Action) != 0)
        {
            //TODO play the cue animation instead?
            /*auto* msg = m_messageBus.post<BilliardBallEvent>(MessageID::BilliardsMessage);
            msg->type = BilliardBallEvent::ShotTaken;*/
            m_cueEntity.getComponent<cro::Skeleton>().play(1);
        }
    }

    m_prevFlags = m_inputFlags;
    
    m_prevMouseMove = m_mouseMove;
    m_mouseMove = 0;
}

void BilliardsInput::setActive(bool active)
{
    m_active = active;

    m_topSpin = 0.f;
    m_sideSpin = 0.f;
    m_power = 0.5f;
    m_spinOffset = glm::quat(1.f, 0.f, 0.f, 0.f);
}

void BilliardsInput::setControlEntities(cro::Entity camera, cro::Entity cue)
{
    CRO_ASSERT(camera.hasComponent<ControllerRotation>(), "");
    CRO_ASSERT(cue.hasComponent<ControllerRotation>(), "");

    m_camEntity = camera;
    m_cueEntity = cue;
}

std::pair<glm::vec3, glm::vec3> BilliardsInput::getImpulse() const
{
    const float TotalRotation = m_camEntity.getComponent<ControllerRotation>().rotation + m_cueEntity.getComponent<ControllerRotation>().rotation;

    glm::vec4 direction(0.f, 0.f, -1.f, 0.f);
    auto rotation = glm::rotate(glm::mat4(1.f), TotalRotation, cro::Transform::Y_AXIS);

    direction = rotation * direction;

    glm::vec4 offset = rotation * glm::vec4(0.f, 0.f, 0.028f, 0.f); //ball radius(ish)
    offset = m_spinOffset * offset;

    return { glm::vec3(direction) * m_power, glm::vec3(offset) };
}

void BilliardsInput::checkController(float dt)
{
    m_analogueAmount = 1.f;

    if (!cro::GameController::isConnected(m_inputBinding.controllerID))
    {
        return;
    }

    //triggers to set power
    float powerUp = static_cast<float>(cro::GameController::getAxisPosition(m_inputBinding.controllerID, cro::GameController::TriggerRight)) / cro::GameController::AxisMax;
    if (powerUp > AnalogueDeadZone)
    {
        m_power = std::min(MaxPower, m_power + (PowerStep * powerUp * dt));
    }

    float powerDown = static_cast<float>(cro::GameController::getAxisPosition(m_inputBinding.controllerID, cro::GameController::TriggerLeft)) / cro::GameController::AxisMax;
    if (powerDown > AnalogueDeadZone)
    {
        m_power = std::max(MinPower, m_power - (PowerStep * powerDown * dt));
    }

    //left stick to adjust spin
    auto startInput = m_inputFlags;
    float xPos = cro::GameController::getAxisPosition(m_inputBinding.controllerID, cro::GameController::AxisLeftX);
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

    float yPos = cro::GameController::getAxisPosition(m_inputBinding.controllerID, cro::GameController::AxisLeftY);
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
    }

    float len2 = (xPos * xPos) + (yPos * yPos);
    static const float MinLen2 = (DeadZone * DeadZone);
    if (len2 > MinLen2)
    {
        m_analogueAmount = std::sqrt(len2) / (cro::GameController::AxisMax - DeadZone);
    }

    if (startInput ^ m_inputFlags)
    {
        m_prevStick = m_inputFlags;
    }


    //right stick emulates mouse movement (but shouldn't overwrite it)
    if (m_mouseMove == 0)
    {
        float xMove = static_cast<float>(cro::GameController::getAxisPosition(m_inputBinding.controllerID, cro::GameController::AxisRightX)) / cro::GameController::AxisMax;

        if (xMove > AnalogueDeadZone || xMove < -AnalogueDeadZone)
        {
            m_mouseMove = xMove * 0.5f;
        }
    }
}