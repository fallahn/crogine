/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2024
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
#include <crogine/core/App.hpp>
#include <crogine/core/Cursor.hpp>
#include <crogine/core/Message.hpp>
#include <crogine/detail/SDLResource.hpp>

#include <SDL.h>
#include <SDL_video.h>

#include "../detail/GLCheck.hpp"
#include "DefaultLoadingScreen.hpp"

#include <algorithm>

using namespace cro;

namespace
{
    const std::int32_t RequestGLMajor = 4;
#ifdef GL41
    const std::int32_t RequestGLMinor = 1;
#else
    const std::int32_t RequestGLMinor = 6;
#endif
}

Window::Window()
    : m_window              (nullptr),
    m_threadContext         (nullptr),
    m_mainContext           (nullptr),
    m_fullscreen            (false),
    m_multisamplingEnabled  (false),
    m_previousWindowSize    (0),
    m_cursor                (nullptr)
{

}

Window::~Window()
{
    destroy();
}

//public
bool Window::create(std::uint32_t width, std::uint32_t height, const std::string& title, std::uint32_t styleFlags)
{
    if (!Detail::SDLResource::valid()) return false;
    
    destroy();

#ifdef PLATFORM_MOBILE
    fullscreen = true;
    borderless = true;
#endif //PLATFORM_MOBILE

    int styleMask = SDL_WINDOW_OPENGL | styleFlags;

    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1); //allows the loading thread to display images

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, RequestGLMajor);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, RequestGLMinor);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

#ifdef CRO_DEBUG_
#ifndef GL41
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif
#endif

    m_window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, styleMask);

    if (!m_window)
    {
        std::string err = SDL_GetError();
        cro::Logger::log(err, Logger::Type::Error, Logger::Output::All);
        cro::FileSystem::showMessageBox("Error", err);
        return false;
    }
    else
    {
        m_threadContext = SDL_GL_CreateContext(m_window);
        m_mainContext = SDL_GL_CreateContext(m_window);
        //SDL_GL_MakeCurrent(m_window, m_mainContext);
        

        int maj, min;
        SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &maj);
        SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &min);

        if (maj != RequestGLMajor || min != RequestGLMinor)
        {
            Logger::log("Unable to create requested context version", Logger::Type::Error, Logger::Output::All);
            Logger::log("Returned version was: " + std::to_string(maj) + "." + std::to_string(min), Logger::Type::Error, cro::Logger::Output::All);

            std::stringstream ss;
            ss << "Hardware Support for OpenGL " << RequestGLMajor << "." << RequestGLMinor << " not found.";
            cro::FileSystem::showMessageBox("Error", ss.str());

            return false; //because our shaders will fail to compile.
        }
        LOG("Created OpenGL context version: " + std::to_string(maj) + "." + std::to_string(min), Logger::Type::Info);

        //place the window at the bottom of the stack
        RenderTarget::m_bufferStack[0] = this;
        setViewport({ 0, 0, static_cast<std::int32_t>(width), static_cast<std::int32_t>(height) });
        setView(FloatRect(getViewport()));

        m_previousWindowSize = { width, height };
    }
    return true;
}

void Window::setVsyncEnabled(bool enabled)
{
    if (m_mainContext)
    {
        if (SDL_GL_SetSwapInterval(enabled ? 1 : 0) != 0)
        {
            LogE << SDL_GetError() << std::endl;
        }
    }
}

bool Window::getVsyncEnabled() const
{
    return SDL_GL_GetSwapInterval() != 0;
}

void Window::setMultisamplingEnabled(bool enabled)
{
    if (enabled != m_multisamplingEnabled)
    {
        if (enabled)
        {
            glCheck(glEnable(GL_MULTISAMPLE));
        }
        else
        {
            glCheck(glDisable(GL_MULTISAMPLE));
        }
        m_multisamplingEnabled = enabled;
    }
}

bool Window::getMultisamplingEnabled() const
{
    return m_multisamplingEnabled;
}

