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

#ifndef __APPLE__
#include <crogine/detail/StackDump.hpp>
#include <signal.h>
static void winAbort(int)
{
    cro::StackDump::dump(cro::StackDump::ABRT);
}

static void winSeg(int)
{
    cro::StackDump::dump(cro::StackDump::SEG);
}

static void winIll(int)
{
    cro::StackDump::dump(cro::StackDump::ILL);
}

static void winFPE(int)
{
    cro::StackDump::dump(cro::StackDump::FPE);
}

#endif

#include <crogine/core/App.hpp>
#include <crogine/core/Log.hpp>
#include <crogine/core/Window.hpp>
#include <crogine/core/Console.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/core/SysTime.hpp>
#include <crogine/core/HiResTimer.hpp>
#include <crogine/detail/Assert.hpp>
#include <crogine/audio/AudioMixer.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/util/String.hpp>

#include <SDL.h>
#include <SDL_joystick.h>
#include <SDL_filesystem.h>

#include <future>

#include "../detail/GLCheck.hpp"
#include "../detail/SDLImageRead.hpp"
#include "../detail/fa-regular-400.hpp"
#include "../detail/IconsFontAwesome6.h"
#include "../imgui/imgui_impl_opengl3.h"
#include "../imgui/imgui_impl_sdl.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../detail/stb_image_write.h"


#include "../audio/AudioRenderer.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

#ifdef CRO_DEBUG_
//#define DEBUG_NO_CONTROLLER
#endif // CRO_DEBUG_

using namespace cro;

cro::App* App::m_instance = nullptr;

namespace
{    
    //storing the dt as float is far more
    //accurate than converting to a Time and
    //back as the underlying SDL timer only
    //supports milliseconds
    constexpr float frameTime = 1.f / 60.f;
    float timeSinceLastUpdate = 0.f;

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

#ifndef GL41
    void APIENTRY glDebugPrint(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* msg, const void*)
    {
        std::stringstream ss;
        switch (source)
        {
        default:
            ss << "OpenGL: ";
            break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER:
            ss << "Shader compiler: ";
            break;
        }

        switch (type)
        {
        default: break;
        case GL_DEBUG_TYPE_ERROR:
            ss << "Error - ";
            break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
            ss << "Deprecated behaviour - ";
            break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            ss << "Undefined behaviour - ";
            break;
        case GL_DEBUG_TYPE_PERFORMANCE:
            ss << "Performance warning - ";
            break;
        }

        ss << msg;

        switch (severity)
        {
        default:
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            //LogI << ss.str() << std::endl;
            break;
        case GL_DEBUG_SEVERITY_LOW:
            LogI << ss.str() << std::endl;
            break;
        case GL_DEBUG_SEVERITY_MEDIUM:
            LogW << ss.str() << std::endl;
            break;
        case GL_DEBUG_SEVERITY_HIGH:
            LogE << ss.str() << std::endl;
            break;
        }
    }
#endif
}


App::App(std::uint32_t styleFlags)
    : m_windowStyleFlags(styleFlags),
    m_frameClock        (nullptr),
    m_running           (false),
    m_controllerCount   (0),
    m_drawDebugWindows  (true),
    m_orgString         ("Trederia"),
    m_appString         ("CrogineApp")
{
    CRO_ASSERT(m_instance == nullptr, "App instance already exists!");

#ifndef CRO_DEBUG_
#ifndef __APPLE__ //mac actually gives a decent stack dump
    //register custom abort which prints the call stack
    signal(SIGABRT, &winAbort);
    signal(SIGSEGV, &winSeg);
    signal(SIGILL, &winIll);
    signal(SIGFPE, &winFPE);
#endif
#endif


#ifdef DEBUG_NO_CONTROLLER
    //urg sometimes some USB driver or something crashes and causes SDL_Init to hang
    //until the PC is restarted - this hacks around it while debugging (but disables controllers)
#define INIT_FLAGS SDL_INIT_EVERYTHING & ~(SDL_INIT_HAPTIC | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK)
#else
#define INIT_FLAGS SDL_INIT_EVERYTHING
#endif

    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5, "1");
