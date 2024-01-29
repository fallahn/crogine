/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2024
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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
#include "golf/ClubhouseState.hpp"
#include "golf/LeagueState.hpp"
#include "golf/GcState.hpp"
#include "golf/MessageOverlayState.hpp"
#include "golf/TrophyState.hpp"
#include "golf/NewsState.hpp"
#include "golf/PlaylistState.hpp"
#include "golf/PlayerManagementState.hpp"
#include "golf/CreditsState.hpp"
#include "golf/UnlockState.hpp"
#include "golf/ProfileState.hpp"
#include "golf/LeaderboardState.hpp"
#include "golf/StatsState.hpp"
#include "golf/MapOverviewState.hpp"
#include "golf/EventOverlay.hpp"
#include "golf/MenuConsts.hpp"
#include "golf/GameConsts.hpp"
#include "golf/MessageIDs.hpp"
#include "golf/PacketIDs.hpp"
#include "golf/UnlockItems.hpp"
#include "editor/BushState.hpp"
#include "sqlite/SqliteState.hpp"
#include "runner/EndlessAttractState.hpp"
#include "runner/EndlessDrivingState.hpp"
#include "runner/EndlessPauseState.hpp"
#include "runner/EndlessShared.hpp"
#include "LoadingScreen.hpp"
#include "SplashScreenState.hpp"
#include "ErrorCheck.hpp"
#include "icon.hpp"
#include "Achievements.hpp"
#include "golf/Clubs.hpp"
#include "golf/XPAwardStrings.hpp"

#include "ImTheme.hpp"

#include <AchievementIDs.hpp>
#include <AchievementStrings.hpp>

#ifdef USE_GNS
#include <AchievementsImpl.hpp>
#include <Social.hpp>
#endif
#ifdef USE_WORKSHOP
#include <WorkshopState.hpp>
#endif

#include <crogine/audio/AudioMixer.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/Message.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/core/SysTime.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/detail/Types.hpp>

#include <crogine/util/Network.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/String.hpp>

#include <filesystem>

using namespace cl;

namespace
{
#include "golf/shaders/TutorialShaders.inl"
#include "golf/shaders/TerrainShader.inl"
#include "golf/shaders/BeaconShader.inl"
#include "golf/shaders/PostProcess.inl"
#include "golf/shaders/ShaderIncludes.inl"
#include "golf/RandNames.hpp"

    els::SharedStateData elsShared;

    struct ShaderDescription final
    {
        const char* fragmentString = nullptr;
        std::string description;
        std::string toolTip;

        ShaderDescription(const char* frag, const std::string& desc, const std::string& tip)
            : fragmentString(frag), description(desc), toolTip(tip) {}
    };

    const std::string TerminalDistorted = "#define DISTORTION\n" + TerminalFragment;

    const std::array PostShaders =
    {
        ShaderDescription(TerminalFragment.c_str(), ShaderNames[0], "by Mostly Hairless."),
        ShaderDescription(TerminalDistorted.c_str(), ShaderNames[1], "by Mostly Hairless."),
        ShaderDescription(BWFragment.c_str(), ShaderNames[2], "by Mostly Hairless."),
        ShaderDescription(CRTFragment.c_str(), ShaderNames[3], "PUBLIC DOMAIN CRT STYLED SCAN-LINE SHADER\nby Timothy Lottes"),
        ShaderDescription(CinematicFragment.c_str(), ShaderNames[4], ""),
    };

    std::vector<CreditEntry> credits;

    void parseCredits()
    {
        cro::ConfigFile file;
        if (file.loadFromFile("assets/credits.tgl"))
        {
            const auto& entries = file.getObjects();
            for (const auto& entry : entries)
            {
                if (entry.getName() == "section")
                {
                    auto& creditEntry = credits.emplace_back();
                    const auto& props = entry.getProperties();
                    for (const auto& prop : props)
                    {
                        const auto& name = prop.getName();
                        if (name == "title")
                        {
                            creditEntry.title = prop.getValue<cro::String>();
                        }
                        else if (name == "name")
                        {
                            creditEntry.names.emplace_back(prop.getValue<cro::String>());
                        }
                    }
                }
            }
        }
    }
}

cro::RenderTarget* GolfGame::m_renderTarget = nullptr;

