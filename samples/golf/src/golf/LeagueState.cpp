/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2024
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

#include "LeagueState.hpp"
#include "SharedStateData.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "TextAnimCallback.hpp"
#include "MessageIDs.hpp"
#include "RandNames.hpp"
#include "Career.hpp"
#include "SharedProfileData.hpp"

#ifdef USE_GNS
#include <DebugUtil.hpp>
#endif

#include <Social.hpp>
#include <AchievementIDs.hpp>
#include <AchievementStrings.hpp>
#include <Achievements.hpp>

#include <crogine/core/Window.hpp>
#include <crogine/core/GameController.hpp>
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

#include <crogine/util/Easings.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

using namespace cl;

namespace
{
#ifdef USE_GNS
    std::vector<ScoreSet> scoreSet;

    const std::array MonthStrings =
    {
        cro::String("January"),
        cro::String("February"),
        cro::String("March"),
        cro::String("April"),
        cro::String("May"),
        cro::String("June"),
        cro::String("July"),
        cro::String("August"),
        cro::String("September"),
        cro::String("October"),
        cro::String("November"),
        cro::String("December"),
    };
#endif

    static constexpr float VerticalSpacing = 13.f;
    static constexpr float TextTop = 268.f;

    struct ScrollData final
    {
        cro::FloatRect bounds = {};
        float xPos = 0.f;
    };

    constexpr std::size_t LeagueButtonIndex = 0;
    constexpr std::size_t InfoButtonIndex = 1;
    constexpr std::size_t RightButtonIndex = 2;
    constexpr std::size_t LeftButtonIndex = RightButtonIndex + LeagueRoundID::Count + 1; //plus online monthly
    constexpr std::size_t CloseButtonIndex = LeftButtonIndex + 1;

    constexpr std::size_t NameButtonIndex = CloseButtonIndex + 5;

    struct MenuID final
    {
        enum
        {
            League, Info
        };
    };

    std::size_t courseIndex = 0;

    bool showStats = false;
}