#ifdef _WIN32
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
#endif
    if (SDL_Init(INIT_FLAGS) < 0)
    {
        const std::string err(SDL_GetError());
        Logger::log("Failed init: " + err, Logger::Type::Error, cro::Logger::Output::All);
    }
    else
    {
        m_instance = this;

        //maps the steam deck rear buttons to the controller paddles
        //auto mapResult = SDL_GameControllerAddMapping("03000000de2800000512000011010000,Steam Deck,platform:Linux,crc:17f6,a:b3,b:b4,x:b5,y:b6,back:b11,guide:b13,start:b12,leftstick:b14,rightstick:b15,leftshoulder:b7,rightshoulder:b8,dpup:b16,dpdown:b17,dpleft:b18,dpright:b19,misc1:b2,paddle1:b21,paddle2:b20,paddle3:b23,paddle4:b22,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a9,righttrigger:a8");
        auto mapResult = SDL_GameControllerAddMapping("03000000de2800000512000011010000,Steam Deck,a:b3,b:b4,back:b11,dpdown:b17,dpleft:b18,dpright:b19,dpup:b16,guide:b13,leftshoulder:b7,leftstick:b14,lefttrigger:a9,leftx:a0,lefty:a1,misc1:b2,paddle1:b21,paddle2:b20,paddle3:b23,paddle4:b22,rightshoulder:b8,rightstick:b15,righttrigger:a8,rightx:a2,righty:a3,start:b12,x:b5,y:b6,platform:Linux");
        //auto mapResult = SDL_GameControllerAddMapping("03000000de2800000512000010010000,Steam Deck,a:b3,b:b4,back:b11,dpdown:b17,dpleft:b18,dpright:b19,dpup:b16,guide:b13,leftshoulder:b7,leftstick:b14,lefttrigger:a9,leftx:a0,lefty:a1,rightshoulder:b8,rightstick:b15,righttrigger:a8,rightx:a2,righty:a3,start:b12,x:b5,y:b6,platform:Linux");
        if (mapResult == -1)
        {
            LogE << SDL_GetError() << std::endl;
        }

        std::fill(m_controllers.begin(), m_controllers.end(), ControllerInfo());
        //controllers are automatically connected as the connect events are raised
        //on start up

        char* pp = SDL_GetPrefPath(m_orgString.c_str(), m_appString.c_str());
        m_prefPath = std::string(pp);
        SDL_free(pp);
        std::replace(m_prefPath.begin(), m_prefPath.end(), '\\', '/');

        if (!AudioRenderer::init())
        {
            Logger::log("Failed to initialise audio renderer", Logger::Type::Error);
        }


#ifdef WIN32
#ifdef CRO_DEBUG_
        //places the console window in the top right so it's a bit more visible when debugging
        HWND consoleWindow = GetConsoleWindow();
        SetWindowPos(consoleWindow, 0, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

        //https://stackoverflow.com/a/45622802
        //enables utf8 encoded strings in the console
        SetConsoleOutputCP(CP_UTF8);

        ////enable buffering to prevent VS from chopping up UTF-8 byte sequences
        //setvbuf(stdout, nullptr, _IOFBF, 1000);
#endif
#endif
    }
}

App::~App()
{
    AudioRenderer::shutdown();
    
    for (auto js : m_joysticks)
    {
        SDL_JoystickClose(js.second);
    }

    for (auto info : m_controllers)
    {
        if (info.haptic)
        {
            SDL_HapticClose(info.haptic);
        }
        SDL_GameControllerClose(info.controller);
    }
    
    //SDL cleanup
    SDL_Quit();
}

