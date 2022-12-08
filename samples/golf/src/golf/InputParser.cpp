/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

#include "InputParser.hpp"
#include "InputBinding.hpp"
#include "MessageIDs.hpp"
#include "Clubs.hpp"
#include "Terrain.hpp"
#include "SharedStateData.hpp"
#include "GameConsts.hpp"

#include <crogine/core/GameController.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>
#include <crogine/util/Easings.hpp>

namespace
{
    static constexpr float RotationSpeed = 1.2f;
    static constexpr float MaxRotation = 0.36f;

    static constexpr float MinPower = 0.01f;
    static constexpr float MaxPower = 1.f - MinPower;

    static constexpr float MinAcceleration = 0.5f;

    const cro::Time DoubleTapTime = cro::milliseconds(200);
}

InputParser::InputParser(const SharedStateData& sd, cro::MessageBus& mb)
    : m_sharedData      (sd),
    m_inputBinding      (sd.inputBinding),
    m_messageBus        (mb),
    m_inputFlags        (0),
    m_prevFlags         (0),
    m_enableFlags       (std::numeric_limits<std::uint16_t>::max()),
    m_prevDisabledFlags (0),
    m_prevStick         (0),
    m_analogueAmount    (0.f),
    m_inputAcceleration (0.f),
    m_mouseWheel        (0),
    m_prevMouseWheel    (0),
    m_mouseMove         (0),
    m_prevMouseMove     (0),
    m_isCPU             (false),
    m_holeDirection     (0.f),
    m_rotation          (0.f),
    m_maxRotation       (MaxRotation),
    m_power             (0.f),
    m_hook              (0.5f),
    m_powerbarDirection (1.f),
    m_active            (false),
    m_suspended         (false),
    m_state             (State::Aim),
    m_currentClub       (ClubID::Driver),
    m_firstClub         (ClubID::Driver),
    m_clubOffset        (0)
{

}