GolfGame::GolfGame()
    : m_stateStack      ({*this, getWindow()}),
    m_activeIndex       (0)
{
    //must be set before anything, else cfg is still loaded from default path
    setApplicationStrings("Trederia", "golf");

    m_stateStack.registerState<SplashState>(StateID::SplashScreen, m_sharedData);
    m_stateStack.registerState<KeyboardState>(StateID::Keyboard, m_sharedData);
    m_stateStack.registerState<NewsState>(StateID::News, m_sharedData);
    m_stateStack.registerState<MenuState>(StateID::Menu, m_sharedData, m_profileData);
    m_stateStack.registerState<ProfileState>(StateID::Profile, m_sharedData, m_profileData);
    m_stateStack.registerState<OptionsState>(StateID::Options, m_sharedData);
    m_stateStack.registerState<CreditsState>(StateID::Credits, m_sharedData, credits);
    m_stateStack.registerState<UnlockState>(StateID::Unlock, m_sharedData);
    m_stateStack.registerState<GolfState>(StateID::Golf, m_sharedData);
    m_stateStack.registerState<ErrorState>(StateID::Error, m_sharedData);
    m_stateStack.registerState<PauseState>(StateID::Pause, m_sharedData);
    m_stateStack.registerState<PlayerManagementState>(StateID::PlayerManagement, m_sharedData);
    m_stateStack.registerState<TutorialState>(StateID::Tutorial, m_sharedData);
    m_stateStack.registerState<PracticeState>(StateID::Practice, m_sharedData);
    m_stateStack.registerState<DrivingState>(StateID::DrivingRange, m_sharedData, m_profileData);
    m_stateStack.registerState<ClubhouseState>(StateID::Clubhouse, m_sharedData, m_profileData, *this);
    m_stateStack.registerState<BilliardsState>(StateID::Billiards, m_sharedData);
    m_stateStack.registerState<TrophyState>(StateID::Trophy, m_sharedData);
    m_stateStack.registerState<PlaylistState>(StateID::Playlist, m_sharedData);
    m_stateStack.registerState<LeaderboardState>(StateID::Leaderboard, m_sharedData);
    m_stateStack.registerState<StatsState>(StateID::Stats, m_sharedData);
    m_stateStack.registerState<LeagueState>(StateID::League, m_sharedData);
    m_stateStack.registerState<MapOverviewState>(StateID::MapOverview, m_sharedData);
    m_stateStack.registerState<BushState>(StateID::Bush, m_sharedData);
    m_stateStack.registerState<EndlessAttractState>(StateID::EndlessAttract, m_sharedData, elsShared);
    m_stateStack.registerState<EndlessDrivingState>(StateID::EndlessRunner, m_sharedData, elsShared);
    m_stateStack.registerState<EndlessPauseState>(StateID::EndlessPause, m_sharedData, elsShared);
    m_stateStack.registerState<MessageOverlayState>(StateID::MessageOverlay, m_sharedData);
    m_stateStack.registerState<EventOverlayState>(StateID::EventOverlay);
    m_stateStack.registerState<GCState>(StateID::GC);

    m_sharedData.courseIndex = courseOfTheMonth();

#ifdef CRO_DEBUG_
    m_stateStack.registerState<SqliteState>(StateID::SQLite);
#endif

#ifdef USE_WORKSHOP
    m_stateStack.registerState<WorkshopState>(StateID::Workshop);
#endif
}

