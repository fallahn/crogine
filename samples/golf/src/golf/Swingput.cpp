/*-----------------------------------------------------------------------

Matt Marchant 2022
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

    constexpr float MaxControllerVelocity = 3000.f;
    constexpr float MaxMouseVelocity = 1700.f;
    constexpr float MaxAccuracy = 20.f;
    //constexpr float CommitDistance = 4.f;// 0.01f; //TODO make this user variable

    constexpr float ControllerAxisRange = 32767.f;
    constexpr std::int16_t MinTriggerMove = 16000;
    constexpr std::int16_t MinStickMove = 8000;
}

Swingput::Swingput(const SharedStateData& sd)
    : m_sharedData  (sd),
    m_enabled       (-1),
    m_backPoint     (0.f),
    m_activePoint   (0.f),
    m_frontPoint    (0.f),
    m_power         (0.f),
    m_hook          (0.f),
    m_maxVelocity   (1.f),
    m_mouseScale    (1.f),
    m_lastLT        (0),
    m_lastRT        (0),
    m_elapsedTime   (0.f)
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
bool Swingput::handleEvent(const cro::Event& evt)
{
    if (m_enabled == -1
        || !m_sharedData.useSwingput)
    {
        return false;
    }

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
    };

    switch (evt.type)
    {
    default: return isActive();
    case SDL_MOUSEBUTTONDOWN:
        if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            startStroke(MaxMouseVelocity);
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
        switch (m_state)
        {
        default: break;
        case State::Swing:
            m_activePoint.x = std::clamp(m_activePoint.x + (static_cast<float>(evt.motion.xrel) / 10.f/* / m_mouseScale*/), -MaxAccuracy, MaxAccuracy);
            m_activePoint.y = std::clamp(m_activePoint.y - (static_cast<float>(evt.motion.yrel)/* / m_mouseScale*/), -(MaxSwingputDistance / 2.f), MaxSwingputDistance / 2.f);
            return true;
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

                return (evt.caxis.value > MinTriggerMove);
            case SDL_CONTROLLER_AXIS_LEFTY:
            case SDL_CONTROLLER_AXIS_RIGHTY:

                if (std::abs(evt.caxis.value) > MinStickMove)
                {
                    if (m_state == State::Swing)
                    {
                        m_activePoint.y = std::pow((static_cast<float>(-evt.caxis.value) / ControllerAxisRange), 7.f) * (MaxSwingputDistance / 2.f);
                        return true;
                    }
                }
                return false;
            case SDL_CONTROLLER_AXIS_LEFTX:
            case SDL_CONTROLLER_AXIS_RIGHTX:
                //just set this and we'll have
                //whichever value was present when
                //the swing is finished

                if (std::abs(evt.caxis.value) > MinStickMove
                    && m_state == State::Swing)
                {
                    m_activePoint.x = (static_cast<float>(evt.caxis.value) / ControllerAxisRange) * MaxAccuracy;
                    return true;
                }
                return false;
            }
        }
        return isActive();
    }

    return isActive();
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

        //we've done full stroke.
        if (m_activePoint.y > (MaxSwingputDistance / 2.f) - /*CommitDistance*/m_sharedData.swingputThreshold)
        {
            m_state = State::Summarise;
            m_elapsedTime = m_timer.restart();
        }

        m_frontPoint = m_activePoint;
    }
    return false;
    case State::Summarise:
        m_state = State::Inactive;

#ifdef CRO_DEBUG_
        debugOutput.distance = (m_frontPoint.y - m_backPoint.y);
        debugOutput.velocity = debugOutput.distance / (m_elapsedTime + 0.0001f); //potential NaN
        debugOutput.accuracy = (m_frontPoint.x - m_backPoint.x);
        m_power = std::clamp(debugOutput.velocity / m_maxVelocity, 0.f, 1.f);
        m_hook = std::clamp(debugOutput.accuracy / MaxAccuracy, -1.f, 1.f);

        //travelling a shorter distance doesn't imply lower
        //velocity so we need to create a distance based modifier
        float multiplier = debugOutput.distance / MaxSwingputDistance;

#else
        float distance = (m_frontPoint.y - m_backPoint.y);
        float velocity = distance / (m_elapsedTime + 0.0001f); //potential NaN
        float accuracy = (m_frontPoint.x - m_backPoint.x);
        m_power = std::clamp(velocity / m_maxVelocity, 0.f, 1.f);
        m_hook = std::clamp(accuracy / MaxAccuracy, -1.f, 1.f);

        //travelling a shorter distance doesn't imply lower
        //velocity so we need to create a distance based modifier
        float multiplier = distance / MaxSwingputDistance;
#endif

        //hmm have to double convert this because the input parser
        //actually calcs it to 0-1 and converts back to -1 1 on return
        m_hook += 1.f;
        m_hook /= 2.f;

        m_power *= multiplier;

        return true;
    }
    return false;
}

void Swingput::setEnabled(std::int32_t enabled)
{
    m_enabled = enabled; 
    m_lastLT = 0;
    m_lastRT = 0;
}