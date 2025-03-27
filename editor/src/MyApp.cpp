/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2022
http://trederia.blogspot.com

crogine editor - Zlib license.

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
#include "ParticleState.hpp"
#include "LayoutState.hpp"
#include "SpriteState.hpp"
#include "LoadingScreen.hpp"
#include "SharedStateData.hpp"
#include "Messages.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/util/String.hpp>

namespace
{
    SharedStateData sharedData;
}

MyApp::MyApp()
    : cro::App  (cro::Window::Resizable),
    m_stateStack({*this, getWindow()})
{
    setApplicationStrings("Trederia", "crogine_editor");

    m_stateStack.registerState<ModelState>(States::ID::ModelViewer, sharedData);
    m_stateStack.registerState<WorldState>(States::ID::WorldEditor, sharedData);
    m_stateStack.registerState<ParticleState>(States::ID::ParticleEditor, sharedData);
    m_stateStack.registerState<LayoutState>(States::ID::LayoutEditor, sharedData);
    m_stateStack.registerState<SpriteState>(States::ID::SpriteEditor, sharedData);
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
    if (msg.id == MessageID::UIMessage)
    {
        const auto& data = msg.getData<UIEvent>();
        switch (data.type)
        {
        default: break;
        case UIEvent::WrotePreferences:
            savePrefs();
            break;
        }
    }

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
    loadPrefs();

    getWindow().setLoadingScreen<LoadingScreen>();
    getWindow().setTitle("Crogine Editor");

    m_stateStack.pushState(States::WorldEditor);
    //m_stateStack.pushState(States::ModelViewer);
    //m_stateStack.pushState(States::ParticleEditor);
    //m_stateStack.pushState(States::LayoutEditor);
    //m_stateStack.pushState(States::SpriteEditor);

    return true;
}

void MyApp::finalise()
{
    m_stateStack.clearStates();
    m_stateStack.simulate(0.f);

    savePrefs();
}

void MyApp::loadPrefs()
{
    cro::ConfigFile prefs;
    if (prefs.loadFromFile(cro::App::getPreferencePath() + "global.cfg"))
    {
        const auto& props = prefs.getProperties();
        for (const auto& prop : props)
        {
            auto name = cro::Util::String::toLower(prop.getName());
            if (name == "working_dir")
            {
                sharedData.workingDirectory = prop.getValue<std::string>();
                std::replace(sharedData.workingDirectory.begin(), sharedData.workingDirectory.end(), '\\', '/');
            }
            else if (name == "skybox")
            {
                sharedData.skymapTexture = prop.getValue<std::string>();
                std::replace(sharedData.skymapTexture.begin(), sharedData.skymapTexture.end(), '\\', '/');
            }
        }
    }
}

void MyApp::savePrefs()
{
    cro::ConfigFile prefsOut;
    prefsOut.addProperty("working_dir").setValue(sharedData.workingDirectory);
    prefsOut.addProperty("skybox").setValue(sharedData.skymapTexture);

    prefsOut.save(cro::App::getPreferencePath() + "global.cfg");
}