//public
void GolfGame::handleEvent(const cro::Event& evt)
{
    switch (evt.type)
    {
    default: break;
    case SDL_MOUSEMOTION:
        //cro::App::getWindow().setMouseCaptured(false);
        break;
    case SDL_KEYUP:
        switch (evt.key.keysym.sym)
        {
        default: break;
#ifdef CRO_DEBUG_
        case SDLK_ESCAPE:
        case SDLK_AC_BACK:
            App::quit();
            break;
        /*case SDLK_KP_PERIOD:
            m_progressIcon->show(0, 10, 10);
            break;*/
#ifndef USE_GNS
        case SDLK_t:
            m_achievements->showTest();
            break;
#endif
#endif
        case SDLK_KP_MINUS:
            togglePixelScale(m_sharedData, false);
            break;
        case SDLK_KP_PLUS:
            togglePixelScale(m_sharedData, true);
            break;
        case SDLK_KP_MULTIPLY:
            m_sharedData.sharedResources->fonts.get(FontID::Label).setSmooth(true);
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
        case SystemEvent::RequestOSK:
            if (m_stateStack.getTopmostState() != StateID::Keyboard)
            {
                if (data.data == 1)
                {
                    m_sharedData.useOSKBuffer = true;
                    m_sharedData.OSKBuffer.clear();
                }
                else
                {
                    m_sharedData.useOSKBuffer = false;
                }
                m_stateStack.pushState(StateID::Keyboard);
            }
            break;
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
    else if (msg.id == Social::MessageID::SocialMessage)
    {
        const auto& data = msg.getData<Social::SocialEvent>();
        if (data.type == Social::SocialEvent::LevelUp)
        {
            switch (data.level)
            {
            default: break;
            case 1:
                Achievements::awardAchievement(AchievementStrings[AchievementID::GettingStarted]);
                break;
            case 10:
                Achievements::awardAchievement(AchievementStrings[AchievementID::Junior]);
                break;
            case 20:
                Achievements::awardAchievement(AchievementStrings[AchievementID::Amateur]);
                break;
            case 30:
                Achievements::awardAchievement(AchievementStrings[AchievementID::Enthusiast]);
                break;
            case 40:
                Achievements::awardAchievement(AchievementStrings[AchievementID::SemiPro]);
                break;
            case 50:
                Achievements::awardAchievement(AchievementStrings[AchievementID::Pro]);
                break;
            }
        }
        else if (data.type == Social::SocialEvent::OverlayActivated)
        {
            if (data.level != 0)
            {
                m_stateStack.pushState(StateID::EventOverlay);
            }
        }
        else if (data.type == Social::SocialEvent::MonthlyProgress)
        {
            m_progressIcon->show(data.challengeID, data.level, data.reason);

            if (data.challengeID != -1 &&
                data.level == data.reason)
            {
                Social::awardXP(500, XPStringID::ChallengeComplete);
                Achievements::awardAchievement(AchievementStrings[AchievementID::UpForTheChallenge]);
            }
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
    m_progressIcon->update(dt);
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

#ifndef USE_GNS
    m_achievements->drawOverlay();
#endif
    m_progressIcon->draw();
}

bool GolfGame::initialise()
{
    auto path = cro::App::getPreferencePath() + "user/";
    if (!cro::FileSystem::directoryExists(path))
    {
        cro::FileSystem::createDirectory(path);
    }
    path = cro::App::getPreferencePath() + "courses/";
    if (!cro::FileSystem::directoryExists(path))
    {
        cro::FileSystem::createDirectory(path);
    }


#if defined USE_GNS
    m_achievements = std::make_unique<SteamAchievements>(MessageID::AchievementMessage);
#else
    m_achievements = std::make_unique<DefaultAchievements>();
#endif
    auto initResult = Achievements::init(*m_achievements);

    //in 1.15 we moved user prefs to individual user IDs
    //so multiple users can have their own profiles on the
    //same device. This just looks for existing data and
    //moves it to the current user's dir, in an attempt to
    //prevent any profile loss.
    if (initResult)
    {
        //however this relies on having successfully
        //init Steam as we need the uid of the logged on user
        convertPreferences();
    }


    //do this first because if we quit early the preferences will otherwise get overwritten by defaults.
    loadPreferences();

    if (!initResult)
    {
        //no point trying to load the menu if we failed to init.
        return false;
    }

    loadAvatars(); //this relies on steam being initialised

//#ifdef USE_WORKSHOP
//    registerCommand("workshop",
//        [&](const std::string&)
//        {
//            m_stateStack.clearStates();
//            m_stateStack.pushState(StateID::Workshop);
//        });
//#endif

#ifdef CRO_DEBUG_
#ifndef USE_GNS
    m_achievements->clearAchievement(AchievementStrings[AchievementID::IntoOrbit]);
#endif // !USE_GNS
#endif

#ifndef USE_GNS
    m_hostAddresses = cro::Util::Net::getLocalAddresses();
    if (m_hostAddresses.empty())
    {
        cro::Logger::log("No suitable host addresses were found", cro::Logger::Type::Error, cro::Logger::Output::All);
        return false;
    }
    Social::userIcon = cropAvatarImage("assets/images/default_profile.png");
#endif

    parseCredits();

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

    registerCommand("log_benchmark", 
        [&](const std::string& state)
        {
            if (state.empty())
            {
                cro::Console::print(m_sharedData.logBenchmarks ? "Logging is enabled" : "Logging is disabled");
                cro::Console::print("Use log_benchmark <true|false> to toggle");
            }
            else
            {
                if (state == "true")
                {
                    m_sharedData.logBenchmarks = true;
                    cro::Console::print("Benchmarks will be logged to " + cro::App::getPreferencePath() + "benchmark/");
                }
                else if (state == "false")
                {
                    m_sharedData.logBenchmarks = false;
                    cro::Console::print("Benchmark logging disabled");
                }
                else
                {
                    cro::Console::print("Use log_benchmark <true|false> to toggle");
                }
            }
        });

    registerCommand("show_userdir",
        [](const std::string&)
        {
            //this assumes that the directory was successfully creates already...
            cro::Util::String::parseURL(Social::getBaseContentPath());
        });

    registerCommand("reset_league", 
        [](const std::string&)
        {
            League l;
            l.reset();
            cro::Console::print("League tables are reset");
        });

    cro::Console::addConvar("shuffle_music", "false", "If true then custom music playlists will be shuffled when loaded.");

    registerCommand("cl_shuffle_music",
        [&](const std::string& param)
        {
            if (cro::Util::String::toLower(param) == "true")
            {
                cro::Console::setConvarValue("shuffle_music", true);
                cro::Console::print("cl_shuffle_music set to TRUE");

                m_sharedData.m3uPlaylist->shuffle();
            }
            else if (cro::Util::String::toLower(param) == "false")
            {
                cro::Console::setConvarValue("shuffle_music", false);
                cro::Console::print("cl_shuffle_music set to FALSE");
            }
            else if (param.empty())
            {
                bool b = cro::Console::getConvarValue<bool>("shuffle_music");
                if (b)
                {
                    cro::Console::print("cl_shuffle_music set to TRUE");
                }
                else
                {
                    cro::Console::print("cl_shuffle_music set to FALSE");
                }
            }
            else
            {
                cro::Console::print(param + ": invalid argument. Set to TRUE or FALSE");
            }
        });

    if (cro::Console::getConvarValue<bool>("shuffle_music"))
    {
        m_sharedData.m3uPlaylist->shuffle();
    }

    getWindow().setLoadingScreen<LoadingScreen>(m_sharedData);
    getWindow().setTitle("Super Video Golf - " + StringVer);
    getWindow().setIcon(icon);
    m_renderTarget = &getWindow();

    cro::AudioMixer::setLabel("Music", MixerChannel::Music);
    cro::AudioMixer::setLabel("Effects", MixerChannel::Effects);
    cro::AudioMixer::setLabel("Menu", MixerChannel::Menu);
    cro::AudioMixer::setLabel("Announcer", MixerChannel::Voice);
    cro::AudioMixer::setLabel("Vehicles", MixerChannel::Vehicles);
    cro::AudioMixer::setLabel("Environment", MixerChannel::Environment);
    cro::AudioMixer::setLabel("User Music", MixerChannel::UserMusic);

    m_sharedData.clientConnection.netClient.create(ConstVal::MaxClients);
    m_sharedData.sharedResources = std::make_unique<cro::ResourceCollection>();

    //texture used to hold name tags
    for (auto& t : m_sharedData.nameTextures)
    {
        t.create(LabelTextureSize.x, LabelTextureSize.y, false);
    }

    //preload resources which will be used in dynamically loaded menus
    initFonts();

    for (const auto& [name, str] : IncludeMappings)
    {
        m_sharedData.sharedResources->shaders.addInclude(name, str);
    }

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
    s.loadFromFile("assets/golf/sprites/facilities_menu.spt", m_sharedData.sharedResources->textures);
    s.loadFromFile("assets/golf/sprites/scoreboard.spt", m_sharedData.sharedResources->textures);
    s.loadFromFile("assets/golf/sprites/controller_buttons.spt", m_sharedData.sharedResources->textures);
    s.loadFromFile("assets/golf/sprites/unlocks.spt", m_sharedData.sharedResources->textures);
    s.loadFromFile("assets/golf/sprites/tutorial.spt", m_sharedData.sharedResources->textures);
    s.loadFromFile("assets/sprites/osk.spt", m_sharedData.sharedResources->textures);

    cro::ModelDefinition md(*m_sharedData.sharedResources);
    md.loadFromFile("assets/golf/models/trophies/trophy01.cmt");
    md.loadFromFile("assets/golf/models/trophies/trophy02.cmt");
    md.loadFromFile("assets/golf/models/trophies/trophy03.cmt");
    md.loadFromFile("assets/golf/models/trophies/trophy04.cmt");
    md.loadFromFile("assets/golf/models/trophies/trophy05.cmt");
    md.loadFromFile("assets/golf/models/trophies/trophy06.cmt");
    md.loadFromFile("assets/golf/models/trophies/trophy07.cmt");
    md.loadFromFile("assets/golf/models/trophies/trophy08.cmt");
    md.loadFromFile("assets/golf/models/trophies/trophy09.cmt");
    md.loadFromFile("assets/golf/models/trophies/level01.cmt");
    md.loadFromFile("assets/golf/models/trophies/level10.cmt");
    md.loadFromFile("assets/golf/models/trophies/level20.cmt");
    md.loadFromFile("assets/golf/models/trophies/level30.cmt");
    md.loadFromFile("assets/golf/models/trophies/level40.cmt");
    md.loadFromFile("assets/golf/models/trophies/level50.cmt");

    for (const auto& str : ul::ModelPaths) //models displayed in 'unlock' state
    {
        md.loadFromFile(str);
    }

    //icon for challenge progress
    m_progressIcon = std::make_unique<ProgressIcon>(m_sharedData.sharedResources->fonts.get(FontID::Label));

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
    float scale = getViewScale(shaderRes);
    glCheck(glUniform2f(m_postShader->getUniformID("u_scale"), scale, scale));
    m_uniformIDs[UniformID::Time] = m_postShader->getUniformID("u_time");
    
    m_postQuad = std::make_unique<cro::SimpleQuad>();
    m_postQuad->setTexture(m_postBuffer->getTexture());
    m_postQuad->setShader(*m_postShader);

    m_activeIndex = m_sharedData.postProcessIndex;

#ifdef CRO_DEBUG_
    //m_stateStack.pushState(StateID::DrivingRange); //can't go straight to this because menu needs to parse avatar data
    //m_stateStack.pushState(StateID::Bush);
    //m_stateStack.pushState(StateID::Clubhouse);
    //m_stateStack.pushState(StateID::SplashScreen);
    m_stateStack.pushState(StateID::Menu);
    //m_stateStack.pushState(StateID::EndlessRunner);
    //m_stateStack.pushState(StateID::EndlessAttract);
    //m_stateStack.pushState(StateID::Workshop);
#else
    m_stateStack.pushState(StateID::SplashScreen);
#endif

    applyImGuiStyle(m_sharedData);

    return true;
}

void GolfGame::finalise()
{
    m_sharedData.clientConnection.netClient.disconnect();
    m_sharedData.serverInstance.stop(); //this waits for any threads to finish first.

    savePreferences();

    m_stateStack.clearStates();
    m_stateStack.simulate(0.f);

    cro::App::unloadPlugin(m_stateStack);

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

    Achievements::shutdown();

    m_sharedData.sharedResources.reset();
    m_postQuad.reset();
    m_postShader.reset();
    m_postBuffer.reset();
    m_achievements.reset();

    getWindow().setCursor(nullptr);
}

void GolfGame::initFonts()
{
    m_sharedData.sharedResources->fonts.load(FontID::UI, "assets/golf/fonts/IBM_CGA.ttf");
    m_sharedData.sharedResources->fonts.load(FontID::Info, "assets/golf/fonts/MCPixel.otf");
    m_sharedData.sharedResources->fonts.load(FontID::Label, "assets/golf/fonts/ProggyClean.ttf");

    m_sharedData.sharedResources->fonts.load(FontID::OSK, "assets/fonts/VeraMono.ttf");

    //international fonts - these mappings are those used in DearImGui
    cro::FontAppendmentContext ctx;
    static const std::array FontMappings =
    {
        std::make_pair("assets/golf/fonts/NotoSans-Regular.ttf", cro::CodePointRange::Cyrillic),
        std::make_pair("assets/golf/fonts/NotoSans-Regular.ttf", cro::CodePointRange::Greek),
        std::make_pair("assets/golf/fonts/NotoSans-Regular.ttf", std::array<std::uint32_t, 2u>({0x0102, 0x0103})), //VT
        std::make_pair("assets/golf/fonts/NotoSans-Regular.ttf", std::array<std::uint32_t, 2u>({0x0110, 0x0111})), //VT
        std::make_pair("assets/golf/fonts/NotoSans-Regular.ttf", std::array<std::uint32_t, 2u>({0x0128, 0x0129})), //VT
        std::make_pair("assets/golf/fonts/NotoSans-Regular.ttf", std::array<std::uint32_t, 2u>({0x0168, 0x0169})), //VT
        std::make_pair("assets/golf/fonts/NotoSans-Regular.ttf", std::array<std::uint32_t, 2u>({0x01A0, 0x01A1})), //VT
        std::make_pair("assets/golf/fonts/NotoSans-Regular.ttf", std::array<std::uint32_t, 2u>({0x01AF, 0x01B0})), //VT
        std::make_pair("assets/golf/fonts/NotoSans-Regular.ttf", std::array<std::uint32_t, 2u>({0x1EA0, 0x1EF9})), //VT

        std::make_pair("assets/golf/fonts/NotoSansThai-Regular.ttf", std::array<std::uint32_t, 2u>({0x2010, 0x205E})),
        std::make_pair("assets/golf/fonts/NotoSansThai-Regular.ttf", std::array<std::uint32_t, 2u>({0x0E00, 0x0E7F})),
        std::make_pair("assets/golf/fonts/NotoSansKR-Regular.ttf", std::array<std::uint32_t, 2u>({0x3131, 0x3163})),
        std::make_pair("assets/golf/fonts/NotoSansKR-Regular.ttf", std::array<std::uint32_t, 2u>({0xAC00, 0xD7A3})),
        std::make_pair("assets/golf/fonts/NotoSansTC-Regular.ttf", std::array<std::uint32_t, 2u>({0x2000, 0x206F})),
        std::make_pair("assets/golf/fonts/NotoSansTC-Regular.ttf", std::array<std::uint32_t, 2u>({0x3000, 0x30FF})),
        std::make_pair("assets/golf/fonts/NotoSansTC-Regular.ttf", std::array<std::uint32_t, 2u>({0x31F0, 0x31FF})),
        std::make_pair("assets/golf/fonts/NotoSansTC-Regular.ttf", std::array<std::uint32_t, 2u>({0xFF00, 0xFFEF})),
        //std::make_pair("assets/golf/fonts/NotoSansTC-Regular.ttf", std::array<std::uint32_t, 2u>({0xFFFD, 0xFFFD})),
        std::make_pair("assets/golf/fonts/NotoSansTC-Regular.ttf", std::array<std::uint32_t, 2u>({0x4e00, 0x9FAF})),
    };

    for (const auto& [path, codepoints] : FontMappings)
    {
        ctx.codepointRange = codepoints;
        m_sharedData.sharedResources->fonts.get(FontID::UI).appendFromFile(path, ctx);
        m_sharedData.sharedResources->fonts.get(FontID::Info).appendFromFile(path, ctx);
        m_sharedData.sharedResources->fonts.get(FontID::Label).appendFromFile(path, ctx);

        m_sharedData.sharedResources->fonts.get(FontID::OSK).appendFromFile(path, ctx);
    }



    //emoji fonts
    ctx.allowBold = false;
    ctx.allowFillColour = true;
    ctx.allowOutline = false;

    static constexpr std::array Ranges =
    {
        cro::CodePointRange::EmojiLower,
        cro::CodePointRange::EmojiMid,
        cro::CodePointRange::EmojiUpper,
    };

#ifdef _WIN32
    const std::string winPath = "C:/Windows/Fonts/seguiemj.ttf";
    //const std::string winPath = "assets/golf/fonts/NotoEmoji-Regular.ttf";
    
    if (cro::FileSystem::fileExists(winPath))
    {
        ctx.allowFillColour = false;
        for (const auto& r : Ranges)
        {
            ctx.codepointRange = r;
            m_sharedData.sharedResources->fonts.get(FontID::UI).appendFromFile(winPath, ctx);
            m_sharedData.sharedResources->fonts.get(FontID::Info).appendFromFile(winPath, ctx);
            m_sharedData.sharedResources->fonts.get(FontID::Label).appendFromFile(winPath, ctx);

            m_sharedData.sharedResources->fonts.get(FontID::OSK).appendFromFile(winPath, ctx);
        }
    }
    else
#endif
    {
        for (const auto& r : Ranges)
        {
            ctx.codepointRange = r;
            m_sharedData.sharedResources->fonts.get(FontID::UI).appendFromFile("assets/golf/fonts/NotoEmoji-Regular.ttf", ctx);
            m_sharedData.sharedResources->fonts.get(FontID::Info).appendFromFile("assets/golf/fonts/NotoEmoji-Regular.ttf", ctx);
            m_sharedData.sharedResources->fonts.get(FontID::Label).appendFromFile("assets/golf/fonts/NotoEmoji-Regular.ttf", ctx);

            m_sharedData.sharedResources->fonts.get(FontID::OSK).appendFromFile("assets/golf/fonts/NotoEmoji-Regular.ttf", ctx);
        }
    }
}

void GolfGame::convertPreferences() const
{
    auto srcPath = cro::App::getPreferencePath();
    const auto dstPath = Social::getBaseContentPath();

    if (cro::FileSystem::directoryExists(dstPath))
    {
        //assume conversion has already been run at some point
        //this is just a catch-all if switching between versions
        //though once 1.15 is released it shouldn't be possible
        //to run 1.14 outside of the non-steam version
        return;
    }

    const std::array FileNames =
    {
        std::string("profiles.tar"),
        std::string("last.gue"),
        std::string("lea.gue"),
        std::string("keys.bind"),
    };

    //make sure the target doesn't yet exist - else
    //chances are this is already done and we're overwriting
    //with old data...
    for (const auto& name : FileNames)
    {
        const auto outPath = dstPath + name;
        const auto inPath = srcPath + name;

        if (!cro::FileSystem::fileExists(outPath))
        {
            //LogI << outPath << " doesn't exist" << std::endl;
            
            if (cro::FileSystem::fileExists(inPath))
            {
                LogI << inPath << " exists - attempting to copy..." << std::endl;

                std::error_code ec;
                std::filesystem::path src = std::filesystem::u8path(inPath);
                std::filesystem::path dst = std::filesystem::u8path(outPath);

                std::filesystem::copy(src, dst, std::filesystem::copy_options::skip_existing | std::filesystem::copy_options::recursive, ec);

                if (ec)
                {
                    LogE << "Failed copying " << inPath << ": " << ec.message() << std::endl;
                }
            }
        }
        else
        {
            //LogI << outPath << " already exists - skipping." << std::endl;
            if (cro::FileSystem::fileExists(inPath))
            {
                std::error_code ec;
                std::filesystem::rename(std::filesystem::u8path(inPath), std::filesystem::u8path(inPath + ".old"), ec);

                if (ec)
                {
                    LogI << "Failed to rename existing file: " << ec.message() << std::endl;
                }
            }
        }
    }

    const std::array DirNames =
    {
        std::string("avatars"),
        std::string("balls"),
        std::string("hair"),
        std::string("music"),
        std::string("profiles"),
    };
    srcPath += "user/";

    for (const auto& dir : DirNames)
    {
        const auto outPath = dstPath + dir;
        const auto inPath = srcPath + dir;

        if (cro::FileSystem::directoryExists(inPath))
        {
            std::error_code ec;
            std::filesystem::path src = std::filesystem::u8path(inPath);
            std::filesystem::path dst = std::filesystem::u8path(outPath);

            std::filesystem::copy(src, dst, std::filesystem::copy_options::skip_existing | std::filesystem::copy_options::recursive, ec);

            if (ec)
            {
                LogE << "Failed copying " << inPath << ": " << ec.message() << std::endl;
            }
            else
            {
                std::filesystem::rename(inPath, inPath + ".old");
            }
        }
    }
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
                else if (name == "swingput_threshold")
                {
                    m_sharedData.swingputThreshold = std::max(ConstVal::MinSwingputThresh, std::min(ConstVal::MaxSwingputThresh, prop.getValue<float>()));
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
                else if (name == "grid_transparency")
                {
                    m_sharedData.gridTransparency = std::max(0.f, std::min(1.f, prop.getValue<float>()));
                }
                else if (name == "tree_quality")
                {
                    std::int32_t quality = std::min(2, std::max(0, prop.getValue<std::int32_t>()));
                    m_sharedData.treeQuality = quality;
                }
                else if (name == "hq_shadows")
                {
                    m_sharedData.hqShadows = prop.getValue<bool>();
                }
                else if (name == "log_benchmark")
                {
                    m_sharedData.logBenchmarks = prop.getValue<bool>();
                }
                else if (name == "show_custom")
                {
                    m_sharedData.showCustomCourses = prop.getValue<bool>();
                }
                else if (name == "show_tutorial")
                {
                    m_sharedData.showTutorialTip = prop.getValue<bool>();
                }
                else if (name == "putting_power")
                {
                    m_sharedData.showPuttingPower = prop.getValue<bool>();
                }
                else if (name == "multisamples")
                {
                    m_sharedData.multisamples = prop.getValue<std::uint32_t>();
                    m_sharedData.antialias = m_sharedData.multisamples != 0;
                }
                else if (name == "use_vibration")
                {
                    m_sharedData.enableRumble = prop.getValue<bool>() ? 1 : 0;
                }
                else if (name == "use_trail")
                {
                    m_sharedData.showBallTrail = prop.getValue<bool>();
                }
                else if (name == "use_beacon_colour")
                {
                    m_sharedData.trailBeaconColour = prop.getValue<bool>();
                }
                else if (name == "fast_cpu")
                {
                    m_sharedData.fastCPU = prop.getValue<bool>();
                }
                else if (name == "clubset")
                {
                    m_sharedData.preferredClubSet = std::clamp(prop.getValue<std::int32_t>(), 0, 2);
                }
                else if (name == "press_hold")
                {
                    m_sharedData.pressHold = prop.getValue<bool>();
                }
            }
        }
    }

    //read keybind bin
    path = Social::getBaseContentPath() + "keys.bind";

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

    //and then the cancel button update
    if (m_sharedData.inputBinding.keys[InputBinding::CancelShot] == SDLK_UNKNOWN)
    {
        m_sharedData.inputBinding.keys[InputBinding::CancelShot] = SDLK_LSHIFT;
    }

    //and the ball spin update
    if (m_sharedData.inputBinding.keys[InputBinding::EmoteMenu] == SDLK_UNKNOWN)
    {
        m_sharedData.inputBinding.keys[InputBinding::EmoteMenu] = SDLK_LCTRL;
    }
    if (m_sharedData.inputBinding.keys[InputBinding::SpinMenu] == SDLK_UNKNOWN)
    {
        m_sharedData.inputBinding.keys[InputBinding::SpinMenu] = SDLK_LALT;
    }

    m_sharedData.inputBinding.clubset = ClubID::DefaultSet;

    if (!cro::FileSystem::directoryExists(Social::getBaseContentPath() + u8"music"))
    {
        cro::FileSystem::createDirectory(Social::getBaseContentPath() + u8"music");
    }

    m_sharedData.m3uPlaylist = std::make_unique<M3UPlaylist>(Social::getBaseContentPath() + "music/");
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
    cfg.addProperty("swingput_threshold").setValue(m_sharedData.swingputThreshold);
    cfg.addProperty("invert_x").setValue(m_sharedData.invertX);
    cfg.addProperty("invert_y").setValue(m_sharedData.invertY);
    cfg.addProperty("show_beacon").setValue(m_sharedData.showBeacon);
    cfg.addProperty("beacon_colour").setValue(m_sharedData.beaconColour);
    cfg.addProperty("imperial_measurements").setValue(m_sharedData.imperialMeasurements);
    cfg.addProperty("grid_transparency").setValue(m_sharedData.gridTransparency);
    cfg.addProperty("tree_quality").setValue(m_sharedData.treeQuality);
    cfg.addProperty("hq_shadows").setValue(m_sharedData.hqShadows);
    cfg.addProperty("log_benchmark").setValue(m_sharedData.logBenchmarks);
    cfg.addProperty("show_custom").setValue(m_sharedData.showCustomCourses);
    cfg.addProperty("show_tutorial").setValue(m_sharedData.showTutorialTip);
    cfg.addProperty("putting_power").setValue(m_sharedData.showPuttingPower);
    cfg.addProperty("multisamples").setValue(m_sharedData.multisamples);
    cfg.addProperty("use_vibration").setValue(m_sharedData.enableRumble == 0 ? false : true);
    cfg.addProperty("use_trail").setValue(m_sharedData.showBallTrail);
    cfg.addProperty("use_beacon_colour").setValue(m_sharedData.trailBeaconColour);
    cfg.addProperty("fast_cpu").setValue(m_sharedData.fastCPU);
    cfg.addProperty("clubset").setValue(m_sharedData.preferredClubSet);
    cfg.addProperty("press_hold").setValue(m_sharedData.pressHold);
    cfg.save(path);


    //keybinds
    path = Social::getBaseContentPath() + "keys.bind";
    cro::RaiiRWops file;
    file.file = SDL_RWFromFile(path.c_str(), "wb");
    if (file.file)
    {
        SDL_RWwrite(file.file, &m_sharedData.inputBinding, sizeof(InputBinding), 1);
    }
}

void GolfGame::loadAvatars()
{
    //if we're updating attept to read existing profiles and convert them
    std::vector<PlayerData> oldProfiles;

    auto path = cro::App::getPreferencePath() + "avatars.cfg";
    cro::ConfigFile cfg;
    if (cro::FileSystem::fileExists(path) &&
        cfg.loadFromFile(path, false))
    {
        const auto& objects = cfg.getObjects();
        for (const auto& obj : objects)
        {
            if (obj.getName() == "avatar")
            {
                auto& profile = oldProfiles.emplace_back();

                const auto& props = obj.getProperties();
                for (const auto& prop : props)
                {
                    const auto& name = prop.getName();
                    if (name == "name")
                    {
                        profile.name = prop.getValue<cro::String>();
                    }
                    else if (name == "ball_id")
                    {
                        auto id = prop.getValue<std::uint32_t>();
                        profile.ballID = id;
                    }
                    else if (name == "hair_id")
                    {
                        auto id = prop.getValue<std::uint32_t>();
                        profile.hairID = id;
                    }
                    else if (name == "skin_id")
                    {
                        auto id = prop.getValue<std::uint32_t>();
                        profile.skinID = id;
                    }
                    else if (name == "flipped")
                    {
                        profile.flipped = prop.getValue<bool>();
                    }

                    else if (name == "flags0")
                    {
                        auto flag = prop.getValue<std::int32_t>() % pc::PairCounts[3];

                        profile.avatarFlags[4] = static_cast<std::uint8_t>(pc::KeyMap[flag][0]);
                        profile.avatarFlags[5] = static_cast<std::uint8_t>(pc::KeyMap[flag][1]);
                    }
                    else if (name == "flags1")
                    {
                        auto flag = prop.getValue<std::int32_t>() % pc::PairCounts[2];
                        
                        profile.avatarFlags[2] = static_cast<std::uint8_t>(pc::KeyMap[flag][0]);
                        profile.avatarFlags[3] = static_cast<std::uint8_t>(pc::KeyMap[flag][1]);
                    }
                    else if (name == "flags2")
                    {
                        auto flag = prop.getValue<std::int32_t>() % pc::PairCounts[1];
                        profile.avatarFlags[1] = static_cast<std::uint8_t>(pc::KeyMap[flag][0]);
                    }
                    else if (name == "flags3")
                    {
                        auto flag = prop.getValue<std::int32_t>() % pc::PairCounts[0];
                        profile.avatarFlags[0] = static_cast<std::uint8_t>(pc::KeyMap[flag][0]);
                    }

                    else if (name == "cpu")
                    {
                        profile.isCPU = prop.getValue<bool>();
                    }
                }
            }
        }
    }

    for (const auto& p : oldProfiles)
    {
        p.saveProfile();
    }

    if (cro::FileSystem::fileExists(path))
    {
        //delete the old file
        std::error_code ec;
        std::filesystem::remove({ path }, ec);
    }

    //parse profile dir and load each profile
    path = Social::getUserContentPath(Social::UserContent::Profile);
    if (!cro::FileSystem::directoryExists(path))
    {
        cro::FileSystem::createDirectory(path);
    }


    std::int32_t i = 0;
#ifdef USE_GNS
    auto uid = Social::getPlayerID();
    
    auto steamPath = path + uid + "/";
    if (!cro::FileSystem::directoryExists(steamPath))
    {
        cro::FileSystem::createDirectory(steamPath);

        PlayerData sPlayer;
        sPlayer.profileID = uid;
        sPlayer.name = Social::getPlayerName();
        sPlayer.saveProfile();

        m_profileData.playerProfiles.push_back(sPlayer);
        i++;
    }
    else
    {
        auto files = cro::FileSystem::listFiles(steamPath);
        files.erase(std::remove_if(files.begin(), files.end(),
            [](const std::string& f)
            {
                return cro::FileSystem::getFileExtension(f) != ".pfl";
            }), files.end());

        if (!files.empty())
        {
            PlayerData pd;
            if (pd.loadProfile(steamPath + files[0], files[0].substr(0, files[0].size() - 4)))
            {
                //always use the current Steam user name
                pd.name = Social::getPlayerName();

                m_profileData.playerProfiles.push_back(pd);
                i++;
            }
        }
    }
    m_profileData.playerProfiles[0].isSteamID = true;
#endif

    auto profileDirs = cro::FileSystem::listDirectories(path);
#ifdef USE_GNS
    //remove the steam profile because we already explicitly parsed it
    profileDirs.erase(std::remove_if(profileDirs.begin(), profileDirs.end(),
        [uid](const std::string& d) 
        {
            return d == uid;
        }), profileDirs.end());

#endif // USE_GNS



    for (const auto& dir : profileDirs)
    {
        auto profilePath = path + dir + "/";
        auto files = cro::FileSystem::listFiles(profilePath);
        files.erase(std::remove_if(files.begin(), files.end(), 
            [](const std::string& f)
            {
                return cro::FileSystem::getFileExtension(f) != ".pfl";
            }), files.end());

        if (!files.empty())
        {
            PlayerData pd;
            if (pd.loadProfile(profilePath + files[0], files[0].substr(0, files[0].size() - 4)))
            {
#ifdef USE_GNS
                //check if we need to convert this to the UID profile
                if (pd.name == Social::getPlayerName())
                {
                    pd.profileID = Social::getPlayerID();
                    pd.saveProfile();
                    m_profileData.playerProfiles[0] = pd;

                    auto copyFiles = cro::FileSystem::listFiles(profilePath);
                    for (const auto& cFile : copyFiles)
                    {
                        if (cro::FileSystem::getFileExtension(cFile) != ".pfl")
                        {
                            auto dbPath = profilePath + cFile;
                            if (cro::FileSystem::fileExists(dbPath))
                            {
                                std::error_code ec;
                                std::filesystem::copy_file(std::filesystem::u8path(dbPath),
                                    std::filesystem::u8path(steamPath + cFile),
                                    std::filesystem::copy_options::update_existing, ec);

                                if (ec)
                                {
                                    LogE << "Failed copying player data, error code: " << ec.value() << std::endl;
                                }
                            }
                        }
                    }
                    //remove the old data so it stops getting sync'd
                    cro::FileSystem::removeDirectory(profilePath);
                }
                else
                {
#endif
                    m_profileData.playerProfiles.push_back(pd);
                    i++;
#ifdef USE_GNS
                }
#endif
            }
        }

        //arbitrary limit on profile loading.
        if (i == ConstVal::MaxProfiles)
        {
            break;
        }
    }

    if (!m_profileData.playerProfiles.empty())
    {
        m_sharedData.localConnectionData.playerData[0] = m_profileData.playerProfiles[0];
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

    float scale = getViewScale(shaderRes);
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