LeagueState::LeagueState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd, SharedProfileData& sp)
    : cro::State            (ss, ctx),
    m_scene                 (ctx.appInstance.getMessageBus(), 480),
    m_sharedData            (sd),
    m_profileData           (sp),
    m_viewScale             (2.f),
    m_currentTab            (0),
    m_currentLeague         (0),
    m_editName              (false),
    m_activeName            (nullptr)
{
    ctx.mainWindow.setMouseCaptured(false);
#ifdef USE_GNS
    //scoreSet = readGameScores();
#endif

    registerCommand("show_league_stats", [](const std::string&) 
        {
            showStats = !showStats;
        });

    registerWindow([&]()
        {
            if (showStats)
            {
                if (ImGui::Begin("League", &showStats))
                {
                    static League league(LeagueRoundID::Club, m_sharedData);

                    const auto& entries = league.getTable();
                    for (const auto& e : entries)
                    {
                        ImGui::Text("Skill: %d - Curve: %d - Score: %d - Name: %d - Mistake Probability: %d  - Quality: %3.3f",
                            e.skill, e.curve, e.currentScore, e.nameIndex, e.outlier, e.quality * 100.f);
                    }
                    ImGui::Text("Iteration: %d", league.getCurrentIteration());
                    ImGui::SameLine();
                    ImGui::Text("Season: %d", league.getCurrentSeason());
                    ImGui::SameLine();
                    ImGui::Text("Score: %d", league.getCurrentScore());

                    /*if (!scoreSet.empty())
                    {
                        if (ImGui::Button("Iterate"))
                        {
                            league.iterate(scoreSet[courseIndex].par, scoreSet[courseIndex].scores, scoreSet[courseIndex].scores.size());
                            courseIndex = (courseIndex + 1) % scoreSet.size();
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Reset"))
                        {
                            league.reset();
                        }

                        if (ImGui::Button("Run 10 Seasons"))
                        {
                            for (auto i = 0; i < 10; ++i)
                            {
                                for (auto j = 0; j < League::MaxIterations; ++j)
                                {
                                    league.iterate(scoreSet[courseIndex].par, scoreSet[courseIndex].scores, scoreSet[courseIndex].scores.size());
                                    courseIndex = (courseIndex + 1) % scoreSet.size();
                                }
                            }
                        }
                    }*/
                }
                ImGui::End();
            }
        });

    registerWindow([&]()
        {
            const glm::vec2 WindowSize = glm::vec2(200.f, 80.f) * m_viewScale.x;
            const auto WindowPos = (glm::vec2(cro::App::getWindow().getSize()) - WindowSize) / 2.f;

            const auto acceptInput = [&]() 
                {
                    if (m_nameBuffer[0] != 0)
                    {
                        *m_activeName = cro::String::fromUtf8(m_nameBuffer.data(), m_nameBuffer.data() + std::strlen(m_nameBuffer.data()));
                        *m_activeName = m_activeName->substr(0, ConstVal::MaxStringChars);

                        if (m_activeName == &m_profileData.playerProfiles[0].name)
                        {
                            m_profileData.playerProfiles[0].saveProfile();
                            Social::setPlayerName(*m_activeName); //this raises a message to refresh any text which uses the name string
                        }
                        else
                        {
                            m_sharedData.leagueNames.write();
                            refreshAllNameLists();
                        }                        

                        m_activeName = nullptr;
                    }
                    m_nameBuffer[0] = 0;
                    m_editName = false;

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                };

            if (m_editName)
            {
                ImGui::SetNextWindowSize({ WindowSize.x, /*WindowSize.y*/0.f });
                ImGui::SetNextWindowPos({ WindowPos.x, WindowPos.y });

                ImGui::GetFont()->Scale *= m_viewScale.x;
                ImGui::PushFont(ImGui::GetFont());

                ImGui::Begin("Enter Name", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
                //ImGui::SetKeyboardFocusHere(); //hm, we want to only trigger this when the window first opens, but that requires faff tracking state
                ImGui::PushItemWidth(176.f * m_viewScale.x);
                if (ImGui::InputText("##", m_nameBuffer.data(), m_nameBuffer.size(), ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    acceptInput();
                }
                ImGui::PopItemWidth();
                if (ImGui::Button("OK ##", {88.f * m_viewScale.x, 0.f}))
                {
                    acceptInput();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", {88.f * m_viewScale.x, 0.f}))
                {
                    m_nameBuffer[0] = 0;
                    m_editName = false;
                    m_activeName = nullptr;

                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
                ImGui::End();

                ImGui::GetFont()->Scale = 1.f;
                ImGui::PopFont();
            }
        });

    buildScene();
}

//public
bool LeagueState::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse
        || m_rootNode.getComponent<cro::Callback>().active)
    {
        return false;
    }

    //name edit box is open
    if (m_editName)
    {
        const auto quitEdit = [&]()
            {
                m_editName = false;
                m_activeName = nullptr;
                m_nameBuffer[0] = 0;

                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            };

        switch (evt.type)
        {
        default: break;
        case SDL_CONTROLLERBUTTONUP:
            if (evt.cbutton.button == cro::GameController::ButtonB)
            {
                quitEdit();
            }
            break;
        case SDL_KEYUP:
            if (evt.key.keysym.sym == SDLK_ESCAPE)
            {
                quitEdit();
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (evt.button.button == SDL_BUTTON_RIGHT)
            {
                quitEdit();
            }
            break;
        }

        return false;
    }

    const auto nextTab = [&]() 
        {
            if (m_currentTab == TabID::League)
            {
                switchLeague(Page::Forward);
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        };
    const auto prevTab = [&]() 
        {
            if (m_currentTab == TabID::League)
            {
                switchLeague(Page::Back);
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
        };

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
            nextTab();
            break;
        case cro::GameController::ButtonLeftShoulder:
            prevTab();
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
    else if (evt.type == SDL_MOUSEWHEEL)
    {
        if (evt.wheel.y > 0)
        {
            prevTab();
        }
        else if (evt.wheel.y < 0)
        {
            nextTab();
        }
    }

    m_scene.getSystem<cro::UISystem>()->handleEvent(evt);
    m_scene.forwardEvent(evt);
    return false;
}

void LeagueState::handleMessage(const cro::Message& msg)
{
    if (msg.id == Social::MessageID::SocialMessage)
    {
        const auto& data = msg.getData<Social::SocialEvent>();
        if (data.type == Social::SocialEvent::PlayerNameChanged)
        {
            refreshAllNameLists();
        }
    }
#ifdef USE_GNS
    else if (msg.id == Social::MessageID::StatsMessage)
    {
        const auto& data = msg.getData<Social::StatEvent>();
        if (data.type == Social::StatEvent::LeagueReceived)
        {
            updateLeagueText();
        }
    }
    else if (msg.id == cro::Message::StateMessage)
    {
        const auto& data = msg.getData<cro::Message::StateEvent>();
        if (data.action == cro::Message::StateEvent::Pushed
            && data.id == StateID::League)
        {
            updateLeagueText();
        }
    }
#endif

    m_scene.forwardMessage(msg);
}

bool LeagueState::simulate(float dt)
{
    m_scene.simulate(dt);
    return true;
}

void LeagueState::render()
{
    m_scene.render();
}

//private
void LeagueState::buildScene()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::UISystem>(mb)->setMouseScrollNavigationEnabled(false);
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
    m_audioEnts[AudioID::No] = m_scene.createEntity();
    m_audioEnts[AudioID::No].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("nope");


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

                m_scene.setSystemActive<cro::AudioPlayerSystem>(true);
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                if (m_sharedData.baseState == StateID::Clubhouse)
                {
                    Social::setStatus(Social::InfoID::Menu, { "Browsing League Table" });
                }
#ifdef USE_GNS
                //remote steam list
                updateLeagueText();
#endif
                //in case we changed our profile name
                refreshAllNameLists();

                activateTab(TabID::League);
                m_leagueNodes[m_currentLeague].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                m_currentLeague = m_sharedData.leagueTable;
                m_leagueNodes[m_currentLeague].getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                m_scene.getSystem<cro::UISystem>()->selectAt(CloseButtonIndex);
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
    spriteSheet.loadFromFile("assets/golf/sprites/league_table.spt", m_sharedData.sharedResources->textures);

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
    rootNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());

    auto bgNode = entity;

    m_tabNodes[TabID::League] = m_scene.createEntity();
    m_tabNodes[TabID::League].addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.2f });
    bgNode.getComponent<cro::Transform>().addChild(m_tabNodes[TabID::League].getComponent<cro::Transform>());

    m_tabNodes[TabID::Info] = m_scene.createEntity();
    m_tabNodes[TabID::Info].addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.2f });
    m_tabNodes[TabID::Info].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    bgNode.getComponent<cro::Transform>().addChild(m_tabNodes[TabID::Info].getComponent<cro::Transform>());


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
    auto closeCallback = m_scene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                quitState();
            }
        });

    const auto createCloseButton = [&](std::size_t group)
        {
            //close button (we have one for each menu group)
            auto entity = m_scene.createEntity();
            entity.addComponent<cro::Transform>().setPosition({ 468.f, 331.f, 0.1f });
            entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("close_highlight");
            entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
            auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
            entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
            entity.addComponent<cro::UIInput>().area = bounds;
            entity.getComponent<cro::UIInput>().setSelectionIndex(CloseButtonIndex);
            entity.getComponent<cro::UIInput>().setGroup(group);
            entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
            entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
            entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = closeCallback;
            entity.addComponent<cro::Callback>().function = MenuTextCallback();
            return entity;
        };
    auto leagueClose = createCloseButton(MenuID::League);
    leagueClose.getComponent<cro::UIInput>().setNextIndex(RightButtonIndex, RightButtonIndex);
    leagueClose.getComponent<cro::UIInput>().setNextIndex(RightButtonIndex, InfoButtonIndex);
    m_tabNodes[TabID::League].getComponent<cro::Transform>().addChild(leagueClose.getComponent<cro::Transform>());

    auto infoClose = createCloseButton(MenuID::Info);
    infoClose.getComponent<cro::UIInput>().setNextIndex(LeagueButtonIndex, LeagueButtonIndex);
    infoClose.getComponent<cro::UIInput>().setPrevIndex(LeagueButtonIndex, LeagueButtonIndex);
    m_tabNodes[TabID::Info].getComponent<cro::Transform>().addChild(infoClose.getComponent<cro::Transform>());



    //tab buttons
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("league_button");
    bgNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());

    m_tabEntity = entity;


    const float stride = 102.f;
    auto sprite = spriteSheet.getSprite("button_highlight");   
    const float offset = 153.f;


    const auto createTabButton = [&](std::size_t groupID, std::int32_t targetMenu)
        {
            //tab button
            auto entity = m_scene.createEntity();
            entity.addComponent<cro::Transform>().setPosition({ (stride * targetMenu) + offset, 19.f, 0.5f });
            entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Sprite>() = sprite;
            entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);

            auto bounds = sprite.getTextureBounds();
            entity.addComponent<cro::UIInput>().area = bounds;
            entity.getComponent<cro::UIInput>().setSelectionIndex(groupID);
            entity.getComponent<cro::UIInput>().setGroup(groupID);
            entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
            entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
            entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
                m_scene.getSystem<cro::UISystem>()->addCallback(
                    [&, targetMenu](cro::Entity e, const cro::ButtonEvent& evt)
                    {
                        if (activated(evt))
                        {
                            activateTab(targetMenu);
                            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
                        }
                    });

            entity.addComponent<cro::Callback>().function = MenuTextCallback();
            entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
            entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());
            bgNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());

            return entity;
        };

    m_tabButtons[InfoButtonIndex] = createTabButton(MenuID::League, MenuID::Info);
    m_tabButtons[InfoButtonIndex].getComponent<cro::UIInput>().setNextIndex(RightButtonIndex, CloseButtonIndex);
    m_tabButtons[InfoButtonIndex].getComponent<cro::UIInput>().setPrevIndex(NameButtonIndex + 15, NameButtonIndex + 15);

    m_tabButtons[LeagueButtonIndex] = createTabButton(MenuID::Info, MenuID::League);
    m_tabButtons[LeagueButtonIndex].getComponent<cro::UIInput>().setNextIndex(InfoButtonIndex, CloseButtonIndex);
    m_tabButtons[LeagueButtonIndex].getComponent<cro::UIInput>().setPrevIndex(InfoButtonIndex, CloseButtonIndex);


    bool unlocked = true;
    for (auto i = 0; i < LeagueRoundID::Count; ++i)
    {
        if (unlocked)
        {
            unlocked = createLeagueTab(bgNode, spriteSheet, i);
        }
    }
