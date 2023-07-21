/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
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

#include "StatsState.hpp"
#include "SharedStateData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "TextAnimCallback.hpp"
#include "MessageIDs.hpp"
#include "Clubs.hpp"
#include "../GolfGame.hpp"
#include "../Colordome-32.hpp"

#include <Social.hpp>
#include <AchievementIDs.hpp>
#include <AchievementStrings.hpp>
#include <Achievements.hpp>

#include <crogine/core/Window.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/core/SysTime.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>

#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/SpriteAnimator.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>

#include <crogine/util/Easings.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    constexpr float PieRadius = 48.f;
    constexpr std::int32_t PieBaseColour = CD32::BlueLight;
    constexpr glm::vec2 PerformanceBoardArea = glm::vec2(416.f, 88.f);
    constexpr float PerformanceBoardGap = 20.f;
    constexpr float PerformanceVerticalOffset = 3.f; //each graph offset by this to reduce overlap

    //indices into the colour palette for each hole graph
    constexpr std::array<std::int32_t, 18> PerformanceColours =
    {
        CD32::GreenMid,
        CD32::GreenLight,
        CD32::Yellow,
        CD32::TanLight,
        CD32::PinkLight,
        CD32::Pink,
        CD32::Red,
        CD32::OrangeDirt,
        CD32::Orange,

        CD32::Yellow,
        CD32::GreenLight,
        CD32::GreenMid,
        CD32::GreenDark,
        CD32::BlueDark,
        CD32::BlueMid,
        CD32::BlueLight,
        CD32::GreyLight,
        CD32::GreyMid,
    };

    const std::array<cro::String, 5u> RangeStrings =
    {
        "1 Week" , "1 Month", "3 Months", "6 Months" , "1 Year"
    };

    struct GraphFadeData final
    {
        float progress = 0.f;
        float target = 1.f;
        static constexpr float MinAlpha = 0.1f;

        std::int32_t graphIndex = 0;
        cro::String detailString;
    };

    struct MenuID final
    {
        enum
        {
            Main
        };
    };
}

StatsState::StatsState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State            (ss, ctx),
    m_scene                 (ctx.appInstance.getMessageBus(), 480),
    m_sharedData            (sd),
    m_viewScale             (2.f),
    m_currentTab            (0),
    m_imperialMeasurements  (sd.imperialMeasurements),
    m_profileIndex          (0),
    m_courseIndex           (0),
    m_showCPUStat           (true),
    m_holeDetailSelected    (false)
{
    ctx.mainWindow.setMouseCaptured(false);

    buildScene();

#ifdef CRO_DEBUG_
    registerCommand("build_dummy_best", [](const std::string&)
        {
            auto path = Social::getUserContentPath(Social::UserContent::Profile);
            auto dirs = cro::FileSystem::listDirectories(path);
            ProfileDB db;
            std::int32_t recordCount = 0;
            for (const auto& dir : dirs)
            {
                db.open(path + dir + "/profile.db3");
                for (auto i = 0; i < 10; ++i)
                {
                    for (auto j = 0; j < 18; ++j)
                    {
                        PersonalBestRecord pb;
                        pb.course = i;
                        pb.hole = j;
                        pb.longestDrive = static_cast<float>(cro::Util::Random::value(20000, 30000)) / 100.f;
                        pb.longestPutt = static_cast<float>(cro::Util::Random::value(300, 1000)) / 100.f;
                        pb.score = cro::Util::Random::value(1, 6);
                        pb.wasPuttAssist = cro::Util::Random::value(0, 1);
                        db.insertPersonalBestRecord(pb);
                    }
                }
                LogI << "created pb for " << dir << std::endl;
            }
        });
    registerCommand("build_dummy_data", [](const std::string&) 
        {
            auto path = Social::getUserContentPath(Social::UserContent::Profile);
            auto dirs = cro::FileSystem::listDirectories(path);
            ProfileDB db;
            std::int32_t recordCount = 0;
            for (const auto& dir : dirs)
            {
                db.open(path + dir + "/profile.db3");

                //just over a year
                auto ts = cro::SysTime::epoch(); //note by default this is overwritten with current time when inserted to DB
                for (auto i = 0; i < 190; ++i)
                {
                    for (auto j = 0; j < 10; ++j)
                    {
                        CourseRecord record;
                        record.courseIndex = j;
                        record.holeCount = 0;
                        record.timestamp = ts;
                        record.wasCPU = cro::Util::Random::value(0, 1);

                        for (auto& score : record.holeScores)
                        {
                            score = cro::Util::Random::value(2, 4);
                            if (cro::Util::Random::value(0, 16) == 0)
                            {
                                score -= 1;
                            }
                            if (cro::Util::Random::value(0, 60) == 0)
                            {
                                score = 13 - score;
                            }
                            record.total += score;
                        }
                        db.insertCourseRecord(record);
                        recordCount++;
                    }
                    ts -= std::uint64_t(60 * 60 * 24) * 2;
                }
                LogI << "Wrote profile DB for " << dir << std::endl;
            }

            cro::Console::print("Inserted " + std::to_string(recordCount) + " records into " + std::to_string(dirs.size()) + " profiles");
        });
#endif
}

//public
bool StatsState::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse
        || m_rootNode.getComponent<cro::Callback>().active)
    {
        return false;
    }

    if (evt.type == SDL_KEYUP)
    {
        if (evt.key.keysym.sym == SDLK_BACKSPACE
            || evt.key.keysym.sym == SDLK_ESCAPE
            || evt.key.keysym.sym == SDLK_p)
        {
            quitState();
            return false;
        }
    }
    else if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_UP:
        case SDLK_DOWN:
        case SDLK_LEFT:
        case SDLK_RIGHT:
            cro::App::getWindow().setMouseCaptured(true);
            break;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        cro::App::getWindow().setMouseCaptured(true);

        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonB:
        case cro::GameController::ButtonStart:
            quitState();
            return false;
        case cro::GameController::ButtonRightShoulder:
            activateTab((m_currentTab + 1) % TabID::Max);
            break;
        case cro::GameController::ButtonLeftShoulder:
            activateTab((m_currentTab + (TabID::Max - 1)) % TabID::Max);
            break;
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONUP)
    {
        if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            quitState();
            return false;
        }
    }
    else if (evt.type == SDL_CONTROLLERAXISMOTION)
    {
        if (evt.caxis.value > LeftThumbDeadZone)
        {
            cro::App::getWindow().setMouseCaptured(true);
        }
    }
    else if (evt.type == SDL_MOUSEMOTION)
    {
        cro::App::getWindow().setMouseCaptured(false);
    }

    m_scene.getSystem<cro::UISystem>()->handleEvent(evt);
    m_scene.forwardEvent(evt);
    return false;
}