//public
void InputParser::handleEvent(const cro::Event& evt)
{
    if (m_active &&
        !m_swingput.handleEvent(evt))
    {
        //apply to input mask
        if (evt.type == SDL_KEYDOWN
            && evt.key.repeat == 0)
        {
            if (m_isCPU && evt.key.windowID != CPU_ID)
            {
                return;
            }

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
            if (m_isCPU && evt.key.windowID != CPU_ID)
            {
                return;
            }

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
            auto controllerID = activeControllerID(m_inputBinding.playerID);
            if (!m_isCPU &&
                evt.cbutton.which == cro::GameController::deviceID(controllerID))
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
            auto controllerID = activeControllerID(m_inputBinding.playerID);
            if (!m_isCPU &&
                evt.cbutton.which == cro::GameController::deviceID(controllerID))
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

        /*else if (evt.type == SDL_MOUSEBUTTONDOWN)
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
        }*/

        /*else if (evt.type == SDL_MOUSEWHEEL)
        {
            m_mouseWheel += evt.wheel.y;
        }
        else if (evt.type == SDL_MOUSEMOTION)
        {
            m_mouseMove += evt.motion.xrel;
        }*/

    }
    else
    {
        m_inputFlags = 0;
    }
}

void InputParser::setHoleDirection(glm::vec3 dir)
{
    //note that this might be looking at a target other than the hole.
    if (auto len2 = glm::length2(dir); len2 > 0)
    {
        auto length = std::sqrt(len2);
        auto direction = dir / length;
        m_holeDirection = std::atan2(-direction.z, direction.x);

        m_rotation = 0.f;
    }
}

void InputParser::setClub(float dist)
{
    //assume each club can go a little further than its rating
    m_currentClub = ClubID::SandWedge;
    while ((Clubs[m_currentClub].target * 1.04f) < dist
        && m_currentClub != m_firstClub)
    {
        auto clubCount = ClubID::Putter - m_firstClub;
        m_clubOffset = (m_clubOffset + (clubCount - 1)) % clubCount;
        m_currentClub = m_firstClub + m_clubOffset;
    }

    auto* msg = m_messageBus.post<GolfEvent>(MessageID::GolfMessage);
    msg->type = GolfEvent::ClubChanged;
}

float InputParser::getYaw() const
{
    return m_holeDirection + m_rotation;
}

float InputParser::getPower() const
{
    return MinPower + (MaxPower * cro::Util::Easing::easeInSine(m_power));
}

float InputParser::getHook() const
{
    return m_hook * 2.f - 1.f;
}

std::int32_t InputParser::getClub() const
{
    return m_currentClub;
}

void InputParser::setActive(bool active, bool isCPU)
{
    m_active = active;
    m_isCPU = isCPU;
    m_state = State::Aim;
    //if the parser was suspended when set active then make sure un-suspending it returns the correct state.
    m_suspended = active;
    if (active)
    {
        resetPower();
        m_inputFlags = 0;

        m_swingput.setEnabled(m_enableFlags == std::numeric_limits<std::uint16_t>::max() ? m_inputBinding.playerID : -1);
    }
    else
    {
        m_swingput.setEnabled(-1);
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

void InputParser::setEnableFlags(std::uint16_t flags)
{
    m_enableFlags = flags;

    m_swingput.setEnabled(flags == std::numeric_limits<std::uint16_t>::max() ? m_swingput.getEnabled() : -1);
}

void InputParser::setMaxClub(float dist)
{
    m_firstClub = ClubID::SandWedge;
    while ((Clubs[m_firstClub].target * 1.05f) < dist
        && m_firstClub != ClubID::Driver)
    {
        m_firstClub--;
    }

    m_currentClub = m_firstClub;
    m_clubOffset = 0;

    auto* msg = m_messageBus.post<GolfEvent>(MessageID::GolfMessage);
    msg->type = GolfEvent::ClubChanged;
}

void InputParser::setMaxClub(std::int32_t clubID)
{
    CRO_ASSERT(clubID < ClubID::Putter, "");
    m_firstClub = m_currentClub = clubID;
    m_clubOffset = 0;

    auto* msg = m_messageBus.post<GolfEvent>(MessageID::GolfMessage);
    msg->type = GolfEvent::ClubChanged;
}

void InputParser::resetPower()
{
    m_power = 0.f;
    m_hook = 0.5f;
    m_powerbarDirection = 1.f;
}

void InputParser::update(float dt, std::int32_t terrainID)
{
    if (m_inputFlags & (InputFlag::Left | InputFlag::Right))
    {
        m_inputAcceleration = std::min(1.f, m_inputAcceleration + dt);
    }
    else
    {
        m_inputAcceleration = 0.f;
    }


    checkControllerInput();
    checkMouseInput();

    //catch the inputs that where filtered by the
    //enable flags so we can raise their own event for them
    auto disabledFlags = (m_inputFlags & ~m_enableFlags);

    if (m_active)
    {
        if (m_swingput.process(dt))
        {
            //we took our shot
            m_power = m_swingput.getPower();
            m_hook = m_swingput.getHook();

            m_powerbarDirection = 1.f;
            m_state = State::Flight;

            auto* msg = m_messageBus.post<GolfEvent>(MessageID::GolfMessage);
            msg->type = GolfEvent::HitBall;
        }

        m_inputFlags &= m_enableFlags;

        switch (m_state)
        {
        default: break;
        case State::Aim:
        {
            const float rotation = RotationSpeed * m_maxRotation * m_analogueAmount * dt;

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
                m_doubleTapClock.restart();
            }

            if ((m_prevFlags & InputFlag::PrevClub) == 0
                && (m_inputFlags & InputFlag::PrevClub))
            {
                auto clubCount = ClubID::Putter - m_firstClub;
                m_clubOffset = (m_clubOffset + 1) % clubCount;
                m_currentClub = m_firstClub + m_clubOffset;

                auto* msg = m_messageBus.post<GolfEvent>(MessageID::GolfMessage);
                msg->type = GolfEvent::ClubChanged;
                msg->score = m_isCPU ? 0 : 1; //tag this with a value so we know the input triggered this and should play a sound.
            }

            if ((m_prevFlags & InputFlag::NextClub) == 0
                && (m_inputFlags & InputFlag::NextClub))
            {
                auto clubCount = ClubID::Putter - m_firstClub;
                m_clubOffset = (m_clubOffset + (clubCount - 1)) % clubCount;
                m_currentClub = m_firstClub + m_clubOffset;

                auto* msg = m_messageBus.post<GolfEvent>(MessageID::GolfMessage);
                msg->type = GolfEvent::ClubChanged;
                msg->score = m_isCPU? 0 : 1;
            }
        }
        break;
        case State::Power:
        {
            //move level to 1 and back (returning to 0 is a fluff)
            float Speed = dt * 0.8f;
            if (terrainID == TerrainID::Green
                && m_sharedData.showPuttingPower)
            {
                Speed /= 1.75f;
            }

            m_power = std::min(1.f, std::max(0.f, m_power + (Speed * m_powerbarDirection)));

            if (m_power == 1)
            {
                m_powerbarDirection = -1.f;
            }

            if (m_power == 0
                || ((m_inputFlags & InputFlag::Action) && ((m_prevFlags & InputFlag::Action) == 0)))
            {
                if (m_doubleTapClock.elapsed() > DoubleTapTime)
                {
                    m_powerbarDirection = 1.f;

                    if (m_sharedData.showPuttingPower
                        && terrainID == TerrainID::Green)
                    {
                        //skip the hook bar cos we're on easy mode
                        m_state = State::Flight;

                        auto* msg = m_messageBus.post<GolfEvent>(MessageID::GolfMessage);
                        msg->type = GolfEvent::HitBall;
                    }
                    else
                    {
                        m_state = State::Stroke;
                    }
                    m_doubleTapClock.restart();
                }
            }
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
                if (m_doubleTapClock.elapsed() > DoubleTapTime)
                {
                    //setActive(false); //can't set this false here because it resets the values before we read them...
                    m_powerbarDirection = 1.f;
                    m_state = State::Flight;

                    auto* msg = m_messageBus.post<GolfEvent>(MessageID::GolfMessage);
                    msg->type = GolfEvent::HitBall;

                    m_doubleTapClock.restart();
                }
            }
            break;
        case State::Flight:
            //do nothing, player's turn is complete
            break;
        }

        //check the filtered inputs
        if (disabledFlags)
        {
            for (auto i = 0u; i < 8u; ++i)
            {
                auto flag = (1 << i);
                if ((disabledFlags & flag)
                    && (m_prevDisabledFlags & flag) == 0)
                {
                    //button was pressed
                    auto* msg = m_messageBus.post<SystemEvent>(MessageID::SystemMessage);
                    msg->type = SystemEvent::InputActivated;
                    msg->data = flag;
                }
            }
        }
    }

    m_prevFlags = m_inputFlags;
    m_prevDisabledFlags = disabledFlags;
}

bool InputParser::inProgress() const
{
    return (m_state == State::Power || m_state == State::Stroke);
}

bool InputParser::getActive() const
{
    return m_active;
}

void InputParser::setMaxRotation(float rotation)
{
    m_maxRotation = std::max(0.05f, std::min(MaxRotation, rotation));
}

//private
void InputParser::rotate(float rotation)
{
    m_rotation = std::min(m_maxRotation, std::max(-m_maxRotation, m_rotation + rotation));
}

void InputParser::checkControllerInput()
{
    //default amount from keyboard input, is overwritten
    //by controller if there is any controller input.
    m_analogueAmount = MinAcceleration + ((1.f - MinAcceleration) * m_inputAcceleration);

    if (m_isCPU ||
        cro::GameController::getControllerCount() == 0
        || m_swingput.isActive())
    {
        return;
    }

    auto controllerID = activeControllerID(m_inputBinding.playerID);

    //left stick
    auto startInput = m_inputFlags;
    float xPos = (cro::GameController::getAxisPosition(controllerID, cro::GameController::AxisLeftX) 
        + cro::GameController::getAxisPosition(controllerID, cro::GameController::AxisLeftY));
    xPos /= 2.f;

    if (xPos < -LeftThumbDeadZone)
    {
        m_inputFlags |= InputFlag::Left;
    }
    else if (m_prevStick & InputFlag::Left)
    {
        m_inputFlags &= ~InputFlag::Left;
    }

    if (xPos > LeftThumbDeadZone)
    {
        m_inputFlags |= InputFlag::Right;
    }
    else if (m_prevStick & InputFlag::Right)
    {
        m_inputFlags &= ~InputFlag::Right;
    }

    float yPos = cro::GameController::getAxisPosition(controllerID, cro::GameController::AxisLeftY);
    if (yPos > (LeftThumbDeadZone))
    {
        m_inputFlags |= InputFlag::Down;
        m_inputFlags &= ~InputFlag::Up;
    }
    else if (m_prevStick & InputFlag::Down)
    {
        m_inputFlags &= ~InputFlag::Down;
    }

    if (yPos < (-LeftThumbDeadZone))
    {
        m_inputFlags |= InputFlag::Up;
        m_inputFlags &= ~InputFlag::Down;
    }
    else if (m_prevStick & InputFlag::Up)
    {
        m_inputFlags &= ~InputFlag::Up;
    }

    float len2 = (xPos * xPos) + (yPos * yPos);
    static const float MinLen2 = static_cast<float>(LeftThumbDeadZone * LeftThumbDeadZone);
    if (len2 > MinLen2)
    {
        m_analogueAmount = std::min(1.f, std::pow(std::sqrt(len2) / (cro::GameController::AxisMax - LeftThumbDeadZone), 3.f));
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