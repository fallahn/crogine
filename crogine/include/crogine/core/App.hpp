/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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
#include <crogine/core/MessageBus.hpp>
#include <crogine/core/Window.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/detail/Types.hpp>

#include <crogine/graphics/Colour.hpp>

#include <vector>
#include <map>

#ifdef CRO_DEBUG_
#define DPRINT(x, y) cro::Console::printStat(x, y)
#else
#define DPRINT(x, y)
#endif //CRO_DEBUG_

namespace cro
{
	namespace Detail
	{
		class SDLResource;
	}
	class Time;
    class GuiClient;

	/*!
	\brief Base class for crogine applications.
	One instance of this must exist before crogine may be used
	*/
	class CRO_EXPORT_API App
	{
	public:
		friend class Detail::SDLResource;
		App();
		virtual ~App();

		App(const App&) = delete;
		App(const App&&) = delete;
		App& operator = (const App&) = delete;
		App& operator = (const App&&) = delete;

		void run();

		void setClearColour(Colour);
		const Colour& getClearColour() const;

        /*!
        \brief Use this to properly close down the application
        */
        static void quit();

        /*!
        \brief Returns a reference to the active window
        */
        static Window& getWindow();

        /*!
        \brief Returns a reference to the system message bus
        */
        MessageBus& getMessageBus() { return m_messageBus; }

        /*!
        \brief Returns the path to the current platform's directory
        for storing preference files.
        */
        static const std::string& getPreferencePath();

        /*!
        \brief Resets the frame clock.
        This should only be used when a state finishes loading assets for a long time
        to prevent the initial delta frame being a large value.
        */
        void resetFrameTime();

        /*!
        brief Returns a reference to the active App instance
        */
        static App& getInstance();

	protected:
		
		virtual void handleEvent(const Event&) = 0;
		virtual void handleMessage(const cro::Message&) = 0;
		/*!
        \brief Used to update the simulation with the time elapsed since the last update.
        It is left to the user to decide if a specific system (such as physics)
        requires fixed updates or not, which can be implemeted with a simple accumulator.
        */
        virtual void simulate(Time) = 0;
        /*!
        \brief Renders any drawables
        */
		virtual void render() = 0;

		/*!
        \brief Called on startup after window is created.
        Use it to perform initial operations such as setting the 
        window title, icon or custom loading screen. Return false
        from this if custom initialisation fails for some reason
        so that the app may safely shut down while calling finalise().
        */
        virtual bool initialise() = 0;

        /*!
        \brief Called before the window is destroyed.
        Use this to clean up any resources which rely on the
        window's OpenGL context.
        */
        virtual void finalise() {};

	private:

		Window m_window;
		Colour m_clearColour;
        Clock* m_frameClock;
        bool m_running;

        void handleEvents();

        MessageBus m_messageBus;
        void handleMessages();

		static App* m_instance;

        std::map<int32, SDL_GameController*> m_controllers;
        std::map<int32, SDL_Joystick*> m_joysticks;
        friend class GameController;

        std::vector<std::pair<std::function<void()>, const GuiClient*>> m_statusControls;
        std::vector<std::pair<std::function<void()>, const GuiClient*>> m_guiWindows;
        void doImGui();


        static void addConsoleTab(const std::string&, const std::function<void()>&, const GuiClient*);
        static void removeConsoleTab(const GuiClient*);

        static void addWindow(const std::function<void()>&, const GuiClient*);
        static void removeWindows(const GuiClient*);

        friend class GuiClient;
        friend class Console;
      
        std::string m_prefPath;
        
        struct WindowSettings final
        {
            std::int32_t width = 800;
            std::int32_t height = 600;
            bool fullscreen = false;
            bool vsync = true;
        };
        WindowSettings loadSettings();
        void saveSettings();
	};
}