void StatsState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool StatsState::simulate(float dt)
{
    m_scene.simulate(dt);
    return true;
}

void StatsState::render()
{
    m_scene.render();
}

//private
void StatsState::buildScene()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::UISystem>(mb)->setColumnCount(4);
    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::SpriteSystem2D>(mb);
    m_scene.addSystem<cro::SpriteAnimator>(mb);
    m_scene.addSystem<cro::TextSystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::RenderSystem2D>(mb);
    m_scene.addSystem<cro::AudioPlayerSystem>(mb);

    m_scene.setSystemActive<cro::AudioPlayerSystem>(false);

    m_menuSounds.loadFromFile("assets/golf/sound/menu.xas", m_sharedData.sharedResources->audio);
    m_audioEnts[AudioID::Accept] = m_scene.createEntity();
    m_audioEnts[AudioID::Accept].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("accept");
    m_audioEnts[AudioID::Back] = m_scene.createEntity();
    m_audioEnts[AudioID::Back].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("back");


    struct RootCallbackData final
    {
        enum
        {
            FadeIn, FadeOut
        }state = FadeIn;
        float currTime = 0.f;
    };

    auto rootNode = m_scene.createEntity();
    rootNode.addComponent<cro::Transform>();
    rootNode.addComponent<cro::Callback>().active = true;
    rootNode.getComponent<cro::Callback>().setUserData<RootCallbackData>();
    rootNode.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& [state, currTime] = e.getComponent<cro::Callback>().getUserData<RootCallbackData>();

        switch (state)
        {
        default: break;
        case RootCallbackData::FadeIn:
            currTime = std::min(1.f, currTime + (dt * 2.f));
            e.getComponent<cro::Transform>().setScale(m_viewScale * cro::Util::Easing::easeOutQuint(currTime));

            //check if the user switched measurements in the options
            if (m_imperialMeasurements != m_sharedData.imperialMeasurements)
            {
                const float scale = m_sharedData.imperialMeasurements ? 1.f : 0.f;
                m_imperialMeasurements = m_sharedData.imperialMeasurements;

                cro::Command cmd;
                cmd.targetFlags = CommandID::Menu::MetricClub | CommandID::Menu::ImperialClub;
                cmd.action = [scale](cro::Entity e, float)
                {
                    if (e.getComponent<cro::CommandTarget>().ID == CommandID::Menu::ImperialClub)
                    {
                        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
                    }
                    else
                    {
                        e.getComponent<cro::Transform>().setScale(glm::vec2(1.f - scale));
                    }
                };
                m_scene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
            }

            if (currTime == 1)
            {
                state = RootCallbackData::FadeOut;
                e.getComponent<cro::Callback>().active = false;

                m_scene.setSystemActive<cro::AudioPlayerSystem>(true);
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                Social::setStatus(Social::InfoID::Menu, { "Browsing Stats" });
            }
            break;
        case RootCallbackData::FadeOut:
            currTime = std::max(0.f, currTime - (dt * 2.f));
            e.getComponent<cro::Transform>().setScale(m_viewScale * cro::Util::Easing::easeOutQuint(currTime));
            if (currTime == 0)
            {
                requestStackPop();

                state = RootCallbackData::FadeIn;

                Social::setStatus(Social::InfoID::Menu, { "Clubhouse" });
            }
            break;
        }

    };

    m_rootNode = rootNode;


    //quad to darken the screen
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.4f });
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-0.5f, 0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(-0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.5f, -0.5f), cro::Colour::Black)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, rootNode](cro::Entity e, float)
    {
        auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
        e.getComponent<cro::Transform>().setScale(size);
        e.getComponent<cro::Transform>().setPosition(size / 2.f);

        auto scale = rootNode.getComponent<cro::Transform>().getScale().x;
        scale = std::min(1.f, scale / m_viewScale.x);

        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        for (auto& v : verts)
        {
            v.colour.setAlpha(BackgroundAlpha * scale);
        }
    };

   
    //background
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/stats_browser.spt", m_sharedData.sharedResources->textures);

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
    rootNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());

    auto bgNode = entity;

    //tab buttons
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("club_stats_button");
    bgNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());

    m_tabEntity = entity;

    auto selectedID = m_scene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e)
        {
            e.getComponent<cro::Callback>().active = true;
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
        });
    auto unselectedID = m_scene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });


    const float stride = 102.f;
    auto sprite = spriteSheet.getSprite("button_highlight");
    bounds = sprite.getTextureBounds();
    const float offset = 51.f;

    for (auto i = 0; i < TabID::Max; ++i)
    {
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ (stride * i) + offset, 13.f, 0.5f });
        entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
        entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());
        entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = sprite;
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);

        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().enabled = i != m_currentTab;
        entity.getComponent<cro::UIInput>().setSelectionIndex(180 + i); //make sure these are always indexed as if at the bottom
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = 
            m_scene.getSystem<cro::UISystem>()->addCallback(
                [&, i](cro::Entity e, const cro::ButtonEvent& evt) 
                {
                    if (activated(evt))
                    {
                        activateTab(i);
                        e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
                    }                
                });

        entity.addComponent<cro::Callback>().function = MenuTextCallback();

        bgNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());

        m_tabButtons[i] = entity;
    }

    parseCourseData();
    parseProfileData();
    createClubStatsTab(bgNode, spriteSheet);
    createPerformanceTab(bgNode, spriteSheet);
    createHistoryTab(bgNode);
    createAwardsTab(bgNode);

    auto updateView = [&, rootNode](cro::Camera& cam) mutable
    {
        glm::vec2 size(GolfGame::getActiveTarget()->getSize());

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        m_viewScale = glm::vec2(getViewScale());
        rootNode.getComponent<cro::Transform>().setScale(m_viewScale);
        rootNode.getComponent<cro::Transform>().setPosition(size / 2.f);

        glLineWidth(m_viewScale.x);

        //updates any text objects / buttons with a relative position
        cro::Command cmd;
        cmd.targetFlags = CommandID::Menu::UIElement;
        cmd.action =
            [&, size](cro::Entity e, float)
        {
            const auto& element = e.getComponent<UIElement>();
            auto pos = element.absolutePosition;
            pos += element.relativePosition * size / m_viewScale;

            pos.x = std::floor(pos.x);
            pos.y = std::floor(pos.y);

            e.getComponent<cro::Transform>().setPosition(glm::vec3(pos, element.depth));
        };
        m_scene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    };

    entity = m_scene.getActiveCamera();
    entity.getComponent<cro::Camera>().resizeCallback = updateView;
    updateView(entity.getComponent<cro::Camera>());

    m_scene.simulate(0.f);
}