#ifdef USE_GNS
    createGlobalLeagueTab(bgNode, spriteSheet);
    m_leagueNodes[LeagueID::Club].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    m_currentLeague = LeagueID::Global;
#endif
    addLeagueButtons(spriteSheet);
    createInfoTab(bgNode);

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

    m_scene.simulate(0.f); //updates all the group IDs

    //makes sure to set correct selection indices
    activateTab(m_currentTab);
}

bool LeagueState::createLeagueTab(cro::Entity parent, const cro::SpriteSheet& spriteSheet, std::int32_t leagueIndex)
{
    League league(leagueIndex, m_sharedData);

    m_leagueNodes[leagueIndex] = m_scene.createEntity();
    m_tabNodes[TabID::League].getComponent<cro::Transform>().addChild(m_leagueNodes[leagueIndex].addComponent<cro::Transform>());
    if (leagueIndex > 0)
    {
        //default to hidden
        m_leagueNodes[leagueIndex].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    }

    const auto centre = parent.getComponent<cro::Sprite>().getTextureBounds().width / 2.f;

    const auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);

    static const std::array<std::string, LeagueRoundID::Count> Titles =
    {
        "The Club League",
        "Career League One",
        "Career League Two",
        "Career League Three",
        "Career League Four",
        "Career League Five",
        "Career League Six",
    };

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ centre, 298.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString(Titles[leagueIndex]);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    centreText(entity);
    m_leagueNodes[leagueIndex].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //stripes
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ centre, 61.f, 0.05f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("stripes");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, 0.f });
    m_leagueNodes[leagueIndex].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto stripeEnt = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 4.f, 219.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("Rank");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    stripeEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto season = league.getCurrentSeason();
    auto gamesPlayed = league.getCurrentIteration();

    auto statusString = "Season " + std::to_string(season) + " - Games played: " + std::to_string(gamesPlayed) + "/" + std::to_string(league.getMaxIterations());
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ centre, 52.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString(statusString);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    centreText(entity);
    m_leagueNodes[leagueIndex].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    const auto& entries = league.getSortedTable();


    //displays the player's change in position since the last round
    float stripePos = 0.f;

    //TODO we could optimise this a bit by batching into a vertex array...
    glm::vec3 spritePos(68.f, TextTop - 12.f, 0.1f);
    auto z = 0; //track our position so we can highlight the name
    for (const auto& entry : entries)
    {
        if (entry.name == -1)
        {
            stripePos = (VerticalSpacing * (entries.size() - (z+ 1)));
        }

        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(spritePos);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("progress");
        entity.addComponent<cro::SpriteAnimation>().play(entry.score == 0 ? 1 : entry.positionChange);
        m_leagueNodes[leagueIndex].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        spritePos.y -= VerticalSpacing;
        z++;
    }
    
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, stripePos + 1.f, 0.01f }); //vertical pad border
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, VerticalSpacing), TextGoldColour),
            cro::Vertex2D(glm::vec2(0.f), TextGoldColour),
            cro::Vertex2D(glm::vec2(bounds.width, VerticalSpacing), TextGoldColour),
            cro::Vertex2D(glm::vec2(bounds.width, 0.f), TextGoldColour),
        });
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLE_STRIP);
    stripeEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //name column
    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Label);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 86.f, TextTop + 1.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
    m_leagueNodes[leagueIndex].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_nameLists[leagueIndex] = entity;


    //points column
    cro::String str;
    for (const auto& e : entries)
    {
        if (e.score < 100)
        {
            str += " ";
        }
        if (e.score < 10)
        {
            str += " ";
        }
        str += std::to_string(e.score) + " Points\n";
    }

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 368.f, TextTop + 1.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString(str);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
    m_leagueNodes[leagueIndex].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //check if a previous season exists and scroll the results (updated by refreshNameList())
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 15.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString(/*str*/" ");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });

    entity.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
    m_leagueNodes[leagueIndex].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<ScrollData>();
    entity.getComponent<cro::Callback>().function =
        [&, leagueIndex](cro::Entity e, float dt)
        {
            float scale = m_leagueNodes[leagueIndex].getComponent<cro::Transform>().getScale().x;
            if (scale == 0)
            {
                e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            }
            else
            {
                e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                auto& [bounds, xPos] = e.getComponent<cro::Callback>().getUserData<ScrollData>();
                xPos -= (dt * 50.f);

                static constexpr float BGWidth = 494.f;

                if (xPos < (-bounds.width))
                {
                    xPos = BGWidth;
                }

                auto pos = e.getComponent<cro::Transform>().getPosition();
                pos.x = std::round(xPos);

                e.getComponent<cro::Transform>().setPosition(pos);

                auto cropping = bounds;
                cropping.left = -pos.x;
                cropping.left += 6.f;
                cropping.width = BGWidth;
                e.getComponent<cro::Drawable2D>().setCroppingArea(cropping);
            }
        };
    m_nameScrollers[leagueIndex] = entity;

    refreshNameList(leagueIndex, league);

    return leagueIndex < LeagueRoundID::RoundOne || league.getCurrentBest() < 4;
}

