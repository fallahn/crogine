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

#include <crogine/core/App.hpp>
#include <crogine/core/Log.hpp>
#include <crogine/core/Window.hpp>
#include <crogine/core/Console.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/core/SysTime.hpp>
#include <crogine/core/HiResTimer.hpp>
#include <crogine/detail/Assert.hpp>
#include <crogine/audio/AudioMixer.hpp>
#include <crogine/gui/imgui.h>

#include <SDL.h>
#include <SDL_joystick.h>
#include <SDL_filesystem.h>

#include "../detail/GLCheck.hpp"
#include "../imgui/imgui_impl_opengl3.h"
#include "../imgui/imgui_impl_sdl.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../graphics/stb_image_write.h"


#include "../audio/AudioRenderer.hpp"

#include <algorithm>

using namespace cro;

cro::App* App::m_instance = nullptr;

namespace
{    
    //storing the dt as float is far more
    //accurate than converting to a Time and
    //back as the underlying SDL timer only
    //supports milliseconds
    const float frameTime = 1.f / 60.f;

#include "../detail/DefaultIcon.inl"

    const std::string cfgName("cfg.cfg");

    void setImguiStyle(ImGuiStyle* dst)
    {
        ImGuiStyle& st = dst ? *dst : ImGui::GetStyle();
        st.FrameBorderSize = 1.0f;
        st.FramePadding = ImVec2(4.0f, 4.0f);
        st.ItemSpacing = ImVec2(8.0f, 4.0f);
        st.WindowBorderSize = 1.0f;
        st.TabBorderSize = 1.0f;
        st.WindowRounding = 1.0f;
        st.ChildRounding = 1.0f;
        st.FrameRounding = 1.0f;
        st.ScrollbarRounding = 1.0f;
        st.GrabRounding = 1.0f;
        st.TabRounding = 1.0f;

        // Setup style
        ImVec4* colors = st.Colors;
        colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 1.00f, 0.95f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.40f, 0.40f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.12f, 0.22f, 0.78f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.95f, 0.95f, 1.00f, 0.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.05f, 0.05f, 0.10f, 0.94f);
        colors[ImGuiCol_Border] = ImVec4(0.50f, 0.50f, 0.59f, 0.46f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.10f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.25f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.22f, 0.26f, 0.40f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.16f, 0.16f, 0.19f, 0.53f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.04f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.00f, 0.00f, 0.05f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.02f, 0.51f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.03f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.33f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.43f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.48f, 0.48f, 0.50f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.79f, 0.79f, 0.79f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.48f, 0.47f, 0.47f, 0.91f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.56f, 0.55f, 0.55f, 0.62f);
        colors[ImGuiCol_Button] = ImVec4(0.42f, 0.42f, 0.50f, 0.63f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.60f, 0.60f, 0.68f, 0.63f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.20f, 0.26f, 0.63f);
        colors[ImGuiCol_Header] = ImVec4(0.54f, 0.54f, 0.68f, 0.58f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.64f, 0.65f, 0.70f, 0.80f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.25f, 0.25f, 0.28f, 0.80f);
        colors[ImGuiCol_Separator] = ImVec4(0.58f, 0.58f, 0.62f, 0.50f);
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.81f, 0.81f, 0.81f, 0.64f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(0.81f, 0.81f, 0.81f, 0.64f);
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.87f, 0.87f, 0.87f, 0.53f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.87f, 0.87f, 0.87f, 0.74f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.87f, 0.87f, 0.87f, 0.74f);
        colors[ImGuiCol_Tab] = ImVec4(0.01f, 0.01f, 0.03f, 0.86f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.29f, 0.29f, 0.33f, 1.00f);
        colors[ImGuiCol_TabActive] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.02f, 0.02f, 0.04f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.19f, 0.19f, 0.21f, 1.00f);
        //colors[ImGuiCol_DockingPreview] = ImVec4(0.38f, 0.48f, 0.60f, 1.00f);
        //colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.68f, 0.68f, 0.68f, 1.00f);
        colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.77f, 0.33f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.87f, 0.55f, 0.08f, 1.00f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.47f, 0.60f, 0.76f, 0.47f);
        colors[ImGuiCol_DragDropTarget] = ImVec4(0.58f, 0.58f, 0.58f, 0.90f);
        colors[ImGuiCol_NavHighlight] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.20f, 0.20f, 0.24f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.24f, 0.35f);
    }
}


App::App()
    : m_frameClock  (nullptr),
    m_running       (false),
    m_orgString     ("Trederia"),
    m_appString     ("CrogineApp")
{
	CRO_ASSERT(m_instance == nullptr, "App instance already exists!");

	if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
	{
		const std::string err(SDL_GetError());
		Logger::log("Failed init: " + err, Logger::Type::Error);
	}
	else
	{
		m_instance = this;

        for (auto i = 0; i < SDL_NumJoysticks(); ++i)
        {
            if (SDL_IsGameController(i))
            {
                //add to game controllers
                m_controllers.insert(std::make_pair(i, SDL_GameControllerOpen(i)));
            }
            else
            {
                //add to joysticks
                m_joysticks.insert(std::make_pair(i, SDL_JoystickOpen(i)));
            }
        }

        if (!AudioRenderer::init())
        {
            Logger::log("Failed to initialise audio renderer", Logger::Type::Error);
        }

        char* pp = SDL_GetPrefPath(m_orgString.c_str(), m_appString.c_str());
        m_prefPath = std::string(pp);
        SDL_free(pp);
	}
}