void StatsState::parseCourseData()
{
    const std::string coursePath = cro::FileSystem::getResourcePath() + "assets/golf/courses/";
    auto dirs = cro::FileSystem::listDirectories(coursePath);

    std::sort(dirs.begin(), dirs.end());

    for (const auto& dir : dirs)
    {
        if (dir.find("course_") != std::string::npos)
        {
            auto filePath = coursePath + dir + "/course.data";
            if (cro::FileSystem::fileExists(filePath))
            {
                cro::ConfigFile cfg;
                cfg.loadFromFile(filePath);
                if (auto* prop = cfg.findProperty("title"); prop != nullptr)
                {
                    const auto courseTitle = prop->getValue<std::string>();
                    m_courseStrings.emplace_back(std::make_pair(dir, cro::String::fromUtf8(courseTitle.begin(), courseTitle.end())));
                }
            }
        }
    }
}

void StatsState::parseProfileData()
{
    //this assumes the startup was successful and profile paths were created - we might
    //want to do some checkin here just to prevent crashes if paths don't exist
    auto path = Social::getUserContentPath(Social::UserContent::Profile);
    if (!cro::FileSystem::directoryExists(path))
    {
        return;
    }

    auto profileDirs = cro::FileSystem::listDirectories(path);
    std::int32_t i = 0;
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
            if (pd.loadProfile(profilePath + files[0], files[0].substr(0, files[0].size() - 4))
                && cro::FileSystem::fileExists(profilePath + "profile.db3"))
            {
                auto& pf = m_profileData.emplace_back();
                pf.name = pd.name;
                pf.dbPath = profilePath + "profile.db3";
            }
        }

        //arbitrary limit on profile loading.
        if (i == ConstVal::MaxProfiles)
        {
            break;
        }
    }
}

void StatsState::createClubStatsTab(cro::Entity parent, const cro::SpriteSheet& spriteSheet)
{
    m_tabNodes[TabID::ClubStats] = m_scene.createEntity();
    m_tabNodes[TabID::ClubStats].addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.2f });
    parent.getComponent<cro::Transform>().addChild(m_tabNodes[TabID::ClubStats].getComponent<cro::Transform>());

    const auto centre = parent.getComponent<cro::Sprite>().getTextureBounds().width / 2.f;

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ centre, 36.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("club_stats");
    entity.getComponent<cro::Transform>().move(glm::vec2(-entity.getComponent<cro::Sprite>().getTextureBounds().width / 2.f, 0.f));
    m_tabNodes[TabID::ClubStats].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);

    constexpr glm::vec2 BasePos(76.f, 302.f);
    constexpr glm::vec2 StatSpacing(156.f, 64.f);
    constexpr std::int32_t RowCount = 4;
    constexpr float MaxBarWidth = StatSpacing.x - 56.f;
    constexpr float BarHeight = 10.f;
    constexpr std::array BarColours =
    {
        cro::Colour(0xb83530ff), //red
        cro::Colour(0x467e3eff), //green
        cro::Colour(0x6eb39dff), //blue
        cro::Colour(0xf2cf5cff), //yellow
        cro::Colour(0xec773dff), //orange
        //CD32::Colours[CD32::OrangeDirt]
    };
    struct ColourID final
    {
        enum
        {
            Novice, Expert, Pro, TopSpin, SideSpin
        };
    };

    auto playerLevel = Social::getLevel();
    auto clubFlags = Social::getUnlockStatus(Social::UnlockType::Club);

    const auto createStat = [&](std::int32_t clubID)
    {
        glm::vec2 statPos = BasePos;
        statPos.x += StatSpacing.x * (clubID / RowCount);
        statPos.y -= StatSpacing.y * (clubID % RowCount);

        //max range level 0,1,2 and spin influence
        auto statNode = m_scene.createEntity();
        statNode.addComponent<cro::Transform>().setPosition(statPos);
        m_tabNodes[TabID::ClubStats].getComponent<cro::Transform>().addChild(statNode.getComponent<cro::Transform>());

        //we need to be able to refresh these labels if the user switched
        //to imperial measurements, so make two versions and hide the unneeded one
        cro::String label = Clubs[clubID].getLabel() + "\n";
        label += Clubs[clubID].getDistanceLabel(false, 0) + "\n";
        label += Clubs[clubID].getDistanceLabel(false, 1) + "\n";
        label += Clubs[clubID].getDistanceLabel(false, 2) + "\n";
        label += "Top/Side";

        const float labelScale = m_sharedData.imperialMeasurements ? 0.f : 1.f;

        auto labelEnt = m_scene.createEntity();
        labelEnt.addComponent<cro::Transform>().setScale(glm::vec2(labelScale));
        labelEnt.addComponent<cro::Drawable2D>();
        labelEnt.addComponent<cro::Text>(font).setString(label);
        labelEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        labelEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
        labelEnt.getComponent<cro::Text>().setVerticalSpacing(-1.f);
        labelEnt.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Right);
        labelEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::MetricClub;
        statNode.getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());


        label = Clubs[clubID].getLabel() + "\n";
        label += Clubs[clubID].getDistanceLabel(true, 0) + "\n";
        label += Clubs[clubID].getDistanceLabel(true, 1) + "\n";
        label += Clubs[clubID].getDistanceLabel(true, 2) + "\n";
        label += "Top/Side";

        labelEnt = m_scene.createEntity();
        labelEnt.addComponent<cro::Transform>().setScale(glm::vec2(1.f - labelScale));
        labelEnt.addComponent<cro::Drawable2D>();
        labelEnt.addComponent<cro::Text>(font).setString(label);
        labelEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        labelEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
        labelEnt.getComponent<cro::Text>().setVerticalSpacing(-1.f);
        labelEnt.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Right);
        labelEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::ImperialClub;
        statNode.getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());




        auto unlockLevel = ClubID::getUnlockLevel(clubID);

        label = ((clubFlags & ClubID::Flags[clubID]) == 0) ? "Level " + std::to_string(unlockLevel) : "|";
        label += "\n";

        if ((playerLevel < 15) || (clubFlags & ClubID::Flags[clubID]) == 0)
        {
            auto l = std::max(15, unlockLevel);
            label += "Level " + std::to_string(l) + "\n";
        }
        else
        {
            label += "|\n";
        }
        if ((playerLevel < 30) || (clubFlags & ClubID::Flags[clubID]) == 0)
        {
            label += "Level 30\n";
        }
        else
        {
            label += "|\n";
        }
        //label += "Top/Side";

        labelEnt = m_scene.createEntity();
        labelEnt.addComponent<cro::Transform>().setPosition({ 4.f, -11.f, 0.3f });
        labelEnt.addComponent<cro::Drawable2D>();
        labelEnt.addComponent<cro::Text>(font).setString(label);
        labelEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        labelEnt.getComponent<cro::Text>().setFillColour(CD32::Colours[CD32::BlueLight]);
        labelEnt.getComponent<cro::Text>().setVerticalSpacing(-1.f);
        statNode.getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());


        std::vector<cro::Vertex2D> verts;

        float barPos = -10.f;
        const auto createBar = [&](float width, std::int32_t colourIndex)
        {
            //fudgenstein.
            auto c = BarColours[colourIndex];
            if (colourIndex == 2 && playerLevel < 30
                || colourIndex == 1 && playerLevel < 15)
            {
                c.setAlpha(0.15f);
            }

            if ((clubFlags & ClubID::Flags[clubID]) == 0)
            {
                c.setAlpha(0.15f);
            }

            verts.emplace_back(glm::vec2(0.f, barPos), c);
            verts.emplace_back(glm::vec2(0.f, barPos - BarHeight), c);
            verts.emplace_back(glm::vec2(width, barPos), c);
            verts.emplace_back(glm::vec2(width, barPos), c);
            verts.emplace_back(glm::vec2(0.f, barPos - BarHeight), c);
            verts.emplace_back(glm::vec2(width, barPos - BarHeight), c);

            barPos -= (BarHeight + 1.f);
        };

        //each level
        for (auto i = 0; i < 3; ++i)
        {
            const float barWidth = MaxBarWidth * (Clubs[clubID].getTargetAtLevel(i) / Clubs[ClubID::Driver].getTargetAtLevel(2));
            createBar(std::round(barWidth), i);
        }

        //and spin influence - this bar is slightly different as it's split into two
        auto c = BarColours[ColourID::TopSpin];
        auto d = BarColours[ColourID::SideSpin];
        if ((clubFlags & ClubID::Flags[clubID]) == 0)
        {
            c.setAlpha(0.15f);
            d.setAlpha(0.15f);
        }

        float width = MaxBarWidth * Clubs[clubID].getTopSpinMultiplier();
        verts.emplace_back(glm::vec2(0.f, barPos), c);
        verts.emplace_back(glm::vec2(0.f, barPos - (BarHeight / 2.f)), c);
        verts.emplace_back(glm::vec2(width, barPos), c);
        verts.emplace_back(glm::vec2(width, barPos), c);
        verts.emplace_back(glm::vec2(0.f, barPos - (BarHeight / 2.f)), c);
        verts.emplace_back(glm::vec2(width, barPos - (BarHeight / 2.f)), c);

        barPos -= (BarHeight / 2.f);
        width = MaxBarWidth * Clubs[clubID].getSideSpinMultiplier();
        verts.emplace_back(glm::vec2(0.f, barPos), d);
        verts.emplace_back(glm::vec2(0.f, barPos - (BarHeight / 2.f)), d);
        verts.emplace_back(glm::vec2(width, barPos), d);
        verts.emplace_back(glm::vec2(width, barPos), d);
        verts.emplace_back(glm::vec2(0.f, barPos - (BarHeight / 2.f)), d);
        verts.emplace_back(glm::vec2(width, barPos - (BarHeight / 2.f)), d);


        //draw all the bars with one entity
        auto barEnt = m_scene.createEntity();
        barEnt.addComponent<cro::Transform>().setPosition({ 3.f, 0.f, 0.f });
        barEnt.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
        barEnt.getComponent<cro::Drawable2D>().setVertexData(verts);
        statNode.getComponent<cro::Transform>().addChild(barEnt.getComponent<cro::Transform>());
    };

    for (auto i = 0; i < ClubID::Putter; ++i)
    {
        createStat(i);
    }
}