//public
void App::run()
{
    SDL_version v;
    SDL_VERSION(&v);

    LogI << "Using SDL " << (int)v.major << "." << (int)v.minor << "." << (int)v.patch << std::endl;

    //hmm we have to *copy* this ?? (it also has to live as long as the app runs)
    std::vector<std::uint8_t> fontBuff(std::begin(FA_Regular400), std::end(FA_Regular400));

    auto settings = loadSettings();
    glm::uvec2 size = settings.fullscreen ? glm::uvec2(settings.windowedSize) : glm::uvec2(settings.width, settings.height);

    if (m_window.create(size.x, size.y, "crogine game", m_windowStyleFlags))
    {
        //load opengl - TODO choose which loader to use based on
        //current platform, ie mobile or desktop
#ifdef PLATFORM_MOBILE
        if (!gladLoadGLES2Loader(SDL_GL_GetProcAddress))
#else
        if (!gladLoadGLLoader(SDL_GL_GetProcAddress))
#endif //PLATFORM_MOBILE
        {
            Logger::log("Failed loading OpenGL", Logger::Type::Error, Logger::Output::All);
            return;
        }

        m_window.setMultisamplingEnabled(glIsEnabled(GL_MULTISAMPLE));

        ImGui::CreateContext();
        setImguiStyle(&ImGui::GetStyle());
        //ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        ImGui_ImplSDL2_InitForOpenGL(m_window.m_window, m_window.m_mainContext);
#ifdef PLATFORM_DESKTOP
#ifdef GL41
        ImGui_ImplOpenGL3_Init("#version 410 core");
#else
        ImGui_ImplOpenGL3_Init("#version 460 core");

#ifdef CRO_DEBUG_
        glDebugMessageCallback(glDebugPrint, nullptr);
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif
#endif
#else
        //load ES2 shaders on mobile
        ImGui_ImplOpenGL3_Init();
#endif


        ImFontConfig config;
        config.MergeMode = true;
        config.FontDataOwnedByAtlas = false; //held in vector above
        config.GlyphMinAdvanceX = 13.0f; // Use if you want to make the icon monospaced
        config.FontBuilderFlags |= (1 << 8) | (1 << 9); //enables colour rendering
        static constexpr ImWchar ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };

        ImGui::GetIO().Fonts->AddFontDefault();
        ImGui::GetIO().Fonts->AddFontFromMemoryTTF(fontBuff.data(), fontBuff.size(), 13.f, &config, ranges);

        m_window.setIcon(defaultIcon);
        m_window.setExclusiveFullscreen(settings.exclusive);
        m_window.setFullScreen(settings.fullscreen);
        m_window.setVsyncEnabled(settings.vsync);
        m_window.setMultisamplingEnabled(settings.useMultisampling);
        Console::init();

        //add any 'built in' convars
        Console::addConvar("drawDebugWindows",
            "true",
            "If true then any GuiClient windows registered with the isDebug flag will be drawn to the UI. Set with r_drawDebugWindows");

        m_drawDebugWindows = Console::getConvarValue<bool>("drawDebugWindows");

        //set the drawDebugWindows flag
        Console::addCommand("r_drawDebugWindows",
            [&](const std::string& param)
            {
                if (param == "0")
                {
                    Console::setConvarValue("drawDebugWindows", false);
                    Console::print("r_drawDebugWindows set to FALSE");
                    m_drawDebugWindows = false;
                }
                else if (param == "1")
                {
                    Console::setConvarValue("drawDebugWindows", true);
                    Console::print("r_drawDebugWindows set to TRUE");
                    m_drawDebugWindows = true;
                }
                else
                {
                    Console::print("Usage: r_drawDebugWindows <0|1>");
                }
            }, nullptr);

        Console::addCommand("list_audio_devices",
            [](const std::string&)
            {
                auto deviceCount = SDL_GetNumAudioDevices(0);
                if (deviceCount)
                {
                    for (auto i = 0; i < deviceCount; ++i)
                    {
                        auto str = SDL_GetAudioDeviceName(i, 0);
                        Console::print(str);
                    }
                }
                else
                {
                    Console::print("No Audio Devices Found");
                }
            }, nullptr);
    }
    else
    {
        FileSystem::showMessageBox("Failed Creating Window", "See output.log for error details.");
        Logger::log("Failed creating main window", Logger::Type::Error, Logger::Output::All);
        return;
    }


    HiResTimer frameClock;
    m_frameClock = &frameClock;
    m_running = initialise();

    if (!m_running)
    {
        Logger::log("App initialise() returned false.", Logger::Type::Error, Logger::Output::All);
    }

    static constexpr std::int32_t MaxFrames = 4; //for every fixed update render no more than these frames
    std::int32_t framesRendered = 0;

    while (m_running)
    {
        timeSinceLastUpdate += frameClock.restart();

        while (timeSinceLastUpdate > frameTime)
        {
            timeSinceLastUpdate -= frameTime;

            Console::newFrame();

            handleEvents();
            handleMessages();

            simulate(frameTime);

            framesRendered = 0;
        }

        if (framesRendered++ < MaxFrames)
        {
            doImGui();

            ImGui::Render();
            m_window.clear();
            render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            m_window.display();
        }
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
    timeSinceLastUpdate = 0.f;
}

