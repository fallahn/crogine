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

#include "LeagueState.hpp"
#include "SharedStateData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "TextAnimCallback.hpp"
#include "MessageIDs.hpp"
#include "League.hpp"
#include "RandNames.hpp"

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
#endif
    std::size_t courseIndex = 0;

    bool showStats = false;
}

LeagueState::LeagueState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State            (ss, ctx),
    m_scene                 (ctx.appInstance.getMessageBus(), 480),
    m_sharedData            (sd),
    m_viewScale             (2.f),
    m_currentTab            (0)
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
                    static League league;

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

void LeagueState::handleMessage(const cro::Message& msg)
{
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

    //tab buttons
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("league_button");
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
    const float offset = 153.f;

    for (auto i = 0; i < TabID::Max; ++i)
    {
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ (stride * i) + offset, 19.f, 0.5f });
        entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
        entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());
        entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = sprite;
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);

        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().enabled = i != m_currentTab;
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

    createLeagueTab(bgNode, spriteSheet);
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

void LeagueState::createLeagueTab(cro::Entity parent, const cro::SpriteSheet& spriteSheet)
{
    m_tabNodes[TabID::League] = m_scene.createEntity();
    m_tabNodes[TabID::League].addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.2f });
    parent.getComponent<cro::Transform>().addChild(m_tabNodes[TabID::League].getComponent<cro::Transform>());

    const auto centre = parent.getComponent<cro::Sprite>().getTextureBounds().width / 2.f;

    const auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ centre, 298.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("The Club League");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    centreText(entity);
    m_tabNodes[TabID::League].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //stripes
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ centre, 61.f, 0.05f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("stripes");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, 0.f });
    m_tabNodes[TabID::League].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto stripeEnt = entity;

    League league;
    auto season = league.getCurrentSeason();
    auto gamesPlayed = league.getCurrentIteration();

    auto statusString = "Season " + std::to_string(season) + " - Games played: " + std::to_string(gamesPlayed) + "/24";
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ centre, 52.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString(statusString);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    centreText(entity);
    m_tabNodes[TabID::League].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto table = league.getTable();
    struct TableEntry final
    {
        std::int32_t score = 0;
        std::int32_t handicap = 0;
        std::int32_t name = -1;
        TableEntry(std::int32_t s, std::int32_t h, std::int32_t n)
            :score(s), handicap(h), name(n) {}
    };
    std::vector<TableEntry> entries;
    for (const auto& p : table)
    {
        entries.emplace_back(p.currentScore, p.outlier + p.curve, p.nameIndex);
    }
    //we'll fake our handicap (it's not a real golf one anyway) with our current level
    entries.emplace_back(league.getCurrentScore(), Social::getLevel() / 2, -1);

    std::sort(entries.begin(), entries.end(),
        [](const TableEntry& a, const TableEntry& b)
        {
            return a.score == b.score ?
                a.handicap > b.handicap :
                a.score > b.score;
        });


    //column of rank badges representing psuedo-level (handicap * 2)
    //or the player's current level.
    static constexpr float VerticalSpacing = 13.f;
    static constexpr float TextTop = 268.f;
    float stripePos = 0.f;

    cro::SpriteSheet shieldSprites;
    shieldSprites.loadFromFile("assets/golf/sprites/lobby_menu.spt", m_sharedData.sharedResources->textures);

    //TODO we could optimise this a bit by batching into a vertex array...
    glm::vec3 spritePos(68.f, TextTop - 12.f, 0.1f);
    auto z = 0; //track our position so we can highlight the name
    for (const auto& entry : entries)
    {
        std::int32_t level = 0;
        if (entry.name == -1)
        {
            level = Social::getLevel() / 10;
            stripePos = (VerticalSpacing * (entries.size() - (z+ 1)));
        }
        else
        {
            level = entry.handicap * 2;
            level /= 10;
        }
        level = std::clamp(level, 0, 5);

        //uhhh there ought to be a good way to swap just these cases
        switch (level)
        {
        default: break;
        case 0:
            level = 2;
            break;
        case 2:
            level = 0;
            break;
        case 3:
            level = 5;
            break;
        case 5:
            level = 3;
            break;
        }

        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(spritePos);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = shieldSprites.getSprite("rank_badge");
        entity.addComponent<cro::SpriteAnimation>().play(level);
        m_tabNodes[TabID::League].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

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
            str += RandomNames[entries[i].name];
        }
        else
        {
            str += playerName;
        }
        str += "\n";
    }

    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Label);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 86.f, TextTop + 1.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString(str);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
    m_tabNodes[TabID::League].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    str.clear();
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
    entity.addComponent<cro::Transform>().setPosition({ 328.f, TextTop + 1.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString(str);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
    m_tabNodes[TabID::League].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //check if a previous season exists and scroll the results
    const auto path = Social::getBaseContentPath() + PrevFileName;
    if (cro::FileSystem::fileExists(path))
    {
        cro::RaiiRWops file;
        file.file = SDL_RWFromFile(path.c_str(), "rb");
        if (file.file)
        {
            auto size = SDL_RWseek(file.file, 0, RW_SEEK_END);
            if (size % sizeof(PreviousEntry) == 0)
            {
                auto count = size / sizeof(PreviousEntry);
                std::vector<PreviousEntry> buff(count);

                SDL_RWseek(file.file, 0, RW_SEEK_SET);
                SDL_RWread(file.file, buff.data(), sizeof(PreviousEntry), count);

                //this assumes everything was sorted correctly when it was saved
                cro::String str = "Previous Season's Results";
                for (auto i = 0u; i < buff.size(); ++i)
                {
                    buff[i].nameIndex = std::clamp(buff[i].nameIndex, -1, static_cast<std::int32_t>(RandomNames.size()) - 1);

                    str += " -~- ";
                    str += std::to_string(i + 1);
                    if (buff[i].nameIndex > -1)
                    {
                        str += ". " + RandomNames[buff[i].nameIndex];
                    }
                    else
                    {
                        str += ". " + playerName;
                    }
                    str += " " + std::to_string(buff[i].score);
                }


                entity = m_scene.createEntity();
                entity.addComponent<cro::Transform>().setPosition({ 0.f, 15.f, 0.1f });
                entity.addComponent<cro::Drawable2D>();
                entity.addComponent<cro::Text>(smallFont).setString(str);
                entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
                entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
                entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
            
                entity.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
                parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

                bounds = cro::Text::getLocalBounds(entity);
                entity.addComponent<cro::Callback>().active = true;
                entity.getComponent<cro::Callback>().setUserData<float>(0.f);
                entity.getComponent<cro::Callback>().function =
                    [&, bounds](cro::Entity e, float dt)
                {
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
                };
            }
        }
    }
}

void LeagueState::createInfoTab(cro::Entity parent)
{
    m_tabNodes[TabID::Info] = m_scene.createEntity();
    m_tabNodes[TabID::Info].addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.2f });
    m_tabNodes[TabID::Info].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    parent.getComponent<cro::Transform>().addChild(m_tabNodes[TabID::Info].getComponent<cro::Transform>());

    const auto centre = parent.getComponent<cro::Sprite>().getTextureBounds().width / 2.f;

    const auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ centre, 298.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("The Club League");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    centreText(entity);
    m_tabNodes[TabID::Info].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    std::string desc = R"(