void LeagueState::createInfoTab(cro::Entity parent)
{
    const auto centre = parent.getComponent<cro::Sprite>().getTextureBounds().width / 2.f;

    const auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ centre, 298.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
#ifdef USE_GNS
    entity.addComponent<cro::Text>(largeFont).setString("League Rules");
#else
    entity.addComponent<cro::Text>(largeFont).setString("The Club League");
#endif
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    centreText(entity);
    m_tabNodes[TabID::Info].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


#ifdef USE_GNS
    constexpr float vertPos = 292.f;
    std::string desc = R"(
Every Monthly Best score on the Steam Leaderboards contributes to the Global League.

The Global League is created from the sum of all Steam players Monthly Best scores and
the season lasts one calendar month, composed of 36 rounds. Beating an existing personal
Monthly Best (for the current month) will also improve your existing league score.

Every Stroke Play or Stableford round you play contributes to the Club League. Each Career
round you play contributes to the selected Career League. These leagues are split into 
seasons, with 24 rounds played for each Club season, and 6 rounds for each Career season.
The Club and Career results are entirely offline, and opponents are fictional.

At the end of a round your scores are converted using the Stableford scoring system with
par being worth 2 points and 1 point extra awarded for each stroke under par. Scores for
each round are summed to give the total score for the current season.

In the offline leagues other player scores are automatically calculated based on their own
stats, randomly generated when the first season begins. Think of it as something like a
fantasy football league. At the end of each season these stats are re-evaluated based
on the previous season's performance. Some players will increase in skill, others will
decrease. The offline leagues are reset if you reset your player progress at any time.
)";
#else
    constexpr float vertPos = 284.f;
    std::string desc = R"(
