/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2022
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include "GolfGame.hpp"
#include "golf/MenuState.hpp"
#include "golf/GolfState.hpp"
#include "golf/BilliardsState.hpp"
#include "golf/ErrorState.hpp"
#include "golf/OptionsState.hpp"
#include "golf/PauseState.hpp"
#include "golf/TutorialState.hpp"
#include "golf/KeyboardState.hpp"
#include "golf/PracticeState.hpp"
#include "golf/DrivingState.hpp"
#include "golf/PuttingState.hpp"
#include "golf/ClubhouseState.hpp"
#include "golf/TrophyState.hpp"
#include "golf/MenuConsts.hpp"
#include "golf/GameConsts.hpp"
#include "golf/MessageIDs.hpp"
#include "golf/PacketIDs.hpp"
#include "rss/Newsfeed.hpp"
#include "LoadingScreen.hpp"
#include "SplashScreenState.hpp"
#include "ErrorCheck.hpp"
#include "icon.hpp"
#include "Achievements.hpp"

#include <crogine/audio/AudioMixer.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/Message.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/detail/Types.hpp>

#include <crogine/util/Network.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/String.hpp>

namespace
{
#include "golf/TutorialShaders.inl"
#include "golf/TerrainShader.inl"
#include "golf/BeaconShader.inl"
#include "golf/PostProcess.inl"
#include "golf/RandNames.hpp"

    struct ShaderDescription final
    {
        const char* fragmentString = nullptr;
        std::string description;
        std::string toolTip;

        ShaderDescription(const char* frag, std::string desc, std::string tip)
            : fragmentString(frag), description(desc), toolTip(tip) {}
    };

    const std::string TerminalDistorted = "#define DISTORTION\n" + TerminalFragment;

    const std::array PostShaders =
    {
        ShaderDescription(TerminalFragment.c_str(), ShaderNames[0], "by Mostly Hairless."),
        ShaderDescription(TerminalDistorted.c_str(), ShaderNames[1], "by Mostly Hairless."),
        ShaderDescription(BWFragment.c_str(), ShaderNames[2], "by Mostly Hairless."),
        ShaderDescription(CRTFragment.c_str(), ShaderNames[3], "PUBLIC DOMAIN CRT STYLED SCAN-LINE SHADER\nby Timothy Lottes"),
    };
}

cro::RenderTarget* GolfGame::m_renderTarget = nullptr;

GolfGame::GolfGame()
    : m_stateStack      ({*this, getWindow()}),
    m_activeIndex       (0)
{
    //must be set before anything, else cfg is still loaded from default path
    setApplicationStrings("Trederia", "golf");

    m_stateStack.registerState<SplashState>(StateID::SplashScreen);
    m_stateStack.registerState<KeyboardState>(StateID::Keyboard);
    m_stateStack.registerState<MenuState>(StateID::Menu, m_sharedData);
    m_stateStack.registerState<GolfState>(StateID::Golf, m_sharedData);
    m_stateStack.registerState<ErrorState>(StateID::Error, m_sharedData);
    m_stateStack.registerState<OptionsState>(StateID::Options, m_sharedData);
    m_stateStack.registerState<PauseState>(StateID::Pause, m_sharedData);
    m_stateStack.registerState<TutorialState>(StateID::Tutorial, m_sharedData);
    m_stateStack.registerState<PracticeState>(StateID::Practice, m_sharedData);
    m_stateStack.registerState<DrivingState>(StateID::DrivingRange, m_sharedData);
    //m_stateStack.registerState<PuttingState>(StateID::PuttingRange, m_sharedData);
    m_stateStack.registerState<ClubhouseState>(StateID::Clubhouse, m_sharedData);
    m_stateStack.registerState<BilliardsState>(StateID::Billiards, m_sharedData);
    m_stateStack.registerState<TrophyState>(StateID::Trophy, m_sharedData);
}

