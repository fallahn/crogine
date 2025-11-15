/*-----------------------------------------------------------------------

Matt Marchant 2025
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

#include "InputBinding.hpp"
#include "OptionsV2.hpp"
#include "SharedStateData.hpp"

#include <crogine/gui/Gui.hpp>

namespace
{

}

OptionsV2::OptionsV2(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State        (ss, ctx),
    m_sharedData        (sd),
    m_showOptions       (false),
    m_animationTarget   (0.f),
    m_animationProgress (0.f)
{
    registerWindow(std::bind(&OptionsV2::optionsWindow, this));
    
    m_flagPreview.init(m_sharedData.flagPath);
    m_flagPreview.setText(m_sharedData.flagText);
}

//public
bool OptionsV2::handleEvent(const cro::Event& evt)
{
    if (m_showOptions)
    {
        const auto prevTab =
            [&]()
            {
                m_navigationContext.tabIndex = (m_navigationContext.tabIndex + (NavigationContext::TabID::Count - 1)) % NavigationContext::TabID::Count;
                m_navigationContext.requestedTab = m_navigationContext.tabIndex;
                //TODO trigger audio somehow
            };

        const auto nextTab =
            [&]()
            {
                m_navigationContext.tabIndex = (m_navigationContext.tabIndex + 1) % NavigationContext::TabID::Count;
                m_navigationContext.requestedTab = m_navigationContext.tabIndex;
                //TODO trigger audio somehow
            };

        const auto setActiveInput =
            [&]()
            {
                //if mouse motion or key down

                //else controller axis or button
            };

        switch (evt.type)
        {
        default: break;
        case SDL_MOUSEBUTTONUP:
            /*if (evt.button.button == SDL_BUTTON_RIGHT)
            {
                sharedData.showOptionsWindow = false;
            }*/
            break;
        case SDL_CONTROLLERBUTTONUP:
            switch (evt.cbutton.button)
            {
            default: break;
                /*case cro::GameController::ButtonB:
                    sharedData.showOptionsWindow = false;
                    break;*/
            case cro::GameController::ButtonLeftShoulder:
                prevTab();
                break;
            case cro::GameController::ButtonRightShoulder:
                nextTab();
                break;
            }
            setActiveInput();
            break;
        case SDL_KEYUP:
            switch (evt.key.keysym.sym)
            {
            default: break;
                /*case SDLK_ESCAPE:
                case SDLK_BACKSPACE:
                    sharedData.showOptionsWindow = false;
                    break;*/
            }

            if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::PrevClub])
            {
                prevTab();
            }
            else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::NextClub])
            {
                nextTab();
            }
            setActiveInput();
            break;
        case SDL_MOUSEMOTION:
            cro::App::getWindow().setMouseCaptured(false);
            setActiveInput();
            break;
        case SDL_CONTROLLERAXISMOTION:
            cro::App::getWindow().setMouseCaptured(true);
            setActiveInput();
            break;
        }
    }

    return false;
}

void OptionsV2::handleMessage(const cro::Message&) {}

bool OptionsV2::simulate(float dt)
{
    const float animSpeed = dt * 4.f;
    if (m_animationTarget < m_animationProgress)
    {
        m_animationProgress = std::max(0.f, m_animationProgress - animSpeed);
        if (m_animationProgress == 0)
        {
            requestStackPop();
        }
    }
    else if (m_animationTarget > m_animationProgress)
    {
        m_animationProgress = std::min(1.f, m_animationProgress + animSpeed);
    }

    return true;
}

void OptionsV2::render() {}


//private
void OptionsV2::onCachedPush()
{
    m_showOptions = true;
    m_animationTarget = 1.f;
}

void OptionsV2::onCachedPop()
{
    m_showOptions = false;
}

void OptionsV2::closeWindow()
{
    m_animationTarget = 0.f;
}