App& App::getInstance()
{
    CRO_ASSERT(m_instance, "");
    return *m_instance;
}

bool App::isValid()
{
    return m_instance != nullptr;
}

void App::saveScreenshot()
{
    //wait for any previous operation
    static std::future<void> writeResult;
    if (writeResult.valid())
    {
        writeResult.wait();
    }

    //TODO this assumes we're calling this with the main buffer
    //active - if a texture buffer is currently active we should be
    //checking the size of that at the very least...
    auto size = m_window.getSize();
    static std::vector<GLubyte> buffer;
    buffer.resize(size.x * size.y * 3);

    glCheck(glPixelStorei(GL_PACK_ALIGNMENT, 1));
    glCheck(glReadPixels(0, 0, size.x, size.y, GL_RGB, GL_UNSIGNED_BYTE, buffer.data()));

    postMessage<Message::SystemEvent>(Message::SystemMessage)->type = Message::SystemEvent::ScreenshotTaken;


    writeResult = std::async(std::launch::async, [size]() {
        //flip row order
        stbi_flip_vertically_on_write(1);

        auto d = SysTime::now();
        std::stringstream ss;
        ss << std::setw(2) << std::setfill('0') << d.year() << "/"
            << std::setw(2) << std::setfill('0') << d.months() << "/"
            << d.days();


        std::string filename = "screenshot_" + ss.str() + "_" + SysTime::timeString() + ".png";
        std::replace(filename.begin(), filename.end(), '/', '_');
        std::replace(filename.begin(), filename.end(), ':', '_');

        auto outPath = getPreferencePath() + "screenshots/";
        std::replace(outPath.begin(), outPath.end(), '\\', '/');

        if (!FileSystem::directoryExists(outPath))
        {
            FileSystem::createDirectory(outPath);
        }

        filename = outPath + filename;

        RaiiRWops out;
        out.file = SDL_RWFromFile(filename.c_str(), "w");
        if (out.file)
        {
            stbi_write_png_to_func(image_write_func, out.file, size.x, size.y, 3, buffer.data(), size.x * 3);
            LogI << "Saved " << filename << std::endl;
        }
        else
        {
            LogE << SDL_GetError() << std::endl;
        }
        });
}

//protected
void App::setApplicationStrings(const std::string& organisation, const std::string& appName)
{
    CRO_ASSERT(!organisation.empty(), "String cannot be empty");
    CRO_ASSERT(!appName.empty(), "String cannot be empty");
    m_orgString = organisation;
    m_appString = appName;

    //remember to update the pref path
    char* pp = SDL_GetPrefPath(m_orgString.c_str(), m_appString.c_str());
    m_prefPath = std::string(pp);
    SDL_free(pp);
    std::replace(m_prefPath.begin(), m_prefPath.end(), '\\', '/');
}

