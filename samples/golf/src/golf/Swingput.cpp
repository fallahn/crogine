/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2024
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

#include "Swingput.hpp"
#include "Clubs.hpp"
#include "InputBinding.hpp"
#include "GameConsts.hpp"
#include "SharedStateData.hpp"
#include "MessageIDs.hpp"

#include <crogine/core/App.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/gui/Gui.hpp>

namespace
{
#ifdef CRO_DEBUG_
    struct DebugInfo final
    {
        float distance = 0.f;
        float velocity = 0.f;
        float accuracy = 0.f;
    }debugOutput;
#endif

    constexpr std::int16_t MinTriggerMove = 16000;

    constexpr float MaxMouseDraw = 20.f;
    constexpr float MaxMouseSwing = -20.f;
    constexpr float MaxMouseHook = 120.f;

    constexpr std::int16_t MaxControllerDraw = (std::numeric_limits<std::int16_t>::max() / 3) * 2;
    constexpr std::int16_t MaxControllerSwing = -std::numeric_limits<std::int16_t>::max() / 2;

    //horrible hack to match up with InputParser::State
    struct StateID final
    {
        enum
        {
            Aim, Power
        };
    };
}

Swingput::Swingput(const SharedStateData& sd)
    : m_sharedData          (sd),
    m_enabled               (-1),
    m_mouseMovement         (0),
    m_hook                  (0.f),
    m_gaugePosition         (0.f),
    m_lastLT                (0),
    m_lastRT                (0),
    m_strokeStartPosition   (0),
    m_activeStick           (SDL_CONTROLLER_AXIS_INVALID),
    m_lastAxisposition      (0),
    m_cancelTimer           (0.f),
    m_inCancelZone          (false)
{
#ifdef CRO_DEBUG_
    //registerWindow([&]()
    //    {
    //        if (ImGui::Begin("Swingput"))
    //        {
    //            ImGui::Text("Active Input %d", m_enabled);
    //            ImGui::Text("State: %s", StateStrings[m_state].c_str());

    //            ImGui::Text("Distance: %3.3f", debugOutput.distance);
    //            ImGui::Text("Velocity: %3.3f", debugOutput.velocity);
    //            ImGui::Text("Accuracy: %3.3f", debugOutput.accuracy);

    //            ImGui::Text("Power: %3.3f", m_power);
    //            ImGui::Text("Hook: %3.3f", m_hook);

    //            ImGui::Text("Active Point %3.3f, %3.3f", m_activePoint.x, m_activePoint.y);
    //            ImGui::Text("Back Point %3.3f, %3.3f", m_backPoint.x, m_backPoint.y);
    //            ImGui::Text("Front Point %3.3f, %3.3f", m_frontPoint.x, m_frontPoint.y);
    //        }
    //        ImGui::End();
    //    });
#endif
}