//public
void GolfGame::handleEvent(const cro::Event& evt)
{
    switch (evt.type)
    {
    default: break;
    case SDL_KEYUP:
        switch (evt.key.keysym.sym)
        {
        default: break;
#ifdef CRO_DEBUG_
        case SDLK_ESCAPE:
        case SDLK_AC_BACK:
            App::quit();
            break;
        case SDLK_t:
            m_achievements->showTest();
            break;
#endif
        case SDLK_KP_MINUS:
            togglePixelScale(m_sharedData, false);
            break;
        case SDLK_KP_PLUS:
            togglePixelScale(m_sharedData, true);
            break;
        }
        break;
    }
    
    m_stateStack.handleEvent(evt);
}

void GolfGame::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::StateMessage)
    {
        const auto& data = msg.getData<cro::Message::StateEvent>();
        if (data.action == cro::Message::StateEvent::Popped)
        {
            switch (data.id)
            {
            default: break;
            case StateID::Options:
                saveSettings();
                savePreferences();
                break;
            }
        }
        else if (data.action == cro::Message::StateEvent::Cleared)
        {
            savePreferences();
        }
    }
    else if (msg.id == cro::Message::ConsoleMessage)
    {
        const auto& data = msg.getData<cro::Message::ConsoleEvent>();
        if (data.type == cro::Message::ConsoleEvent::Closed)
        {
            savePreferences();
        }
    }
    else if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            if (m_sharedData.usePostProcess)
            {
                recreatePostProcess();
            }
        }
    }
    else if (msg.id == MessageID::SystemMessage)
    {
        const auto& data = msg.getData<SystemEvent>();
        switch (data.type)
        {
        default: break;
        case SystemEvent::PostProcessToggled:
            if (m_postShader->getGLHandle() != 0)
            {
                m_sharedData.usePostProcess = !m_sharedData.usePostProcess;

                auto windowSize = cro::App::getWindow().getSize();
                if (m_sharedData.usePostProcess)
                {
                    recreatePostProcess();
                    m_renderTarget = m_postBuffer.get();
                }
                else
                {
                    m_renderTarget = &cro::App::getWindow();
                }

                //create a fake resize event to trigger any camera callbacks.
                SDL_Event resizeEvent;
                resizeEvent.type = SDL_WINDOWEVENT_SIZE_CHANGED;
                resizeEvent.window.windowID = 0;
                resizeEvent.window.data1 = windowSize.x;
                resizeEvent.window.data2 = windowSize.y;
                SDL_PushEvent(&resizeEvent);
            }
            break;
        case SystemEvent::PostProcessIndexChanged:
            applyPostProcess();
            break;
        }
    }

    m_stateStack.handleMessage(msg);
}

void GolfGame::simulate(float dt)
{
    if (m_sharedData.usePostProcess)
    {
        //update optional uniforms (should be -1 if not loaded)
        static float accum = 0.f;
        accum += dt;

        glUseProgram(m_postShader->getGLHandle());
        glUniform1f(m_uniformIDs[UniformID::Time], accum);
    }

    m_stateStack.simulate(dt);

    Achievements::update();
}

void GolfGame::render()
{
    if (m_sharedData.usePostProcess)
    {
        m_postBuffer->clear();
        m_stateStack.render();
        m_postBuffer->display();

        m_postQuad->draw();
    }
    else
    {
        m_stateStack.render();
    }

    m_achievements->drawOverlay();
}