Every Stroke Play or Stableford round you play contributes to the Club League. Each
Career round you play contributes to the selected Career League. The leagues are split
into seasons, with 24 rounds played for each Club season, and 6 rounds for each Career
season. At the end of a season the top three players win an award.

At the end of a round your scores are converted using the Stableford scoring system with
par being worth 2 points and 1 point extra awarded for each stroke under par. Scores for
each round are summed to give the total score for the current season.

Other player scores are automatically calculated based on their own stats, randomly
generated when the first season begins. Think of it as something like a fantasy football
league. At the end of each season these stats are re-evaluated based on the previous
season's performance. Some players will increase in skill, others will decrease.

Can you top the every league table by the end of the season? Good luck!

(Note: the leagues are reset if you reset your player progress at any time.)
)";
#endif

    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ centre, vertPos, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString(desc);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    m_tabNodes[TabID::Info].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void LeagueState::refreshNameList(std::int32_t leagueIndex, const League& league)
{
    if (m_nameLists[leagueIndex].isValid())
    {
        const auto& entries = league.getSortedTable();

        cro::String playerName;
        if (Social::isAvailable())
        {
            playerName = Social::getPlayerName();
        }
        else
        {
            playerName = m_sharedData.localConnectionData.playerData[0].name;
        }

        cro::String str;
        for (auto i = 0u; i < entries.size(); ++i)
        {
            if (i < 9)
            {
                str += " ";
            }

            str += std::to_string(i + 1);
            str += ". ";

            if (entries[i].name > -1)
            {
                str += m_sharedData.leagueNames[entries[i].name];
            }
            else
            {
                str += playerName;
            }
            str += "\n";
        }
        m_nameLists[leagueIndex].getComponent<cro::Text>().setString(str);


        str = league.getPreviousResults(playerName);
        if (str.empty())
        {
            str = " ";
            //LogI << "String is empty" << std::endl;
        }
        m_nameScrollers[leagueIndex].getComponent<cro::Text>().setString(str);
        auto bounds = cro::Text::getLocalBounds(m_nameScrollers[leagueIndex]);
        m_nameScrollers[leagueIndex].getComponent<cro::Callback>().getUserData<ScrollData>().bounds = bounds;
    }
}

