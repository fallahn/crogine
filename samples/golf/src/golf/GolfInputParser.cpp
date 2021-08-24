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

#include "GolfInputParser.hpp"
#include "InputBinding.hpp"
#include "MessageIDs.hpp"
#include "Clubs.hpp"

#include <crogine/core/GameController.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

namespace
{
    constexpr float RotationSpeed = 1.2f;
    constexpr float MaxRotation = 0.25f;
    constexpr std::int16_t DeadZone = 8000;

    constexpr float MinPower = 0.01f;
    constexpr float MaxPower = 1.f - MinPower;
}

namespace golf
{
    InputParser::InputParser(InputBinding ip, cro::MessageBus& mb)
        : m_inputBinding    (ip),
        m_messageBus        (mb),
        m_inputFlags        (0),
        m_prevFlags         (0),
        m_prevStick         (0),
        m_analogueAmount    (0.f),
        m_mouseWheel        (0),
        m_prevMouseWheel    (0),
        m_mouseMove         (0),
        m_prevMouseMove     (0),
        m_holeDirection     (0.f),
        m_rotation          (0.f),
        m_power             (0.f),
        m_hook              (0.5f),
        m_powerbarDirection (1.f),
        m_active            (false),
        m_suspended         (false),
        m_state             (State::Aim),
        m_currentClub       (ClubID::Driver)
    {

    }

