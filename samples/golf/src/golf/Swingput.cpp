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
#include "InputBinding.hpp"
#include "GameConsts.hpp"
#include "SharedStateData.hpp"

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

    //constexpr float MaxControllerVelocity = 3000.f;
    //constexpr float MaxMouseVelocity = 1700.f;
    //constexpr float MaxAccuracy = 20.f;
    //constexpr float CommitDistance = 4.f;// 0.01f; //TODO make this user variable

    //constexpr float ControllerAxisRange = 32767.f;
    //constexpr std::int16_t MinStickMove = 8000;

    //these we do use
    constexpr std::int16_t MinTriggerMove = 16000;

    constexpr std::int32_t MaxMouseDraw = 20;
    constexpr std::int32_t MaxMouseSwing = -20;
    constexpr std::int32_t MaxMouseHook = 120;

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
    : m_sharedData  (sd),
    m_enabled       (-1),
    m_mouseMovement (0),
    m_hook          (0.f),
    m_gaugePosition (0.f),
    m_lastLT        (0),
    m_lastRT        (0)
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
            m_mouseMovement = { 0,0 };
            m_hook = 0.f;
            m_gaugePosition = 0.f;

            cro::App::getWindow().setMouseCaptured(true);
        }
    };

    const auto endStroke = [&]()
    {
        m_state = State::Inactive;
    };

    switch (evt.type)
    {
    default: return isActive();
    case SDL_MOUSEBUTTONDOWN:
        if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            startStroke();
            return true;
        }
        return false;
    case SDL_MOUSEBUTTONUP:
        if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            endStroke();
            return true;
        }
        return false;
    case SDL_MOUSEMOTION:
        //TODO we need to scale this down relative to the game buffer size
        //or use some sort of mouse sensitivity
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
                    m_mouseMovement.y += evt.motion.yrel;
                    if (m_mouseMovement.y > MaxMouseDraw)
                    {
                        inputFlags |= (InputFlag::Action | InputFlag::Swingput);
                    }
                }
                break;
            case StateID::Power:
                if (evt.motion.yrel < -4)
                {
                    //measure the distance as it's travelled
                    //as well as the X axis for accuracy
                    m_mouseMovement.x += evt.motion.xrel;
                    m_mouseMovement.y += evt.motion.yrel;

                    if (m_mouseMovement.y < MaxMouseSwing)
                    {
                        inputFlags |= (InputFlag::Action | InputFlag::Swingput);
                        m_hook = std::clamp(static_cast<float>(m_mouseMovement.x) / MaxMouseHook, -1.f, 1.f);
                        m_hook += 1.f;
                        m_hook /= 2.f;

                        m_state = State::Inactive;
                    }
                }
                break;
            }
            setGaugeFromMouse();
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
                else
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
                            m_tempoTimer.restart();
                        }
                    }
                    else if (state == StateID::Power)
                    {
                        //pushing forward rapidly should create a button press
                        //the swingput flag should tell the InputParser to skip
                        //the accuracy stage...
                        if (evt.caxis.value < MaxControllerSwing)
                        {
                            //TODO measure the tempo of the swing and use it to set the accuracy
                            LogI << "Tempo is " << m_tempoTimer.restart() << std::endl;

                            inputFlags |= (InputFlag::Action | InputFlag::Swingput);
                            m_state = State::Inactive;
                        }
                    }
                    setGaugeFromController(evt.caxis.value);
                }

                return false;
            case SDL_CONTROLLER_AXIS_LEFTX:
            case SDL_CONTROLLER_AXIS_RIGHTX:

                return false;
            }
        }
        return isActive();
    }

    return isActive();
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
    //gauge id +/- MaxSwingputDistance / 2
    //but also INVERTED from the actual values *sigh*
    float norm = 0.f;
    if (m_mouseMovement.y > 0)
    {
        //draw
        norm = std::min(1.f, static_cast<float>(m_mouseMovement.y) / MaxMouseDraw) * -1.f;
    }
    else
    {
        //swing
        norm = std::min(1.f, static_cast<float>(m_mouseMovement.y) / MaxMouseSwing);
    }
    m_gaugePosition = (MaxSwingputDistance / 2.f) * norm;
}

void Swingput::setGaugeFromController(std::int16_t position)
{
    const float norm = (static_cast<float>(position) / std::numeric_limits<std::int16_t>::max()) * -1.f;
    m_gaugePosition = (MaxSwingputDistance / 2.f) * norm;
}