void StatsState::createPerformanceTab(cro::Entity parent, const cro::SpriteSheet& spriteSheet)
{
    m_tabNodes[TabID::Performance] = m_scene.createEntity();
    m_tabNodes[TabID::Performance].addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.2f });
    m_tabNodes[TabID::Performance].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    parent.getComponent<cro::Transform>().addChild(m_tabNodes[TabID::Performance].getComponent<cro::Transform>());

    const auto centre = parent.getComponent<cro::Sprite>().getTextureBounds().width / 2.f;

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ centre, 56.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("performance");
    entity.getComponent<cro::Transform>().move(glm::vec2(-entity.getComponent<cro::Sprite>().getTextureBounds().width / 2.f, 0.f));
    m_tabNodes[TabID::Performance].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto performanceEnt = entity;

    const auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ centre, 296.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString(m_courseStrings[0].second);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    centreText(entity);
    m_tabNodes[TabID::Performance].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto titleText = entity;
    const auto buttonCallback = [&](cro::Entity e, float)
    {
        e.getComponent<cro::UIInput>().enabled =
            m_tabNodes[TabID::Performance].getComponent<cro::Transform>().getScale().x != 0;
    };
    const auto selectedID = m_scene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::SpriteAnimation>().play(1);
            e.getComponent<cro::AudioEmitter>().play();
        });
    const auto unselectedID = m_scene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e) 
        {
            e.getComponent<cro::SpriteAnimation>().play(0);
        });

    //dummy - 32
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIInput>().enabled = false;
    entity.getComponent<cro::UIInput>().setSelectionIndex(32);
    m_tabNodes[TabID::Performance].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //previous course - 33
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 100.f, 286.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("arrow_left");
    entity.addComponent<cro::SpriteAnimation>();
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setSelectionIndex(33);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [&, titleText](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_courseIndex = (m_courseIndex + (m_courseStrings.size() - 1)) % m_courseStrings.size();
                    titleText.getComponent<cro::Text>().setString(m_courseStrings[m_courseIndex].second);
                    centreText(titleText);

                    refreshPerformanceTab(false);

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = buttonCallback;
    m_tabNodes[TabID::Performance].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //next course - 34 
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 406.f - 13.f, 286.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("arrow_right");
    entity.addComponent<cro::SpriteAnimation>();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setSelectionIndex(34);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [&, titleText](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_courseIndex = (m_courseIndex + 1) % m_courseStrings.size();
                    titleText.getComponent<cro::Text>().setString(m_courseStrings[m_courseIndex].second);
                    centreText(titleText);

                    refreshPerformanceTab(false);

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = buttonCallback;
    m_tabNodes[TabID::Performance].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //dummy - 35
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIInput>().enabled = false;
    entity.getComponent<cro::UIInput>().setSelectionIndex(35);
    m_tabNodes[TabID::Performance].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    const auto spriteSelected = m_scene.getSystem<cro::UISystem>()->addCallback([](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
        });
    const auto spriteUnselected = m_scene.getSystem<cro::UISystem>()->addCallback([](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });

    //each hole highlight button (and dummies) happen here
    //so checkbox index is bumped by 120

    //cpu checkbox - 156
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 13.f, 1.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("checkbox_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setSelectionIndex(156);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = spriteSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = spriteUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_showCPUStat = !m_showCPUStat;
                    refreshPerformanceTab(false);

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = buttonCallback;
    performanceEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //inner checkbox
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 15.f, 3.f, 0.2f });
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, 5.f), CD32::Colours[CD32::Yellow]),
            cro::Vertex2D(glm::vec2(0.f), CD32::Colours[CD32::Yellow]),
            cro::Vertex2D(glm::vec2(5.f), CD32::Colours[CD32::Yellow]),
            cro::Vertex2D(glm::vec2(5.f, 0.f), CD32::Colours[CD32::Yellow])
        });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        float scale = m_showCPUStat ? 1.f : 0.f;
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };
    performanceEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //profile name
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ std::floor(performanceEnt.getComponent<cro::Sprite>().getTextureBounds().width / 2.f), 8.f, 0.1f});
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString(m_profileData.empty() ? "No Profiles" : m_profileData[0].name);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    centreText(entity);
    performanceEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto profileName = entity;


    //previous profile - 157
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 112.f, -2.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("arrow_left");
    entity.addComponent<cro::SpriteAnimation>();
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setSelectionIndex(157);
    if (!m_profileData.empty())
    {
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;

        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            m_scene.getSystem<cro::UISystem>()->addCallback(
                [&, profileName](cro::Entity e, const cro::ButtonEvent& evt) mutable
                {
                    if (activated(evt))
                    {
                        m_profileIndex = (m_profileIndex + (m_profileData.size() - 1)) % m_profileData.size();
                        profileName.getComponent<cro::Text>().setString(m_profileData[m_profileIndex].name);
                        centreText(profileName);

                        refreshPerformanceTab(true);

                        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    }
                });

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function = buttonCallback;
    }
    else
    {
        entity.getComponent<cro::UIInput>().enabled = false;
    }
    performanceEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //next profile - 158
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 320.f, -2.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("arrow_right");
    entity.addComponent<cro::SpriteAnimation>();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setSelectionIndex(158);
    if (!m_profileData.empty())
    {
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;

        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            m_scene.getSystem<cro::UISystem>()->addCallback(
                [&, profileName](cro::Entity e, const cro::ButtonEvent& evt) mutable
                {
                    if (activated(evt))
                    {
                        m_profileIndex = (m_profileIndex + 1) % m_profileData.size();
                        profileName.getComponent<cro::Text>().setString(m_profileData[m_profileIndex].name);
                        centreText(profileName);

                        refreshPerformanceTab(true);

                        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    }
                });

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function = buttonCallback;
    }
    else
    {
        entity.getComponent<cro::UIInput>().enabled = false;
    }
    performanceEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //range button sprite
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 360.f, -2.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("range_button");
    performanceEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto buttonEnt = entity;
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();

    //range button text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 10.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString(RangeStrings[0]);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    centreText(entity);
    buttonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto rangeText = entity;

    //range button - 159
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -2.f, -2.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("range_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setSelectionIndex(159);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = spriteSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = spriteUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [&, rangeText](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_dateRange = (m_dateRange + 1) % DateRange::Count;
                    rangeText.getComponent<cro::Text>().setString(RangeStrings[m_dateRange]);
                    centreText(rangeText);

                    refreshPerformanceTab(false);

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });

    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = buttonCallback;
    buttonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //background for showing personal best when a hole is highlighted
    auto c = CD32::Colours[CD32::TanDarkest];
    c.setAlpha(0.5f);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(HoleDetail::Top, 0.6f));
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, PerformanceBoardArea.y), c),
            cro::Vertex2D(glm::vec2(0.f), c),
            cro::Vertex2D(PerformanceBoardArea, c),
            cro::Vertex2D(glm::vec2(PerformanceBoardArea.x, 0.f), c)
        });
    performanceEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_holeDetail.background = entity;
    //and personal best text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ PerformanceBoardArea.x / 2.f, PerformanceBoardArea.y - 14.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("Hole 1\n\nNo Personal Best");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setVerticalSpacing(2.f);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    m_holeDetail.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_holeDetail.text = entity;



    //create and attach empty ents to hold the graphs, updated by refreshPerformanceTab()
    //and mouse-over buttons to dim them

    const auto fadeFunction = 
        [&](cro::Entity e, float dt)
    {
        e.getComponent<cro::UIInput>().enabled =
            m_tabNodes[TabID::Performance].getComponent<cro::Transform>().getScale().x != 0;

        auto& data = e.getComponent<cro::Callback>().getUserData<GraphFadeData>();
        if (!m_holeDetailSelected)
        {
            data.target = 1.f;
        }

        if (data.target > data.progress)
        {
            data.progress = std::min(data.target, data.progress + (dt * 3.f));
        }
        else if (data.target < data.progress)
        {
            data.progress = std::max(data.target, data.progress - (dt * 3.f));
        }

        float alpha = ((1.f - data.MinAlpha) * data.progress) + data.MinAlpha;
        alpha = std::clamp(alpha, 0.f, 1.f);

        auto& verts = m_graphEntities[data.graphIndex].getComponent<cro::Drawable2D>().getVertexData();
        for (auto& v : verts)
        {
            v.colour.setAlpha(alpha);
        }
    };
    auto holeSelect = m_scene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e)
        {
            //fade out other entities if nothing was selected yet
            for (auto ent : m_holeDetailEntities)
            {
                ent.getComponent<cro::Callback>().getUserData<GraphFadeData>().target = 0.f;
            }

            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::Callback>().getUserData<GraphFadeData>().target = 1.f;
            auto index = e.getComponent<cro::Callback>().getUserData<GraphFadeData>().graphIndex;

            if (index < 9)
            {
                m_holeDetail.background.getComponent<cro::Transform>().setPosition(HoleDetail::Bottom);
            }
            else
            {
                m_holeDetail.background.getComponent<cro::Transform>().setPosition(HoleDetail::Top);
            }
            m_holeDetail.background.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
            m_holeDetail.text.getComponent<cro::Text>().setString(e.getComponent<cro::Callback>().getUserData<GraphFadeData>().detailString);

            m_holeDetailSelected = true;
        });
    auto holeUnselect = m_scene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
            e.getComponent<cro::Callback>().getUserData<GraphFadeData>().target = 0.f;

            m_holeDetail.background.getComponent<cro::Transform>().setScale(glm::vec2(0.f));

            m_holeDetailSelected = false;
        });

    auto selectionIndex = 36;
    glm::vec3 mouseOverPos(-6.f, 201.f, 0.1f);
    bounds = spriteSheet.getSprite("hole_over").getTextureBounds();
    bounds.left -= 3.f;
    bounds.width += 22.f;
    bounds.bottom -= 1.f;
    bounds.height += 2.f;

    float yPos = 155.f;
    for (auto i = 0u; i < 18; ++i)
    {
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 27.f, yPos, 0.2f + (0.01f * i)});
        entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_LINE_STRIP);
        entity.getComponent<cro::Drawable2D>().setVertexData(
            {
                cro::Vertex2D(glm::vec2(0.f), CD32::Colours[PerformanceColours[i]]),
                cro::Vertex2D(glm::vec2(PerformanceBoardArea.x, 0.f), CD32::Colours[PerformanceColours[i]]),
            });

        performanceEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_graphEntities[i] = entity;


        //mouse-over button
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(mouseOverPos);
        entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("hole_over");
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = holeSelect;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = holeUnselect;
        entity.getComponent<cro::UIInput>().setSelectionIndex(selectionIndex);
        GraphFadeData fd;
        fd.graphIndex = i;
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<GraphFadeData>(fd);
        entity.getComponent<cro::Callback>().function = fadeFunction;
        performanceEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_holeDetailEntities[i] = entity;
        selectionIndex++;

        //dummy ents to pack columns
        for (auto d = 0; d < 3; ++d)
        {
            entity = m_scene.createEntity();
            entity.addComponent<cro::Transform>();
            entity.addComponent<cro::UIInput>().enabled = false;
            entity.getComponent<cro::UIInput>().setSelectionIndex(selectionIndex++);
        }

        mouseOverPos.y -= 9.f;
        yPos -= PerformanceVerticalOffset;

        if (i == 8)
        {
            yPos -= 81.f; //gap in front/back area
            mouseOverPos.y -= 27.f;
        }
    }
    //grid lines
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 26.f, 19.f, 0.05f });
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_LINES);
    performanceEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_gridEntity = entity;

    //and the record count text in the middle
    const auto& infoFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ centre, 176.f, 0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(infoFont).setString("No Records Found.");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    centreText(entity);
    m_tabNodes[TabID::Performance].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    m_recordCountEntity = entity;

    refreshPerformanceTab(true);
}

