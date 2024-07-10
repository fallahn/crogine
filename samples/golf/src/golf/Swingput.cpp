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
#include <crogine/util/Maths.hpp>

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

    std::int32_t lastActiveID = -1;

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
    m_hook                  (0.5f),
    m_gaugePosition         (0.f),
    m_lastLT                (0),
    m_lastRT                (0),
    m_strokeStartPosition   (0),
    m_activeStick           (SDL_CONTROLLER_AXIS_INVALID),
    m_lastAxisposition      (0),
    m_cancelTimer           (0.f),
    m_inCancelZone          (false),
    m_humanCount            (1),
    m_activeControllerID    (-1)
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
            m_hook = 0.5f;
            m_gaugePosition = 0.f;
            m_strokeStartPosition = 0;

            m_mouseSwing.active = false; //gets confused if holding the right mouse before swinging with a controller...

            cro::App::getWindow().setMouseCaptured(true);
        }
    };

    const auto endStroke = [&]()
    {
        m_state = State::Inactive;
        m_activeStick = SDL_CONTROLLER_AXIS_INVALID;
        m_activeControllerID = -1;

        if (state == StateID::Power)
        {
            //cancel the shot
            inputFlags |= (InputFlag::Cancel | InputFlag::Swingput);
        }
    };

    const auto acceptInput = [&](std::int32_t joyID)
        {
            return cro::GameController::controllerID(joyID) == activeControllerID(m_enabled)
                || (m_humanCount == 1 && (m_activeControllerID == -1 || m_activeControllerID == joyID));
        };

    switch (evt.type)
    {
    default: return isActive();
    case SDL_MOUSEBUTTONDOWN:
        if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            m_state = State::Swing;
            cro::App::getWindow().setMouseCaptured(true);
            m_mouseSwing.startStroke();
            return true;
        }
        return isActive();
    case SDL_MOUSEBUTTONUP:
        if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            m_state = State::Inactive;
            m_mouseSwing.endStroke();
            return true;
        }
        return isActive();
    case SDL_MOUSEMOTION:
        if (m_state == State::Swing)
        {
            //TODO we need to scale this down relative to the game buffer size
            m_mouseSwing.activePoint.x = std::clamp(m_mouseSwing.activePoint.x + (static_cast<float>(evt.motion.xrel) / 10.f), -m_mouseSwing.MaxAccuracy, m_mouseSwing.MaxAccuracy);
            m_mouseSwing.activePoint.y = std::clamp(m_mouseSwing.activePoint.y - (static_cast<float>(evt.motion.yrel)), -(MaxSwingputDistance / 2.f), MaxSwingputDistance / 2.f);
        }
        return false;

    case SDL_CONTROLLERBUTTONDOWN:
        if (acceptInput(evt.cbutton.which))
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
        switch (evt.caxis.axis)
        {
        default: break;
        case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
            if (evt.caxis.value > MinTriggerMove)
            {
                m_activeControllerID = evt.caxis.which;
            }
            break;
        }
        
        if (acceptInput(evt.caxis.which))
        {
            m_thumbsticks.setValue(evt.caxis.axis, evt.caxis.value);

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
                            const float timingError = ((0.033f - std::min(t, 0.066f)) / 3.f);
                            m_hook = 0.5f + timingError;

                            const auto timing = std::floor(t * 1000.f);
                            if (timing == 33.f)
                            {
                                auto* msg = cro::App::postMessage<GolfEvent>(cl::MessageID::GolfMessage);
                                msg->type = GolfEvent::NiceTiming;
                            }

                            auto x = m_thumbsticks.getValue(evt.caxis.axis == cro::GameController::AxisLeftY ? cro::GameController::AxisLeftX : cro::GameController::AxisRightX);
                            //higher level club sets require better accuracy
                            //https://www.desmos.com/calculator/u8hmy5q3mz
                            static constexpr std::array LevelMultipliers = { 19.f, 11.f, 7.f };
                            const float xAmount = std::pow(std::clamp(static_cast<float>(x) / 22000.f, -1.f, 1.f), LevelMultipliers[Club::getClubLevel()]);
                            
                            //we want to make sure any timing error is 'complimented'
                            //by inaccuracy, else a hooked shot can actually correct for poor
                            //timing, which isn't the idea...
                            if (cro::Util::Maths::sgn(timingError) == cro::Util::Maths::sgn(xAmount)
                                || timing == 33.f) //we had perfect timing but still want direction error
                            {
                                m_hook += (xAmount * 0.31f);
                            }

                            lastActiveID = evt.caxis.which;
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
                m_activeControllerID = -1;
            }
        }
    }
}