void LeagueState::refreshAllNameLists()
{
    for (auto i = 1; i < LeagueRoundID::Count; ++i)
    {
        const auto& league = Career::instance(m_sharedData).getLeagueTables()[i - 1];
        refreshNameList(i, league);
    }

    League l(LeagueID::Club, m_sharedData);
    refreshNameList(LeagueID::Club, l);
}

#ifdef USE_GNS
void LeagueState::createGlobalLeagueTab(cro::Entity parent, const cro::SpriteSheet& spriteSheet)
{
    m_leagueNodes[LeagueID::Global] = m_scene.createEntity();
    m_tabNodes[TabID::League].getComponent<cro::Transform>().addChild(m_leagueNodes[LeagueID::Global].addComponent<cro::Transform>());

    const auto centre = parent.getComponent<cro::Sprite>().getTextureBounds().width / 2.f;

    const auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ centre, 298.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("Global League for " + MonthStrings[Social::getMonth()]);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    centreText(entity);
    m_leagueNodes[LeagueID::Global].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //stripes
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ centre, 61.f, 0.05f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("stripes");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, 0.f });
    m_leagueNodes[LeagueID::Global].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto stripeEnt = entity;
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 4.f, 219.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("P/Cm");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    stripeEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //cro::String str(" 1/36\n23/36\n 7/36\n33/36\n 1/36\n23/36\n 7/36\n33/36\n 1/36\n23/36\n 7/36\n33/36\n 1/36\n23/36\n 7/36\n33/36");
    const auto& str = Social::getMonthlyLeague();
    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Label);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 68.f, TextTop + 1.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString(str[2]);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
    m_leagueNodes[LeagueID::Global].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_leagueText.games = entity;


    //str = "Big\nJim\nBeef\nFrank\nSteve\nMelissa\nJean\nSavoury\nRegina Philange Banana Hammock";
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 114.f, TextTop + 1.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString(str[0]);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
    m_leagueNodes[LeagueID::Global].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_leagueText.names = entity;

    //str = "  1 Point\n 20 Points\n120 Points\n  1 Point\n 20 Points\n120 Points\n  1 Point\n 20 Points\n120 Points\n  1 Point\n 20 Points\n120 Points\n  1 Point\n 20 Points\n120 Points\n123 Points";
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 437.f, TextTop + 1.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString(str[1]);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Right);
    m_leagueNodes[LeagueID::Global].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_leagueText.scores = entity;

    //auto statusString = "PLAYER SCORE HERE";
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ centre, 52.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString(str[3]);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    centreText(entity);
    m_leagueNodes[LeagueID::Global].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_leagueText.personal = entity;


    //text scroller
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 15.f, 0.25f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("-~-");// .setString("this is a test string. nothing to see here people, move along.");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });

    entity.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
    m_leagueNodes[LeagueID::Global].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            const auto bounds = cro::Text::getLocalBounds(e);
            float scale = m_leagueNodes[LeagueID::Global].getComponent<cro::Transform>().getScale().x;
            if (scale == 0)
            {
                e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            }
            else
            {
                e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                float& xPos = e.getComponent<cro::Callback>().getUserData<float>();
                xPos -= (dt * 50.f);

                static constexpr float BGWidth = 494.f;

                if (xPos < (-bounds.width))
                {
                    xPos = BGWidth;
                }

                auto pos = e.getComponent<cro::Transform>().getPosition();
                pos.x = std::round(xPos);

                e.getComponent<cro::Transform>().setPosition(pos);

                auto cropping = bounds;
                cropping.left = -pos.x;
                cropping.left += 6.f;
                cropping.width = BGWidth;
                e.getComponent<cro::Drawable2D>().setCroppingArea(cropping);
            }
        };

    m_leagueText.previous = entity;
}

void LeagueState::updateLeagueText()
{
    const auto& str = Social::getMonthlyLeague();
    m_leagueText.games.getComponent<cro::Text>().setString(str[2]);
    m_leagueText.names.getComponent<cro::Text>().setString(str[0]);
    m_leagueText.scores.getComponent<cro::Text>().setString(str[1]);
    m_leagueText.personal.getComponent<cro::Text>().setString(str[3]);
    m_leagueText.previous.getComponent<cro::Text>().setString(str[4]);
    centreText(m_leagueText.personal);
}
#endif

