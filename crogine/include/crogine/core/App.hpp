/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2026
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
#include <crogine/core/Clock.hpp>
#include <crogine/core/Console.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/core/Keyboard.hpp>
#include <crogine/core/MessageBus.hpp>
#include <crogine/core/OSK.hpp>
#include <crogine/core/Window.hpp>
#include <crogine/detail/Types.hpp>

#include <crogine/graphics/Colour.hpp>

#include <any>
#include <map>
#include <memory>
#include <vector>

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
        bool isPSLayout(SDL_Gamepad*);
    }
    class GuiClient;
    class HiResTimer;
    class StateStack;

    /*!
    \brief Base class for crogine applications.
    One instance of this must exist before crogine may be used
    */
    class CRO_EXPORT_API App
    {
    public:
        friend class Detail::SDLResource;
        /*!
        \param windowStyleFlags Style flags with which to create the default window
        \see Window
        */
        explicit App(std::uint32_t windowStyleFlags = 0);
        virtual ~App();

        App(const App&) = delete;
        App(const App&&) = delete;
        App& operator = (const App&) = delete;
        App& operator = (const App&&) = delete;

        void run(bool resetSettings = false);

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
        Before using this the application's organisation name and app
        name should be set with setApplicationStrings()
        \see setApplicationStrings()
        */
        static const std::filesystem::path& getPreferencePath();

        /*!
        \brief Resets the frame clock.
        This should only be used when a state finishes loading assets for a long time
        to prevent the initial delta frame being a large value.
        */
        void resetFrameTime();

        /*!
        \brief Returns a reference to the active App instance
        */
        static App& getInstance();

        /*!
        \brief Returns true if there is a valid instance of this class
        */
        static bool isValid();

        /*!
        \brief Helper function for posting messages to the MessageBus
        */
        template <typename T>
        static T* postMessage(std::int32_t id)
        {
            return getInstance().getMessageBus().post<T>(id);
        };

        /*!
        brief Saves a copy of the window contents to disk as an
        image in the current working directory.
        */
        void saveScreenshot();

    protected:
        
        virtual void handleEvent(const Event&) = 0;
        virtual void handleMessage(const cro::Message&) = 0;
        /*!
        \brief Used to update the simulation with a fixed timestep of 60Hz
        */
        virtual void simulate(float) = 0;

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

        /*!
        \brief Set the organisation name and application name.
        These should be set immediately on construction of your application,
        as they are used to form the preferences path retrieved with 
        getPreferencePath(). The application name should be unique to prevent
        overwriting settings files used by other crogine applications.
        */
        void setApplicationStrings(const std::string& organisation, const std::string& applicationName);

        /*!
        \brief Saves the current window and AudioMixer settings
        */
        void saveSettings();

        /*!
        \brief Load and execute an external project from a plugin
        \param path Path to the directory containing the plugin. Shared library name is automatically appended
        \param stack A reference to the StateStack with which to execute the plugin code.
        */
        void loadPlugin(const std::filesystem::path& path, StateStack& stack);
        
        /*!
        \brief Unload any loaded project plugin
        \param stack A reference to the StateStack used to load the plugin
        */
        void unloadPlugin(StateStack& stack);

        /*!
        \brief Returns true if a plugin is currently loaded
        */
        bool pluginLoaded() const { return m_pluginHandle != nullptr; }

    private:
        std::uint32_t m_windowStyleFlags;
        Window m_window;
        Colour m_clearColour;
        HiResTimer* m_frameClock;
        bool m_running;

        friend class OSK; //what a horrible oroborean state of affairs...
        std::unique_ptr<OSK> m_osk;

        void handleEvents();

        MessageBus m_messageBus;
        void handleMessages();

        static App* m_instance;

        struct GamepadInfo final
        {
            String printableName;
            SDL_Gamepad* gamepad = nullptr;
        };
        std::unordered_map<SDL_JoystickID, GamepadInfo> m_gamepads;
        std::vector<SDL_Gamepad*> m_sortedGamepads; //sorted by player index
        void refreshGamepads(const SDL_Gamepad* gamepadToRemove); //might be null if there's nothing to remove

        friend class GameController;

        std::vector<std::pair<std::function<void()>, const GuiClient*>> m_debugWindows;
        std::vector<std::pair<std::function<void()>, const GuiClient*>> m_guiWindows;
        bool m_drawDebugWindows;
        void doImGui();

        static void addStats(const std::function<void()>&, const GuiClient*);
        static void removeStats(const GuiClient*);

        static void addConsoleTab(const std::string&, const std::function<void()>&, const GuiClient*);
        static void removeConsoleTab(const GuiClient*);

        static void addWindow(const std::function<void()>&, const GuiClient*, bool isDebug);
        static void removeWindows(const GuiClient*);

        friend class GuiClient;
        friend class Console;
      
        std::string m_orgString;
        std::string m_appString;
        std::filesystem::path m_prefPath;
        
        struct WindowSettings final
        {
            std::int32_t width = 800;
            std::int32_t height = 600;
            float framelimit = 240.f;
            bool fullscreen = false;
            bool exclusive = false;
            bool vsync = false;
            bool border = true;
            bool useMultisampling = false;
            glm::vec2 windowedSize = glm::vec2(0.f);
            glm::vec2 fullscreenSize = glm::vec2(0.f);
        };
        WindowSettings loadSettings() const;


        //loading external plugins
        std::any m_pluginSharedData;
#ifdef _WIN32
        HINSTANCE m_pluginHandle = nullptr;
#else
        void* m_pluginHandle = nullptr;
#endif //_win32
    };
}