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

#include "GolfGame.hpp"
#include "golf/MenuState.hpp"
#include "golf/GolfState.hpp"
#include "golf/ErrorState.hpp"
#include "golf/OptionsState.hpp"
#include "golf/PauseState.hpp"
#include "golf/MenuConsts.hpp"
#include "golf/GameConsts.hpp"
#include "golf/MessageIDs.hpp"
#include "LoadingScreen.hpp"
#include "SplashScreenState.hpp"

#include <crogine/audio/AudioMixer.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/Message.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/detail/Types.hpp>

namespace
{

}

GolfGame::GolfGame()
    : m_stateStack({*this, getWindow()})
{
    //must be sest before anything else cfg is still loaded from default path
    setApplicationStrings("trederia", "golf");

    m_stateStack.registerState<SplashState>(StateID::SplashScreen);
    m_stateStack.registerState<MenuState>(StateID::Menu, m_sharedData);
    m_stateStack.registerState<GolfState>(StateID::Game, m_sharedData);
    m_stateStack.registerState<ErrorState>(StateID::Error, m_sharedData);
    m_stateStack.registerState<OptionsState>(StateID::Options, m_sharedData);
    m_stateStack.registerState<PauseState>(StateID::Pause, m_sharedData);
}

//public
void GolfGame::handleEvent(const cro::Event& evt)
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

void GolfGame::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::StateMessage)
    {
        const auto& data = msg.getData<cro::Message::StateEvent>();
        if (data.action == cro::Message::StateEvent::Popped)
        {
            switch (data.id)
            {
            default: break;
            case StateID::Options:
                savePreferences();
                saveSettings();
                break;
            }
        }
    }

    m_stateStack.handleMessage(msg);
}

void GolfGame::simulate(float dt)
{
    m_stateStack.simulate(dt);
}

void GolfGame::render()
{
    m_stateStack.render();
}

bool GolfGame::initialise()
{
    registerConsoleTab("Advanced",
        [&]()
        {
            bool usePost = m_sharedData.usePostProcess;
            if (ImGui::Checkbox("PostProcess", &usePost))
            {
                //we rely on the message handler to actually
                //set the shared data property, in case we need to first create the buffer

                //raise a message to resize the post process buffers
                auto* msg = getMessageBus().post<SystemEvent>(MessageID::SystemMessage);
                msg->type = SystemEvent::PostProcessToggled;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("Not fully implemented, cos it's a bit crap.");
                ImGui::EndTooltip();
            }


            if (ImGui::Button("Choose Shader"))
            {
                cro::FileSystem::showMessageBox("", "Maybe I'll implement this one day");
            }
        });

    getWindow().setLoadingScreen<LoadingScreen>();
    getWindow().setTitle("Lotec Golf!");

    cro::AudioMixer::setLabel("Music", MixerChannel::Music);
    cro::AudioMixer::setLabel("Effects", MixerChannel::Effects);
    cro::AudioMixer::setLabel("Menu", MixerChannel::Menu);

    loadPreferences();

    m_sharedData.clientConnection.netClient.create(4);
    m_sharedData.sharedResources = std::make_unique<cro::ResourceCollection>();
    std::fill(m_sharedData.controllerIDs.begin(), m_sharedData.controllerIDs.end(), 0);

    //preload resources which will be used in dynamically loaded menus
    m_sharedData.sharedResources->fonts.load(FontID::UI, "assets/golf/fonts/IBM_CGA.ttf");
    m_sharedData.sharedResources->fonts.load(FontID::Info, "assets/golf/fonts/MCPixel.otf");

    m_sharedData.resolutions = getWindow().getAvailableResolutions();
    std::reverse(m_sharedData.resolutions.begin(), m_sharedData.resolutions.end());
    for (auto r : m_sharedData.resolutions)
    {
        auto& str = m_sharedData.resolutionStrings.emplace_back();
        str += std::to_string(r.x) + " x " + std::to_string(r.y);
    }

    cro::SpriteSheet s;
    s.loadFromFile("assets/golf/sprites/options.spt", m_sharedData.sharedResources->textures);
    s.loadFromFile("assets/golf/sprites/ui.spt", m_sharedData.sharedResources->textures);
    s.loadFromFile("assets/golf/sprites/scoreboard.spt", m_sharedData.sharedResources->textures);



#ifdef CRO_DEBUG_
    m_stateStack.pushState(StateID::Menu);
    //m_stateStack.pushState(StateID::SplashScreen);
#else
    m_stateStack.pushState(StateID::SplashScreen);
#endif

    return true;
}

void GolfGame::finalise()
{
    savePreferences();

    m_stateStack.clearStates();
    m_stateStack.simulate(0.f);

    for (auto& c : m_sharedData.avatarTextures)
    {
        for (auto& t : c)
        {
            t = {};
        }
    }
    m_sharedData.sharedResources.reset();
}

void GolfGame::loadPreferences()
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
                    m_sharedData.usePostProcess = prop.getValue<bool>();
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
                SDL_RWread(file.file, &m_sharedData.inputBinding, size, 1);
                LOG("Read keybinds file", cro::Logger::Type::Info);
            }
        }
    }
}

void GolfGame::savePreferences()
{
    auto path = getPreferencePath() + "prefs.cfg";
    cro::ConfigFile cfg("preferences");

    //advanced options
    cfg.addProperty("use_post_process").setValue(m_sharedData.usePostProcess);

    cfg.save(path);


    //keybinds
    path = getPreferencePath() + "keys.bind";
    cro::RaiiRWops file;
    file.file = SDL_RWFromFile(path.c_str(), "wb");
    if (file.file)
    {
        SDL_RWwrite(file.file, &m_sharedData.inputBinding, sizeof(InputBinding), 1);
    }
}