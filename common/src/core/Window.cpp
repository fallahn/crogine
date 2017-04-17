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

#include <crogine/core/Window.hpp>
#include <crogine/core/Log.hpp>
#include <crogine/detail/SDLResource.hpp>

#include <SDL.h>
#include <SDL_video.h>

#include "../detail/glad.h"

#include <algorithm>

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
	if (!Detail::SDLResource::valid()) return false;
	
	destroy();

#ifdef __ANDROID__
    fullscreen = true;
    borderless = true;
#endif //__ANDROID__

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
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

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
    //glClearColor(1.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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

glm::uvec2 Window::getSize() const
{
    CRO_ASSERT(m_window, "window not created");
    int32 x, y;
    SDL_GetWindowSize(m_window, &x, &y);
    return { x, y };
}

void Window::setSize(glm::uvec2 size)
{
    CRO_ASSERT(m_window, "window not created");
    SDL_SetWindowSize(m_window, size.x, size.y);
}

void Window::setFullScreen(bool fullscreen)
{
    CRO_ASSERT(m_window, "window not created");
    SDL_SetWindowFullscreen(m_window, fullscreen ? SDL_WINDOW_FULLSCREEN : 0);
}

void Window::setPosition(int32 x, int32 y)
{
    CRO_ASSERT(m_window, "window not created");
    if (x < 0) x = SDL_WINDOWPOS_CENTERED;
    if (y < 0) y = SDL_WINDOWPOS_CENTERED;
    SDL_SetWindowPosition(m_window, x, y);
}

void Window::setIcon(const uint8* data)
{
    CRO_ASSERT(m_window, "window not created");
    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom((void*)(data), 16, 16, 32, 16 * 4, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
    if (surface)
    {
        SDL_SetWindowIcon(m_window, surface);
        SDL_FreeSurface(surface);
    }
    else
    {
        Logger::log("Failed creating icon from pixel data", Logger::Type::Error);
    }
}

const std::vector<glm::uvec2>& Window::getAvailableResolutions() const
{
    CRO_ASSERT(m_window, "window not created");
    if (m_resolutions.empty())
    {
        auto modeCount = SDL_GetNumDisplayModes(0);
        if (modeCount > 0)
        {
            SDL_DisplayMode mode = { SDL_PIXELFORMAT_UNKNOWN, 0, 0, 0, 0 };
            for (auto i = 0; i < modeCount; ++i)
            {
                if (SDL_GetDisplayMode(0, i, &mode) == 0)
                {
                    if (SDL_BITSPERPIXEL(mode.format) == 24)
                    {
                        m_resolutions.emplace_back(mode.w, mode.h);
                    }
                }
            }
            m_resolutions.erase(std::unique(std::begin(m_resolutions), std::end(m_resolutions)), std::end(m_resolutions));
        }
        else
        {
            std::string err(SDL_GetError());
            Logger::log("failed retrieving available resolutions: " + err, Logger::Type::Error, Logger::Output::All);
        }
    }
    return m_resolutions;
}

void Window::setTitle(const std::string& title)
{
    CRO_ASSERT(m_window, "window not created");
    SDL_SetWindowTitle(m_window, title.c_str());
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