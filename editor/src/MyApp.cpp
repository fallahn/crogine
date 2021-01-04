/*-----------------------------------------------------------------------

Matt Marchant 2020
http://trederia.blogspot.com

crogine model viewer/importer - Zlib license.

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
#include "ModelState.hpp"
#include "WorldState.hpp"
#include "LoadingScreen.hpp"
#include "SharedStateData.hpp"

#include <crogine/core/Clock.hpp>

namespace
{
    SharedStateData sharedData;
}

MyApp::MyApp()
    : m_stateStack({*this, getWindow()})
{
    setApplicationStrings("Trederia", "Crogine Editor");

    sharedData.gizmo = &m_gizmo;
    m_stateStack.registerState<ModelState>(States::ID::ModelViewer, sharedData);
    m_stateStack.registerState<WorldState>(States::ID::WorldEditor, sharedData);
}

//public
void MyApp::handleEvent(const cro::Event& evt)
{
    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
#ifdef CRO_DEBUG_
        case SDLK_ESCAPE:
        case SDLK_AC_BACK:
            App::quit();
            break;
#endif
        }
    }
    
    m_stateStack.handleEvent(evt);
}

void MyApp::handleMessage(const cro::Message& msg)
{
    m_stateStack.handleMessage(msg);
}

void MyApp::simulate(float dt)
{
    m_stateStack.simulate(dt);

    //all states have updated so should be safe to unlock
    m_gizmo.unlock();
}

void MyApp::render()
{
    m_stateStack.render();

    m_gizmo.draw();
}

bool MyApp::initialise()
{
    getWindow().setLoadingScreen<LoadingScreen>();
    getWindow().setTitle("Crogine Editor");

    m_gizmo.init();

    //m_stateStack.pushState(States::WorldEditor);
    m_stateStack.pushState(States::ModelViewer);

    return true;
}

void MyApp::finalise()
{
    m_stateStack.clearStates();
    m_stateStack.simulate(0.f);

    m_gizmo.finalise();
}