/*-----------------------------------------------------------------------

Matt Marchant 2023
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

#include "LeaderboardState.hpp"
#include "SharedStateData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "GameConsts.hpp"
#include "TextAnimCallback.hpp"
#include "MessageIDs.hpp"
#include "../GolfGame.hpp"
#ifdef USE_GNS
#include <HallOfFame.hpp>
#endif
#include <crogine/core/Window.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/gui/Gui.hpp>

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

#include <crogine/ecs/InfoFlags.hpp>
#include <crogine/util/Easings.hpp>
#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    struct MenuID final
    {
        enum
        {
            Dummy, Leaderboard,
            MonthSelect
        };
    };

    const std::array<std::string, 13u> PageNames =
    {
        "All Time",
        "January",
        "February",
        "March",
        "April",
        "May",
        "June",
        "July",
        "August",
        "September",
        "October",
        "November",
        "December"
    };

    const std::array<std::string, 3u> HoleNames =
    {
        "18 Holes", "Front 9", "Back 9"
    };

    struct ButtonID final
    {
        enum
        {
            Nearest = 100,
            HoleCount,
            DateRange,
            Close,
            Course,
            HIO,
            Rank,
            Streak,

            NextCourse,
            PrevCourse
        };
    };
}

LeaderboardState::LeaderboardState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State        (ss, ctx),
    m_scene             (ctx.appInstance.getMessageBus(), 256/*, cro::INFO_FLAG_SYSTEMS_ACTIVE*/),
    m_sharedData        (sd),
    m_viewScale         (2.f)
{
    Social::updateHallOfFame();
    parseCourseDirectory();
    buildScene();

    //registerWindow([&]()
    //    {
    //        if (ImGui::Begin("Stats"))
    //        {
    //            ImGui::Text("Hole count %d", m_displayContext.holeCount);
    //        }
    //        ImGui::End();
    //    });
}

//public
bool LeaderboardState::handleEvent(const cro::Event& evt)
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
        case cro::GameController::ButtonLeftShoulder:
            if (m_displayContext.boardIndex == BoardIndex::Course)
            {
                m_displayContext.courseIndex = (m_displayContext.courseIndex + (m_courseStrings.size() - 1)) % m_courseStrings.size();
                Social::refreshHallOfFame(m_courseStrings[m_displayContext.courseIndex].first);
                refreshDisplay();

                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
            break;
        case cro::GameController::ButtonRightShoulder:
            if (m_displayContext.boardIndex == BoardIndex::Course)
            {
                m_displayContext.courseIndex = (m_displayContext.courseIndex + 1) % m_courseStrings.size();
                Social::refreshHallOfFame(m_courseStrings[m_displayContext.courseIndex].first);
                refreshDisplay();

                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
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

void LeaderboardState::handleMessage(const cro::Message& msg)
{
    if (msg.id == Social::MessageID::StatsMessage)
    {
        const auto& data = msg.getData<Social::StatEvent>();
        if (data.type == Social::StatEvent::HOFReceived)
        {
            refreshDisplay();
        }
        /*else if (data.type == Social::StatEvent::StatUnavailable)
        {
            m_displayContext.leaderboardText.getComponent<cro::Text>().setString("No Scores Set");
            m_displayContext.personalBest.getComponent<cro::Text>().setString(" ");
        }*/
    }

    m_scene.forwardMessage(msg);
}

bool LeaderboardState::simulate(float dt)
{
    m_scene.simulate(dt);
    return true;
}

void LeaderboardState::render()
{
    m_scene.render();
}

//private
void LeaderboardState::parseCourseDirectory()
{
    m_resources.textures.setFallbackColour(cro::Colour::Transparent);

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
                cfg.loadFromFile(filePath, false);
                if (auto* prop = cfg.findProperty("title"); prop != nullptr)
                {
                    const auto courseTitle = prop->getValue<std::string>();
                    m_courseStrings.emplace_back(std::make_pair(dir, cro::String::fromUtf8(courseTitle.begin(), courseTitle.end())));


                    filePath = /*coursePath*/"assets/golf/courses/" + dir + "/preview.png";
                    //if this fails we still need the fallback to pad the vector
                    m_courseThumbs.push_back(&m_resources.textures.get(filePath));
                }
            }
        }
    }
}

