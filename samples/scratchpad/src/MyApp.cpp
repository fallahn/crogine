/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2021
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

#include "MyApp.hpp"
#include "MenuState.hpp"
#include "batcat/BatcatState.hpp"
#include "bsp/BspState.hpp"
#include "golf/GolfMenuState.hpp"
#include "golf/GolfState.hpp"
#include "golf/ErrorState.hpp"
#include "LoadingScreen.hpp"

#include <crogine/core/Clock.hpp>

namespace
{

}

MyApp::MyApp()
    : m_stateStack({*this, getWindow()})
{
    m_stateStack.registerState<sp::MenuState>(States::ScratchPad::MainMenu);
    m_stateStack.registerState<BatcatState>(States::ScratchPad::BatCat);
    m_stateStack.registerState<BspState>(States::ScratchPad::BSP);


    m_stateStack.registerState<GolfMenuState>(States::Golf::Menu, m_sharedGolfData);
    m_stateStack.registerState<GolfState>(States::Golf::Game, m_sharedGolfData);
    m_stateStack.registerState<ErrorState>(States::Golf::Error, m_sharedGolfData);
}

//public
void MyApp::handleEvent(const cro::Event& evt)
{
#ifdef CRO_DEBUG_
    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_ESCAPE:
        case SDLK_AC_BACK:
            App::quit();
            break;
        }
    }
#endif
    
    m_stateStack.handleEvent(evt);
}

void MyApp::handleMessage(const cro::Message& msg)
{
    m_stateStack.handleMessage(msg);
}

void MyApp::simulate(float dt)
{
    m_stateStack.simulate(dt);
}

void MyApp::render()
{
    m_stateStack.render();
}

bool MyApp::initialise()
{
    getWindow().setLoadingScreen<LoadingScreen>();
    getWindow().setTitle("Scratchpad Browser");

    //TODO we only want to do this if launching the golf game
    m_sharedGolfData.localPlayer.playerData[0].name = "Ralph";
    m_sharedGolfData.localPlayer.playerData[1].name = "Stacy";
    m_sharedGolfData.localPlayer.playerCount = 2;
    m_sharedGolfData.targetIP = "255.255.255.255";
    m_sharedGolfData.mapDirectory = "course_01";

    m_sharedGolfData.clientConnection.netClient.create(4);

#ifdef CRO_DEBUG_
    m_stateStack.pushState(States::Golf::Menu);
    //m_stateStack.pushState(States::ScratchPad::MainMenu);
#else
    m_stateStack.pushState(States::ScratchPad::MainMenu);
#endif

    return true;
}

void MyApp::finalise()
{
    m_stateStack.clearStates();
    m_stateStack.simulate(0.f);
}