/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine - Zlib license.

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

#include <crogine/core/App.hpp>
#include <crogine/core/Log.hpp>
#include <crogine/core/Window.hpp>
#include <crogine/core/Console.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/detail/Assert.hpp>

#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>
#include <SDL_joystick.h>
#include <SDL_filesystem.h>

#include "../detail/GLCheck.hpp"
#include "../imgui/imgui_render.h"
#include "../imgui/imgui.h"

#include "../audio/AudioRenderer.hpp"

#include <algorithm>

using namespace cro;

cro::App* App::m_instance = nullptr;

namespace
{    
    const Time frameTime = seconds(1.f / 60.f);
	Time timeSinceLastUpdate;

#include "../detail/DefaultIcon.inl"

#ifndef ORG_PATH
#define ORG_PATH "Trederia"
#endif
#ifndef APP_PATH
#define APP_PATH "CrogineApp"
#endif

    const std::string cfgName("cfg.cfg");
}

App::App()
    : m_frameClock(nullptr), m_running (false), m_showStats(true)
{
	CRO_ASSERT(m_instance == nullptr, "App instance already exists!");

    //audio should be initialised specifically only if the
    //audio implementation requires it (and therefore by the implementation itself)
	if (SDL_Init(SDL_INIT_EVERYTHING & ~SDL_INIT_AUDIO) < 0)
	{
		const std::string err(SDL_GetError());
		Logger::log("Failed init: " + err, Logger::Type::Error);
	}
	else
	{
		m_instance = this;
        if (TTF_Init() == -1)
        {
            Logger::log("Something when wrong initialising TTF fonts!", Logger::Type::Error);
        }

        for (auto i = 0; i < SDL_NumJoysticks(); ++i)
        {
            if (SDL_IsGameController(i))
            {
                //add to game controllers
                m_controllers.insert(std::make_pair(i, SDL_GameControllerOpen(i)));
            }
            else
            {
                //add to joysticks
                m_joysticks.insert(std::make_pair(i, SDL_JoystickOpen(i)));
            }
        }

        if (!AudioRenderer::init())
        {
            Logger::log("Failed to initialise audio renderer", Logger::Type::Error);
        }

        char* pp = SDL_GetPrefPath(ORG_PATH, APP_PATH);
        m_prefPath = std::string(pp);
        SDL_free(pp);
	}
}

App::~App()
{
    AudioRenderer::shutdown();
    
    for (auto js : m_joysticks)
    {
        SDL_JoystickClose(js.second);
    }

    for (auto ct : m_controllers)
    {
        SDL_GameControllerClose(ct.second);
    }
    
    //SDL cleanup
    TTF_Quit();
	SDL_Quit();
}

//public
void App::run()
{
    int32 width = 800;
    int32 height = 600;
    bool fullscreen = false;

    ConfigFile cfg;
    if (cfg.loadFromFile(m_prefPath + cfgName))
    {
        const auto& properties = cfg.getProperties();
        for (const auto& prop : properties)
        {
            if (prop.getName() == "width" && prop.getValue<int>() > 0)
            {
                width = prop.getValue<int>();
            }
            else if (prop.getName() == "height" && prop.getValue<int>() > 0)
            {
                height = prop.getValue<int>();
            }
            else if (prop.getName() == "fullscreen")
            {
                fullscreen = prop.getValue<bool>();
            }
        }
    }

	if (m_window.create(width, height, "crogine game"))
	{
		//load opengl
		if (!gladLoadGLES2Loader(SDL_GL_GetProcAddress))
		{
			Logger::log("Failed loading OpenGL", Logger::Type::Error);
			return;
		}
        IMGUI_INIT(m_window.m_window);
        m_window.setIcon(defaultIcon);
        m_window.setFullScreen(fullscreen);
        Console::init();
	}
	else
	{
		Logger::log("Failed creating main window", Logger::Type::Error);
		return;
	}
    initialise();

    Clock frameClock;
    m_frameClock = &frameClock;
    m_running = true;
	while (m_running)
	{
		timeSinceLastUpdate = frameClock.restart();
        //Let's go with flexible time and let physics systems
        //themselves worry about fixed steps (may even facilitate threading)

		//while (timeSinceLastUpdate > frameTime)
		{
			//timeSinceLastUpdate -= frameTime;

            handleEvents();
            handleMessages();

			//simulate(frameTime);
            simulate(timeSinceLastUpdate);
		}
        //DPRINT("Frame time", std::to_string(timeSinceLastUpdate.asMilliseconds()));
        IMGUI_UPDATE;

		m_window.clear();
        render();
        IMGUI_RENDER;
		m_window.display();

        //SDL_Delay((frameTime - timeSinceLastUpdate).asMilliseconds());
	}

    auto size = m_window.getSize();
    fullscreen = m_window.isFullscreen();
    ConfigFile saveSettings;
    saveSettings.addProperty("width", std::to_string(size.x));
    saveSettings.addProperty("height", std::to_string(size.y));
    saveSettings.addProperty("fullscreen", fullscreen ? "true" : "false");
    saveSettings.save(m_prefPath + cfgName);

    Console::finalise();
    m_messageBus.disable(); //prevents spamming a load of quit messages
    finalise();
    IMGUI_UNINIT;
    m_window.close();
}

void App::setClearColour(Colour colour)
{
	m_clearColour = colour;
	glCheck(glClearColor(colour.getRed(), colour.getGreen(), colour.getBlue(), colour.getAlpha()));
}

const Colour& App::getClearColour() const
{
	return m_clearColour;
}

void App::quit()
{
    //properly quit the application
    if (m_instance)
    {
        m_instance->m_running = false;
    }
}