void LeaderboardState::buildScene()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::UISystem>(mb);
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


    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/leaderboard_browser.spt", m_resources.textures);


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

            if (currTime == 1)
            {
                state = RootCallbackData::FadeOut;
                e.getComponent<cro::Callback>().active = false;

                m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Leaderboard);
                m_scene.setSystemActive<cro::AudioPlayerSystem>(true);
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();


                //apply the selected course data from shared data
                //so we get the current leaderboards eg from the lobby
                m_displayContext.courseIndex = m_sharedData.courseIndex % m_courseStrings.size();
                m_displayContext.holeCount = m_sharedData.holeCount;
                m_displayContext.page = 0;

                m_displayContext.holeText.getComponent<cro::Text>().setString(HoleNames[m_displayContext.holeCount]);
                centreText(m_displayContext.holeText);

                m_displayContext.monthText.getComponent<cro::Text>().setString(PageNames[0]);
                centreText(m_displayContext.monthText);

                Social::refreshHallOfFame(m_courseStrings[m_displayContext.courseIndex].first);
                refreshDisplay();

                if (m_sharedData.baseState == StateID::Clubhouse)
                {
                    //ideally we always want to set this but returning to lobby and
                    //fining the correct tag is a faff. I might do this one day :)
                    Social::setStatus(Social::InfoID::Menu, { "Viewing Leaderboards" });
                }
            }
            break;
        case RootCallbackData::FadeOut:
            currTime = std::max(0.f, currTime - (dt * 2.f));
            e.getComponent<cro::Transform>().setScale(m_viewScale * cro::Util::Easing::easeOutQuint(currTime));
            if (currTime == 0)
            {
                requestStackPop();

                state = RootCallbackData::FadeIn;

                if (m_sharedData.baseState == StateID::Clubhouse)
                {
                    Social::setStatus(Social::InfoID::Menu, { "Clubhouse" });
                }
            }
            break;
        }

    };

    m_rootNode = rootNode;


    //title text
    auto title = m_scene.createEntity();
    title.addComponent<cro::Transform>();
    title.addComponent<cro::Drawable2D>();
    title.addComponent<cro::Sprite>() = spriteSheet.getSprite("title");
    auto origin = title.getComponent<cro::Sprite>().getTextureBounds();
    title.getComponent<cro::Transform>().setOrigin({ origin.width / 2.f, origin.height / 2.f });
    rootNode.getComponent<cro::Transform>().addChild(title.getComponent<cro::Transform>());

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
        [&, rootNode, title](cro::Entity e, float) mutable
    {
        auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
        e.getComponent<cro::Transform>().setScale(size);
        
        size /= 2.f;
        e.getComponent<cro::Transform>().setPosition(size);

        auto scale = rootNode.getComponent<cro::Transform>().getScale().x;
        scale = std::min(1.f, scale / m_viewScale.x);

        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        for (auto& v : verts)
        {
            v.colour.setAlpha(BackgroundAlpha * scale);
        }

        size.y *= 0.73f;
        size.y = std::round(size.y);

        scale = cro::Util::Easing::easeOutBounce(scale);

        title.getComponent<cro::Transform>().setPosition(glm::vec3(0.f, size.y / m_viewScale.y, 0.3f));
        title.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };

   
    //background
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, (bounds.height / 2.f) + 10.f });
    rootNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());
    const float bgCentre = bounds.width / 2.f;
    auto bgNode = entity;


    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);

    //course name
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bgCentre, bounds.height - 26.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Westington Links, Isle Of Pendale");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    centreText(entity);
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_displayContext.courseTitle = entity;

    //personal best
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 182.f, 76.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("No Personal Score");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextHighlightColour);
    centreText(entity);
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_displayContext.personalBest = entity;

    //leaderboard names text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 22.f, 216.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setVerticalSpacing(LeaderboardTextSpacing);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setString("Waiting for data...");
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_displayContext.leaderboardText = entity;

    //leaderboard scores text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 288.f, 216.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setVerticalSpacing(LeaderboardTextSpacing);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_displayContext.leaderboardScores = entity;


    //course thumbnail
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 356.f, 116.f, 0.1f });
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.2125f)); //yeah IDK
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(*m_courseThumbs[0]);
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_displayContext.thumbnail = entity;

    createFlyout(bgNode);


    //tab sprite
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 8.f, 38.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("tabs");
    entity.addComponent<cro::SpriteAnimation>();
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto tabSprite = entity;

    auto& uiSystem = *m_scene.getSystem<cro::UISystem>();

    auto selectedID = uiSystem.addCallback(
        [](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White); 
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Callback>().active = true;
        });
    auto unselectedID = uiSystem.addCallback(
        [](cro::Entity e) 
        { 
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });
    
    //tab buttons
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 50.f, 45.f, 0.2f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("tab_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::UIInput>().enabled = m_displayContext.boardIndex != BoardIndex::Course;
    };
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    auto tabArea = bounds;
    tabArea.left += 2.f;
    tabArea.width -= 4.f;
    entity.addComponent<cro::UIInput>().area = tabArea;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Leaderboard);
    entity.getComponent<cro::UIInput>().setSelectionIndex(ButtonID::Course);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::HIO, ButtonID::HIO);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::Close, ButtonID::Close);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback(
            [&, tabSprite](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_displayContext.boardIndex = BoardIndex::Course;
                    tabSprite.getComponent<cro::SpriteAnimation>().play(BoardIndex::Course);
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    Social::refreshHallOfFame(m_courseStrings[m_displayContext.courseIndex].first);
                    refreshDisplay();
                    updateButtonIndices();
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_buttons[DynamicButton::Course] = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 134.f, 45.f, 0.2f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("tab_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::UIInput>().enabled = m_displayContext.boardIndex != BoardIndex::Hio;
    };
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::UIInput>().area = tabArea;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Leaderboard);
    entity.getComponent<cro::UIInput>().setSelectionIndex(ButtonID::HIO);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::Rank, ButtonID::Rank);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::Close, ButtonID::PrevCourse);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback(
            [&, tabSprite](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_displayContext.boardIndex = BoardIndex::Hio;
                    tabSprite.getComponent<cro::SpriteAnimation>().play(BoardIndex::Hio);
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    Social::refreshGlobalBoard(0);
                    refreshDisplay();
                    updateButtonIndices();
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_buttons[DynamicButton::HIO] = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 218.f, 45.f, 0.2f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("tab_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::UIInput>().enabled = m_displayContext.boardIndex != BoardIndex::Rank;
    };
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::UIInput>().area = tabArea;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Leaderboard);
    entity.getComponent<cro::UIInput>().setSelectionIndex(ButtonID::Rank);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::Streak, ButtonID::Streak);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::HIO, ButtonID::HIO);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback(
            [&, tabSprite](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_displayContext.boardIndex = BoardIndex::Rank;
                    tabSprite.getComponent<cro::SpriteAnimation>().play(BoardIndex::Rank);
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    Social::refreshGlobalBoard(1);
                    refreshDisplay();
                    updateButtonIndices();
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_buttons[DynamicButton::Rank] = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 302.f, 45.f, 0.2f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("tab_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::UIInput>().enabled = m_displayContext.boardIndex != BoardIndex::Streak;
    };
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::UIInput>().area = tabArea;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Leaderboard);
    entity.getComponent<cro::UIInput>().setSelectionIndex(ButtonID::Streak);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::Close, ButtonID::Close);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::Rank, ButtonID::NextCourse);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback(
            [&, tabSprite](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_displayContext.boardIndex = BoardIndex::Streak;
                    tabSprite.getComponent<cro::SpriteAnimation>().play(BoardIndex::Streak);
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    Social::refreshGlobalBoard(2);
                    refreshDisplay();
                    updateButtonIndices();
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_buttons[DynamicButton::Streak] = entity;


    //button to enable nearest scores / refresh
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 365.f, 102.f, 0.1f });
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("checkbox_centre");
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto innerEnt = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 363.f, 100.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("checkbox_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::Callback>().function = HighlightAnimationCallback();
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Leaderboard);
    entity.getComponent<cro::UIInput>().setSelectionIndex(ButtonID::Nearest);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::HoleCount, ButtonID::HoleCount);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::Close, ButtonID::Close);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&, innerEnt](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_displayContext.showNearest = !m_displayContext.showNearest;
                    float scale = m_displayContext.showNearest ? 1.f : 0.f;
                    innerEnt.getComponent<cro::Transform>().setScale(glm::vec2(scale));
                    refreshDisplay();
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //button to set hole count index / refresh
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 424.f, 91.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("18 Holes");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    centreText(entity);
    m_displayContext.holeText = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 423.f, 87.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("flyout_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::Callback>().function = HighlightAnimationCallback();
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), bounds.height / 2.f });
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Leaderboard);
    entity.getComponent<cro::UIInput>().setSelectionIndex(ButtonID::HoleCount);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::DateRange, ButtonID::DateRange);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::Nearest, ButtonID::Nearest);
    bounds.bottom += 4.f;
    bounds.height -= 8.f;
    entity.getComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_displayContext.holeCount = (m_displayContext.holeCount + 1) % 3;
                    m_displayContext.holeText.getComponent<cro::Text>().setString(HoleNames[m_displayContext.holeCount]);
                    centreText(m_displayContext.holeText);
                    refreshDisplay();

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });

    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());




    //button to set month index / refresh
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 424.f, 70.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(PageNames[0]);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    centreText(entity);
    m_displayContext.monthText = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 423.f, 66.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("flyout_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::Callback>().function = HighlightAnimationCallback();
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), bounds.height / 2.f });
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Leaderboard);
    entity.getComponent<cro::UIInput>().setSelectionIndex(ButtonID::DateRange);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::PrevCourse, ButtonID::Close);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::NextCourse, ButtonID::HoleCount);
    bounds.bottom += 4.f;
    bounds.height -= 8.f;
    entity.getComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_flyout.background.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                    //set column/row count for menu
                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::MonthSelect);
                    m_scene.getSystem<cro::UISystem>()->setColumnCount(3);
                    m_scene.getSystem<cro::UISystem>()->selectAt(std::max(12, m_displayContext.page - 1));

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });

    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_buttons[DynamicButton::Date] = entity;


    //close button
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 423.f, 45.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("flyout_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::Callback>().function = HighlightAnimationCallback();
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Leaderboard);
    entity.getComponent<cro::UIInput>().setSelectionIndex(ButtonID::Close);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::Course, ButtonID::Nearest);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::Streak, ButtonID::DateRange);
    entity.getComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    quitState();
                }
            });
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), bounds.height / 2.f });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_buttons[DynamicButton::Close] = entity;

    selectedID = uiSystem.addCallback([](cro::Entity e) 
        {
            e.getComponent<cro::SpriteAnimation>().play(1);
            e.getComponent<cro::AudioEmitter>().play();
        });
    unselectedID = uiSystem.addCallback([](cro::Entity e)
        {
            e.getComponent<cro::SpriteAnimation>().play(0);
        });
    spriteSheet.loadFromFile("assets/golf/sprites/lobby_menu.spt", m_resources.textures);

    auto enableCallback = [&](cro::Entity e, float)
    {
        if (m_displayContext.boardIndex == BoardIndex::Course)
        {
            e.getComponent<cro::UIInput>().enabled = true;
            e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        }
        else
        {
            e.getComponent<cro::UIInput>().enabled = false;
            e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        }
    };

    //prev button - decrement course index and selectCourse()
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 40.f, 68.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("arrow_left");
    entity.addComponent<cro::SpriteAnimation>();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = enableCallback;
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), 0.f });
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Leaderboard);
    entity.getComponent<cro::UIInput>().setSelectionIndex(ButtonID::PrevCourse);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::NextCourse, ButtonID::HIO);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::DateRange, ButtonID::HIO);
    entity.getComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_displayContext.courseIndex = (m_displayContext.courseIndex + (m_courseStrings.size() - 1)) % m_courseStrings.size();
                    Social::refreshHallOfFame(m_courseStrings[m_displayContext.courseIndex].first);
                    refreshDisplay();

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });

    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //next button - increment course index and select course
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 322.f, 68.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("arrow_right");
    entity.addComponent<cro::SpriteAnimation>();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = enableCallback;
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), 0.f });
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Leaderboard);
    entity.getComponent<cro::UIInput>().setSelectionIndex(ButtonID::NextCourse);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::DateRange, ButtonID::Streak);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::PrevCourse, ButtonID::Streak);
    entity.getComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_displayContext.courseIndex = (m_displayContext.courseIndex + 1) % m_courseStrings.size();
                    Social::refreshHallOfFame(m_courseStrings[m_displayContext.courseIndex].first);
                    refreshDisplay();

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });

    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //scrolling text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 100.f, 0.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(Social::getTickerMessage());
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, bounds, bgCentre](cro::Entity e, float dt)
    {
        auto pos = e.getComponent<cro::Transform>().getPosition();
        const auto bgWidth = bgCentre * 2.f;

        pos.x -= 20.f * dt;
        pos.y = 23.f;
        pos.z = 0.3f;

        static constexpr float Offset = 10.f;
        if (pos.x < -bounds.width + Offset)
        {
            pos.x = bgWidth;
            pos.x -= Offset;
        }

        e.getComponent<cro::Transform>().setPosition(pos);


        cro::FloatRect cropping = { -pos.x + Offset, -16.f, bgWidth - (Offset * 2.f), 18.f };
        e.getComponent<cro::Drawable2D>().setCroppingArea(cropping);
    };
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //dummy ent for dummy menu
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Dummy);


    auto updateView = [&, rootNode](cro::Camera& cam) mutable
    {
        glm::vec2 size(GolfGame::getActiveTarget()->getSize());

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        m_viewScale = glm::vec2(getViewScale());
        rootNode.getComponent<cro::Transform>().setScale(m_viewScale);
        rootNode.getComponent<cro::Transform>().setPosition(size / 2.f);

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

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().resizeCallback = updateView;
    m_scene.setActiveCamera(entity);
    updateView(entity.getComponent<cro::Camera>());

    m_scene.simulate(0.f);
    updateButtonIndices();
}