bool GolfGame::initialise()
{
    m_hostAddresses = cro::Util::Net::getLocalAddresses();
    if (m_hostAddresses.empty())
    {
        LogE << "No suitable host addresses were found" << std::endl;
        return false;
    }

    registerConsoleTab("Advanced",
        [&]()
        {
            ImGui::Text("Post Process:");
            bool usePost = m_sharedData.usePostProcess;
            if (ImGui::Checkbox("PostProcess", &usePost))
            {
                //we rely on the message handler to actually
                //set the shared data property, in case we need to first create the buffer

                //raise a message to resize the post process buffers
                auto* msg = getMessageBus().post<SystemEvent>(MessageID::SystemMessage);
                msg->type = SystemEvent::PostProcessToggled;
            }

            ImGui::PushItemWidth(260.f);
            if (ImGui::BeginCombo("Shader", PostShaders[m_sharedData.postProcessIndex].description.c_str()))
            {
                for (auto i = 0u; i < PostShaders.size(); ++i)
                {
                    bool selected = (m_sharedData.postProcessIndex == static_cast<std::int32_t>(i));
                    if (ImGui::Selectable(PostShaders[i].description.c_str(), selected))
                    {
                        m_sharedData.postProcessIndex = static_cast<std::int32_t>(i);
                    }

                    if (selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("%s", PostShaders[m_sharedData.postProcessIndex].toolTip.c_str());
                ImGui::EndTooltip();
            }

            if (!m_sharedData.customShaderPath.empty())
            {
                ImGui::Text("Custom shader: %s", m_sharedData.customShaderPath.c_str());
            }
            else
            {
                ImGui::Text("No Custom Shader Loaded.");
            }

            if (ImGui::Button("Apply"))
            {
                applyPostProcess();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("Apply the selected built-in shader.");
                ImGui::EndTooltip();
            }

            ImGui::SameLine();
            if (ImGui::Button("Open"))
            {
                auto path = cro::FileSystem::openFileDialogue("", "txt,frag,glsl");
                std::replace(path.begin(), path.end(), '\\', '/');
                if (!path.empty())
                {
                    cro::RaiiRWops file;
                    file.file = SDL_RWFromFile(path.c_str(), "r");
                    if (file.file)
                    {
                        auto size = SDL_RWsize(file.file);
                        std::vector<char> buffer(size);
                        if (SDL_RWread(file.file, buffer.data(), size, 1))
                        {
                            //teminate the string!
                            buffer.push_back(0);
                            if (setShader(buffer.data()))
                            {
                                m_sharedData.customShaderPath = path;
                            }
                            else
                            {
                                m_sharedData.customShaderPath.clear();
                            }
                        }
                        else
                        {
                            cro::FileSystem::showMessageBox("Error", "Could not read file");
                        }
                    }
                    else
                    {
                        cro::FileSystem::showMessageBox("Error", "Failed to open file");
                    }
                }
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("Open a fragment shader from a text file.");
                ImGui::EndTooltip();
            }
        });

    registerConsoleTab("Credits",
        []()
        {
            ImGui::Text("Credits:");
            ImGui::Text("OPL VST synthesiser: https://github.com/jpcima/ADLplug");
            ImGui::Text("Sound Effects: https://freesound.org (see assets directory for specific credits)");
            ImGui::Text("CRT Effect Post Process Shader by Timothy Lottes (Public Domain)");
            ImGui::Text("Artwork: Blender and Aseprite https://blender.org https://aseprite.org");
            ImGui::Text("Colour Palette: Colordome32 https://lospec.com/palette-list/colordome-32");
            ImGui::Text("Programming: Matt Marchant, source available at: https://github.com/fallahn/crogine");
            ImGui::NewLine();
            ImGui::Text("Check out other games available from https://fallahn.itch.io !");
            if (ImGui::Button("Visit Website"))
            {
                cro::Util::String::parseURL("https://fallahn.itch.io/vga-golf");
            }
        });

    getWindow().setLoadingScreen<LoadingScreen>(m_sharedData);
    getWindow().setTitle("VGA Golf - " + StringVer);
    getWindow().setIcon(icon);
    m_renderTarget = &getWindow();

    cro::AudioMixer::setLabel("Music", MixerChannel::Music);
    cro::AudioMixer::setLabel("Effects", MixerChannel::Effects);
    cro::AudioMixer::setLabel("Menu", MixerChannel::Menu);
    cro::AudioMixer::setLabel("Announcer", MixerChannel::Voice);
    cro::AudioMixer::setLabel("Vehicles", MixerChannel::Vehicles);

    loadPreferences();
    loadAvatars();

    m_sharedData.clientConnection.netClient.create(ConstVal::MaxClients);
    m_sharedData.sharedResources = std::make_unique<cro::ResourceCollection>();
    std::fill(m_sharedData.controllerIDs.begin(), m_sharedData.controllerIDs.end(), 0);

    //texture used to hold name tags
    for (auto& t : m_sharedData.nameTextures)
    {
        t.create(LabelTextureSize.x, LabelTextureSize.y, false);
    }

    //preload resources which will be used in dynamically loaded menus
    m_sharedData.sharedResources->fonts.load(FontID::UI, "assets/golf/fonts/IBM_CGA.ttf");
    m_sharedData.sharedResources->fonts.load(FontID::Info, "assets/golf/fonts/MCPixel.otf");

    m_sharedData.sharedResources->shaders.loadFromString(ShaderID::TutorialSlope, TutorialVertexShader, TutorialSlopeShader);
    m_sharedData.sharedResources->shaders.loadFromString(ShaderID::Beacon, BeaconVertex, BeaconFragment, "#define SPRITE\n");


    m_sharedData.resolutions = getWindow().getAvailableResolutions();
    std::reverse(m_sharedData.resolutions.begin(), m_sharedData.resolutions.end());
    for (auto r : m_sharedData.resolutions)
    {
        auto& str = m_sharedData.resolutionStrings.emplace_back();
        str += std::to_string(r.x) + " x " + std::to_string(r.y);
    }

    cro::SpriteSheet s;
    s.loadFromFile("assets/golf/sprites/options.spt", m_sharedData.sharedResources->textures);
    s.loadFromFile("assets/golf/sprites/ui.spt", m_sharedData.sharedResources->textures);
    s.loadFromFile("assets/golf/sprites/scoreboard.spt", m_sharedData.sharedResources->textures);
    s.loadFromFile("assets/golf/sprites/controller_buttons.spt", m_sharedData.sharedResources->textures);
    s.loadFromFile("assets/golf/sprites/tutorial.spt", m_sharedData.sharedResources->textures);

    cro::ModelDefinition md(*m_sharedData.sharedResources);
    md.loadFromFile("assets/golf/models/trophies/trophy01.cmt");
    md.loadFromFile("assets/golf/models/trophies/trophy02.cmt");
    md.loadFromFile("assets/golf/models/trophies/trophy03.cmt");
    md.loadFromFile("assets/golf/models/trophies/trophy04.cmt");
    md.loadFromFile("assets/golf/models/trophies/trophy05.cmt");
    md.loadFromFile("assets/golf/models/trophies/trophy06.cmt");
    md.loadFromFile("assets/golf/models/trophies/trophy07.cmt");
    md.loadFromFile("assets/golf/models/trophies/trophy08.cmt");

    //set up the post process
    auto windowSize = cro::App::getWindow().getSize();
    m_postBuffer = std::make_unique<cro::RenderTexture>();
    m_postBuffer->create(windowSize.x, windowSize.y, false);
    m_postShader = std::make_unique<cro::Shader>();
    if (!m_sharedData.customShaderPath.empty())
    {
        cro::RaiiRWops file;
        file.file = SDL_RWFromFile(m_sharedData.customShaderPath.c_str(), "r");
        if (file.file)
        {
            auto size = SDL_RWsize(file.file);
            std::vector<char> buffer(size);
            if (SDL_RWread(file.file, buffer.data(), size, 1))
            {
                //teminate the string!
                buffer.push_back(0);
                if (!m_postShader->loadFromString(PostVertex, buffer.data()))
                {
                    m_postShader->loadFromString(PostVertex, PostShaders[m_sharedData.postProcessIndex].fragmentString);
                }
            }
            else
            {
                LogE << "Failed reading " << m_sharedData.customShaderPath << std::endl;
                m_sharedData.customShaderPath.clear();
            }
        }
        else
        {
            LogE << "Could not open " << m_sharedData.customShaderPath << std::endl;
            m_sharedData.customShaderPath.clear();
        }
    }
    else
    {
        m_postShader->loadFromString(PostVertex, PostShaders[m_sharedData.postProcessIndex].fragmentString);
    }
    auto shaderRes = glm::vec2(windowSize);
    glCheck(glUseProgram(m_postShader->getGLHandle()));
    glCheck(glUniform2f(m_postShader->getUniformID("u_resolution"), shaderRes.x, shaderRes.y));
    float scale = std::floor(shaderRes.y / calcVPSize().y);
    glCheck(glUniform2f(m_postShader->getUniformID("u_scale"), scale, scale));
    m_uniformIDs[UniformID::Time] = m_postShader->getUniformID("u_time");
    
    m_postQuad = std::make_unique<cro::SimpleQuad>();
    m_postQuad->setTexture(m_postBuffer->getTexture());
    m_postQuad->setShader(*m_postShader);

    m_activeIndex = m_sharedData.postProcessIndex;


    registerCommand("clubhouse", [&](const std::string&)
        {
            if (m_stateStack.getTopmostState() != StateID::Clubhouse)
            {
                //forces clubhouse state to clear any existing net connection
                m_sharedData.tutorial = true;

                m_sharedData.courseIndex = 0;

                m_stateStack.clearStates();
                m_stateStack.pushState(StateID::Clubhouse);
            }
            else
            {
                cro::Console::print("Already in clubhouse.");
            }
        });

#ifdef CRO_DEBUG_
    //m_stateStack.pushState(StateID::DrivingRange); //can't go straight to this because menu needs to parse avatar data
    m_stateStack.pushState(StateID::Menu);
    //m_stateStack.pushState(StateID::Clubhouse);
    //m_stateStack.pushState(StateID::SplashScreen);
#else
    m_stateStack.pushState(StateID::SplashScreen);
#endif

    m_achievements = std::make_unique<DefaultAchievements>(getMessageBus());
    Achievements::init(*m_achievements);

    return true;
}

void GolfGame::finalise()
{
    savePreferences();

    m_stateStack.clearStates();
    m_stateStack.simulate(0.f);

    for (auto& c : m_sharedData.avatarTextures)
    {
        for (auto& t : c)
        {
            t = {};
        }
    }

    for (auto& t : m_sharedData.nameTextures)
    {
        t = {};
    }

    m_sharedData.sharedResources.reset();
    m_postQuad.reset();
    m_postShader.reset();
    m_postBuffer.reset();
    m_achievements.reset();

    getWindow().setCursor(nullptr);
}

void GolfGame::loadPreferences()
{
    auto path = getPreferencePath() + "prefs.cfg";
    if (cro::FileSystem::fileExists(path))
    {
        cro::ConfigFile cfg;
        if (cfg.loadFromFile(path, false))
        {
            const auto& properties = cfg.getProperties();
            for (const auto& prop : properties)
            {
                const auto& name = prop.getName();
                if (name == "use_post_process")
                {
                    m_sharedData.usePostProcess = prop.getValue<bool>();
                }
                else if (name == "post_index")
                {
                    m_sharedData.postProcessIndex = std::max(0, std::min(static_cast<std::int32_t>(PostShaders.size()) - 1, prop.getValue<std::int32_t>()));
                }
                else if (name == "custom_shader")
                {
                    m_sharedData.customShaderPath = prop.getValue<std::string>();
                }
                else if (name == "last_ip")
                {
                    auto str = prop.getValue<std::string>();

                    if (!str.empty())
                    {
                        m_sharedData.targetIP = str.substr(0, ConstVal::MaxIPChars);
                    }
                    else
                    {
                        m_sharedData.targetIP = "255.255.255.255";
                    }
                }
                else if (name == "pixel_scale")
                {
                    m_sharedData.pixelScale = prop.getValue<bool>();
                }
                else if (name == "fov")
                {
                    m_sharedData.fov = std::max(MinFOV, std::min(MaxFOV, prop.getValue<float>()));
                }
                else if (name == "vertex_snap")
                {
                    m_sharedData.vertexSnap = prop.getValue<bool>();
                }
                else if (name == "mouse_speed")
                {
                    m_sharedData.mouseSpeed = std::max(ConstVal::MinMouseSpeed, std::min(ConstVal::MaxMouseSpeed, prop.getValue<float>()));
                }
                else if (name == "invert_x")
                {
                    m_sharedData.invertX = prop.getValue<bool>();
                }
                else if (name == "invert_y")
                {
                    m_sharedData.invertY = prop.getValue<bool>();
                }
                else if (name == "show_beacon")
                {
                    m_sharedData.showBeacon = prop.getValue<bool>();
                }
                else if (name == "beacon_colour")
                {
                    m_sharedData.beaconColour = std::fmod(prop.getValue<float>(), 1.f);
                }
                else if (name == "imperial_measurements")
                {
                    m_sharedData.imperialMeasurements = prop.getValue<bool>();
                }
            }
        }
    }

    //read keybind bin
    path = getPreferencePath() + "keys.bind";

    if (cro::FileSystem::fileExists(path))
    {
        cro::RaiiRWops file;
        file.file = SDL_RWFromFile(path.c_str(), "rb");
        if (file.file)
        {
            auto size = SDL_RWsize(file.file);
            if (size == sizeof(InputBinding))
            {
                SDL_RWread(file.file, &m_sharedData.inputBinding, size, 1);
                LOG("Read keybinds file", cro::Logger::Type::Info);
            }
        }
    }

    //hack for existing keybinds which weren't expecting the billiards update
    if (m_sharedData.inputBinding.keys[InputBinding::Up] == SDLK_UNKNOWN)
    {
        m_sharedData.inputBinding.keys[InputBinding::Up] = SDLK_w;
    }
    if (m_sharedData.inputBinding.keys[InputBinding::Down] == SDLK_UNKNOWN)
    {
        m_sharedData.inputBinding.keys[InputBinding::Down] = SDLK_s;
    }
}

void GolfGame::savePreferences()
{
    auto path = getPreferencePath() + "prefs.cfg";
    cro::ConfigFile cfg("preferences");

    //advanced options
    cfg.addProperty("use_post_process").setValue(m_sharedData.usePostProcess);
    cfg.addProperty("post_index").setValue(m_sharedData.postProcessIndex);
    if (!m_sharedData.customShaderPath.empty())
    {
        cfg.addProperty("custom_shader").setValue(m_sharedData.customShaderPath);
    }
    cfg.addProperty("last_ip").setValue(m_sharedData.targetIP.toAnsiString());
    cfg.addProperty("pixel_scale").setValue(m_sharedData.pixelScale);
    cfg.addProperty("fov").setValue(m_sharedData.fov);
    cfg.addProperty("vertex_snap").setValue(m_sharedData.vertexSnap);
    cfg.addProperty("mouse_speed").setValue(m_sharedData.mouseSpeed);
    cfg.addProperty("invert_x").setValue(m_sharedData.invertX);
    cfg.addProperty("invert_y").setValue(m_sharedData.invertY);
    cfg.addProperty("show_beacon").setValue(m_sharedData.showBeacon);
    cfg.addProperty("beacon_colour").setValue(m_sharedData.beaconColour);
    cfg.addProperty("imperial_measurements").setValue(m_sharedData.imperialMeasurements);
    cfg.save(path);


    //keybinds
    path = getPreferencePath() + "keys.bind";
    cro::RaiiRWops file;
    file.file = SDL_RWFromFile(path.c_str(), "wb");
    if (file.file)
    {
        SDL_RWwrite(file.file, &m_sharedData.inputBinding, sizeof(InputBinding), 1);
    }
}

void GolfGame::loadAvatars()
{
    auto path = cro::App::getPreferencePath() + "avatars.cfg";
    cro::ConfigFile cfg;
    if (cfg.loadFromFile(path, false))
    {
        std::uint32_t i = 0;

        const auto& objects = cfg.getObjects();
        for (const auto& obj : objects)
        {
            if (obj.getName() == "avatar"
                && i < m_sharedData.localConnectionData.MaxPlayers)
            {
                const auto& props = obj.getProperties();
                for (const auto& prop : props)
                {
                    const auto& name = prop.getName();
                    if (name == "name")
                    {
                        //TODO try running this through unicode parser
                        m_sharedData.localConnectionData.playerData[i].name = prop.getValue<std::string>();
                    }
                    else if (name == "ball_id")
                    {
                        auto id = prop.getValue<std::uint32_t>();
                        m_sharedData.localConnectionData.playerData[i].ballID = id;
                    }
                    else if (name == "hair_id")
                    {
                        auto id = prop.getValue<std::uint32_t>();
                        m_sharedData.localConnectionData.playerData[i].hairID = id;
                    }
                    else if (name == "skin_id")
                    {
                        auto id = prop.getValue<std::uint32_t>();
                        m_sharedData.localConnectionData.playerData[i].skinID = id;
                    }
                    else if (name == "flipped")
                    {
                        m_sharedData.localConnectionData.playerData[i].flipped = prop.getValue<bool>();
                    }

                    else if (name == "flags0")
                    {
                        auto flag = prop.getValue<std::int32_t>();
                        flag = std::min(pc::ColourID::Count - 1, std::max(0, flag));
                        m_sharedData.localConnectionData.playerData[i].avatarFlags[0] = static_cast<std::uint8_t>(flag);
                    }
                    else if (name == "flags1")
                    {
                        auto flag = prop.getValue<std::int32_t>();
                        flag = std::min(pc::ColourID::Count - 1, std::max(0, flag));
                        m_sharedData.localConnectionData.playerData[i].avatarFlags[1] = static_cast<std::uint8_t>(flag);
                    }
                    else if (name == "flags2")
                    {
                        auto flag = prop.getValue<std::int32_t>();
                        flag = std::min(pc::ColourID::Count - 1, std::max(0, flag));
                        m_sharedData.localConnectionData.playerData[i].avatarFlags[2] = static_cast<std::uint8_t>(flag);
                    }
                    else if (name == "flags3")
                    {
                        auto flag = prop.getValue<std::int32_t>();
                        flag = std::min(pc::ColourID::Count - 1, std::max(0, flag));
                        m_sharedData.localConnectionData.playerData[i].avatarFlags[3] = static_cast<std::uint8_t>(flag);
                    }

                    else if (name == "cpu")
                    {
                        m_sharedData.localConnectionData.playerData[i].isCPU = prop.getValue<bool>();
                    }
                }

                i++;
            }
        }
    }

    if (m_sharedData.localConnectionData.playerData[0].name.empty())
    {
        m_sharedData.localConnectionData.playerData[0].name = RandomNames[cro::Util::Random::value(0u, RandomNames.size() - 1)];
    }
}

void GolfGame::recreatePostProcess()
{
    m_uniformIDs[UniformID::Time] = m_postShader->getUniformID("u_time");
    m_uniformIDs[UniformID::Scale] = m_postShader->getUniformID("u_scale");

    auto windowSize = cro::App::getWindow().getSize();
    m_postBuffer->create(windowSize.x, windowSize.y, false);
    m_postQuad->setTexture(m_postBuffer->getTexture()); //resizes the quad's view

    auto shaderRes = glm::vec2(windowSize);
    glCheck(glUseProgram(m_postShader->getGLHandle()));
    glCheck(glUniform2f(m_postShader->getUniformID("u_resolution"), shaderRes.x, shaderRes.y));

    float scale = std::floor(shaderRes.y / calcVPSize().y);
    glCheck(glUniform2f(m_postShader->getUniformID("u_scale"), scale, scale));
}

void GolfGame::applyPostProcess()
{
    if (m_activeIndex != m_sharedData.postProcessIndex
        || !m_sharedData.customShaderPath.empty())
    {
        if (setShader(PostShaders[m_sharedData.postProcessIndex].fragmentString))
        {
            m_sharedData.customShaderPath.clear();
        }
    }
}

bool GolfGame::setShader(const char* frag)
{
    std::unique_ptr<cro::Shader> shader = std::make_unique<cro::Shader>();
    if (shader->loadFromString(PostVertex, frag))
    {
        //if the shader loaded successfully
        //swap with current shader.
        m_postShader.swap(shader);
        m_postQuad->setShader(*m_postShader);

        m_sharedData.usePostProcess = false; //message handler flips this???
        auto* msg = getMessageBus().post<SystemEvent>(MessageID::SystemMessage);
        msg->type = SystemEvent::PostProcessToggled;

        m_activeIndex = m_sharedData.postProcessIndex;
        return true;
    }
    else
    {
        cro::FileSystem::showMessageBox("Error", "Failed to compile shader\nSee console for more details");
    }
    return false;
};