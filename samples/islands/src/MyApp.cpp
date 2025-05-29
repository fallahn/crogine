/*-----------------------------------------------------------------------

Matt Marchant 2020
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
#include "GameState.hpp"
#include "MenuState.hpp"
#include "ErrorState.hpp"
#include "PauseState.hpp"
#include "LoadingScreen.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/util/Random.hpp>

namespace
{
    const std::vector<cro::String> playernames =
    {
        "Edwardo", "Cliff", "Theodora", "Annabelle", "Mr.Wilson", "Sarah"
    };
    std::string configPath;
}

MyApp::MyApp()
    : m_stateStack({*this, getWindow()})
{
    setApplicationStrings("crogine", "islands");

    m_stateStack.registerState<MenuState>(States::ID::MainMenu, m_sharedData);
    m_stateStack.registerState<GameState>(States::ID::Game, m_sharedData);
    m_stateStack.registerState<ErrorState>(States::ID::Error, m_sharedData);
    m_stateStack.registerState<PauseState>(States::ID::Pause, m_sharedData);
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
            getWindow().setMouseCaptured(false);
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
}

void MyApp::render()
{
    m_stateStack.render();
}

bool MyApp::initialise()
{
    getWindow().setLoadingScreen<LoadingScreen>();
    getWindow().setTitle("Desert Island Duel");

    m_stateStack.pushState(States::MainMenu);

    m_sharedData.clientConnection.netClient.create(4);

    configPath = cro::App::getPreferencePath() + "settings.cfg";
    loadSettings();

    return true;
}

void MyApp::finalise()
{
    saveSettings();

    m_stateStack.clearStates();
    m_stateStack.simulate(0.f);

    m_sharedData.serverInstance.stop();
}

void MyApp::loadSettings()
{
    cro::ConfigFile cfg;
    if (cfg.loadFromFile(configPath))
    {
        if (auto* name = cfg.findProperty("player_name"); name)
        {
            m_sharedData.localPlayer.name = name->getValue<std::string>().substr(0, ConstVal::MaxStringChars);
        }
        else
        {
            m_sharedData.localPlayer.name = playernames[cro::Util::Random::value(0u, playernames.size() - 1)];
        }

        if (auto* ip = cfg.findProperty("target_ip"); ip)
        {
            m_sharedData.targetIP = ip->getValue<std::string>();
        }
        else
        {
            m_sharedData.targetIP = "127.0.0.1";
        }

        //TODO read controller/keyboard settings
        m_sharedData.inputBindings[1].keys =
        {
            SDLK_HOME,
            SDLK_PAGEUP,
            SDLK_KP_0,
            SDLK_RSHIFT,
            SDLK_DELETE,
            SDLK_END,
            SDLK_RCTRL,
            SDLK_LEFT,
            SDLK_RIGHT,
            SDLK_UP,
            SDLK_DOWN
        };

        m_sharedData.inputBindings[2].controllerID = 0;
        m_sharedData.inputBindings[2].keys =
        {
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
        };

        m_sharedData.inputBindings[3].controllerID = 1;
        m_sharedData.inputBindings[3].keys =
        {
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
        };
    }
    else
    {
        //fill in some defaults
        m_sharedData.localPlayer.name = playernames[cro::Util::Random::value(0u, playernames.size() - 1)];
        m_sharedData.targetIP = "127.0.0.1";

        m_sharedData.inputBindings[1].keys =
        {
            SDLK_HOME,
            SDLK_PAGEUP,
            SDLK_KP_0,
            SDLK_RSHIFT,
            SDLK_DELETE,
            SDLK_END,
            SDLK_RCTRL,
            SDLK_LEFT,
            SDLK_RIGHT,
            SDLK_UP,
            SDLK_DOWN
        };

        m_sharedData.inputBindings[2].controllerID = 0;
        m_sharedData.inputBindings[2].keys =
        {
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
        };

        m_sharedData.inputBindings[3].controllerID = 1;
        m_sharedData.inputBindings[3].keys =
        {
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
        };
    }
}

void MyApp::saveSettings()
{
    cro::ConfigFile cfg;
    cfg.addProperty("player_name").setValue(m_sharedData.localPlayer.name);
    cfg.addProperty("target_ip").setValue(m_sharedData.targetIP);

    //TODO write key/controller bindings

    cfg.save(configPath);
}