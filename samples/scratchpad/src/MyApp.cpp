/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2024
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
#include "bounce/BounceState.hpp"
#include "bush/BushState.hpp"
#include "bsp/BspState.hpp"
#include "collision/CollisionState.hpp"
#include "frustum/FrustumState.hpp"
#include "voxels/VoxelState.hpp"
#include "vats/VatsState.hpp"
#include "retro/RetroState.hpp"
#include "rolling/RollingState.hpp"
#include "swingput/SwingState.hpp"
#include "animblend/AnimBlendState.hpp"
#include "ssao/SSAOState.hpp"
#include "log/LogState.hpp"
#include "gc/GcState.hpp"
#include "interiormapping/InteriormappingState.hpp"
#include "endless/EndlessDrivingState.hpp"
#include "LoadingScreen.hpp"

#include <crogine/core/Clock.hpp>

namespace
{
    std::string getBasePath()
    {
        auto path = cro::App::getPreferencePath() + "user/";
        if (!cro::FileSystem::directoryExists(path))
        {
            cro::FileSystem::createDirectory(path);
        }
        return path;
    }

    std::string getContentPath()
    {
        return getBasePath() + "content/";
    }
}

MyApp::MyApp()
    : m_stateStack({*this, getWindow()})
{
}

//public
void MyApp::loadPlugin(const std::string& path)
{
    cro::App::loadPlugin(path, m_stateStack);
}

void MyApp::unloadPlugin()
{
    cro::App::unloadPlugin(m_stateStack);
}

//private
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
        //case SDLK_p:
        //{
        //    cro::FileSystem::createDirectory(getPreferencePath() + u8"wisp€fun");
        //    cro::FileSystem::createDirectory(getBasePath());
        //    cro::FileSystem::createDirectory(getContentPath());

        //    auto dirs = cro::FileSystem::listDirectories(getPreferencePath());
        //    for (auto dir : dirs)
        //    {
        //        std::cout << dir << std::endl;
        //        cro::ConfigFile cfg;
        //        cfg.save(getPreferencePath() + dir + ".cfg");
        //    }
        //    LogI << "found " << dirs.size() << " entries" << std::endl;

        //    auto files = cro::FileSystem::listFiles(getPreferencePath());
        //    for (auto file : files)
        //    {
        //        std::cout << file << std::endl;
        //        cro::ConfigFile cfg;
        //        cfg.save(getBasePath() + file);
        //        cfg.save(getContentPath() + file);
        //    }
        //    LogI << "found " << files.size() << " files" << std::endl;
        //}
        //    break;
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

    m_stateStack.registerState<sp::MenuState>(States::ScratchPad::MainMenu, *this);
    m_stateStack.registerState<BatcatState>(States::ScratchPad::BatCat);
    m_stateStack.registerState<BilliardsState>(States::ScratchPad::Billiards);
    m_stateStack.registerState<BushState>(States::ScratchPad::Bush);
    m_stateStack.registerState<BspState>(States::ScratchPad::BSP);
    m_stateStack.registerState<CollisionState>(States::ScratchPad::MeshCollision);
    m_stateStack.registerState<VoxelState>(States::ScratchPad::Voxels);
    m_stateStack.registerState<VatsState>(States::ScratchPad::VATs);
    m_stateStack.registerState<RetroState>(States::ScratchPad::Retro);
    m_stateStack.registerState<FrustumState>(States::ScratchPad::Frustum);
    m_stateStack.registerState<RollingState>(States::ScratchPad::Rolling);
    m_stateStack.registerState<SwingState>(States::ScratchPad::Swing);
    m_stateStack.registerState<AnimBlendState>(States::ScratchPad::AnimBlend);
    m_stateStack.registerState<SSAOState>(States::ScratchPad::SSAO);
    m_stateStack.registerState<LogState>(States::ScratchPad::Log);
    m_stateStack.registerState<GCState>(States::ScratchPad::GC);
    m_stateStack.registerState<BounceState>(States::ScratchPad::Bounce);
    m_stateStack.registerState<InteriorMappingState>(States::ScratchPad::InteriorMapping);
    m_stateStack.registerState<EndlessDrivingState>(States::ScratchPad::EndlessDriving);

#ifdef CRO_DEBUG_
    m_stateStack.pushState(States::ScratchPad::EndlessDriving);
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

    cro::App::unloadPlugin(m_stateStack);
}