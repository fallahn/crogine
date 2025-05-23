/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2025
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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/graphics/RenderTarget.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/detail/SDLResource.hpp>

#include <SDL_video.h>
#include <SDL_events.h>
#include <SDL_atomic.h>

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace cro
{   
    class LoadingScreen;
    class Cursor;

    /*!
    \brief Creates a window to which to draw.
    */
    class CRO_EXPORT_API Window final : public RenderTarget// : public cro::Detail::SDLResource
    {
    public:
        enum StyleFlags
        {
            FullScreen = SDL_WINDOW_FULLSCREEN_DESKTOP,
            Borderless = SDL_WINDOW_BORDERLESS,
            Resizable  = SDL_WINDOW_RESIZABLE
        };

        Window();
        ~Window();
        Window(const Window&) = delete;
        Window(const Window&&) = delete;
        Window& operator = (const Window&) = delete;
        Window& operator = (const Window&&) = delete;

        /*!
        \brief Creates a window with the given parameters
        \param width Width of the window to create in pixels
        \param height Height of the widow to create in pixels
        \param title Title to appear on the window (if supported by the current platform)
        \param styleFlags An unsigned integer containing 0 or more Window::StyleFlags OR'd together
        \returns true on success, else false
        */
        bool create(std::uint32_t width, std::uint32_t height, const std::string& title, std::uint32_t styleFlags = 0);

        /*!
        \brief Enables or disables vsync
        */
        void setVsyncEnabled(bool);

        /*!
        \brief Returns whether or not vsync is enabled
        */
        bool getVsyncEnabled() const;

        /*!
        \brief Attempts to enable or disable MSAA if multisampling is available on the current platform
        */
        void setMultisamplingEnabled(bool);

        /*!
        \brief Returns whether or not multisampling is enabled if it is available on the current platform
        Note not all hardware supports multisampling so this may falsely return true if multisampling
        has attempted to be set but failed.
        */
        bool getMultisamplingEnabled() const;

        /*!
        \brief Clears the window for drawing with the given colour
        */
        void clear();

        /*!
        \brief Swaps the buffers and displays the result of drawing
        */
        void display();

        /*!
        \brief Fills the given event struct with the current event data
        \returns true while events remain on the event stack
        */
        bool pollEvent(Event&);

        /*!
        \brief Closes the window
        */
        void close();

        /*!
        \brief Returns true if window is open
        */
        bool isOpen() const
        {
            return m_window != nullptr;
        }

        /*!
        \brief Returns the current size of the window
        */
        glm::uvec2 getSize() const override;

        /*!
        \brief Returns the total renderable size of the window taking
        into account any high-DPI scaling that may currently be applied.
        */
        glm::uvec2 getScaledSize() const;

        /*!
        \brief Attempts to set the window to the given size if it is valid
        */
        void setSize(glm::uvec2);

        /*!
        \brief Attempts to toggle full screen mode
        */
        void setFullScreen(bool);

        /*!
        \brief returns true if window is currently full screen
        */
        bool isFullscreen() const { return m_fullscreen; }

        /*!
        \brief Sets the full screen mode
        \param exclusive If set to true the window is set to 'true' full screen
        mode at the current resolutions when full screen mode is activated
        else the window is set to borderless full screen at the current desktop
        resolution.
        This has no effect on macOS which is always borderless full screen
        */
        void setExclusiveFullscreen(bool exclusive) { m_exclusiveFullScreen = exclusive; }

        /*!
        \brief Returns the current full screen mode
        \see setExclusiveFullScreen()
        */
        bool getExclusiveFullscreen() const { return m_exclusiveFullScreen; }

        /*!
        \brief Set the window position in desktop coordinates.
        \param x Horizontal position to place the window. A negative number
        will centre the window horizontally
        \param y Vertical position to place the window. A negative number
        will centre the window vertically
        */
        void setPosition(std::int32_t x, std::int32_t y);

        /*
        \brief Returns the window's position in destop coordinates
        */
        glm::ivec2 getPosition() const;

        /*!
        \brief Sets the window icon from an array of RGBA pixels.
        The image size is assumed to be 16x16 pixels in 8-bit format.
        To use an image from disk load it first with cro::Image then pass
        the pixel pointer to this function.
        */
        void setIcon(const std::uint8_t*);

        /*!
        \brief Returns a reference to a vector containing a list of available
        display resolutions of the first monitor.
        */
        const std::vector<glm::uvec2>& getAvailableResolutions() const;

        /*!
        \brief Sets the window's title
        */
        void setTitle(const std::string&);

        /*!
        \brief Executes a given function in its own thread, while displaying
        a loading screen. Usually you pass a std::function object which loads
        OpenGL resources.
        */
        void loadResources(const std::function<void()>&, bool threaded = false);

        /*!
        \brief Sets a custom loading screen.
        Loading screens should use the LoadingScreen interface.
        \see LoadingScreen
        */
        template <typename T, typename... Args>
        void setLoadingScreen(Args&&...);

        /*!
        \brief Sets the progress of the loading screen by calling LoadingScreen::setProgress()
        This does nothing with the default loading screen. This should be called manually
        from the function passed via loadResources(), with the threaded param set to FALSE
        \param progress Normalised progress value to which to set the loading screen
        */
        void setLoadingProgress(float progress);

        /*!
        \brief Sets if the mouse cursor is captured or not.
        When this is true the mouse cursor is hidden and only
        relative mouse move events are received. Useful for
        games such as first or third person shooters where
        mouse movements are used to move the camera.
        */
        void setMouseCaptured(bool);

        /*!
        \brief Returns whether or not the mouse cursor is currently
        captured by the window.
        */
        bool getMouseCaptured() const;

        /*!
        \brief Sets the cursor icon of the window.
        \param cursor A pointer to a valid cursor, or nullptr
        As windows only keep a reference to the active cursor then
        the cursor instance pointed to must be kept alive for as
        long as it is used. It is possible to have multiple cursor
        types and switch between them at run time. Setting this to
        nullptr returns the cursor to the default icon.
        Cursor icons vary by platform so in cases where a particular
        cursor is unsupported this function will do nothing.
        */
        void setCursor(const Cursor* cursor);

        /*!
        \brief Returns a pointer to the active cursor, or nullptr
        if no specific cursor has been set.
        */
        const Cursor* getCursor() const;

        /*!
        \brief Sets the size of the window to use when returning from
        full screen
        */
        void setWindowedSize(glm::uvec2 size);

        /*!
        \brief Returns the current windowed size used when returning
        from full screen mode
        */
        glm::uvec2 getWindowedSize() const;


    private:

        SDL_Window* m_window;
        SDL_GLContext m_threadContext;
        SDL_GLContext m_mainContext;

        std::unique_ptr<LoadingScreen> m_loadingScreen;

        mutable std::vector<glm::uvec2> m_resolutions;

        bool m_fullscreen;
        bool m_exclusiveFullScreen;
        bool m_multisamplingEnabled;

        glm::uvec2 m_previousWindowSize; //restore to this if toggling full screen

        const Cursor* m_cursor;
        friend class Cursor;

        void destroy();

        friend class App;

        std::uint32_t getFrameBufferID() const override { return 0; }
    };

    template <typename T, typename... Args>
    void Window::setLoadingScreen(Args&&... args)
    {
        static_assert(std::is_base_of<LoadingScreen, T>::value, "must be a LoadingScreen type");
        CRO_ASSERT(Detail::SDLResource::valid(), "Window must be created and in a valid state first!");
        m_loadingScreen = std::make_unique<T>(std::forward<Args>(args)...);
    }
}
