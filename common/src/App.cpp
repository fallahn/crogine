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

#include "glad/glad.h"

using namespace cro;

App* App::m_instance = nullptr;

App::App()
{
	CRO_ASSERT(m_instance == nullptr, "App instance already exists!");

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		Logger::log("Failed init video system");
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