void App::debugPrint(const std::string& name, const std::string& value)
{
    CRO_ASSERT(m_instance, "Not now, fuzznuts");
#if defined _DEBUG_ && defined USE_IMGUI
    m_instance->m_debugLines.emplace_back(name + " : " + value);
#endif
}

Window& App::getWindow()
{
    CRO_ASSERT(m_instance, "No valid app instance");
    return m_instance->m_window;
}

const std::string& App::getPreferencePath()
{
    CRO_ASSERT(m_instance, "No valid app instance");
    return m_instance->m_prefPath;
}

void App::resetFrameTime()
{
    CRO_ASSERT(m_frameClock, "App not initialised");
    m_frameClock->restart();
}

//private
void App::handleEvents()
{
    cro::Event evt;
    while (m_window.pollEvent(evt))
    {
        bool eventConsumed = false;
        IMGUI_EVENTS(evt);

        switch (evt.type)
        {
        default: break;
        case SDL_KEYUP:
#ifdef USE_IMGUI
            if (evt.key.keysym.sym == SDLK_F1)
            {
                m_showStats = !m_showStats;
            }
            else if (evt.key.keysym.scancode == SDL_SCANCODE_GRAVE)
            {
                Console::show();
            }
#endif //USE_IMGUI
            break;
        case SDL_QUIT:
            quit();
            break;
        case SDL_WINDOWEVENT:
        {
            auto* msg = m_messageBus.post<Message::WindowEvent>(Message::Type::WindowMessage);
            msg->event = evt.window.event;
            msg->windowID = evt.window.windowID;
            msg->data0 = evt.window.data1;
            msg->data1 = evt.window.data2;
        }
        //prevents spamming the next frame with a giant DT
        if (evt.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
        {
            resetFrameTime();
        }

            break;
        case SDL_CONTROLLERDEVICEADDED:
        {
            auto id = evt.cdevice.which;
            if (SDL_IsGameController(id))
            {
                m_controllers.insert(std::make_pair(id, SDL_GameControllerOpen(id)));
            }
            else
            {
                m_joysticks.insert(std::make_pair(id, SDL_JoystickOpen(id)));
            }
        }
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
        {
            auto id = evt.cdevice.which;

            if (m_controllers.count(id) > 0)
            {
                SDL_GameControllerClose(m_controllers[id]);
                m_controllers.erase(id);
            }

            if (m_joysticks.count(id) > 0)
            {
                SDL_JoystickClose(m_joysticks[id]);
                m_joysticks.erase(id);
            }
        }
            break;
        }

        if(!eventConsumed) handleEvent(evt);
    }
}

void App::handleMessages()
{
    while (!m_messageBus.empty())
    {
        auto msg = m_messageBus.poll();

        /*switch (msg.id)
        {

        default: break;
        }*/

        handleMessage(msg);
    }
}

//#ifdef USE_IMGUI
void App::doImGui()
{
    ImGui_ImplSdlGL3_NewFrame(m_window.m_window);
    //ImGui::ShowTestWindow(&m_showStats);

    //show other windows (console etc)
    Console::draw();

    for (auto& f : m_guiWindows) f.first();

#ifdef USE_IMGUI
    if (m_showStats)
    {
        ImGui::SetNextWindowSizeConstraints({ 360.f, 200.f }, { 400.f, 1000.f });
        ImGui::Begin("Stats:", &m_showStats);
        static bool vsync = true;
        bool lastSync = vsync;
        ImGui::Checkbox("Vsync", &vsync);
        if (lastSync != vsync)
        {
            m_window.setVsyncEnabled(vsync);
        }

        ImGui::SameLine();
        static bool fullScreen = m_window.isFullscreen();
        bool lastFS = fullScreen;
        ImGui::Checkbox("Full Screen", &fullScreen);
        if (lastFS != fullScreen)
        {
            m_window.setFullScreen(fullScreen);
        }

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::NewLine();

        //display any registered controls
        for (const auto& func : m_statusControls)
        {
            func.first();
        }

        //print any debug lines       
        for (const auto& p : m_debugLines)
        {
            ImGui::Text(p.c_str());
        }

        ImGui::End();
    }
    m_debugLines.clear();
    m_debugLines.reserve(10);

#endif //USE_IMGUI
}

void App::addStatusControl(const std::function<void()>& func, const GuiClient* c)
{
    CRO_ASSERT(m_instance, "App not properly instanciated!");
#ifdef USE_IMGUI
    m_instance->m_statusControls.push_back(std::make_pair(func, c));
#endif
}

void App::removeStatusControls(const GuiClient* c)
{
    CRO_ASSERT(m_instance, "App not properly instanciated!");

#ifdef USE_IMGUI

    m_instance->m_statusControls.erase(
        std::remove_if(std::begin(m_instance->m_statusControls), std::end(m_instance->m_statusControls),
            [c](const std::pair<std::function<void()>, const GuiClient*>& pair)
    {
        return pair.second == c;
    }), std::end(m_instance->m_statusControls));

#endif //USE_IMGUI
}

void App::addWindow(const std::function<void()>& func, const GuiClient* c)
{
    CRO_ASSERT(m_instance, "App not properly instanciated!");
#ifdef USE_IMGUI
    m_instance->m_guiWindows.push_back(std::make_pair(func, c));
#endif
}

void App::removeWindows(const GuiClient* c)
{
    CRO_ASSERT(m_instance, "App not properly instanciated!");

#ifdef USE_IMGUI

    m_instance->m_guiWindows.erase(
        std::remove_if(std::begin(m_instance->m_guiWindows), std::end(m_instance->m_guiWindows),
            [c](const std::pair<std::function<void()>, const GuiClient*>& pair)
    {
        return pair.second == c;
    }), std::end(m_instance->m_guiWindows));

#endif //USE_IMGUI
}



//#endif
