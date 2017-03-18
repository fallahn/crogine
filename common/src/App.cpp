/*---------------------------------------------------------------------- -

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

---------------------------------------------------------------------- -*/

#include <crogine/App.hpp>
#include <crogine/Log.hpp>
#include <crogine/Window.hpp>
#include <crogine/detail/Assert.hpp>

#include <SDL.h>
#include <SDL_mixer.h>

#include "glad/glad.h"

using namespace cro;

App* App::m_instance = nullptr;

namespace
{
	void testAudio()
	{
		if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) >= 0)
		{
			auto mus = Mix_LoadMUS("assets/test_music.ogg");
			if (!mus)
			{
				std::string err(Mix_GetError());
				Logger::log("Failed to load music: " + err);
			}
			else
			{
				Mix_PlayMusic(mus, 0);
				//blocks but good enough for a test
				while (Mix_PlayingMusic()) {}
				Mix_FreeMusic(mus);
			}

			auto effect = Mix_LoadWAV("assets/test_effect.wav");
			if (!effect)
			{
				std::string err(Mix_GetError());
				Logger::log("Failed to open Effect: " + err, Logger::Type::Error);
			}
			else
			{
				Mix_PlayChannel(-1, effect, 0);
				while (Mix_Playing(-1)) {}
				Mix_FreeChunk(effect);
			}
			Mix_Quit();
		}
		else
		{
			std::string err(Mix_GetError());
			Logger::log("Failed to init SDL2 Mixer: " + err);
		}
	}
}

App::App()
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
	cro::Window win;
	if (win.create(800, 600, "cro works!"))
	{
		//load opengl
		if (!gladLoadGLES2Loader(SDL_GL_GetProcAddress))
		{
			Logger::log("Failed loading OpenGL", Logger::Type::Error);
			return;
		}
	}
	else
	{
		Logger::log("Failed creating main window", Logger::Type::Error);
		return;
	}

	testAudio();

	while (win.isOpen())
	{
		cro::Event evt;
		while (win.pollEvent(evt))
		{
			if (evt.type == SDL_QUIT)
			{
				win.close();
			}
		}

		win.clear();
		draw();
		win.display();
	}
}


//private