//public
bool Swingput::handleEvent(const cro::Event& evt, std::uint16_t& inputFlags, std::int32_t state)
{
    if (m_enabled == -1
        || !m_sharedData.useSwingput)
    {
        return false;
    }

    const auto startStroke = [&]()
    {
        if (m_state == State::Inactive
            && (m_lastLT < MinTriggerMove)
            && (m_lastRT < MinTriggerMove))
        {
            m_state = State::Swing;
            m_mouseMovement = { 0.f,0.f };
            m_hook = 0.5f;
            m_gaugePosition = 0.f;
            m_strokeStartPosition = 0;

            cro::App::getWindow().setMouseCaptured(true);
        }
    };

    const auto endStroke = [&]()
    {
        m_state = State::Inactive;
        m_activeStick = SDL_CONTROLLER_AXIS_INVALID;

        if (state == StateID::Power)
        {
            //cancel the shot
            inputFlags |= (InputFlag::Cancel | InputFlag::Swingput);
        }
    };

    switch (evt.type)
    {
    default: return isActive();
    case SDL_MOUSEBUTTONDOWN:
        return false; //disable for now
        if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            startStroke();
            return true;
        }
        return false;
    case SDL_MOUSEBUTTONUP:
        return false; //disable for now
        if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            endStroke();
            return true;
        }
        return false;
    case SDL_MOUSEMOTION:
        return false;
        //TODO we need to scale this down relative to the game buffer size
        if (m_state == State::Swing)
        {
            switch (state)
            {
            default: break;
            case StateID::Aim:
                if (evt.motion.yrel > 1)
                {
                    //measure this then trigger action
                    //once a certain distance is met
                    m_mouseMovement.y += static_cast<float>(evt.motion.yrel) * m_sharedData.swingputThreshold;
                    if (m_mouseMovement.y > MaxMouseDraw)
                    {
                        inputFlags |= (InputFlag::Action | InputFlag::Swingput);
                    }

                    setGaugeFromMouse();
                    return true;
                }
                break;
            case StateID::Power:
                if (evt.motion.yrel < -4)
                {
                    //measure the distance as it's travelled
                    //as well as the X axis for accuracy
                    m_mouseMovement.x += static_cast<float>(evt.motion.xrel); //hmm this is going to be dependent on screen resolution
                    m_mouseMovement.y += static_cast<float>(evt.motion.yrel) * m_sharedData.swingputThreshold;

                    if (m_mouseMovement.y < MaxMouseSwing)
                    {
                        inputFlags |= (InputFlag::Action | InputFlag::Swingput);
                        m_hook = std::clamp(m_mouseMovement.x / MaxMouseHook, -1.f, 1.f);
                        m_hook += 1.f;
                        m_hook /= 2.f;

                        m_state = State::Inactive;
                    }

                    setGaugeFromMouse();
                    return true;
                }
                break;
            }
        }
        return false;

    case SDL_CONTROLLERBUTTONDOWN:
        if (cro::GameController::controllerID(evt.cbutton.which) == activeControllerID(m_enabled))
        {
            if (evt.cbutton.button == cro::GameController::ButtonB
                && m_state == State::Swing)
            {
                endStroke();
            }
        }
        return false;
        //we allow either trigger or either stick
        //to aid handedness of players
    case SDL_CONTROLLERAXISMOTION:
        if (cro::GameController::controllerID(evt.caxis.which) == activeControllerID(m_enabled))
        {
            switch (evt.caxis.axis)
            {
            default: break;
            case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
            case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
                if (evt.caxis.value > MinTriggerMove)
                {
                    startStroke();
                }
                else if (evt.caxis.value < 6000) //some arbitrary deadzone
                {
                    endStroke();
                }

                if (evt.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
                {
                    m_lastLT = evt.caxis.value;
                }
                else
                {
                    m_lastRT = evt.caxis.value;
                }

                return (evt.caxis.value > MinTriggerMove);
            case SDL_CONTROLLER_AXIS_LEFTY:
            case SDL_CONTROLLER_AXIS_RIGHTY:
                if (m_state == State::Swing)
                {
                    //one of the triggers is held
                    if (state == StateID::Aim)
                    {
                        //pulling back should create a button press - as this
                        //should more or less immediately change state to power
                        //we *shouldn't* get multiple presses. InputParser will
                        //reset these flags for us automatically.
                        if (evt.caxis.value > MaxControllerDraw)
                        {
                            inputFlags |= (InputFlag::Action | InputFlag::Swingput);
                            m_strokeStartPosition = evt.caxis.value;
                            m_activeStick = evt.caxis.axis;
                        }

                        if (evt.caxis.value > cro::GameController::RightThumbDeadZone
                            || evt.caxis.value < -cro::GameController::RightThumbDeadZone)
                        {
                            setGaugeFromController(evt.caxis.value);
                        }
                        return true;
                    }
                    else if (state == StateID::Power)
                    {
                        //pushing forward rapidly should create a button press
                        //the swingput flag should tell the InputParser to skip
                        //the accuracy stage...
                        if (evt.caxis.value < MaxControllerSwing)
                        {
                            inputFlags |= (InputFlag::Action | InputFlag::Swingput);
                            m_state = State::Inactive;

                            const float t = m_tempoTimer.restart();
                            m_hook = 0.5f + ((0.033f - std::min(t, 0.066f)) / 2.f);

                            if (std::floor(t * 1000.f) == 33.f)
                            {
                                auto* msg = cro::App::postMessage<GolfEvent>(cl::MessageID::GolfMessage);
                                msg->type = GolfEvent::NiceTiming;
                            }

                            auto x = cro::GameController::getAxisPosition(activeControllerID(m_enabled), 
                                evt.caxis.axis == cro::GameController::AxisLeftY ? cro::GameController::AxisLeftX : cro::GameController::AxisRightX);
                            //higher level club sets require better accuracy
                            //https://www.desmos.com/calculator/u8hmy5q3mz
                            static constexpr std::array LevelMultipliers = { 19.f, 11.f, 7.f };
                            const float xAmount = std::pow(std::clamp(static_cast<float>(x) / 22000.f, -1.f, 1.f), LevelMultipliers[Club::getClubLevel()]);
                            
                            m_hook += (xAmount * 0.122f);
                        }

                        //see if we started moving back after beginning the power mode
                        if (evt.caxis.value < m_strokeStartPosition)
                        {                            
                            m_tempoTimer.restart();
                            
                            //prevent this triggering again
                            m_strokeStartPosition = std::numeric_limits<std::int16_t>::min();
                        }


                        //check if this is the active axis and if it's in the cancel
                        //zone - ie the user let go of the stick before swinging
                        if (evt.caxis.axis == m_activeStick)
                        {
                            m_inCancelZone = (evt.caxis.value < cro::GameController::RightThumbDeadZone && evt.caxis.value > -cro::GameController::RightThumbDeadZone);

                            //reset the timer if we entered the zone for the first time
                            if (m_inCancelZone && m_lastAxisposition > cro::GameController::RightThumbDeadZone)
                            {
                                //TODO do we need to check if it was below the deadzone? We should only be travelling in one dir
                                m_cancelTimer = 0.f;
                            }
                            m_lastAxisposition = evt.caxis.value;
                            setGaugeFromController(evt.caxis.value);
                        }

                        return true;
                    }                    
                }

                return false;
            }
        }
        return isActive();
    }

    return isActive();
}