void StatsState::createHistoryTab(cro::Entity parent)
{
    m_tabNodes[TabID::History] = m_scene.createEntity();
    m_tabNodes[TabID::History].addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.2f });
    m_tabNodes[TabID::History].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    parent.getComponent<cro::Transform>().addChild(m_tabNodes[TabID::History].getComponent<cro::Transform>());

    //TODO call for a global refresh when opening menu
    //TODO add message handler to refresh the pie charts when new data received

    //pie charts - number of times personally played a course
    //and aggregated stats from steam of course plays
    for (const auto& [course, _] : m_courseStrings)
    {
        m_pieCharts[0].addValue(Social::getCompletionCount(course, false));
        m_pieCharts[1].addValue(Social::getCompletionCount(course, true));
    }

    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);
    const auto& labelFont = m_sharedData.sharedResources->fonts.get(FontID::Label);

    //our pie
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 111.f, 230.f, 0.2f });
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    m_tabNodes[TabID::History].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_pieCharts[0].setEntity(entity);

    //our pie title
    auto pie = entity;
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, PieRadius + 12.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Your Play Count");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    centreText(entity);
    pie.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //our pie total
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -(PieRadius + 26.f), 10.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(labelFont).setString(std::to_string(static_cast<std::int32_t>(m_pieCharts[0].getTotal())) + "\nRounds");
    entity.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    pie.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //global pie
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 111.f, 100.f, 0.2f });
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    m_tabNodes[TabID::History].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_pieCharts[1].setEntity(entity);

    //global pie title
    pie = entity;
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, PieRadius + 12.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Global Play Count");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    centreText(entity);
    pie.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //global pie total
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -(PieRadius + 26.f), 10.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(labelFont).setString(std::to_string(static_cast<std::int32_t>(m_pieCharts[1].getTotal())) + "\nRounds");
    entity.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    pie.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    m_pieCharts[0].updateVerts();
    m_pieCharts[1].updateVerts();


    //title
    const auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 253.f, 304.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("Course Play History");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    centreText(entity);
    m_tabNodes[TabID::History].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //list of course names and corresponding colours - hmm percentages here
    //are not easily refreshable...

    cro::String nameList;
    cro::String countList;
    std::vector<cro::Vertex2D> verts;
    const float VerticalSpacing = -17.f;
    glm::vec2 vertPos = glm::vec2(0.f);
    constexpr glm::vec2 ColourSize(5.f); //actually half size
    std::int32_t colourIndex = PieBaseColour;

    for (auto i = 0u; i < m_courseStrings.size(); ++i)
    {
        m_playTimeChart.addBar(Achievements::getAvgStat(m_courseStrings[i].first));

        std::stringstream ss;
        ss << std::int32_t(m_pieCharts[0].getValue(i)) << "|" << std::int32_t(m_pieCharts[1].getValue(i));
        countList += ss.str() + "\n";

        nameList += m_courseStrings[i].second + "\n";

        verts.emplace_back(glm::vec2(-ColourSize.x, ColourSize.y) + vertPos, CD32::Colours[colourIndex]);
        verts.emplace_back(-ColourSize + vertPos, CD32::Colours[colourIndex]);
        verts.emplace_back(ColourSize + vertPos, CD32::Colours[colourIndex]);

        verts.emplace_back(ColourSize + vertPos, CD32::Colours[colourIndex]);
        verts.emplace_back(-ColourSize + vertPos, CD32::Colours[colourIndex]);
        verts.emplace_back(glm::vec2(ColourSize.x, -ColourSize.y) + vertPos, CD32::Colours[colourIndex]);

        vertPos.y += VerticalSpacing;
        colourIndex++;
    }

    //course name list
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 248.f, 282.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(labelFont).setString(nameList);
    entity.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setVerticalSpacing(4.f);
    m_tabNodes[TabID::History].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //colours
    auto strEnt = entity;
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -11.f, -5.f, 0.f });
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    entity.getComponent<cro::Drawable2D>().setVertexData(verts);
    strEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //play count list
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 225.f, 282.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(labelFont).setString(countList);
    entity.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setVerticalSpacing(4.f);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Right);
    m_tabNodes[TabID::History].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());





    //bar chart for play time
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 218.f, 98.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("Your Average Play Duration");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    m_tabNodes[TabID::History].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 236.f, 40.f, 0.1f });
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    m_tabNodes[TabID::History].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    
    m_playTimeChart.setEntity(entity);
    m_playTimeChart.updateVerts();

    cro::String timeString;
    const float playTime = m_playTimeChart.getMaxValue();
    if (playTime > 60.f)
    {
        auto secs = static_cast<std::int32_t>(playTime);
        auto mins = secs / 60;
        secs %= 60;

        timeString = std::to_string(mins) + "m " + std::to_string(secs) + "s";
    }
    else
    {
        timeString = std::to_string(static_cast<std::int32_t>(playTime)) + "s";
    }
    timeString += "\n|\n|\n0s";

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 384.f, 84.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(labelFont).setString(timeString);
    entity.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    m_tabNodes[TabID::History].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void StatsState::createAwardsTab(cro::Entity parent)
{
    m_tabNodes[TabID::Awards] = m_scene.createEntity();
    m_tabNodes[TabID::Awards].addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.2f });
    m_tabNodes[TabID::Awards].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    parent.getComponent<cro::Transform>().addChild(m_tabNodes[TabID::Awards].getComponent<cro::Transform>());

    const auto centre = parent.getComponent<cro::Sprite>().getTextureBounds().width / 2.f;

    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ centre, 240.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("No Awards... sad :(");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    centreText(entity);
    m_tabNodes[TabID::Awards].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void StatsState::activateTab(std::int32_t tabID)
{
    //update old
    m_tabButtons[m_currentTab].getComponent<cro::UIInput>().enabled = true;
    m_tabNodes[m_currentTab].getComponent<cro::Transform>().setScale(glm::vec2(0.f));

    //update index
    m_currentTab = tabID;

    //update new
    m_tabButtons[m_currentTab].getComponent<cro::UIInput>().enabled = false;
    m_tabNodes[m_currentTab].getComponent<cro::Transform>().setScale(glm::vec2(1.f));

    //update the button selection graphic
    auto bounds = m_tabEntity.getComponent<cro::Sprite>().getTextureRect();
    bounds.bottom = bounds.height * m_currentTab;
    m_tabEntity.getComponent<cro::Sprite>().setTextureRect(bounds);

    //TODO set column count to make performance menu more intuitive when
    //navigating with a controller / steam deck

    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
}

