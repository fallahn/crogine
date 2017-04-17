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

#ifndef CRO_WINDOW_HPP_
#define CRO_WINDOW_HPP_

#include <crogine/Config.hpp>
#include <crogine/detail/Types.hpp>

#include <SDL_video.h>
#include <SDL_events.h>

#include <string>
#include <vector>

namespace cro
{	
	/*!
	\brief Creates a window to which to draw.
	*/
	class CRO_EXPORT_API Window final// : public cro::Detail::SDLResource
	{
	public:
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
		\param fullscreen If set to true the window is created in fullscreen mode
		\param borderless If set to true the window is rendered borderless when not fullscreen
		\returns true on success, else false
		*/
		bool create(uint32 width, uint32 height, const std::string& title, bool fullscreen = false, bool borderless = false);
		/*!
		\brief Enables or disables vsync
		*/
		void setVsyncEnabled(bool);
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
        glm::uvec2 getSize() const;
        /*!
        \brief Attempts to set the window to the given size if it is valid
        */
        void setSize(glm::uvec2);
        /*!
        \brief Attempts to toggle full screen mode
        */
        void setFullScreen(bool);
        /*!
        \brief Set the window position in desktop coordinates.
        \param x Horizontal position to place the window. A negative number
        will centre the window horizonally
        \param y Vertical position to place the window. A negative number
        will centre the window vertically
        */
        void setPosition(int32 x, int32 y);
        /*!
        \brief Sets the window icon from an array of RGBA pixels.
        The image size is assumed to be 16x16 pixels in 8-bit format.
        To use an image from disk load it first with cro::Image then pass
        the pixel pointer to this function.
        */
        void setIcon(const uint8*);
        /*!
        \brief Returns a refernce to a vector containing a list of available
        display resolutions of the first monitor.
        */
        const std::vector<glm::uvec2>& getAvailableResolutions() const;
        /*!
        \brief Sets the window's title
        */
        void setTitle(const std::string&);

	private:

		SDL_Window* m_window;
		SDL_GLContext m_context;

        mutable std::vector<glm::uvec2> m_resolutions;

		void destroy();

        friend class App;
	};
}

#endif //CRO_WINDOW_HPP_