void LeaderboardState::createFlyout(cro::Entity parent)
{
    static constexpr glm::vec2 TextPadding(2.f, 10.f);
    static constexpr glm::vec2 IconSize(68.f, 12.f);
    static constexpr float IconPadding = 4.f;

    static constexpr std::int32_t ColumnCount = 3;
    static constexpr std::int32_t RowCount = 4;

    static constexpr float BgWidth = (ColumnCount * (IconSize.x + IconPadding)) + IconPadding;
    static constexpr float BgHeight = ((RowCount + 1.f) * IconSize.y) + (RowCount * IconPadding) + IconPadding;

    //background
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(138.f, 88.f, 0.2f));
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);

    auto verts = createMenuBackground({ BgWidth, BgHeight });
    entity.getComponent<cro::Drawable2D>().setVertexData(verts);

    m_flyout.background = entity;
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //highlight
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ IconPadding, IconPadding, 0.1f });
    entity.addComponent<cro::Drawable2D>().setVertexData(createMenuHighlight(IconSize, TextGoldColour));
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);

    m_flyout.highlight = entity;
    m_flyout.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //buttons/menu items
    m_flyout.activateCallback = m_scene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt) mutable
        {
            auto quitMenu = [&]()
            {
                m_flyout.background.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Leaderboard);
                m_scene.getSystem<cro::UISystem>()->setColumnCount(1);
            };

            if (activated(evt))
            {
                m_displayContext.page = e.getComponent<cro::Callback>().getUserData<std::uint8_t>();
                m_displayContext.monthText.getComponent<cro::Text>().setString(PageNames[m_displayContext.page]);
                centreText(m_displayContext.monthText);
                refreshDisplay();

                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                quitMenu();
            }
            else if (deactivated(evt))
            {
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

                quitMenu();
            }
        });
    m_flyout.selectCallback = m_scene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e)
        {
            m_flyout.highlight.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition() - glm::vec3(TextPadding, 0.f));
            e.getComponent<cro::AudioEmitter>().play();
        });

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);
    for (auto j = 0u; j < 12u; ++j)
    {
        //the Y order is reversed so that the navigation
        //direction of keys/controller is correct in the grid
        std::size_t x = j % ColumnCount;
        std::size_t y = (RowCount - 1) - (j / ColumnCount);

        glm::vec2 pos = { x * (IconSize.x + IconPadding), y * (IconSize.y + IconPadding) };
        pos += TextPadding;
        pos += IconPadding;

        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(pos, 0.1f));
        entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setString(PageNames[(j + 1)]);
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
        entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });

        entity.addComponent<cro::UIInput>().setGroup(MenuID::MonthSelect);
        entity.getComponent<cro::UIInput>().area = { -TextPadding, IconSize };
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_flyout.selectCallback;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_flyout.activateCallback;
        entity.addComponent<cro::Callback>().setUserData<std::uint8_t>(j + 1);

        m_flyout.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }

    //add 'all' option
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(78.f, 76.f, 0.1f));
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(PageNames[0]);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });

    entity.addComponent<cro::UIInput>().setGroup(MenuID::MonthSelect);
    entity.getComponent<cro::UIInput>().area = { -TextPadding, IconSize };
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_flyout.selectCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_flyout.activateCallback;
    entity.addComponent<cro::Callback>().setUserData<std::uint8_t>(0);

    m_flyout.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void LeaderboardState::refreshDisplay()
{
    const auto refreshGlobalBoard = [&]()
    {
        const auto& entry = Social::getGlobalBoard(m_displayContext.boardIndex - 1);

        if (entry.hasData)
        {
            if (m_displayContext.showNearest)
            {
                if (entry.nearestTenNames.empty())
                {
                    m_displayContext.leaderboardText.getComponent<cro::Text>().setString("No Score Entered");
                    m_displayContext.leaderboardScores.getComponent<cro::Text>().setString(" ");
                }
                else
                {
                    m_displayContext.leaderboardText.getComponent<cro::Text>().setString(entry.nearestTenNames);
                    m_displayContext.leaderboardScores.getComponent<cro::Text>().setString(entry.nearestTenScores);
                }
            }
            else
            {
                if (entry.topTenNames.empty())
                {
                    m_displayContext.leaderboardText.getComponent<cro::Text>().setString("Waiting...");
                    m_displayContext.leaderboardScores.getComponent<cro::Text>().setString(" ");
                }
                else
                {
                    m_displayContext.leaderboardText.getComponent<cro::Text>().setString(entry.topTenNames);
                    m_displayContext.leaderboardScores.getComponent<cro::Text>().setString(entry.topTenScores);
                }
            }
            m_displayContext.personalBest.getComponent<cro::Text>().setString(entry.personalBest);
            centreText(m_displayContext.personalBest);
        }
        else
        {
            m_displayContext.personalBest.getComponent<cro::Text>().setString(" ");
            m_displayContext.leaderboardText.getComponent<cro::Text>().setString("Waiting...");
            m_displayContext.leaderboardScores.getComponent<cro::Text>().setString(" ");
        }
    };

    switch (m_displayContext.boardIndex)
    {
    default:
    case BoardIndex::Course:
    {
        const auto& title = m_courseStrings[m_displayContext.courseIndex].second;

        m_displayContext.courseTitle.getComponent<cro::Text>().setString(title);
        m_displayContext.thumbnail.getComponent<cro::Sprite>().setTexture(*m_courseThumbs[m_displayContext.courseIndex]);

        const auto& entry = Social::getHallOfFame(m_courseStrings[m_displayContext.courseIndex].first, m_displayContext.page, m_displayContext.holeCount);
        
        if (entry.hasData)
        {
            if (m_displayContext.showNearest)
            {
                if (entry.nearestTenNames.empty())
                {
                    m_displayContext.leaderboardText.getComponent<cro::Text>().setString("No Attempt");
                    m_displayContext.leaderboardScores.getComponent<cro::Text>().setString(" ");
                }
                else
                {
                    m_displayContext.leaderboardText.getComponent<cro::Text>().setString(entry.nearestTenNames);
                    m_displayContext.leaderboardScores.getComponent<cro::Text>().setString(entry.nearestTenScores);
                }
            }
            else
            {
                if (entry.topTenNames.empty())
                {
                    m_displayContext.leaderboardText.getComponent<cro::Text>().setString("No Scores");
                    m_displayContext.leaderboardScores.getComponent<cro::Text>().setString(" ");
                }
                else
                {
                    m_displayContext.leaderboardText.getComponent<cro::Text>().setString(entry.topTenNames);
                    m_displayContext.leaderboardScores.getComponent<cro::Text>().setString(entry.topTenScores);
                }
            }

            if (!entry.personalBest.empty())
            {
                m_displayContext.personalBest.getComponent<cro::Text>().setString(entry.personalBest);
            }
            else
            {
                m_displayContext.personalBest.getComponent<cro::Text>().setString("No Personal Best");
            }
            centreText(m_displayContext.personalBest);
        }
        else
        {
            m_displayContext.personalBest.getComponent<cro::Text>().setString(" ");
            m_displayContext.leaderboardText.getComponent<cro::Text>().setString("Waiting...");
            m_displayContext.leaderboardScores.getComponent<cro::Text>().setString(" ");
        }
    }
        break;
    case BoardIndex::Hio:
        m_displayContext.courseTitle.getComponent<cro::Text>().setString("Most Holes In One");
        refreshGlobalBoard();
        break;
    case BoardIndex::Rank:
        m_displayContext.courseTitle.getComponent<cro::Text>().setString("Highest Ranked Players");
        refreshGlobalBoard();
        break;
    case BoardIndex::Streak:
        m_displayContext.courseTitle.getComponent<cro::Text>().setString("Longest Daily Streak");
        refreshGlobalBoard();
        break;

    }
    centreText(m_displayContext.courseTitle);
};