void StatsState::refreshPerformanceTab(bool newProfile)
{
    if (m_profileData.empty())
    {
        return;
    }

    if (newProfile)
    {
        if (!m_profileDB.open(m_profileData[m_profileIndex].dbPath))
        {
            LogE << "Failed opening " << m_profileData[m_profileIndex].dbPath << std::endl;
            return;
        }
    }

    std::size_t maxPoints = 52; //8px apart

    //set a max number of entries in a given period
    //then average the number of results if greater
    auto ts = cro::SysTime::epoch();
    static constexpr std::uint64_t Day = 60 * 60 * 24;
    switch (m_dateRange)
    {
    default:
    case DateRange::Week:
        ts -= (Day * 7);
        maxPoints /= 2;
        break;
    case DateRange::Month:
        ts -= (Day * 28);
        break;
    case DateRange::Quarter:
        ts -= (Day * 84);
        break;
    case DateRange::Half:
        ts -= (Day * 168);
        break;
    case DateRange::Year:
        ts -= (Day * 365);
        maxPoints *= 2;
        break;
    }

    auto records = m_profileDB.getCourseRecords(m_courseIndex, ts, m_showCPUStat);
    if (records.empty())
    {
        //reset the graph
        for (auto e : m_graphEntities)
        {
            e.getComponent<cro::Drawable2D>().getVertexData().clear();
        }
        m_gridEntity.getComponent<cro::Drawable2D>().getVertexData().clear();
        m_recordCountEntity.getComponent<cro::Text>().setString("No Records Found.");
        centreText(m_recordCountEntity);
        return;
    }
    maxPoints = std::min(records.size(), maxPoints);
    const auto stride = records.size() / maxPoints;

    //TODO this only shows up to half the results if, say, the record count is ~100
    //really we want to take a floating point stride then alternate it by average
    //eg 1/2 1/2 over the course of the record collection


    //TODO build the vert arrays async, then check the results
    //in the process() loop and apply them when found

    const float HorizontalPixelStride = PerformanceBoardArea.x / maxPoints;
    //there's a 12 stroke limit, but each graph is also offset by PerformanceVerticalOffset
    const float VerticalPixelStride = (PerformanceBoardArea.y / 12.f) - PerformanceVerticalOffset; 
    std::vector<cro::Vertex2D> verts;
    std::vector<cro::Vertex2D> gridVerts;
    for (auto j = 0; j < 18; ++j)
    {
        std::int32_t score = 0;
        for (auto i = 0u; i < maxPoints; ++i)
        {
            score = (i * stride < records.size()) ? records[i * stride].holeScores[j] : 0;
            float xPos = PerformanceBoardArea.x - (i * HorizontalPixelStride);
            verts.emplace_back(glm::vec2(xPos, (score * VerticalPixelStride)), CD32::Colours[PerformanceColours[j]]);

            //if we're on the first hole update the grid verts too
            if (j == 0)
            {
                xPos = std::round(xPos);
                gridVerts.emplace_back(glm::vec2(xPos, 0.f), CD32::Colours[CD32::GreyDark]);
                gridVerts.emplace_back(glm::vec2(xPos, PerformanceBoardArea.y), CD32::Colours[CD32::GreyDark]);
                gridVerts.emplace_back(glm::vec2(xPos, PerformanceBoardArea.y + PerformanceBoardGap), CD32::Colours[CD32::GreyDark]);
                gridVerts.emplace_back(glm::vec2(xPos, (PerformanceBoardArea.y * 2.f) + PerformanceBoardGap), CD32::Colours[CD32::GreyDark]);
            }
        }

        verts.emplace_back(glm::vec2(0.f, (score * VerticalPixelStride)), CD32::Colours[PerformanceColours[j]]);

        m_graphEntities[j].getComponent<cro::Drawable2D>().setVertexData(verts);
        verts.clear();
    }
    m_gridEntity.getComponent<cro::Drawable2D>().setVertexData(gridVerts);

    m_recordCountEntity.getComponent<cro::Text>().setString("Previous Hole Scores: " + std::to_string(records.size()) + " Records Available");
    centreText(m_recordCountEntity);


    //personal best info for each hole
    auto personalBest = m_profileDB.getPersonalBest(m_courseIndex);
    for (auto i = 0u; i < m_holeDetailEntities.size(); ++i)
    {
        m_holeDetailEntities[i].getComponent<cro::Callback>().getUserData<GraphFadeData>().detailString = "Hole " +std::to_string(i+1) +"\n\nNo Hole Information";
    }

    if (!personalBest.empty())
    {
        for (const auto& pb : personalBest)
        {
            if (pb.hole >= 0 && pb.hole < 18)
            {
                float drive = pb.longestDrive;
                float putt = pb.longestPutt;
                std::stringstream sd;
                sd << std::fixed << std::setprecision(2);
                std::stringstream sp;
                sp << std::fixed << std::setprecision(2);

                if (m_sharedData.imperialMeasurements)
                {
                    drive *= 1.09361f; //to yards
                    putt *= 1.09361f;
                    putt *= 3.f; //to feet

                    sd << drive << " yards";
                    sp << putt << " feet";
                }
                else
                {
                    sd << drive << " metres";
                    if (putt < 1)
                    {
                        putt *= 100.f;
                        sp << putt << " centimetres";
                    }
                    else
                    {
                        sp << putt << " metres";
                    }
                }

                std::string str = "Hole " + std::to_string(pb.hole + 1);
                str += "\n\nLongest Drive: " + sd.str();
                str += "\nLongest Putt: " + sp.str();
                str += "\nBest Score: " + std::to_string(pb.score);
                if (pb.wasPuttAssist)
                {
                    str += " (Using Putt Assist)";
                }
                m_holeDetailEntities[pb.hole].getComponent<cro::Callback>().getUserData<GraphFadeData>().detailString = str;
            }
        }
    }
}