Every Stroke Play or Stableford round you play contributes to the club league. The league
is split into seasons, with 24 rounds played for each season. At the end of a season the
top three players win an award.

At the end of a round your scores are converted using the Stableford scoring system with
par being worth 2 points and 1 point extra awarded for each stroke under par. Scores for
each round are summed to give the total score for the current season.

Other player scores are automatically calculated based on their own stats, randomly
generated when the first season begins. Think of it as something like a fantasy football
league. At the end of each season these stats are re-evaluated based on the previous
season's performance. Some players will increase in skill, others will decrease.

Can you top the league table by the end of the season? Good luck!


(Note: the league is reset if you reset your player progress at any time.)
)";


    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ centre, 258.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString(desc);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    m_tabNodes[TabID::Info].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void LeagueState::activateTab(std::int32_t tabID)
{
    if (tabID != m_currentTab)
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

        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
    }

    //for reasons I've given up trying to understand, we need to
    //delay the selection by one frame.
    // const auto selectNext = [&](std::int32_t idx)
    // {
    //     auto e = m_scene.createEntity();
    //     e.addComponent<cro::Callback>().active = true;
    //     e.getComponent<cro::Callback>().function =
    //         [&, idx](cro::Entity f, float)
    //     {
    //         m_scene.getSystem<cro::UISystem>()->selectByIndex(idx);
    //         f.getComponent<cro::Callback>().active = false;
    //         m_scene.destroyEntity(f);
    //     };
    // };

    //update the selection order depending on which page we're on
    /*switch (tabID)
    {
    default: break;
    case TabID::ClubStats:
        m_tabButtons[TabID::Performance].getComponent<cro::UIInput>().setNextIndex(ButtonHistory, ButtonHistory);
        m_tabButtons[TabID::Performance].getComponent<cro::UIInput>().setPrevIndex(ButtonAwards, ButtonAwards);

        m_tabButtons[TabID::History].getComponent<cro::UIInput>().setNextIndex(ButtonAwards, ButtonAwards);
        m_tabButtons[TabID::History].getComponent<cro::UIInput>().setPrevIndex(ButtonPerformance, ButtonPerformance);

        m_tabButtons[TabID::Awards].getComponent<cro::UIInput>().setNextIndex(ButtonPerformance, ButtonPerformance);
        m_tabButtons[TabID::Awards].getComponent<cro::UIInput>().setPrevIndex(ButtonHistory, ButtonHistory);

        selectNext(ButtonPerformance);
        break;
    case TabID::Performance:
        m_tabButtons[TabID::ClubStats].getComponent<cro::UIInput>().setNextIndex(ButtonHistory, Perf01);
        m_tabButtons[TabID::ClubStats].getComponent<cro::UIInput>().setPrevIndex(ButtonAwards, PerfCPU);

        m_tabButtons[TabID::History].getComponent<cro::UIInput>().setNextIndex(ButtonAwards, PerfNextCourse);
        m_tabButtons[TabID::History].getComponent<cro::UIInput>().setPrevIndex(ButtonClubset, PerfNextProf);

        m_tabButtons[TabID::Awards].getComponent<cro::UIInput>().setNextIndex(ButtonClubset, PerfDate);
        m_tabButtons[TabID::Awards].getComponent<cro::UIInput>().setPrevIndex(ButtonHistory, PerfDate);
        
        selectNext(ButtonHistory);
        break;
    case TabID::History:
        m_tabButtons[TabID::ClubStats].getComponent<cro::UIInput>().setNextIndex(ButtonPerformance, ButtonPerformance);
        m_tabButtons[TabID::ClubStats].getComponent<cro::UIInput>().setPrevIndex(ButtonAwards, ButtonAwards);

        m_tabButtons[TabID::Performance].getComponent<cro::UIInput>().setNextIndex(ButtonAwards, ButtonAwards);
        m_tabButtons[TabID::Performance].getComponent<cro::UIInput>().setPrevIndex(ButtonClubset, ButtonClubset);

        m_tabButtons[TabID::Awards].getComponent<cro::UIInput>().setNextIndex(ButtonClubset, ButtonClubset);
        m_tabButtons[TabID::Awards].getComponent<cro::UIInput>().setPrevIndex(ButtonPerformance, ButtonPerformance);

        selectNext(ButtonAwards);
        break;
    case TabID::Awards:
        m_tabButtons[TabID::ClubStats].getComponent<cro::UIInput>().setNextIndex(ButtonPerformance, ButtonPerformance);
        m_tabButtons[TabID::ClubStats].getComponent<cro::UIInput>().setPrevIndex(ButtonHistory, ButtonHistory);
        
        m_tabButtons[TabID::Performance].getComponent<cro::UIInput>().setNextIndex(ButtonHistory, AwardPrevious);
        m_tabButtons[TabID::Performance].getComponent<cro::UIInput>().setPrevIndex(ButtonClubset, AwardPrevious);

        m_tabButtons[TabID::History].getComponent<cro::UIInput>().setNextIndex(ButtonClubset, AwardNext);
        m_tabButtons[TabID::History].getComponent<cro::UIInput>().setPrevIndex(ButtonPerformance, AwardNext);

        selectNext(ButtonClubset);
        break;
    }*/
}

void LeagueState::quitState()
{
    //m_scene.setSystemActive<cro::AudioPlayerSystem>(false);
    m_rootNode.getComponent<cro::Callback>().active = true;
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}