void LeaderboardState::updateButtonIndices()
{
    const auto selectNext = [&](std::int32_t idx)
    {
        auto e = m_scene.createEntity();
        e.addComponent<cro::Callback>().active = true;
        e.getComponent<cro::Callback>().function =
            [&, idx](cro::Entity f, float)
        {
            m_scene.getSystem<cro::UISystem>()->selectByIndex(idx);
            f.getComponent<cro::Callback>().active = false;
            m_scene.destroyEntity(f);
        };
    };

    switch (m_displayContext.boardIndex)
    {
    default: break;
    case BoardIndex::Course:
        m_buttons[DynamicButton::Course].getComponent<cro::UIInput>().setNextIndex(ButtonID::HIO, ButtonID::PrevCourse);
        m_buttons[DynamicButton::Course].getComponent<cro::UIInput>().setPrevIndex(ButtonID::Close, ButtonID::PrevCourse);
        
        m_buttons[DynamicButton::HIO].getComponent<cro::UIInput>().setNextIndex(ButtonID::Rank, ButtonID::PrevCourse);
        m_buttons[DynamicButton::HIO].getComponent<cro::UIInput>().setPrevIndex(ButtonID::Close, ButtonID::PrevCourse);

        m_buttons[DynamicButton::Rank].getComponent<cro::UIInput>().setNextIndex(ButtonID::Streak, ButtonID::Streak);
        m_buttons[DynamicButton::Rank].getComponent<cro::UIInput>().setPrevIndex(ButtonID::HIO, ButtonID::HIO);

        m_buttons[DynamicButton::Streak].getComponent<cro::UIInput>().setNextIndex(ButtonID::Close, ButtonID::NextCourse);
        m_buttons[DynamicButton::Streak].getComponent<cro::UIInput>().setPrevIndex(ButtonID::Rank, ButtonID::NextCourse);

        m_buttons[DynamicButton::Date].getComponent<cro::UIInput>().setNextIndex(ButtonID::PrevCourse, ButtonID::Close);
        m_buttons[DynamicButton::Date].getComponent<cro::UIInput>().setPrevIndex(ButtonID::NextCourse, ButtonID::HoleCount);

        m_buttons[DynamicButton::Close].getComponent<cro::UIInput>().setNextIndex(ButtonID::HIO, ButtonID::Nearest);
        m_buttons[DynamicButton::Close].getComponent<cro::UIInput>().setPrevIndex(ButtonID::Streak, ButtonID::DateRange);

        selectNext(ButtonID::HIO);
        break;
    case BoardIndex::Hio:
        m_buttons[DynamicButton::Course].getComponent<cro::UIInput>().setNextIndex(ButtonID::Rank, ButtonID::Rank);
        m_buttons[DynamicButton::Course].getComponent<cro::UIInput>().setPrevIndex(ButtonID::Close, ButtonID::Close);

        m_buttons[DynamicButton::HIO].getComponent<cro::UIInput>().setNextIndex(ButtonID::Rank, ButtonID::Rank);
        m_buttons[DynamicButton::HIO].getComponent<cro::UIInput>().setPrevIndex(ButtonID::Course, ButtonID::Course);

        m_buttons[DynamicButton::Rank].getComponent<cro::UIInput>().setNextIndex(ButtonID::Streak, ButtonID::Streak);
        m_buttons[DynamicButton::Rank].getComponent<cro::UIInput>().setPrevIndex(ButtonID::Course, ButtonID::Course);
        
        m_buttons[DynamicButton::Streak].getComponent<cro::UIInput>().setNextIndex(ButtonID::Close, ButtonID::Close);
        m_buttons[DynamicButton::Streak].getComponent<cro::UIInput>().setPrevIndex(ButtonID::Rank, ButtonID::Rank);

        m_buttons[DynamicButton::Date].getComponent<cro::UIInput>().setNextIndex(ButtonID::Close, ButtonID::Close);
        m_buttons[DynamicButton::Date].getComponent<cro::UIInput>().setPrevIndex(ButtonID::HoleCount, ButtonID::HoleCount);

        m_buttons[DynamicButton::Close].getComponent<cro::UIInput>().setNextIndex(ButtonID::Course, ButtonID::Nearest);
        m_buttons[DynamicButton::Close].getComponent<cro::UIInput>().setPrevIndex(ButtonID::Streak, ButtonID::DateRange);

        selectNext(ButtonID::Rank);
        break;
    case BoardIndex::Rank:
        m_buttons[DynamicButton::Course].getComponent<cro::UIInput>().setNextIndex(ButtonID::HIO, ButtonID::HIO);
        m_buttons[DynamicButton::Course].getComponent<cro::UIInput>().setPrevIndex(ButtonID::Close, ButtonID::Close);
        
        m_buttons[DynamicButton::HIO].getComponent<cro::UIInput>().setNextIndex(ButtonID::Streak, ButtonID::Streak);
        m_buttons[DynamicButton::HIO].getComponent<cro::UIInput>().setPrevIndex(ButtonID::Course, ButtonID::Course);

        m_buttons[DynamicButton::Rank].getComponent<cro::UIInput>().setNextIndex(ButtonID::Streak, ButtonID::Streak);
        m_buttons[DynamicButton::Rank].getComponent<cro::UIInput>().setPrevIndex(ButtonID::HIO, ButtonID::HIO);

        m_buttons[DynamicButton::Streak].getComponent<cro::UIInput>().setNextIndex(ButtonID::Close, ButtonID::Close);
        m_buttons[DynamicButton::Streak].getComponent<cro::UIInput>().setPrevIndex(ButtonID::HIO, ButtonID::HIO);

        m_buttons[DynamicButton::Date].getComponent<cro::UIInput>().setNextIndex(ButtonID::Close, ButtonID::Close);
        m_buttons[DynamicButton::Date].getComponent<cro::UIInput>().setPrevIndex(ButtonID::HoleCount, ButtonID::HoleCount);

        m_buttons[DynamicButton::Close].getComponent<cro::UIInput>().setNextIndex(ButtonID::Course, ButtonID::Nearest);
        m_buttons[DynamicButton::Close].getComponent<cro::UIInput>().setPrevIndex(ButtonID::Streak, ButtonID::DateRange);

        selectNext(ButtonID::Streak);
        break;
    case BoardIndex::Streak:
        m_buttons[DynamicButton::Course].getComponent<cro::UIInput>().setNextIndex(ButtonID::HIO, ButtonID::HIO);
        m_buttons[DynamicButton::Course].getComponent<cro::UIInput>().setPrevIndex(ButtonID::Close, ButtonID::Close);
        
        m_buttons[DynamicButton::HIO].getComponent<cro::UIInput>().setNextIndex(ButtonID::Rank, ButtonID::Rank);
        m_buttons[DynamicButton::HIO].getComponent<cro::UIInput>().setPrevIndex(ButtonID::Course, ButtonID::Course);

        m_buttons[DynamicButton::Rank].getComponent<cro::UIInput>().setNextIndex(ButtonID::Close, ButtonID::Close);
        m_buttons[DynamicButton::Rank].getComponent<cro::UIInput>().setPrevIndex(ButtonID::HIO, ButtonID::HIO);

        m_buttons[DynamicButton::Streak].getComponent<cro::UIInput>().setNextIndex(ButtonID::Close, ButtonID::Close);
        m_buttons[DynamicButton::Streak].getComponent<cro::UIInput>().setPrevIndex(ButtonID::Rank, ButtonID::Rank);

        m_buttons[DynamicButton::Date].getComponent<cro::UIInput>().setNextIndex(ButtonID::Close, ButtonID::Close);
        m_buttons[DynamicButton::Date].getComponent<cro::UIInput>().setPrevIndex(ButtonID::HoleCount, ButtonID::HoleCount);

        m_buttons[DynamicButton::Close].getComponent<cro::UIInput>().setNextIndex(ButtonID::Course, ButtonID::Nearest);
        m_buttons[DynamicButton::Close].getComponent<cro::UIInput>().setPrevIndex(ButtonID::Rank, ButtonID::DateRange);

        selectNext(ButtonID::Course);
        break;
    }
}

void LeaderboardState::quitState()
{
    //m_scene.setSystemActive<cro::AudioPlayerSystem>(false);
    m_rootNode.getComponent<cro::Callback>().active = true;
    m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}