//private
void App::handleEvents()
{
    cro::Event evt;
    while (m_window.pollEvent(evt))
    {
        ImGui_ImplSDL2_ProcessEvent(&evt);

        //update the count first because handling
        //the event below might query getControllerCount()
        if (evt.type == SDL_CONTROLLERDEVICEADDED)
        {
            m_controllerCount++;
        }
        else if (evt.type == SDL_CONTROLLERDEVICEREMOVED)
        {
            m_controllerCount--;
        }

        //HOWEVER
        //handle events first in case user events
        //want to read disconnected controllers
        //before the following cases handle removal
        handleEvent(evt);

        switch (evt.type)
        {
        default: break;
        case SDL_AUDIODEVICEADDED:
            //index of the device
        {
            //auto str = SDL_GetAudioDeviceName(evt.adevice.which, evt.adevice.iscapture);
            //LogI << str << " was connected " << std::endl;
            if (evt.adevice.iscapture)
            {
                AudioRenderer::onRecordConnect();
            }
            else
            {
                AudioRenderer::onPlaybackConnect();
            }
            postMessage<Message::SystemEvent>(Message::SystemMessage)->type = Message::SystemEvent::AudioDeviceChanged;
        }
            break;
        case SDL_AUDIODEVICEREMOVED:
            //SDL ID of device
        {
            if (evt.adevice.iscapture)
            {
                AudioRenderer::onRecordDisconnect();
            }
            else
            {
                AudioRenderer::onPlaybackDisconnect();
            }
            postMessage<Message::SystemEvent>(Message::SystemMessage)->type = Message::SystemEvent::AudioDeviceChanged;

            //TODO how do we get something useful like the index/name of this device?
            //LogI << "Device " << evt.adevice.which << " was disconnected" << std::endl;
        }
            break;
        case SDL_CONTROLLERBUTTONUP:
            if (Console::isVisible()
                && (evt.cbutton.button == GameController::ButtonB || evt.cbutton.button == GameController::ButtonBack))
            {
                Console::show();
                if (!Console::isVisible())
                {
                    saveSettings();
                }
            }
            [[fallthrough]];
        case SDL_CONTROLLERBUTTONDOWN:
            GameController::m_lastControllerIndex = GameController::controllerID(evt.cbutton.which);
            break;
        case SDL_CONTROLLERAXISMOTION:
            if (std::abs(evt.caxis.value) > GameController::RightThumbDeadZone)
            {
                GameController::m_lastControllerIndex = GameController::controllerID(evt.caxis.which);
            }
            break;
        case SDL_KEYUP:
            switch (evt.key.keysym.sym)
            {
            default: break;
            case SDLK_F1:
                Console::show();
                if (!Console::isVisible())
                {
                    saveSettings();
                }
                break;
#ifdef __APPLE__
            case SDLK_q:
                if (evt.key.keysym.mod & KMOD_GUI)
#else
            case SDLK_F4:
                if (evt.key.keysym.mod & KMOD_ALT)
#endif
                {
                    quit();
                }
                break;
#ifdef __APPLE__
            case SDLK_f:
                if (evt.key.keysym.mod & (KMOD_GUI | KMOD_CTRL))
#else
            case SDLK_RETURN:
                if (evt.key.keysym.mod & KMOD_ALT)
#endif
                {
                    auto fs = m_window.isFullscreen();
                    fs = !fs;
                    m_window.setFullScreen(fs);

                    //hack to hide console if it's open as the full screen
                    //checkbox won't be updated
                    if (Console::isVisible())
                    {
                        Console::show();
                    }
                    saveSettings();
                }
                break;
            case SDLK_F5:
                saveScreenshot();
                break;
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
                ControllerInfo ci;
                ci.controller = SDL_GameControllerOpen(id);
                if (ci.controller)
                {
                    //the actual index is different to the id of the event
                    auto* j = SDL_GameControllerGetJoystick(ci.controller);
                    ci.joystickID = SDL_JoystickInstanceID(j);                    
                    ci.psLayout = Detail::isPSLayout(ci.controller);

                    auto idx = SDL_GameControllerGetPlayerIndex(ci.controller);
                    if (idx != -1)
                    {
                        m_controllers[idx] = ci;
                    }
                }
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

            //as the device is disconnected we can't query SDL and have to find the index manually
            std::int32_t controllerIndex = -1;// SDL_GameControllerGetPlayerIndex(SDL_GameControllerFromInstanceID(id));
            for (auto i = 0; i < GameController::MaxControllers; ++i)
            {
                if (m_controllers[i].joystickID == id)
                {
                    controllerIndex = i;
                    break;
                }
            }

            if (controllerIndex > -1 &&
                m_controllers[controllerIndex].controller)
            {
                if (m_controllers[controllerIndex].haptic)
                {
                    SDL_HapticClose(m_controllers[controllerIndex].haptic);
                }
                
                SDL_GameControllerClose(m_controllers[controllerIndex].controller);
                m_controllers[controllerIndex] = {};
            }

            if (m_joysticks.count(id) > 0)
            {
                SDL_JoystickClose(m_joysticks[id]);
                m_joysticks.erase(id);
            }
        }
            break;
        }
    }
}

