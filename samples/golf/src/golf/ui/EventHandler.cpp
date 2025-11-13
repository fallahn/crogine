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

#include "../UserInterface.hpp"
#include "../SharedStateData.hpp"
#include "../MessageIDs.hpp"

namespace
{

}

bool handleTopLevelEvent(const cro::Event& evt, SharedStateData& sharedData, HelpNav& helpNav, OptionsContext& optionsContext)
{
    if (sharedData.showHelp)
    {
        const auto doScroll =
            [&]()
            {
                helpNav.wantsScroll = true;
                auto* msg = cro::App::getInstance().postMessage<SystemEvent>(cl::MessageID::SystemMessage);
                msg->type = SystemEvent::MenuSwitched;
            };

        const auto scrollUp =
            [&]()
            {
                helpNav.targetIndex = (helpNav.selectedScroll + (helpNav.chapterCount - 1)) % helpNav.chapterCount;
                doScroll();
            };
        const auto scrollDown =
            [&]()
            {
                helpNav.targetIndex = (helpNav.selectedScroll + 1) % helpNav.chapterCount;
                doScroll();
            };

        switch (evt.type)
        {
        default: break;
        case SDL_MOUSEBUTTONUP:
            if (evt.button.button == SDL_BUTTON_RIGHT)
            {
                sharedData.showHelp = false;
            }
            break;
        case SDL_CONTROLLERBUTTONUP:
            switch (evt.cbutton.button)
            {
            default: break;
            case cro::GameController::ButtonB:
                sharedData.showHelp = false;
                break;
            case cro::GameController::DPadDown:
                scrollDown();
                break;
            case cro::GameController::DPadUp:
                scrollUp();
                break;
            }
            break;
        case SDL_KEYUP:
            switch (evt.key.keysym.sym)
            {
            default: break;
            case SDLK_ESCAPE:
            case SDLK_BACKSPACE:
                sharedData.showHelp = false;
                break;
            case SDLK_DOWN:
                scrollDown();
                break;
            case SDLK_UP:
                scrollUp();
                break;
            }
            break;
        }
        return true;
    }

    //TODO fix quit conditions which might be
    //otherwise triggered when interacting with
    //the options menu
    if (sharedData.showOptionsWindow)
    {
        const auto prevTab = 
            [&]()
            {
                optionsContext.tabIndex = (optionsContext.tabIndex + (OptionsContext::TabID::Count - 1)) % OptionsContext::TabID::Count;
                optionsContext.requestedTab = optionsContext.tabIndex;
                //TODO trigger audio somehow
            };

        const auto nextTab =
            [&]()
            {
                optionsContext.tabIndex = (optionsContext.tabIndex + 1) % OptionsContext::TabID::Count;
                optionsContext.requestedTab = optionsContext.tabIndex;
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
            
            if (evt.key.keysym.sym == sharedData.inputBinding.keys[InputBinding::PrevClub])
            {
                prevTab();
            }
            else if (evt.key.keysym.sym == sharedData.inputBinding.keys[InputBinding::NextClub])
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
        return true;
    }

    return false;
}