void Window::clear()
{
    //we don't need to call RenderTarget::setActive()
    //for the window as it is automatically set to be
    //the bottom most target in the stack.
    auto vp = getViewport();
    glCheck(glViewport(vp.left, vp.bottom, vp.width, vp.height));

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
    std::int32_t x, y;
#ifdef PLATFORM_MOBILE
    SDL_GL_GetDrawableSize(m_window, &x, &y);
#else
    SDL_GetWindowSize(m_window, &x, &y);
#endif //PLATFORM_MOBILE
    return { x, y };
}

void Window::setSize(glm::uvec2 size)
{
    m_previousWindowSize = size;

    CRO_ASSERT(m_window, "window not created");
    SDL_SetWindowSize(m_window, size.x, size.y);
    SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    setViewport({ 0, 0, static_cast<std::int32_t>(size.x), static_cast<std::int32_t>(size.y) });
    setView(FloatRect(getViewport()));

    m_fullscreen = false;
}

void Window::setFullScreen(bool fullscreen)
{
    if (fullscreen == m_fullscreen)
    {
        return;
    }

#ifdef __APPLE__
#define FS_MODE SDL_WINDOW_FULLSCREEN_DESKTOP
#else
#define FS_MODE SDL_WINDOW_FULLSCREEN_DESKTOP
#endif

    std::int32_t mode = 0;
    if (fullscreen)
    {
        mode = FS_MODE;
        m_previousWindowSize = getSize();
    }

    CRO_ASSERT(m_window, "window not created");
    if (SDL_SetWindowFullscreen(m_window, mode) == 0)
    {
        m_fullscreen = fullscreen;
        if (!fullscreen)
        {
            SDL_DisplayMode dm;
            SDL_GetDesktopDisplayMode(SDL_GetWindowDisplayIndex(m_window), &dm);
            if (dm.w == m_previousWindowSize.x
                && dm.h == m_previousWindowSize.y) 
            {
                m_previousWindowSize = { 640u, 480u };
            }

            SDL_SetWindowSize(m_window, m_previousWindowSize.x, m_previousWindowSize.y);
            SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        }

        auto* msg = App::postMessage<Message::SystemEvent>(Message::SystemMessage);
        msg->type = Message::SystemEvent::FullScreenToggled;
    }
    else
    {
        LogE << SDL_GetError() << std::endl;
    }

    setViewport(getDefaultViewport());
    setView(FloatRect(getViewport()));
}

void Window::setPosition(std::int32_t x, std::int32_t y)
{
    CRO_ASSERT(m_window, "window not created");
    if (x < 0) x = SDL_WINDOWPOS_CENTERED;
    if (y < 0) y = SDL_WINDOWPOS_CENTERED;
    SDL_SetWindowPosition(m_window, x, y);
}

glm::ivec2 Window::getPosition() const
{
    CRO_ASSERT(m_window, "window not created");

    glm::ivec2 ret(0);
    SDL_GetWindowPosition(m_window, &ret.x, &ret.y);
    return ret;
}