void StatsState::quitState()
{
    //m_scene.setSystemActive<cro::AudioPlayerSystem>(false);
    m_rootNode.getComponent<cro::Callback>().active = true;
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}


/////-------------------------/////


PieChart::PieChart()
    : m_total(0.f)
{

}

//public
void PieChart::reset()
{
    m_total = 0.f;
    m_values.clear();
}

void PieChart::addValue(float value)
{
    m_total += value;
    m_values.push_back(value);
}

void PieChart::updateVerts()
{
    if (m_total == 0)
    {
        return;
    }

    if (m_entity.isValid()
        && m_entity.hasComponent<cro::Drawable2D>())
    {
        constexpr float SegmentCount = 32.f; //could be more/less as segments per value are rounded
        std::vector<cro::Vertex2D> verts;

        std::int32_t colourIndex = PieBaseColour;

        float currentAngle = 0.f;
        for (auto v : m_values)
        {
            //number of rads this wedge occupies
            const auto percent = (v / m_total);
            const float totalAngle = cro::Util::Const::TAU * percent;

            //number of segments for this wedge
            const float wedgeSegments = std::ceil(SegmentCount * percent);
            const float segmentAngle = totalAngle / wedgeSegments;

            std::vector<glm::vec2> wedgePoints;
            for (auto i = 0.f; i < wedgeSegments; i++)
            {
                wedgePoints.emplace_back(-std::cos(currentAngle), std::sin(currentAngle));
                currentAngle += segmentAngle;
            }
            wedgePoints.emplace_back(-std::cos(currentAngle), std::sin(currentAngle));

            for (auto i = 0; i < wedgePoints.size() - 1; ++i)
            {
                verts.emplace_back(glm::vec2(0.f), CD32::Colours[colourIndex]);
                verts.emplace_back(wedgePoints[i+1] * PieRadius, CD32::Colours[colourIndex]);
                verts.emplace_back(wedgePoints[i] * PieRadius, CD32::Colours[colourIndex]);
            }

            colourIndex++;
            //unlikely to make more that 24 courses - but who knows? :)
            CRO_ASSERT(colourIndex < 32, "Ran out of colours!");
        }

        m_entity.getComponent<cro::Drawable2D>().setVertexData(verts);
    }
}