App::~App()
{
    AudioRenderer::shutdown();
    
    for (auto js : m_joysticks)
    {
        SDL_JoystickClose(js.second);
    }

    for (auto ct : m_controllers)
    {
        SDL_GameControllerClose(ct.second);
    }
    
    //SDL cleanup
	SDL_Quit();
}

//public
void App::run()
{
    auto settings = loadSettings();

	if (m_window.create(settings.width, settings.height, "crogine game"))
	{
		//load opengl - TODO choose which loader to use based on
        //current platform, ie mobile or desktop
#ifdef PLATFORM_MOBILE
		if (!gladLoadGLES2Loader(SDL_GL_GetProcAddress))
#else
        if (!gladLoadGLLoader(SDL_GL_GetProcAddress))
#endif //PLATFORM_MOBILE
		{
			Logger::log("Failed loading OpenGL", Logger::Type::Error);
			return;
		}
        
        ImGui::CreateContext();
        setImguiStyle(&ImGui::GetStyle());
        ImGui_ImplSDL2_InitForOpenGL(m_window.m_window, m_window.m_mainContext);
#ifdef PLATFORM_DESKTOP
        ImGui_ImplOpenGL3_Init("#version 150");
#else
        //load ES2 shaders on mobile
        ImGui_ImplOpenGL3_Init();
#endif

        m_window.setIcon(defaultIcon);
        m_window.setFullScreen(settings.fullscreen);
        m_window.setVsyncEnabled(settings.vsync);
        Console::init();
	}
	else
	{
		Logger::log("Failed creating main window", Logger::Type::Error);
		return;
	}
    
    
    HiResTimer frameClock;
    m_frameClock = &frameClock;
    m_running = initialise();

    float timeSinceLastUpdate = 0.f;

	while (m_running)
	{
		timeSinceLastUpdate += frameClock.restart();

		while (timeSinceLastUpdate > frameTime)
		{
			timeSinceLastUpdate -= frameTime;

            handleEvents();
            handleMessages();

			simulate(frameTime);
		}
        //DPRINT("Frame time", std::to_string(timeSinceLastUpdate.asMilliseconds()));
        doImGui();

        ImGui::Render();
		m_window.clear();
        render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		m_window.display();
	}

    saveSettings();

    Console::finalise();
    m_messageBus.disable(); //prevents spamming a load of quit messages
    finalise();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
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

Window& App::getWindow()
{
    CRO_ASSERT(m_instance, "No valid app instance");
    return m_instance->m_window;
}

const std::string& App::getPreferencePath()
{
    CRO_ASSERT(m_instance, "No valid app instance");
    return m_instance->m_prefPath;
}

void App::resetFrameTime()
{
    CRO_ASSERT(m_frameClock, "App not initialised");
    m_frameClock->restart();
}

App& App::getInstance()
{
    CRO_ASSERT(m_instance, "");
    return *m_instance;
}

//protected
void App::setApplicationStrings(const std::string& organisation, const std::string& appName)
{
    CRO_ASSERT(!organisation.empty(), "String cannot be empty");
    CRO_ASSERT(!appName.empty(), "String cannot be empty");
    m_orgString = organisation;
    m_appString = appName;
}

//private
void App::handleEvents()
{
    cro::Event evt;
    while (m_window.pollEvent(evt))
    {
        bool eventConsumed = false;
        ImGui_ImplSDL2_ProcessEvent(&evt);

        switch (evt.type)
        {
        default: break;
        case SDL_KEYUP:
            if (evt.key.keysym.sym == SDLK_F1)
            {
                Console::show();
            }
            else if (evt.key.keysym.sym == SDLK_F5)
            {
                saveScreenshot();
            }

            break;
        case SDL_QUIT:
            quit();
            break;
        case SDL_WINDOWEVENT:
        {
            auto* msg = m_messageBus.post<Message::WindowEvent>(Message::Type::WindowMessage);
            msg->event = evt.window.event;
            msg->windowID = evt.window.windowID;
            msg->data0 = evt.window.data1;
            msg->data1 = evt.window.data2;
        }
        //prevents spamming the next frame with a giant DT
        if (evt.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
        {
            resetFrameTime();
        }

            break;
        case SDL_CONTROLLERDEVICEADDED:
        {
            auto id = evt.cdevice.which;
            if (SDL_IsGameController(id))
            {
                m_controllers.insert(std::make_pair(id, SDL_GameControllerOpen(id)));
            }
            else
            {
                m_joysticks.insert(std::make_pair(id, SDL_JoystickOpen(id)));
            }
        }
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
        {
            auto id = evt.cdevice.which;

            if (m_controllers.count(id) > 0)
            {
                SDL_GameControllerClose(m_controllers[id]);
                m_controllers.erase(id);
            }

            if (m_joysticks.count(id) > 0)
            {
                SDL_JoystickClose(m_joysticks[id]);
                m_joysticks.erase(id);
            }
        }
            break;
        }

        if(!eventConsumed) handleEvent(evt);
    }
}

void App::handleMessages()
{
    while (!m_messageBus.empty())
    {
        auto msg = m_messageBus.poll();

        /*switch (msg.id)
        {

        default: break;
        }*/

        handleMessage(msg);
    }
}

void App::doImGui()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(m_window.m_window);
    ImGui::NewFrame();

    //show other windows (console etc)
    Console::draw();

    for (auto& f : m_guiWindows) f.first();
}

void App::addConsoleTab(const std::string& name, const std::function<void()>& func, const GuiClient* c)
{
    Console::addConsoleTab(name, func, c);
}

void App::removeConsoleTab(const GuiClient* c)
{
    Console::removeConsoleTab(c);
}

void App::addWindow(const std::function<void()>& func, const GuiClient* c)
{
    CRO_ASSERT(m_instance, "App not properly instanciated!");
    m_instance->m_guiWindows.push_back(std::make_pair(func, c));
}

void App::removeWindows(const GuiClient* c)
{
    CRO_ASSERT(m_instance, "App not properly instanciated!");

    m_instance->m_guiWindows.erase(
        std::remove_if(std::begin(m_instance->m_guiWindows), std::end(m_instance->m_guiWindows),
            [c](const std::pair<std::function<void()>, const GuiClient*>& pair)
    {
        return pair.second == c;
    }), std::end(m_instance->m_guiWindows));

}

App::WindowSettings App::loadSettings() 
{
    WindowSettings settings;

    ConfigFile cfg;
    if (cfg.loadFromFile(m_prefPath + cfgName))
    {
        const auto& properties = cfg.getProperties();
        for (const auto& prop : properties)
        {
            if (prop.getName() == "width" && prop.getValue<int>() > 0)
            {
                settings.width = prop.getValue<int>();
            }
            else if (prop.getName() == "height" && prop.getValue<int>() > 0)
            {
                settings.height = prop.getValue<int>();
            }
            else if (prop.getName() == "fullscreen")
            {
                settings.fullscreen = prop.getValue<bool>();
            }
            else if (prop.getName() == "vsync")
            {
                settings.vsync = prop.getValue<bool>();
            }
        }

        //load mixer settings
        const auto& objects = cfg.getObjects();
        auto aObj = std::find_if(objects.begin(), objects.end(),
            [](const ConfigObject& o)
            {
                return o.getName() == "audio";
            });

        if (aObj != objects.end())
        {
            const auto& objProperties = aObj->getProperties();
            for (const auto& p : objProperties)
            {
                if (p.getName() == "master")
                {
                    AudioMixer::setMasterVolume(p.getValue<float>());
                }
                else
                {
                    auto name = p.getName();
                    auto found = name.find("channel");
                    if (found != std::string::npos)
                    {
                        auto ident = name.substr(found + 7);
                        try
                        {
                            auto channel = std::stoi(ident);
                            AudioMixer::setVolume(p.getValue<float>(), channel);
                        }
                        catch (...)
                        {
                            continue;
                        }
                    }
                }
            }
        }
    }

    return settings;
}

void App::saveSettings()
{
    auto size = m_window.getSize();
    auto fullscreen = m_window.isFullscreen();
    auto vsync = m_window.getVsyncEnabled();

    ConfigFile saveSettings;
    saveSettings.addProperty("width", std::to_string(size.x));
    saveSettings.addProperty("height", std::to_string(size.y));
    saveSettings.addProperty("fullscreen", fullscreen ? "true" : "false");
    saveSettings.addProperty("vsync", vsync ? "true" : "false");

    auto* aObj = saveSettings.addObject("audio");
    aObj->addProperty("master", std::to_string(AudioMixer::getMasterVolume()));
    for (auto i = 0u; i < AudioMixer::MaxChannels; ++i)
    {
        aObj->addProperty("channel" + std::to_string(i), std::to_string(AudioMixer::getVolume(i)));
    }

    saveSettings.save(m_prefPath + cfgName);
}

void image_write_func(void* context, void* data, int size)
{
    SDL_RWops* file = (SDL_RWops*)context;
    SDL_RWwrite(file, data, size, 1);
}

void App::saveScreenshot()
{
    auto size = m_window.getSize();
    std::vector<GLubyte> buffer(size.x * size.y * 4);
    glCheck(glPixelStorei(GL_PACK_ALIGNMENT, 1));
    glCheck(glReadPixels(0, 0, size.x, size.y, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data()));

    //flip row order
    stbi_flip_vertically_on_write(1);

    std::string filename = "screenshot_" + SysTime::dateString() + "_" + SysTime::timeString() + ".png";
    std::replace(filename.begin(), filename.end(), '/', '_');
    std::replace(filename.begin(), filename.end(), ':', '_');

    RaiiRWops out;
    out.file = SDL_RWFromFile(filename.c_str(), "w");
    stbi_write_png_to_func(image_write_func, out.file, size.x, size.y, 4, buffer.data(), size.x * 4);
}