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
#include "golf/GolfMenuState.hpp"
#include "golf/GolfState.hpp"
#include "golf/ErrorState.hpp"
#include "golf/OptionsState.hpp"
#include "golf/MenuConsts.hpp"
#include "LoadingScreen.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/core/Message.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/detail/Types.hpp>

namespace
{

}

MyApp::MyApp()
    : m_stateStack({*this, getWindow()})
{
    m_stateStack.registerState<GolfMenuState>(States::Golf::Menu, m_sharedGolfData);
    m_stateStack.registerState<GolfState>(States::Golf::Game, m_sharedGolfData);
    m_stateStack.registerState<ErrorState>(States::Golf::Error, m_sharedGolfData);
    m_stateStack.registerState<OptionsState>(States::Golf::Options, m_sharedGolfData);
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
    if (msg.id == cro::Message::StateMessage)
    {
        const auto& data = msg.getData<cro::Message::StateEvent>();
        if (data.action == cro::Message::StateEvent::Popped)
        {
            switch (data.id)
            {
            default: break;
            case States::Golf::Options:
                savePreferences();
                break;
            }
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
    registerConsoleTab("Advanced",
        [&]()
        {
            if (ImGui::Checkbox("PostProcess (not yet implemented)", &m_sharedGolfData.usePostProcess))
            {
                //TODO remove this - it just stops the assertion failure for having
                //no post process buffer created yet
                m_sharedGolfData.usePostProcess = false;
                if (m_sharedGolfData.usePostProcess)
                {
                    //TODO raise a message or something to resize the post process buffers
                }
            }        
        });

    getWindow().setLoadingScreen<LoadingScreen>();
    getWindow().setTitle("Golf!");

    setApplicationStrings("trederia", "golf");

    loadPreferences();

    m_sharedGolfData.clientConnection.netClient.create(4);
    m_sharedGolfData.sharedResources = std::make_unique<cro::ResourceCollection>();

    //preload resources which will be used in dynamically loaded menus
    m_sharedGolfData.sharedResources->fonts.load(FontID::UI, "assets/golf/fonts/IBM_CGA.ttf");

    cro::SpriteSheet s;
    s.loadFromFile("assets/golf/sprites/options.spt", m_sharedGolfData.sharedResources->textures);





#ifdef CRO_DEBUG_
    m_stateStack.pushState(States::Golf::Menu);
#else
    m_stateStack.pushState(States::Golf::Menu);
#endif

    return true;
}

void MyApp::finalise()
{
    savePreferences();

    for (auto& c : m_sharedGolfData.avatarTextures)
    {
        for (auto& t : c)
        {
            t = {};
        }
    }
    m_sharedGolfData.sharedResources.reset();

    m_stateStack.clearStates();
    m_stateStack.simulate(0.f);
}

void MyApp::loadPreferences()
{
    auto path = getPreferencePath() + "prefs.cfg";
    if (cro::FileSystem::fileExists(path))
    {
        cro::ConfigFile cfg;
        if (cfg.loadFromFile(path))
        {
            const auto& properties = cfg.getProperties();
            for (const auto& prop : properties)
            {
                const auto& name = prop.getName();
                if (name == "use_post_process")
                {
                    m_sharedGolfData.usePostProcess = prop.getValue<bool>();
                }
            }
        }
    }

    //read keybind bin
    path = getPreferencePath() + "keys.bind";

    if (cro::FileSystem::fileExists(path))
    {
        cro::RaiiRWops file;
        file.file = SDL_RWFromFile(path.c_str(), "rb");
        if (file.file)
        {
            auto size = SDL_RWsize(file.file);
            if (size == sizeof(InputBinding))
            {
                SDL_RWread(file.file, &m_sharedGolfData.inputBinding, size, 1);
                LOG("Read keybinds file", cro::Logger::Type::Info);
            }
        }
    }
}

void MyApp::savePreferences()
{
    auto path = getPreferencePath() + "prefs.cfg";
    cro::ConfigFile cfg("preferences");

    //advanced options
    cfg.addProperty("use_post_process").setValue(m_sharedGolfData.usePostProcess);

    cfg.save(path);


    //keybinds
    path = getPreferencePath() + "keys.bind";
    cro::RaiiRWops file;
    file.file = SDL_RWFromFile(path.c_str(), "wb");
    if (file.file)
    {
        SDL_RWwrite(file.file, &m_sharedGolfData.inputBinding, sizeof(InputBinding), 1);
    }
}