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
#include <crogine/core/GameController.hpp>
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
    constexpr std::int16_t MinControllerMove = 16000;
}

InputHandler::InputHandler()
    : m_backPoint       (0.f),
    m_activePoint       (0.f),
    m_frontPoint        (0.f),
    m_power             (0.f),
    m_hook              (0.f),
    m_elapsedTime       (0.f)
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

                ImGui::Text("Active Point %3.3f, %3.3f", m_activePoint.x, m_activePoint.y);
                ImGui::Text("Back Point %3.3f, %3.3f", m_backPoint.x, m_backPoint.y);
                ImGui::Text("Front Point %3.3f, %3.3f", m_frontPoint.x, m_frontPoint.y);
            }
            ImGui::End();
        });
}

//public
void InputHandler::handleEvent(const cro::Event& evt)
{
    const auto startStroke = [&]()
    {
        if (m_state == State::Inactive
            && cro::GameController::getAxisPosition(0, cro::GameController::TriggerLeft) == 0
            && cro::GameController::getAxisPosition(0, cro::GameController::TriggerRight) == 0)
        {
            m_state = State::Swing;
            m_backPoint = { 0.f, 0.f };
            m_activePoint = { 0.f, 0.f };
            m_frontPoint = { 0.f, 0.f };
            m_power = 0.f;
            m_hook = 0.f;
            LogI << "buns" << std::endl;
            cro::App::getWindow().setMouseCaptured(true);
        }
    };

    const auto endStroke = [&]()
    {
        m_state = State::Inactive;
        m_activePoint = { 0.f, 0.f };
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
        //TODO we need to scale this down relative to the game buffer size
        //TODO we need to the scale it up to match the controller speed
        switch (m_state)
        {
        default: break;
        case State::Swing:
            m_activePoint.x = std::clamp(m_activePoint.x + evt.motion.xrel, -MaxAccuracy, MaxAccuracy);
            m_activePoint.y = std::clamp(m_activePoint.y - evt.motion.yrel, -(MaxDistance / 2.f), MaxDistance / 2.f);
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
                //TODO sometimes this is activated when letting go of trigger...
                //probably because the move when letting go enters this block
                //and the state has already been made inactive by stroke summary
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
                if (m_state == State::Swing)
                {
                    m_activePoint.y = (static_cast<float>(-evt.caxis.value) / ControllerAxisRange) * (MaxDistance / 2.f);
                }
            }
            break;
        case SDL_CONTROLLER_AXIS_LEFTX:
        case SDL_CONTROLLER_AXIS_RIGHTX:
            //just set this and we'll have
            //whichever value was present when
            //the swing is finished
            if (std::abs(evt.caxis.value) > MinControllerMove
                && m_state == State::Swing)
            {
                m_activePoint.x = (static_cast<float>(evt.caxis.value) / ControllerAxisRange) * MaxAccuracy;
            }
            break;
        }
        break;
    }
}

void InputHandler::process(float)
{
    switch (m_state)
    {
    default: break;
    case State::Swing:
    {
        //moving down
        if (m_activePoint.y < m_frontPoint.y)
        {
            m_backPoint = m_activePoint;
        }

        //started moving back up so time the ascent
        if (m_activePoint.y > m_backPoint.y
            && m_frontPoint.y == m_backPoint.y)
        {
            m_timer.restart();
        }

        //we've done full stroke
        if (m_activePoint.y > (MaxDistance / 2.f) - CommitDistance)
        {
            m_state = State::Summarise;
            m_elapsedTime = m_timer.restart();
        }

        m_frontPoint = m_activePoint;
    }
        break;
    case State::Summarise:
        m_state = State::Inactive;

        debug.distance = (m_frontPoint.y - m_backPoint.y);
        debug.velocity = debug.distance / (m_elapsedTime + 0.0001f); //potential NaN
        debug.accuracy = (m_frontPoint.x - m_backPoint.x);
        
        m_power = std::clamp(debug.velocity / MaxVelocity, 0.f, 1.f);
        m_hook = std::clamp(debug.accuracy / MaxAccuracy, -1.f, 1.f);


        //travelling a shorter distance doesn't imply lower
        //velocity so we need to create a distance based modifier
        float multiplier = debug.distance / MaxDistance;
        m_power *= multiplier;

        break;
    }
}