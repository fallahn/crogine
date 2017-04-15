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

#include "glad/GLCheck.hpp"
#include "imgui/imgui_render.h"
#include "imgui/imgui.h"

using namespace cro;

cro::App* App::m_instance = nullptr;

namespace
{
	const Time frameTime = seconds(1.f / 60.f);
	Time timeSinceLastUpdate;
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
	}
}

App::~App()
{
	//SDL cleanup
	SDL_Quit();
}

//public
void App::run()
{
	if (m_window.create(1280, 720, "cro works!"))
	{
		//load opengl
		if (!gladLoadGLES2Loader(SDL_GL_GetProcAddress))
		{
			Logger::log("Failed loading OpenGL", Logger::Type::Error);
			return;
		}
        IMGUI_INIT(m_window.m_window);
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
		timeSinceLastUpdate += frameClock.restart();

		while (timeSinceLastUpdate > frameTime)
		{
			timeSinceLastUpdate -= frameTime;

			cro::Event evt;
			while (m_window.pollEvent(evt))
			{
                
                if (evt.type == SDL_QUIT)
				{
                    quit();
				}

                IMGUI_EVENTS(evt) handleEvent(evt);
			}
            m_debugLines.clear();
            m_debugLines.reserve(10);
			simulate(frameTime);
		}

        IMGUI_UPDATE;

		m_window.clear();
		render();
        IMGUI_RENDER;
		m_window.display();
	}
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
    m_instance->m_debugLines.emplace_back(name + " : " + value);
}

//private
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
