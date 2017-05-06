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
#include <crogine/core/Clock.hpp>
#include <crogine/detail/Assert.hpp>

#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include "../detail/GLCheck.hpp"
#include "../imgui/imgui_render.h"
#include "../imgui/imgui.h"

using namespace cro;

cro::App* App::m_instance = nullptr;

namespace
{
	const Time frameTime = seconds(1.f / 60.f);
	Time timeSinceLastUpdate;

#include "../detail/DefaultIcon.inl"
}

App::App()
    : m_running (false)
{
	CRO_ASSERT(m_instance == nullptr, "App instance already exists!");

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
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
	}
}

App::~App()
{
	//SDL cleanup
    TTF_Quit();
	SDL_Quit();
}

//public
void App::run()
{
	if (m_window.create(1280, 720, "crogine game"))
	{
		//load opengl
		if (!gladLoadGLES2Loader(SDL_GL_GetProcAddress))
		{
			Logger::log("Failed loading OpenGL", Logger::Type::Error);
			return;
		}
        IMGUI_INIT(m_window.m_window);
        m_window.setIcon(defaultIcon);
	}
	else
	{
		Logger::log("Failed creating main window", Logger::Type::Error);
		return;
	}

	Clock frameClock;
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
#ifdef _DEBUG_
            m_debugLines.clear();
            m_debugLines.reserve(10);
#endif //_DEBUG_
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
#ifdef _DEBUG_
    m_instance->m_debugLines.emplace_back(name + " : " + value);
#endif
}

Window& App::getWindow()
{
    CRO_ASSERT(m_instance, "No valid app instance");
    return m_instance->m_window;
}

//private
void App::handleEvents()
{
    cro::Event evt;
    while (m_window.pollEvent(evt))
    {
        if (evt.type == SDL_QUIT)
        {
            quit();
        }
        else if (evt.type == SDL_WINDOWEVENT)
        {
            auto* msg = m_messageBus.post<Message::WindowEvent>(Message::Type::WindowMessage);
            msg->event = evt.window.event;
            msg->windowID = evt.window.windowID;
            msg->data0 = evt.window.data1;
            msg->data1 = evt.window.data2;
        }

        IMGUI_EVENTS(evt) handleEvent(evt);
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

//#ifndef __ANDROID__
void App::doImGui()
{
    ImGui_ImplSdlGL3_NewFrame(m_window.m_window);

    ImGui::Begin("Stats:");
    static bool vsync = true;
    bool lastSync = vsync;
    ImGui::Checkbox("Vsync", &vsync);
    if (lastSync != vsync)
    {
        m_window.setVsyncEnabled(vsync);
    }

    ImGui::SameLine();
    static bool fullScreen = false;
    bool lastFS = fullScreen;
    ImGui::Checkbox("Full Screen", &fullScreen);
    if (lastFS != fullScreen)
    {
        m_window.setFullScreen(fullScreen);
    }

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    //print any debug lines
    ImGui::NewLine();
    for (const auto& p : m_debugLines)
    {
        ImGui::Text(p.c_str());
    }

    ImGui::End();
}
//#endif
