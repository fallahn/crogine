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

#include <crogine/Window.hpp>
#include <crogine/Log.hpp>

#include <sdl2/SDL.h>

#include "glad/glad.h"

using namespace cro;

namespace
{

}

Window::Window()
	: m_window	(nullptr),
	m_context	(nullptr)
{

}

Window::~Window()
{
	destroy();
}

//public
bool Window::create(uint32 width, uint32 height, const std::string& title, bool fullscreen, bool borderless)
{
	destroy();

	int styleMask = SDL_WINDOW_OPENGL;
	if (fullscreen) styleMask |= SDL_WINDOW_FULLSCREEN;
	if (borderless) styleMask |= SDL_WINDOW_BORDERLESS;
	//TODO set up proper masks for all window options

	m_window = SDL_CreateWindow(title.c_str(),SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, styleMask);

	if (!m_window)
	{
		Logger::log("failed creating Window");
		return false;
	}
	else
	{
		m_context = SDL_GL_CreateContext(m_window);

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

		int maj, min;
		SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &maj);
		SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &min);

		if (maj < 3 || min < 3)
		{
			Logger::log("Unable to create requested context version");
			Logger::log("Returned version was: " + std::to_string(maj) + "." + std::to_string(min));
		}
		LOG("Returned version was: " + std::to_string(maj) + "." + std::to_string(min), Logger::Type::Info);
	}
	return true;
}

void Window::setVsyncEnabled(bool enabled)
{
	if (m_context)
	{
		SDL_GL_SetSwapInterval(enabled ? 1 : 0);
	}
}

void Window::clear()
{
	glClearColor(1.f, 0.f, 0.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
}

void Window::display()
{
	SDL_GL_SwapWindow(m_window);
}

bool Window::pollEvent(Event& evt)
{
	return SDL_PollEvent(&evt);
}

void Window::close()
{
	destroy();
}

//private
void Window::destroy()
{
	if (m_context)
	{
		SDL_GL_DeleteContext(m_context);
		m_context = nullptr;
	}

	if (m_window)
	{
		SDL_DestroyWindow(m_window);
		m_window = nullptr;
	}
}