void Swingput::assertIdled(float dt, std::uint16_t& inputFlags, std::int32_t state)
{
    if (state == StateID::Power
        && m_activeStick != SDL_CONTROLLER_AXIS_INVALID)
    {
        if (m_inCancelZone)
        {
            static constexpr float Timeout = 0.08f;
            m_cancelTimer += dt;

            if (m_cancelTimer > Timeout)
            {
                inputFlags |= (InputFlag::Cancel | InputFlag::Swingput);
                //m_state = State::Inactive;
                m_activeStick = SDL_CONTROLLER_AXIS_INVALID;
            }
        }
    }
}

void Swingput::setEnabled(std::int32_t enabled)
{
    m_enabled = enabled; 
    m_lastLT = 0;
    m_lastRT = 0;
}

//private
void Swingput::setGaugeFromMouse()
{
    //gauge is +/- MaxSwingputDistance / 2
    //but also INVERTED from the actual values *sigh*
    float norm = 0.f;
    if (m_mouseMovement.y > 0)
    {
        //draw
        norm = std::min(1.f, m_mouseMovement.y / MaxMouseDraw) * -1.f;
    }
    else
    {
        //swing
        norm = std::min(1.f, m_mouseMovement.y / MaxMouseSwing);
    }
    m_gaugePosition = (MaxSwingputDistance / 2.f) * norm;
}

void Swingput::setGaugeFromController(std::int16_t position)
{
    const float norm = (static_cast<float>(position) / std::numeric_limits<std::int16_t>::max()) * -1.f;
    m_gaugePosition = (MaxSwingputDistance / 2.f) * norm;
}