void Window::setIcon(const std::uint8_t* data)
{
    //let the bundle set the icon on mac
#ifndef __APPLE__
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
#endif
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
                    m_resolutions.emplace_back(mode.w, mode.h);
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

namespace
{
    struct ThreadData final
    {
        SDL_Window* window = nullptr;
        SDL_GLContext context = nullptr;
        SDL_atomic_t threadFlag = {};
        LoadingScreen* loadingScreen = nullptr;
    };

    int loadingDisplayFunc(void* data)
    {
        ThreadData* threadData = static_cast<ThreadData*>(data);
        SDL_GL_MakeCurrent(threadData->window, threadData->context);
        threadData->loadingScreen->launch();

        while (SDL_AtomicGet(&threadData->threadFlag) != 1)
        {
            threadData->loadingScreen->update();
            glCheck(glClear(GL_COLOR_BUFFER_BIT));
            threadData->loadingScreen->draw();
            SDL_GL_SwapWindow(threadData->window);
        }

        SDL_GL_MakeCurrent(threadData->window, nullptr);
        return 0;
    }
}

void Window::loadResources(const std::function<void()>& loader)
{
    if (!m_loadingScreen)
    {
        m_loadingScreen = std::make_unique<DefaultLoadingScreen>();
    }

#ifdef PLATFORM_DESKTOP
    //macs crashes with loading screens
#ifdef __APPLE__
    //if (m_fullscreen)
    {
        /*m_loadingScreen->launch();
        m_loadingScreen->update();
        glCheck(glClear(GL_COLOR_BUFFER_BIT));
        m_loadingScreen->draw();
        SDL_GL_SwapWindow(m_window);*/

        glCheck(glClear(GL_COLOR_BUFFER_BIT));
        SDL_GL_SwapWindow(m_window);
        loader();
    }
#else
    //else
    {

    //create thread
    ThreadData data;
    data.context = m_threadContext;
    data.window = m_window;
    data.threadFlag.value = 0;
    data.loadingScreen = m_loadingScreen.get();

    SDL_Thread* thread = SDL_CreateThread(loadingDisplayFunc, "Loading Thread", static_cast<void*>(&data));

    loader();
    glFinish(); //make sure to wait for gl stuff to finish before continuing

    SDL_AtomicIncRef(&data.threadFlag);

    std::int32_t result = 0;
    SDL_WaitThread(thread, &result);

    //SDL_GL_MakeCurrent(m_window, m_mainContext);

    }
#endif
#else

    //android doesn't appear to like running the thread - so we'll display the loading screen once
    //before loading resources
    m_loadingScreen->launch();
    m_loadingScreen->update();
    glCheck(glClear(GL_COLOR_BUFFER_BIT));
    m_loadingScreen->draw();
    SDL_GL_SwapWindow(m_window);

    loader();

#endif //PLATFORM_DESKTOP

    App::getInstance().resetFrameTime();
}

void Window::setMouseCaptured(bool captured)
{
    SDL_SetRelativeMouseMode(captured ? SDL_TRUE : SDL_FALSE);
    /*if (captured)
    {
        SDL_WarpMouseInWindow(m_window, 1, 1);
    }*/
    //auto centre = getSize() / 2u;
    //SDL_WarpMouseInWindow(m_window, centre.x, centre.y);

    //SDL_CaptureMouse(captured ? SDL_TRUE : SDL_FALSE);
}

bool Window::getMouseCaptured() const
{
    return (SDL_GetRelativeMouseMode() == SDL_TRUE);
}

void Window::setCursor(const Cursor* cursor)
{
    if (cursor == m_cursor)
    {
        return;
    }

    if (cursor && cursor->m_cursor == nullptr)
    {
        return;
    }

    if (m_cursor)
    {
        m_cursor->m_inUse = false;
    }

    m_cursor = cursor;

    if (m_cursor)
    {
        m_cursor->m_inUse = true;
        SDL_SetCursor(m_cursor->m_cursor);
    }
    else
    {
        SDL_SetCursor(nullptr);
    }
}

const Cursor* Window::getCursor() const
{
    return m_cursor;
}

void Window::setWindowedSize(glm::uvec2 size)
{
    SDL_DisplayMode dm;
    SDL_GetDesktopDisplayMode(SDL_GetWindowDisplayIndex(m_window), &dm);
    if (dm.w == m_previousWindowSize.x
        && dm.h == m_previousWindowSize.y)
    {
        m_previousWindowSize = { 640u, 480u };
    }
    else
    {
        m_previousWindowSize = size;
    }
}

glm::uvec2 Window::getWindowedSize() const
{
    return m_previousWindowSize;
}

//private
void Window::destroy()
{
    if (m_mainContext)
    {
        m_loadingScreen.reset(); //delete this while we still have a valid context!
        SDL_GL_DeleteContext(m_mainContext);
        m_mainContext = nullptr;
    }

    if(m_threadContext)
    {
        SDL_GL_DeleteContext(m_threadContext);
        m_threadContext = nullptr;
    }

    if (m_window)
    {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
}