    //public
    void InputParser::handleEvent(const cro::Event& evt)
    {
        //apply to input mask
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
                m_inputFlags |= InputFlag::NextClub;
            }
            else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::PrevClub])
            {
                m_inputFlags |= InputFlag::PrevClub;
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
            if (evt.button.button == SDL_BUTTON_LEFT)
            {
                m_inputFlags |= InputFlag::Action;
            }
            else if (evt.button.button == SDL_BUTTON_RIGHT)
            {
                m_inputFlags |= InputFlag::NextClub;
            }
        }
        else if (evt.type == SDL_MOUSEBUTTONUP)
        {
            if (evt.button.button == SDL_BUTTON_LEFT)
            {
                m_inputFlags &= ~InputFlag::Action;
            }
            else if (evt.button.button == SDL_BUTTON_RIGHT)
            {
                m_inputFlags &= ~InputFlag::NextClub;
            }
        }  

        else if (evt.type == SDL_MOUSEWHEEL)
        {
            m_mouseWheel += evt.wheel.y;
        }
        else if (evt.type == SDL_MOUSEMOTION)
        {
            m_mouseMove += evt.motion.xrel;
        }
    }

    void InputParser::setHoleDirection(glm::vec3 dir, bool selectClub)
    {
        //this assumes that dir is a vector from the player to
        //the hole - otherwise the club selection will be wrong.

        if (auto len2 = glm::length2(dir); len2 > 0)
        {
            auto length = std::sqrt(len2);
            
            if (selectClub)
            {
                setClub(length);
            }

            auto direction = dir / length;
            m_holeDirection = std::atan2(-direction.z, direction.x);

            m_rotation = 0.f;
        }
    }

    float InputParser::getYaw() const
    {
        return m_holeDirection + m_rotation;
    }

    float InputParser::getPower() const
    {
        return MinPower + (MaxPower * m_power);
    }

    float InputParser::getHook() const
    {
        return m_hook * 2.f - 1.f;
    }

    std::int32_t InputParser::getClub() const
    {
        return m_currentClub;
    }

    void InputParser::setActive(bool active)
    {
        m_active = active;
        m_state = State::Aim;
        //if the parser was suspended when set active then make sure un-suspending it returns the correct state.
        m_suspended = active;
        if (active)
        {
            resetPower();
        }
    }

    void InputParser::setSuspended(bool suspended)
    {
        if (suspended)
        {
            m_suspended = m_active;
            m_active = false;
        }
        else
        {
            m_active = m_suspended;
        }

    }

    void InputParser::resetPower()
    {
        m_power = 0.f;
        m_hook = 0.5f;
        m_powerbarDirection = 1.f;
    }

    void InputParser::update(float dt)
    {
        if (m_active)
        {
            checkControllerInput();
            checkMouseInput();

            switch (m_state)
            {
            default: break;
            case State::Aim:
            {
                const float rotation = RotationSpeed * MaxRotation * m_analogueAmount * dt;

                if (m_inputFlags & InputFlag::Left)
                {
                    rotate(rotation);
                }

                if (m_inputFlags & InputFlag::Right)
                {
                    rotate(-rotation);
                }

                if (m_inputFlags & InputFlag::Action)
                {
                    m_state = State::Power;
                }

                if ((m_prevFlags & InputFlag::PrevClub) == 0
                    && (m_inputFlags & InputFlag::PrevClub))
                {
                    m_currentClub = (m_currentClub + ClubID::PitchWedge) % ClubID::Putter;

                    auto* msg = m_messageBus.post<GolfEvent>(MessageID::GolfMessage);
                    msg->type = GolfEvent::ClubChanged;
                }

                if ((m_prevFlags & InputFlag::NextClub) == 0
                    && (m_inputFlags & InputFlag::NextClub))
                {
                    m_currentClub = (m_currentClub + 1) % ClubID::Putter;

                    auto* msg = m_messageBus.post<GolfEvent>(MessageID::GolfMessage);
                    msg->type = GolfEvent::ClubChanged;
                }
            }
            break;
            case State::Power:
                //move level to 1 and back (returning to 0 is a fluff)
                m_power = std::min(1.f, std::max(0.f, m_power + (dt * m_powerbarDirection)));

                if (m_power == 1)
                {
                    m_powerbarDirection = -1.f;
                }

                if (m_power == 0
                    || ((m_inputFlags & InputFlag::Action) && ((m_prevFlags & InputFlag::Action) == 0)))
                {
                    m_powerbarDirection = 1.f;
                    m_state = State::Stroke;
                }

                break;
            case State::Stroke:
                m_hook = std::min(1.f, std::max(0.f, m_hook + (dt * m_powerbarDirection)));

                if (m_hook == 1)
                {
                    m_powerbarDirection = -1.f;
                }

                if (m_hook == 0
                    || ((m_inputFlags & InputFlag::Action) && ((m_prevFlags & InputFlag::Action) == 0)))
                {
                    m_powerbarDirection = 1.f;
                    //setActive(false); //can't set this false here because it resets the values before we read them...
                    m_state = State::Flight;

                    auto* msg = m_messageBus.post<GolfEvent>(MessageID::GolfMessage);
                    msg->type = GolfEvent::HitBall;
                }
                break;
            case State::Flight:
                //do nothing, player's turn is complete
                break;
            }
        }
        m_prevFlags = m_inputFlags;
    }

    bool InputParser::inProgress() const
    {
        return (m_state == State::Power || m_state == State::Stroke);
    }

    //private
    void InputParser::setClub(float dist)
    {
        m_currentClub = ClubID::NineIron;
        while (Clubs[m_currentClub].target < dist
            && m_currentClub != ClubID::Driver)
        {
            m_currentClub = (m_currentClub + ClubID::PitchWedge) % ClubID::Putter;
        }

        auto* msg = m_messageBus.post<GolfEvent>(MessageID::GolfMessage);
        msg->type = GolfEvent::ClubChanged;
    }

    void InputParser::rotate(float rotation)
    {
        m_rotation = std::min(MaxRotation, std::max(-MaxRotation, m_rotation + rotation));
    }

    void InputParser::checkControllerInput()
    {
        m_analogueAmount = 1.f;

        if (!cro::GameController::isConnected(m_inputBinding.controllerID))
        {
            return;
        }


        //left stick (xInput controller)
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
    }

    void InputParser::checkMouseInput()
    {
        if (m_mouseWheel > 0)
        {
            m_inputFlags |= InputFlag::PrevClub;
        }
        else if (m_mouseWheel < 0)
        {
            m_inputFlags |= InputFlag::NextClub;
        }
        else if (m_prevMouseWheel > 0)
        {
            m_inputFlags &= ~InputFlag::PrevClub;
        }
        else if (m_prevMouseWheel < 0)
        {
            m_inputFlags &= ~InputFlag::NextClub;
        }

        m_prevMouseWheel = m_mouseWheel;
        m_mouseWheel = 0;

        //TODO make this not crap.

        //if (m_mouseMove > 0)
        //{
        //    m_inputFlags |= InputFlag::Right;
        //}
        //else if (m_mouseMove < 0)
        //{
        //    m_inputFlags |= InputFlag::Left;
        //}
        //else if (m_prevMouseMove > 0)
        //{
        //    m_inputFlags &= ~InputFlag::Right;
        //}
        //else if (m_prevMouseMove < 0)
        //{
        //    m_inputFlags &= ~InputFlag::Left;
        //}
        //m_prevMouseMove = m_mouseMove;
        //m_mouseMove = 0;
    }
}