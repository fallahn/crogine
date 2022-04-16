/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2022
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
#include "billiards/BilliardsState.hpp"
#include "bsp/BspState.hpp"
#include "collision/CollisionState.hpp"
#include "voxels/VoxelState.hpp"
#include "LoadingScreen.hpp"

#include <crogine/core/Clock.hpp>

namespace
{

}

MyApp::MyApp()
    : m_stateStack({*this, getWindow()})
{

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

    m_stateStack.registerState<sp::MenuState>(States::ScratchPad::MainMenu);
    m_stateStack.registerState<BatcatState>(States::ScratchPad::BatCat);
    m_stateStack.registerState<BilliardsState>(States::ScratchPad::Billiards);
    m_stateStack.registerState<BspState>(States::ScratchPad::BSP);
    m_stateStack.registerState<CollisionState>(States::ScratchPad::MeshCollision);
    m_stateStack.registerState<VoxelState>(States::ScratchPad::Voxels);

#ifdef CRO_DEBUG_
    //m_stateStack.pushState(States::ScratchPad::Billiards);
    m_stateStack.pushState(States::ScratchPad::MainMenu);
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