/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#include "DrivingState.hpp"
#include "SharedStateData.hpp"
#include "../GolfGame.hpp"

#include <crogine/gui/Gui.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{

}

DrivingState::DrivingState(cro::StateStack& stack, cro::State::Context context, SharedStateData& sd)
    : cro::State    (stack, context),
    m_sharedData    (sd),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus())
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    });
}

//public
bool DrivingState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_p:
        case SDLK_ESCAPE:
        case SDLK_BACKSPACE:
        case SDLK_PAUSE:
            requestStackPush(StateID::Pause);
            break;
            //make sure sysem buttons don't do anything
        case SDLK_F1:
        case SDLK_F5:

            break;
#ifdef CRO_DEBUG_

#endif
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        if (evt.cbutton.which == cro::GameController::deviceID(0))
        {
            switch (evt.cbutton.button)
            {
            default: break;
            case cro::GameController::ButtonStart:
                requestStackPush(StateID::Pause);
                break;
            }
        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void DrivingState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool DrivingState::simulate(float dt)
{
    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void DrivingState::render()
{
    //TODO render scene to buffer.
    m_gameScene.render(*GolfGame::getActiveTarget());
    m_uiScene.render(*GolfGame::getActiveTarget());
}

//private
void DrivingState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

}

void DrivingState::loadAssets()
{

}

void DrivingState::createScene()
{


    
}

void DrivingState::createUI()
{

}