void LeagueState::addLeagueButtons(const cro::SpriteSheet& spriteSheet)
{
    auto spriteRight = spriteSheet.getSprite("button_right");
    auto spriteLeft = spriteSheet.getSprite("button_left");

    auto boundsBottom = spriteRight.getTextureRect().bottom;

    auto& uiSystem = *m_scene.getSystem<cro::UISystem>();
    auto unselected = uiSystem.addCallback([boundsBottom](cro::Entity e)
        {
            auto bounds = e.getComponent<cro::Sprite>().getTextureRect();
            bounds.bottom = boundsBottom;
            e.getComponent<cro::Sprite>().setTextureRect(bounds);
        });
    boundsBottom += 16.f;
    auto selected = uiSystem.addCallback([boundsBottom](cro::Entity e)
        {
            auto bounds = e.getComponent<cro::Sprite>().getTextureRect();
            bounds.bottom = boundsBottom;
            e.getComponent<cro::Sprite>().setTextureRect(bounds);

            e.getComponent<cro::AudioEmitter>().play();
        });

    cro::Entity buttonRight = m_scene.createEntity();
    buttonRight.addComponent<cro::Transform>().setPosition({ 370.f, 286.f, 0.2f });
    buttonRight.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonRight.addComponent<cro::Drawable2D>();
    buttonRight.addComponent<cro::Sprite>() = spriteRight;

    m_tabNodes[TabID::League].getComponent<cro::Transform>().addChild(buttonRight.getComponent<cro::Transform>());


    cro::Entity buttonLeft = m_scene.createEntity();
    buttonLeft.addComponent<cro::Transform>().setPosition({ 128.f, 286.f, 0.2f });
    buttonLeft.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonLeft.addComponent<cro::Drawable2D>();
    buttonLeft.addComponent<cro::Sprite>() = spriteLeft;

    m_tabNodes[TabID::League].getComponent<cro::Transform>().addChild(buttonLeft.getComponent<cro::Transform>());


    buttonRight.addComponent<cro::UIInput>().area = spriteRight.getTextureBounds();
    buttonRight.getComponent<cro::UIInput>().setSelectionIndex(RightButtonIndex);
    buttonRight.getComponent<cro::UIInput>().setNextIndex(LeftButtonIndex, NameButtonIndex);
    buttonRight.getComponent<cro::UIInput>().setPrevIndex(LeftButtonIndex, CloseButtonIndex);
    buttonRight.getComponent<cro::UIInput>().setGroup(MenuID::League);
    buttonRight.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selected;
    buttonRight.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselected;
    buttonRight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = uiSystem.addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt) mutable
        {
            if (activated(evt))
            {
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                switchLeague(Page::Forward);
            }
        });


    buttonLeft.addComponent<cro::UIInput>().area = spriteLeft.getTextureBounds();
    buttonLeft.getComponent<cro::UIInput>().setSelectionIndex(LeftButtonIndex);
    buttonLeft.getComponent<cro::UIInput>().setNextIndex(RightButtonIndex, NameButtonIndex);
    buttonLeft.getComponent<cro::UIInput>().setPrevIndex(RightButtonIndex, CloseButtonIndex);
    buttonLeft.getComponent<cro::UIInput>().setGroup(MenuID::League);
    buttonLeft.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selected;
    buttonLeft.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselected;
    buttonLeft.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = uiSystem.addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt) mutable
        {
            if (activated(evt))
            {
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                switchLeague(Page::Back);
            }
        });


    //name change buttons
    unselected = uiSystem.addCallback([](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });

    selected = uiSystem.addCallback([](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
        });

    auto activate = uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                auto idx = e.getComponent<cro::UIInput>().getSelectionIndex() - NameButtonIndex;
                if (m_currentLeague < LeagueID::Global)
                {
                    const auto league = League(m_currentLeague, m_sharedData);

                    if (league.getSortedTable()[idx].name != -1)
                    {
                        //launch name editor for this index
                        auto& names = m_sharedData.leagueNames;
                        m_activeName = &names[league.getSortedTable()[idx].name];
                    }
                    else
                    {
                        m_activeName = &m_profileData.playerProfiles[0].name;
                    }

                    auto utf = m_activeName->toUtf8();
                    if (utf.size() < m_nameBuffer.size())
                    {
                        std::memcpy(m_nameBuffer.data(), utf.data(), utf.size());
                        m_nameBuffer[utf.size()] = 0;
#ifdef USE_GNS
                        if (Social::isSteamdeck())
                        {
                            const auto cb = [&](bool accepted, const char* buff)
                                {
                                    if (accepted
                                        && buff[0] != 0)
                                    {
                                        *m_activeName = cro::String::fromUtf8(buff, buff + std::strlen(buff));
                                        *m_activeName = m_activeName->substr(0, ConstVal::MaxStringChars);

                                        if (m_activeName == &m_profileData.playerProfiles[0].name)
                                        {
                                            m_profileData.playerProfiles[0].saveProfile();
                                            Social::setPlayerName(*m_activeName);
                                        }
                                        else
                                        {
                                            m_sharedData.leagueNames.write();
                                            refreshAllNameLists();
                                        }
                                        m_activeName = nullptr;
                                    }
                                    m_nameBuffer[0] = 0;
                                    m_editName = false;
                                };
                            Social::showTextInput(cb, "Enter Name", m_nameBuffer.size(), m_nameBuffer.data());
                        }
                        else
#endif
                        {
                            m_editName = true;
                        }

                        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    }
                    else
                    {
                        m_activeName = nullptr;
                        LogE << "Can't edit name - string is too long." << std::endl;
                        m_audioEnts[AudioID::No].getComponent<cro::AudioEmitter>().play();
                    }
                }