bool Swingput::processMouseSwing()
{
    if (m_mouseSwing.active)
    {
        switch (m_state)
        {
        default: break;
        case State::Swing:
        {
            //moving down
            if (m_mouseSwing.activePoint.y < m_mouseSwing.frontPoint.y)
            {
                m_mouseSwing.backPoint = m_mouseSwing.activePoint;
            }

            //started moving back up so time the ascent
            if (m_mouseSwing.activePoint.y > m_mouseSwing.backPoint.y
                && m_mouseSwing.frontPoint.y == m_mouseSwing.backPoint.y)
            {
                m_mouseSwing.timer.restart();
            }

            //we've done full stroke.
            if (m_mouseSwing.activePoint.y > (MaxSwingputDistance / 2.f) - m_sharedData.swingputThreshold)
            {
                m_state = State::Summarise;
                m_mouseSwing.elapsedTime = m_mouseSwing.timer.restart();
            }

            m_mouseSwing.frontPoint = m_mouseSwing.activePoint;

            m_gaugePosition = m_mouseSwing.activePoint.y;
        }
        return false;
        case State::Summarise:
            m_state = State::Inactive;

//#ifdef CRO_DEBUG_
            //debugOutput.distance = (m_frontPoint.y - m_backPoint.y);
            //debugOutput.velocity = debugOutput.distance / (m_elapsedTime + 0.0001f); //potential NaN
            //debugOutput.accuracy = (m_frontPoint.x - m_backPoint.x);
            //m_power = std::clamp(debugOutput.velocity / m_maxVelocity, 0.f, 1.f);
            //m_hook = std::clamp(debugOutput.accuracy / MaxAccuracy, -1.f, 1.f);

            ////travelling a shorter distance doesn't imply lower
            ////velocity so we need to create a distance based modifier
            //float multiplier = debugOutput.distance / MaxSwingputDistance;

//#else
            float distance = (m_mouseSwing.frontPoint.y - m_mouseSwing.backPoint.y);
            float velocity = distance / (m_mouseSwing.elapsedTime + 0.0001f); //potential NaN
            float accuracy = (m_mouseSwing.frontPoint.x - m_mouseSwing.backPoint.x);
            m_mouseSwing.power = std::clamp(velocity / m_mouseSwing.MaxVelocity, 0.f, 1.f);
            m_mouseSwing.hook = std::clamp(accuracy / m_mouseSwing.MaxAccuracy, -1.f, 1.f);

            //travelling a shorter distance doesn't imply lower
            //velocity so we need to create a distance based modifier
            float multiplier = distance / MaxSwingputDistance;
//#endif

            //hmm have to double convert this because the input parser
            //actually calcs it to 0-1 and converts back to -1 1 on return
            m_mouseSwing.hook += 1.f;
            m_mouseSwing.hook /= 2.f;

            m_mouseSwing.power *= multiplier;

            return true;
        }
    }
    return false;
}

void Swingput::setEnabled(std::int32_t enabled)
{
    m_enabled = enabled; 
    m_lastLT = 0;
    m_lastRT = 0;
    m_thumbsticks.reset();
}

std::int32_t Swingput::getLastActiveController() const
{
    return lastActiveID;
}

//private
void Swingput::setGaugeFromController(std::int16_t position)
{
    const float norm = (static_cast<float>(position) / std::numeric_limits<std::int16_t>::max()) * -1.f;
    m_gaugePosition = (MaxSwingputDistance / 2.f) * norm;
}