void App::handleMessages()
{
    while (!m_messageBus.empty())
    {
        const auto& msg = m_messageBus.poll();

        switch (msg.id)
        {
        case Message::SystemMessage:
        {
            const auto& data = msg.getData<Message::SystemEvent>();
            if (data.type == Message::SystemEvent::ResumedFromSuspend)
            {
                AudioRenderer::resume();
            }
        }
            break;
        default: break;
        }

        handleMessage(msg);
    }
}

void App::doImGui()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(m_window.m_window);
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();

    //show other windows (console etc)
    Console::draw();
    
    for (const auto& f : m_guiWindows)
    {
        f.first();
    }
    if (m_drawDebugWindows)
    {
        for (const auto& f : m_debugWindows)
        {
            f.first();
        }
    }
}

void App::addConsoleTab(const std::string& name, const std::function<void()>& func, const GuiClient* c)
{
    Console::addConsoleTab(name, func, c);
}

void App::removeConsoleTab(const GuiClient* c)
{
    Console::removeConsoleTab(c);
}

void App::addWindow(const std::function<void()>& func, const GuiClient* c, bool isDebug)
{
    CRO_ASSERT(m_instance, "App not properly instanciated!");

    if (isDebug)
    {
        m_instance->m_debugWindows.push_back(std::make_pair(func, c));
    }
    else
    {
        m_instance->m_guiWindows.push_back(std::make_pair(func, c));
    }
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


    m_instance->m_debugWindows.erase(
        std::remove_if(std::begin(m_instance->m_debugWindows), std::end(m_instance->m_debugWindows),
            [c](const std::pair<std::function<void()>, const GuiClient*>& pair)
            {
                return pair.second == c;
            }), std::end(m_instance->m_debugWindows));
}

App::WindowSettings App::loadSettings() const
{
    WindowSettings settings;

    ConfigFile cfg;
    if (cfg.loadFromFile(m_prefPath + cfgName, false))
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
            else if (prop.getName() == "exclusive")
            {
                settings.exclusive = prop.getValue<bool>();
            }
            else if (prop.getName() == "vsync")
            {
                settings.vsync = prop.getValue<bool>();
            }
            else if (prop.getName() == "multisample")
            {
                settings.useMultisampling = prop.getValue<bool>();
            }
            else if (prop.getName() == "window_size")
            {
                settings.windowedSize = prop.getValue<glm::vec2>();
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

    ConfigFile saveSettings;
    saveSettings.addProperty("width", std::to_string(size.x));
    saveSettings.addProperty("height", std::to_string(size.y));
    saveSettings.addProperty("fullscreen").setValue(m_window.isFullscreen());
    saveSettings.addProperty("exclusive").setValue(m_window.getExclusiveFullscreen());
    saveSettings.addProperty("vsync").setValue(m_window.getVsyncEnabled());
    saveSettings.addProperty("multisample").setValue(m_window.getMultisamplingEnabled());
    saveSettings.addProperty("window_size").setValue(m_window.getWindowedSize());

    auto* aObj = saveSettings.addObject("audio");
    aObj->addProperty("master", std::to_string(AudioMixer::getMasterVolume()));
    for (auto i = 0u; i < AudioMixer::MaxChannels; ++i)
    {
        aObj->addProperty("channel" + std::to_string(i), std::to_string(AudioMixer::getVolume(i)));
    }

    saveSettings.save(m_prefPath + cfgName);
}

bool Detail::isPSLayout(SDL_GameController* gc)
{
    const std::array NameStrings =
    {
        std::string("ps1"),
        std::string("ps2"),
        std::string("ps3"),
        std::string("ps4"),
        std::string("ps5"),
        std::string("psx"),
        std::string("dualshock"),
        std::string("dualsense"),
        std::string("playstation"),
        std::string("sony"),
    };

    auto name = SDL_GameControllerName(gc);
    if (name)
    {
        auto nameString = cro::Util::String::toLower(name);
        for (const auto& str : NameStrings)
        {
            if (nameString.find(str) != std::string::npos)
            {
                return true;
            }
        }
    }

    return false;
}