#ifdef USE_GNS
                else
                {
                    Social::showLeaguePlayer(idx);
                }
#endif
            }
        });


    glm::vec3 pos(63.f, 255.f, 0.2f);
    std::vector<cro::Entity> temp;
    auto sprite = spriteSheet.getSprite("name_highlight");
    auto bounds = sprite.getTextureBounds();

    for (auto i = 0u; i < 16u; ++i)
    {
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(pos);
        entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        entity.addComponent<cro::Drawable2D>(); 
        entity.addComponent<cro::Sprite>() = sprite;
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);

        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().setSelectionIndex(NameButtonIndex + i);
        entity.getComponent<cro::UIInput>().setNextIndex((NameButtonIndex + i) + 1);
        entity.getComponent<cro::UIInput>().setPrevIndex((NameButtonIndex + i) - 1);
        entity.getComponent<cro::UIInput>().setGroup(MenuID::League);

        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selected;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselected;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = activate;


        m_tabNodes[TabID::League].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        pos.y -= 13.f;

        temp.push_back(entity);
    }

    temp[0].getComponent<cro::UIInput>().setPrevIndex(RightButtonIndex, RightButtonIndex);
    temp.back().getComponent<cro::UIInput>().setNextIndex(InfoButtonIndex, InfoButtonIndex);
}

void LeagueState::activateTab(std::int32_t tabID)
{
    if (tabID != m_currentTab)
    {
        //update old
        m_tabNodes[m_currentTab].getComponent<cro::Transform>().setScale(glm::vec2(0.f));

        //update index
        m_currentTab = tabID;

        //update new
        m_tabNodes[m_currentTab].getComponent<cro::Transform>().setScale(glm::vec2(1.f));

        //update the button selection graphic
        auto bounds = m_tabEntity.getComponent<cro::Sprite>().getTextureRect();
        bounds.bottom = bounds.height * m_currentTab;
        m_tabEntity.getComponent<cro::Sprite>().setTextureRect(bounds);

        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

        m_scene.getSystem<cro::UISystem>()->setActiveGroup(tabID);
    }
}

void LeagueState::switchLeague(std::int32_t forward)
{
    std::int32_t next = 0;
    if (forward == Page::Forward)
    {
        next = (m_currentLeague + 1) % LeagueID::Count;
        while (next != m_currentLeague && !m_leagueNodes[next].isValid())
        {
            next = (next + 1) % LeagueID::Count;
        }
    }
    else
    {
        next = (m_currentLeague + (LeagueID::Count - 1)) % LeagueID::Count;
        while (next != m_currentLeague && !m_leagueNodes[next].isValid())
        {
            next = (next + (LeagueID::Count - 1)) % LeagueID::Count;
        }
    }

    cro::Entity ent = m_scene.createEntity();
    ent.addComponent<cro::Callback>().active = true;
    ent.getComponent<cro::Callback>().function =
        [&, next](cro::Entity f, float) mutable
        {
            m_leagueNodes[m_currentLeague].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            m_leagueNodes[next].getComponent<cro::Transform>().setScale(glm::vec2(1.f));
            m_currentLeague = next;

            f.getComponent<cro::Callback>().active = false;
            m_scene.destroyEntity(f);
        };
}

void LeagueState::quitState()
{
    //m_scene.setSystemActive<cro::AudioPlayerSystem>(false);
    m_rootNode.getComponent<cro::Callback>().active = true;
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}
