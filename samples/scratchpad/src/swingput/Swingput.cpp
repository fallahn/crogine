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

#include "Swingput.hpp"

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
    constexpr float MaxControllerVelocity = 3000.f;
    constexpr float MaxMouseVelocity = 1700.f;
    constexpr float MaxAccuracy = 20.f;
    constexpr float CommitDistance = 4.f;// 0.01f;

    constexpr float ControllerAxisRange = 32767.f;
    constexpr std::int16_t MinTriggerMove = 16000;
    constexpr std::int16_t MinStickMove = 8000;
}

Swingput::Swingput()
    : m_backPoint       (0.f),
    m_activePoint       (0.f),
    m_frontPoint        (0.f),
    m_power             (0.f),
    m_hook              (0.f),
    m_maxVelocity       (1.f),
    m_lastLT            (0),
    m_lastRT            (0),
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
bool Swingput::handleEvent(const cro::Event& evt)
{
    const auto startStroke = [&](float maxVelocity)
    {
        if (m_state == State::Inactive
            && (m_lastLT < MinTriggerMove)
            && (m_lastRT < MinTriggerMove))
        {
            m_state = State::Swing;
            m_backPoint = { 0.f, 0.f };
            m_activePoint = { 0.f, 0.f };
            m_frontPoint = { 0.f, 0.f };
            m_power = 0.f;
            m_hook = 0.f;

            m_maxVelocity = maxVelocity;

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
    default: return false;
    case SDL_MOUSEBUTTONDOWN:
        if (evt.button.button == SDL_BUTTON_LEFT)
        {
            startStroke(MaxMouseVelocity);
        }
        return true;
    case SDL_MOUSEBUTTONUP:
        if (evt.button.button == SDL_BUTTON_LEFT)
        {
            endStroke();
        }
        return true;
    case SDL_MOUSEMOTION:
        //TODO we need to scale this down relative to the game buffer size
        switch (m_state)
        {
        default: break;
        case State::Swing:
            m_activePoint.x = std::clamp(m_activePoint.x + evt.motion.xrel, -MaxAccuracy, MaxAccuracy);
            m_activePoint.y = std::clamp(m_activePoint.y - evt.motion.yrel, -(MaxDistance / 2.f), MaxDistance / 2.f);
            return true;
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
                startStroke(MaxControllerVelocity);
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
            return true;
        case SDL_CONTROLLER_AXIS_LEFTY:
        case SDL_CONTROLLER_AXIS_RIGHTY:

            if (std::abs(evt.caxis.value) > MinStickMove)
            {
                if (m_state == State::Swing)
                {
                    m_activePoint.y = std::pow((static_cast<float>(-evt.caxis.value) / ControllerAxisRange), 5.f) * (MaxDistance / 2.f);
                }
            }
            return true;
        case SDL_CONTROLLER_AXIS_LEFTX:
        case SDL_CONTROLLER_AXIS_RIGHTX:
            //just set this and we'll have
            //whichever value was present when
            //the swing is finished

            if (std::abs(evt.caxis.value) > MinStickMove
                && m_state == State::Swing)
            {
                m_activePoint.x = (static_cast<float>(evt.caxis.value) / ControllerAxisRange) * MaxAccuracy;
            }
            return true;
        }
        return false;
    }

    return false;
}

bool Swingput::process(float)
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

        //we've done full stroke. TODO commit distance needs
        //to be changable - stick ranges vary between controllers
        //and even between sticks on the same controller...
        if (m_activePoint.y > (MaxDistance / 2.f) - CommitDistance)
        {
            m_state = State::Summarise;
            m_elapsedTime = m_timer.restart();
        }

        m_frontPoint = m_activePoint;
    }
    return false;
    case State::Summarise:
        m_state = State::Inactive;

        debug.distance = (m_frontPoint.y - m_backPoint.y);
        debug.velocity = debug.distance / (m_elapsedTime + 0.0001f); //potential NaN
        debug.accuracy = (m_frontPoint.x - m_backPoint.x);
        
        m_power = std::clamp(debug.velocity / m_maxVelocity, 0.f, 1.f);
        m_hook = std::clamp(debug.accuracy / MaxAccuracy, -1.f, 1.f);


        //travelling a shorter distance doesn't imply lower
        //velocity so we need to create a distance based modifier
        float multiplier = debug.distance / MaxDistance;
        m_power *= multiplier;

        return true;
    }
    return false;
}