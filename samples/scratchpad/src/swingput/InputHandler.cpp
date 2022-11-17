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

#include "InputHandler.hpp"

#include <crogine/core/App.hpp>
#include <crogine/gui/Gui.hpp>

namespace
{
    struct DebugInfo final
    {
        float distance = 0.f;
        float velocity = 0.f;
        float accuracy = 0.f;
    }debug;

    constexpr float MaxDistance = 200.f;
    constexpr float MaxVelocity = 1600.f;
    constexpr float MaxAccuracy = 20.f;
    constexpr float CommitDistance = 0.01f;

    constexpr float ControllerAxisRange = 32767.f;
    constexpr std::int16_t MinControllerMove = 1200;
}

InputHandler::InputHandler()
    : m_backPoint           (0.f),
    m_frontPoint            (0.f),
    m_previousControllerAxis(0),
    m_power                 (0.f),
    m_hook                  (0.f),
    m_elapsedTime           (0.f)
{
    registerWindow([&]()
        {
            if (ImGui::Begin("Swingput"))
            {
                ImGui::Text("State: %s", StateStrings[m_state].c_str());

                ImGui::Text("Distance: %3.3f", debug.distance);
                ImGui::Text("Velocity: %3.3f", debug.velocity);
                ImGui::Text("Accuracy: %3.3f", debug.accuracy);

                ImGui::Text("Power: %3.3f", m_power);
                ImGui::Text("Hook: %3.3f", m_hook);

                /*ImGui::SliderFloat("Max Distance", &MaxDistance, 100.f, 200.f);
                ImGui::SliderFloat("Max Velocity", &MaxVelocity, 100.f, 200.f);
                ImGui::SliderFloat("Max Accuracy", &MaxAccuracy, 10.f, 60.f);*/
            }
            ImGui::End();
        });
}

//public
void InputHandler::handleEvent(const cro::Event& evt)
{
    const auto startStroke = [&]()
    {
        if (m_state == State::Inactive)
        {
            m_state = State::Draw;
            m_backPoint = { 0.f, -MaxDistance / 4.f };
            m_frontPoint = { 0.f, 0.f };
            m_previousControllerAxis = 0;
            m_power = 0.f;
            m_hook = 0.f;

            cro::App::getWindow().setMouseCaptured(true);
        }
    };

    const auto commitStroke = [&]()
    {
        m_frontPoint = m_backPoint;
        m_state = State::Push;
        m_timer.restart();
    };

    const auto endStroke = [&]()
    {
        if (m_state == State::Push)
        {
            m_state = State::Summarise;
            m_elapsedTime = m_timer.restart();
        }
        else
        {
            m_state = State::Inactive;
        }
        debug = {};
        cro::App::getWindow().setMouseCaptured(false);
    };

    switch (evt.type)
    {
    default: break;
    case SDL_MOUSEBUTTONDOWN:
        if (evt.button.button == SDL_BUTTON_LEFT)
        {
            startStroke();
        }
        break;
    case SDL_MOUSEBUTTONUP:
        if (evt.button.button == SDL_BUTTON_LEFT)
        {
            endStroke();
        }
        break;
    case SDL_MOUSEMOTION:
        switch (m_state)
        {
        default: break;
            //move in both directions but commit to swing once past origin point
        case State::Draw:
            if(m_backPoint.y < CommitDistance)
            {
                //drawing back
                m_backPoint.x += evt.motion.xrel;
                m_backPoint.y -= evt.motion.yrel;

                //clamp Y value
                m_backPoint.y = std::max(m_backPoint.y, -MaxDistance / 2.f);
            }
            else
            {
                //changed direction
                commitStroke();
            }
            break;
            //only moev forward, reversing or releaseing button takes shot
        case State::Push:
            if (evt.motion.yrel < 0)
            {
                m_frontPoint.x += evt.motion.xrel;
                m_frontPoint.y -= evt.motion.yrel;

                m_frontPoint.y = std::min(m_frontPoint.y, MaxDistance / 2.f);
            }
            else
            {
                endStroke();
            }
            break;
        }
        break;

        //we allow either trigger or either stick
        //to aid handedness of players
    case SDL_CONTROLLERAXISMOTION:
        switch (evt.caxis.axis)
        {
        default: break;
        case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
            if (evt.caxis.value > MinControllerMove)
            {
                startStroke();
            }
            else
            {
                endStroke();
            }
            break;
        case SDL_CONTROLLER_AXIS_LEFTY:
        case SDL_CONTROLLER_AXIS_RIGHTY:
            if (std::abs(evt.caxis.value) > MinControllerMove)
            {
                if (m_state == State::Draw)
                {
                    m_backPoint.y = (static_cast<float>(evt.caxis.value) / ControllerAxisRange) * (MaxDistance / 2.f);

                    if(m_backPoint.y > CommitDistance)
                    {
                        //changed direction
                        commitStroke();
                    }
                }
                else if (m_state == State::Push)
                {
                    m_frontPoint.y = (static_cast<float>(evt.caxis.value) / ControllerAxisRange) * (MaxDistance / 2.f);

                    if (evt.caxis.value > m_previousControllerAxis)
                    {
                        endStroke();
                    }
                }
                m_previousControllerAxis = evt.caxis.value;
            }
            break;
        case SDL_CONTROLLER_AXIS_LEFTX:
        case SDL_CONTROLLER_AXIS_RIGHTX:
            //just set this and we'll have
            //whichever value was present when
            //the swing is finished
            if (std::abs(evt.caxis.value) > MinControllerMove)
            {
                m_frontPoint.x = (static_cast<float>(evt.caxis.value) / ControllerAxisRange) * MaxAccuracy;
            }
            break;
        }
        break;
    }
}

void InputHandler::process(float)
{
    if (m_state == State::Summarise)
    {
        //check we went forward past starting point
        if (m_frontPoint.y > 0)
        {
            debug.distance = (m_frontPoint.y - m_backPoint.y);
            debug.velocity = debug.distance / (m_elapsedTime + 0.0001f); //potential NaN
            debug.accuracy = (m_frontPoint.x - m_backPoint.x);
            
            float multiplier = m_frontPoint.y / ((MaxDistance / 2.f));

            m_power = std::clamp(debug.velocity / MaxVelocity, 0.f, 1.f) * multiplier;
            m_hook = std::clamp(debug.accuracy / MaxAccuracy, -1.f, 1.f);
        }

        m_state = State::Inactive;
    }
}