void PieChart::setEntity(cro::Entity e)
{
    m_entity = e;
}

float PieChart::getPercentage(std::uint32_t index) const
{
    if (m_total != 0)
    {
        return (m_values[index] / m_total) * 100.f;
    }
    return 0.f;
}


/////-----------------/////


BarChart::BarChart()
    : m_maxValue(0.f)
{

}

//public
void BarChart::addBar(float value)
{
    m_maxValue = std::max(value, m_maxValue);
    m_values.push_back(value);
}

void BarChart::updateVerts()
{
    if (m_maxValue != 0
        && m_entity.isValid()
        && m_entity.hasComponent<cro::Drawable2D>())
    {
        constexpr float BarWidth = 12.f;
        constexpr float Stride = BarWidth + 1.f;
        std::int32_t colourIndex = PieBaseColour;

        std::vector<cro::Vertex2D> verts;

        float xPos = 0.f;
        for (auto v : m_values)
        {
            const float barHeight = std::round(MaxHeight * (v / m_maxValue)) + 1.f;

            verts.emplace_back(glm::vec2(xPos + BarWidth, barHeight), CD32::Colours[colourIndex]);
            verts.emplace_back(glm::vec2(xPos, barHeight), CD32::Colours[colourIndex]);
            verts.emplace_back(glm::vec2(xPos, 0.f), CD32::Colours[colourIndex]);

            verts.emplace_back(glm::vec2(xPos, 0.f), CD32::Colours[colourIndex]);
            verts.emplace_back(glm::vec2(xPos + BarWidth, 0.f), CD32::Colours[colourIndex]);
            verts.emplace_back(glm::vec2(xPos + BarWidth, barHeight), CD32::Colours[colourIndex]);

            xPos += Stride;
            colourIndex++;
        }


        m_entity.getComponent<cro::Drawable2D>().setVertexData(verts);
    }
}

void BarChart::setEntity(cro::Entity e)
{
    m_entity = e;
}