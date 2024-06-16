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

#include "CareerState.hpp"
#include "SharedStateData.hpp"
#include "SharedCourseData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "MessageIDs.hpp"
#include "Utility.hpp"
#include "MenuCallbacks.hpp"
#include "MenuConsts.hpp"
#include "TextAnimCallback.hpp"
#include "PacketIDs.hpp"
#include "Clubs.hpp"
#include "../GolfGame.hpp"
#include "../Colordome-32.hpp"

#include <Achievements.hpp>
#include <AchievementStrings.hpp>
#include <Input.hpp>

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

#include <crogine/util/Easings.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    struct ConfirmationData final
    {
        float progress = 0.f;
        enum
        {
            In, Out
        }dir = In;
    };

    struct MenuID final
    {
        enum
        {
            Dummy,
            Career, ConfirmQuit,
            Info
        };
    };

    enum
    {
        CareerOptions = 10,
        
        CareerGimme,
        CareerWeather,
        CareerClubs,
        CareerNight,
        CareerClubStats,
        CareerQuit,
        CareerProfile,
        CareerLeagueBrowser,
        CareerStart,
        CareerInfo,


        CareerSeason = 100
    };

    constexpr glm::vec3 LeagueListPosition = glm::vec3(119.f, 216.f, 0.2f);
    constexpr float LeagueLineSpacing = 14.f;

    const std::string ConfigFile("career.cfg");
}

CareerState::CareerState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State    (ss, ctx),
    m_scene         (ctx.appInstance.getMessageBus()),
    m_sharedData    (sd),
    m_maxLeagueIndex(0),
    m_viewScale     (2.f),
    m_currentMenu   (MenuID::Career)
{
    ctx.mainWindow.setMouseCaptured(false);

    std::fill(m_progressPositions.begin(), m_progressPositions.end(), 0);

    addSystems();
    buildScene();
}

CareerState::~CareerState()
{
    //this might be quitting from a cached state and not
    //necessarily startinga career game.
    if (m_sharedData.leagueRoundID != LeagueRoundID::Club)
    {
        saveConfig();
    }
}

//public
bool CareerState::handleEvent(const cro::Event& evt)
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
        if (evt.cbutton.button == cro::GameController::ButtonB)
        {
            quitState();
            return false;
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

void CareerState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::StateMessage)
    {
        const auto& data = msg.getData<cro::Message::StateEvent>();
        if (data.action == cro::Message::StateEvent::Popped
            && getStateCount() == 2) //hmm we really want to be able to check the top state is this one
        {
            m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;

            if (data.id == StateID::Unlock
                && m_sharedData.showCredits)
            {
                Achievements::awardAchievement(AchievementStrings[AchievementID::SemiRetired]);

                m_sharedData.showCredits = false;
                requestStackPush(StateID::Credits);
            }
        }
    }
    else if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            //if we have a window over the top (eg profile editor)
            //we want to activate this on window resize so layout
            //is correctly updated.
            m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
        }
    }
    else if (msg.id == Social::MessageID::SocialMessage)
    {
        const auto& data = msg.getData<Social::SocialEvent>();
        if (data.type == Social::SocialEvent::PlayerNameChanged)
        {
            m_playerName.getComponent<cro::Text>().setString(Social::getPlayerName());
            for (auto& table : Career::instance(m_sharedData).getLeagueTables())
            {
                //this is misleading - it says it's cont however it updates the
                //internal string data with the player's name...
                table.getPreviousResults(Social::getPlayerName());
            }
        }
    }

    m_scene.forwardMessage(msg);
}

bool CareerState::simulate(float dt)
{
    m_scene.simulate(dt);
    return true;
}

void CareerState::render()
{
    m_scene.render();
}

//private
void CareerState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::UISystem>(mb);// ->initDebug("Career UI");
    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::SpriteAnimator>(mb);
    m_scene.addSystem<cro::SpriteSystem2D>(mb);
    m_scene.addSystem<cro::TextSystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::RenderSystem2D>(mb);
    m_scene.addSystem<cro::AudioPlayerSystem>(mb);

    m_scene.setSystemActive<cro::UISystem>(false);
    m_scene.setSystemActive<cro::AudioPlayerSystem>(false);
}

void CareerState::buildScene()
{
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
                e.getComponent<cro::Transform>().setScale({ m_viewScale.x, m_viewScale.y * cro::Util::Easing::easeOutQuint(currTime) });
                if (currTime == 1)
                {
                    state = RootCallbackData::FadeOut;
                    e.getComponent<cro::Callback>().active = false;

                    m_scene.setSystemActive<cro::AudioPlayerSystem>(true);

                    m_scene.setSystemActive<cro::UISystem>(true);
                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Career);
                    m_scene.getSystem<cro::UISystem>()->selectAt(CareerStart);
                    Social::setStatus(Social::InfoID::Menu, { "Making Career Decisions" });

                    applySettingsValues(); //loadConfig() might not load anything
                    loadConfig();
                    
                    //check if we just completed a league, and if we did and it's
                    //one less than max leagues, increment our current round id
                    auto currIdx = std::max(0, m_sharedData.leagueRoundID - 1);
                    if (currIdx != 0 &&
                        currIdx == m_maxLeagueIndex - 1)
                    {
                        if (Career::instance(m_sharedData).getLeagueTables()[currIdx].getCurrentIteration() == 0)
                        {
                            m_sharedData.leagueRoundID = std::min(std::int32_t(LeagueRoundID::RoundSix), m_sharedData.leagueRoundID + 1);
                        }
                    }

                    if (m_sharedData.leagueRoundID == LeagueRoundID::Club)
                    {
                        selectLeague(m_maxLeagueIndex);
                    }
                    else
                    {
                        selectLeague(m_sharedData.leagueRoundID - 1);
                    }

                    if (!m_sharedData.unlockedItems.empty())
                    {
                        requestStackPush(StateID::Unlock);
                    }
                    else
                    {
                        auto idx = m_sharedData.leagueRoundID - LeagueRoundID::RoundOne;
                        if (idx == 0 && m_progressPositions[idx] == 0 //no completed holes
                            && Career::instance(m_sharedData).getLeagueTables()[idx].getCurrentIteration() == 0)
                        {
                            //if we're on the first league/season show the info
                            enterInfoCallback();
                        }
                    }

                    //start title animation
                    cro::Command cmd;
                    cmd.targetFlags = CommandID::Menu::TitleText;
                    cmd.action = [](cro::Entity t, float)
                        {
                            t.getComponent<cro::Callback>().setUserData<float>(0.f);
                            t.getComponent<cro::Callback>().active = true;
                        };
                    m_scene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
                }
                break;
            case RootCallbackData::FadeOut:
                currTime = std::max(0.f, currTime - (dt * 2.f));
                e.getComponent<cro::Transform>().setScale({ m_viewScale.x, m_viewScale.y * cro::Util::Easing::easeOutQuint(currTime) });


                //interestingly only clang tells us capturing a structured binding is C++20 (we're using 17)
                auto ct = currTime;

                cro::Command cmd;
                cmd.targetFlags = CommandID::Menu::TitleText;
                cmd.action =
                    [ct](cro::Entity t, float)
                    {
                        t.getComponent<cro::Transform>().setScale(glm::vec2(ct));
                    };
                m_scene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


                if (currTime == 0)
                {
                    requestStackPop();

                    state = RootCallbackData::FadeIn;
                    m_scene.setSystemActive<cro::AudioPlayerSystem>(false);
                }
                break;
            }

        };

    m_rootNode = rootNode;

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/career_menu.spt", m_sharedData.sharedResources->textures);

    //background
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().absolutePosition = { 0.f, 10.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto bgEnt = entity;
    createProfileLayout(bgEnt, spriteSheet);

    //title
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.9f };
    entity.getComponent<UIElement>().depth = 1.6f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::TitleText;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("title");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function = TitleTextCallback();
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //dummy menu ent for transitions
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Dummy);

    //cursor
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setOrigin({ 23.f, -3.f, -0.1f });
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("cursor");
    entity.addComponent<cro::SpriteAnimation>().play(0);

    auto selectCursor = m_scene.getSystem<cro::UISystem>()->addCallback(
        [entity](cro::Entity e) mutable
        {
            entity.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

            e.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            e.getComponent<cro::AudioEmitter>().play();
        });
    auto unselectCursor = m_scene.getSystem<cro::UISystem>()->addCallback(
        [entity](cro::Entity e) mutable
        {
            entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        });

    auto selectHighlight = m_scene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Callback>().active = true;
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
        });
    auto unselectHighlight = m_scene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });

    auto selectOffset = m_scene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            auto bounds = e.getComponent<cro::Sprite>().getTextureRect();
            bounds.bottom += bounds.height;
            e.getComponent<cro::Sprite>().setTextureRect(bounds);
            e.getComponent<cro::AudioEmitter>().play();
        });
    auto unselectOffset = m_scene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            auto bounds = e.getComponent<cro::Sprite>().getTextureRect();
            bounds.bottom -= bounds.height;
            e.getComponent<cro::Sprite>().setTextureRect(bounds);
        });



    cro::String playerName;
    if (Social::isAvailable())
    {
        playerName = Social::getPlayerName();
    }
    else
    {
        playerName = m_sharedData.localConnectionData.playerData[0].name;
    }

    const auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    std::vector<cro::Entity> buttons; //temp store these here so we can correct button indices


    //league items
    const auto& leagueData = Career::instance(m_sharedData).getLeagueData();
    const auto& leagueTables = Career::instance(m_sharedData).getLeagueTables();
    glm::vec3 position = LeagueListPosition;

    //active league highlight
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(position);
    entity.getComponent<cro::Transform>().setOrigin(glm::vec3(0.f, 3.f, -0.1f));
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(-99.f, 7.f), TextGoldColour),
            cro::Vertex2D(glm::vec2(-99.f, -7.f), TextGoldColour),
            cro::Vertex2D(glm::vec2(99.f, 7.f), TextGoldColour),
            cro::Vertex2D(glm::vec2(99.f, -7.f), TextGoldColour)
        });
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_leagueDetails.highlight = entity;

    std::vector<std::uint8_t> temp(18);

    for (auto i = 0u; i < Career::MaxLeagues; ++i)
    {
        //this just builds up the string if needed, and finds the previous result (if any)
        leagueTables[i].getPreviousResults(playerName);

        const bool unlocked = (i == 0 || ((i > 0) && (leagueTables[i - 1].getCurrentBest() < CareerLeagueThreshold)));

        //league titles, listed on left
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(position);
        entity.getComponent<cro::Transform>().setOrigin(glm::vec3(0.f, 0.f, -0.2f));
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(largeFont).setString(leagueData[i].title);
        entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
        entity.getComponent<cro::Text>().setFillColour(TextHighlightColour);
        entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);

        const auto addButton = [&](cro::Entity parent, std::uint32_t index)
            {
                auto buttonEntity = m_scene.createEntity();
                buttonEntity.addComponent<cro::Transform>().setPosition({ 0.f, -3.f, 0.5f });
                buttonEntity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
                buttonEntity.addComponent<cro::Drawable2D>();
                buttonEntity.addComponent<cro::Sprite>() = spriteSheet.getSprite("league_highlight");
                buttonEntity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
                buttonEntity.addComponent<cro::Callback>().function = MenuTextCallback();
                auto b = buttonEntity.getComponent<cro::Sprite>().getTextureBounds();
                buttonEntity.addComponent<cro::UIInput>().area = b;
                buttonEntity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
                buttonEntity.getComponent<cro::UIInput>().setSelectionIndex(CareerSeason + index);
                buttonEntity.getComponent<cro::UIInput>().setNextIndex((CareerSeason + index) + 1, (CareerSeason + index) + 1);
                buttonEntity.getComponent<cro::UIInput>().setPrevIndex((CareerSeason + index) - 1, (CareerSeason + index) - 1);
                buttonEntity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectHighlight;
                buttonEntity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectHighlight;
                buttonEntity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
                    m_scene.getSystem<cro::UISystem>()->addCallback(
                        [&, i](cro::Entity e, const cro::ButtonEvent& evt)
                        {
                            if (activated(evt))
                            {
                                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                                selectLeague(i);
                            }
                        });
                buttonEntity.getComponent<cro::Transform>().setOrigin({ b.width / 2.f, b.height / 2.f });
                parent.getComponent<cro::Transform>().addChild(buttonEntity.getComponent<cro::Transform>());

                return buttonEntity;
            };


        //activating updates preview
        if (unlocked)
        {
            entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);

            //add a trophy if previously complete
            if (leagueTables[i].getCurrentBest() < 4)
            {
                auto statusEnt = m_scene.createEntity();
                statusEnt.addComponent<cro::Transform>().setPosition({ 5.f, position.y - 9.f, 0.1f });
                statusEnt.addComponent<cro::Drawable2D>();
                statusEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("league_status");
                statusEnt.addComponent<cro::SpriteAnimation>().play(leagueTables[i].getCurrentBest() - 1);
                bgEnt.getComponent<cro::Transform>().addChild(statusEnt.getComponent<cro::Transform>());
            }

            //add a button - include index as metadata so we know which info to display when selected
            auto button = addButton(entity, i);
            buttons.push_back(button);

            //if there's a save file this will update the current hole
            if (Progress::read(i + 1, m_progressPositions[i], temp))
            {
                m_progressPositions[i] += 1; //convert from index to hole number
            }
        }
        else
        {
            //padlock
            auto statusEnt = m_scene.createEntity();
            statusEnt.addComponent<cro::Transform>().setPosition({ 5.f, position.y - 9.f, 0.1f });
            statusEnt.addComponent<cro::Drawable2D>();
            statusEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("league_status");
            statusEnt.addComponent<cro::SpriteAnimation>().play(3);
            bgEnt.getComponent<cro::Transform>().addChild(statusEnt.getComponent<cro::Transform>());
        }


        bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        position.y -= LeagueLineSpacing;
    }
    //correct selection indices for first / last
    CRO_ASSERT(!buttons.empty(), "");
    buttons.front().getComponent<cro::UIInput>().setPrevIndex(CareerProfile, CareerProfile);
    buttons.back().getComponent<cro::UIInput>().setNextIndex(CareerGimme, CareerGimme);

    //put player name on bottom row of the box
    position.y -= LeagueLineSpacing;
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(position);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString(playerName);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(glm::vec2(1.f, -1.f));
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    m_playerName = entity;
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    //gimme
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 72.f, 96.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("No Gimme");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_settingsDetails.gimme = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 72.f, 92.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Callback>().function = MenuTextCallback();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("gimme_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerGimme);
    entity.getComponent<cro::UIInput>().setNextIndex(CareerWeather, Social::getClubLevel() ? CareerClubs : CareerClubStats);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerWeather, buttons.back().getComponent<cro::UIInput>().getSelectionIndex());
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_scene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                m_sharedData.gimmeRadius = (m_sharedData.gimmeRadius + 1) % 3;
                m_settingsDetails.gimme.getComponent<cro::Text>().setString(GimmeString[m_sharedData.gimmeRadius]);
            }
        }
    );
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, std::floor(bounds.height / 2.f) });
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //weather icon
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("weather_icon");
    auto iconEnt = entity;
    bounds = entity.getComponent<cro::Sprite>().getTextureRect();

    //text for current weather type
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 174.f, 96.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setString(WeatherStrings[m_sharedData.weatherType]);
    centreText(entity);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<std::uint32_t>(WeatherType::Count);
    entity.getComponent<cro::Callback>().function =
        [&, iconEnt, bounds](cro::Entity e, float) mutable
        {
            auto& lastType = e.getComponent<cro::Callback>().getUserData<std::uint32_t>();
            if (lastType != m_sharedData.weatherType)
            {
                e.getComponent<cro::Text>().setString(WeatherStrings[m_sharedData.weatherType]);
                centreText(e);

                auto x = std::round(cro::Text::getLocalBounds(e).width) + 2.f;
                iconEnt.getComponent<cro::Transform>().setPosition(glm::vec3(x, -9.f, 0.1f));

                auto rect = bounds;
                rect.left += bounds.width * m_sharedData.weatherType;
                iconEnt.getComponent<cro::Sprite>().setTextureRect(rect);
            }
            lastType = m_sharedData.weatherType;
        };

    entity.getComponent<cro::Transform>().addChild(iconEnt.getComponent<cro::Transform>());
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 179.f, 92.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Callback>().function = MenuTextCallback();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("weather_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerWeather);
    entity.getComponent<cro::UIInput>().setNextIndex(CareerGimme, CareerNight);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerGimme, buttons.back().getComponent<cro::UIInput>().getSelectionIndex());
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_scene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                m_sharedData.weatherType = (m_sharedData.weatherType + 1) % WeatherType::Count;
            }
        }
    );
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, std::floor(bounds.height / 2.f) });
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //checkbox to show night time status
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 150.f, 72.f, 0.1f });
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("square");
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_settingsDetails.night = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 148.f, 70.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("square_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    bounds.width += 60.f;
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerNight);
    entity.getComponent<cro::UIInput>().setNextIndex(Social::getClubLevel() ? CareerClubs : CareerClubStats, CareerClubStats);
    entity.getComponent<cro::UIInput>().setPrevIndex(Social::getClubLevel() ? CareerClubs : CareerWeather, CareerWeather);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = 
        m_scene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e) 
        {
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
        });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = 
        m_scene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_sharedData.nightTime = m_sharedData.nightTime ? 0 : 1;
                    const float scale = m_sharedData.nightTime ? 1.f : 0.f;
                    m_settingsDetails.night.getComponent<cro::Transform>().setScale(glm::vec2(scale));

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //info button highlight
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 487.f, 43.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Callback>().function = MenuTextCallback();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("info_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerInfo);
    entity.getComponent<cro::UIInput>().setNextIndex(CareerClubStats, CareerStart);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerClubStats, CareerOptions);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    enterInfoCallback();
                }
            });
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //right box - updated by selectLeague()
    static constexpr float CentrePos = 366.f;
    //title
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ CentrePos, 226.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("Course Title Goes Here");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_leagueDetails.courseTitle = entity;
    //desc
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ CentrePos, 214.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("This is where the longer description will be");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_leagueDetails.courseDescription = entity;
    //hole count
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ CentrePos, 82.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("All 18 Holes");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_leagueDetails.holeCount = entity;
    //league info
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ CentrePos, 66.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("Round 3/6\nCurrent Position: 14th\nPrevious Best: 5th");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_leagueDetails.leagueDetails = entity;
    //thumb
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 296, 90.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>();//138x104
    if (!m_sharedData.courseData->courseThumbs.empty())
    {
        entity.getComponent<cro::Sprite>().setTexture(*m_sharedData.courseData->courseThumbs.begin()->second);
        entity.getComponent<cro::Transform>().setScale(CourseThumbnailSize / glm::vec2(entity.getComponent<cro::Sprite>().getTexture()->getSize()));
    }
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_leagueDetails.thumbnail = entity;

    //select the most recent league
    m_maxLeagueIndex = buttons.size() - 1;
    //selectLeague(m_maxLeagueIndex); //this is done each time the menu is shown, so prob not necessary here


    //options button
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 461.f, 241.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Callback>().function = MenuTextCallback();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("options_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerOptions);
    entity.getComponent<cro::UIInput>().setNextIndex(CareerSeason, CareerInfo);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerSeason, CareerStart);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_scene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                m_scene.getActiveCamera().getComponent<cro::Camera>().active = false; //don't update menu when there's another state active

                requestStackPush(StateID::Options);
            }
        }
    );
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //select club set
    if (Social::getClubLevel())
    {
        m_sharedData.preferredClubSet %= (Social::getClubLevel() + 1);

        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 69.f, 69.f, 0.1f });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("bag_select");
        entity.addComponent<cro::SpriteAnimation>().play(m_sharedData.preferredClubSet);
        bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        auto buttonEnt = entity;
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ -32.f, 9.f, 0.f });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(smallFont).setString("Clubs:");
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
        entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
        buttonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ -2.f, -3.f, 0.f });
        entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("bag_highlight");
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        entity.addComponent<cro::Callback>().function = MenuTextCallback();
        bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
        entity.getComponent<cro::UIInput>().setSelectionIndex(CareerClubs);
        entity.getComponent<cro::UIInput>().setNextIndex(CareerNight, CareerClubStats);
        entity.getComponent<cro::UIInput>().setPrevIndex(CareerQuit, CareerGimme);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectHighlight;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectHighlight;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = 
            m_scene.getSystem<cro::UISystem>()->addCallback(
            [&, buttonEnt](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    //we're not connected yet so we have to rely on joining the server to send tis
                    m_sharedData.preferredClubSet = (m_sharedData.preferredClubSet + 1) % (Social::getClubLevel() + 1);
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    buttonEnt.getComponent<cro::SpriteAnimation>().play(m_sharedData.preferredClubSet);
                }
            });
        bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
        entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());
        buttonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_settingsDetails.clubset = buttonEnt;
    }


    //club stats button
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 5.f, 23.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Callback>().function = MenuTextCallback();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("clubinfo_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerClubStats);
    entity.getComponent<cro::UIInput>().setNextIndex(CareerInfo, CareerProfile);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerInfo, Social::getClubLevel() ? CareerClubs : CareerGimme);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_scene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                m_scene.getActiveCamera().getComponent<cro::Camera>().active = false; //don't update menu when there's another state active

                requestStackPush(StateID::Stats);
            }
        }
    );
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //entity with confirmation for starting round
    createConfirmMenu(rootNode);

    //and info overlay
    createInfoMenu(rootNode);


    //banner across bottom
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, BannerPosition, -0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("banner_small");
    auto spriteRect = entity.getComponent<cro::Sprite>().getTextureRect();
    entity.addComponent<UIElement>().absolutePosition = { 0.f, BannerPosition };
    entity.getComponent<UIElement>().depth = -0.1f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, spriteRect](cro::Entity e, float)
        {
            auto rect = spriteRect;
            rect.width = static_cast<float>(GolfGame::getActiveTarget()->getSize().x) * m_viewScale.x;
            e.getComponent<cro::Sprite>().setTextureRect(rect);
            e.getComponent<cro::Callback>().active = false;
        };
    auto bannerEnt = entity;
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //quit
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 20.f, 2.f };
    entity.getComponent<UIElement>().depth = 0.2f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("exit");
    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerQuit);
    entity.getComponent<cro::UIInput>().setNextIndex(CareerProfile, CareerStart);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerStart, CareerClubStats);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectOffset;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectOffset;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    quitState();
                }
            });
    bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //profile editor
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("profile_icon");
    entity.addComponent<UIElement>().absolutePosition = { -80.f, 5.f };
    entity.getComponent<UIElement>().relativePosition = { 0.3334f, 0.f };
    entity.getComponent<UIElement>().depth = 0.2f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerProfile);
    entity.getComponent<cro::UIInput>().setNextIndex(CareerLeagueBrowser, CareerSeason);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerQuit, CareerClubStats);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectCursor;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectCursor;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_scene.getSystem<cro::UISystem>()->addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    m_scene.getActiveCamera().getComponent<cro::Camera>().active = false;
                    requestStackPush(StateID::Profile);
                }
            });
    bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    iconEnt = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 16.f, 10.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("Edit Profile");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    iconEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    iconEnt.getComponent<cro::UIInput>().area.width += cro::Text::getLocalBounds(entity).width;


    //rank icon
    if (Social::getLevel() > 0)
    {
        auto iconEnt = m_scene.createEntity();
        iconEnt.addComponent<cro::Transform>();
        iconEnt.addComponent<cro::Drawable2D>();
        iconEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("rank_icon");
        iconEnt.addComponent<cro::SpriteAnimation>().play(std::min(5, Social::getLevel() / 10));
        auto bounds = iconEnt.getComponent<cro::Sprite>().getTextureBounds();
        iconEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
        iconEnt.addComponent<UIElement>().relativePosition = { 0.5f, 0.f };
        iconEnt.getComponent<UIElement>().absolutePosition = { 0.f, 11.f };
        iconEnt.getComponent<UIElement>().depth = 0.2f;
        iconEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
        bannerEnt.getComponent<cro::Transform>().addChild(iconEnt.getComponent<cro::Transform>());
    }

    //league browser
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("league_icon");
    entity.addComponent<UIElement>().absolutePosition = { 0.f, 5.f };
    entity.getComponent<UIElement>().relativePosition = { 0.6667f, 0.f };
    entity.getComponent<UIElement>().depth = 0.2f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerLeagueBrowser);
    entity.getComponent<cro::UIInput>().setNextIndex(CareerStart, CareerOptions);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerProfile, CareerOptions);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectCursor;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectCursor;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_scene.getSystem<cro::UISystem>()->addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_sharedData.leagueTable = m_sharedData.leagueRoundID;
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    m_scene.getActiveCamera().getComponent<cro::Camera>().active = false;
                    requestStackPush(StateID::League);
                }
            });
    bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    iconEnt = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 16.f, 10.f, 0.1f });;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("View Leagues");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    iconEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    iconEnt.getComponent<cro::UIInput>().area.width += cro::Text::getLocalBounds(entity).width;

    //start
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { -16.f, 5.f };
    entity.getComponent<UIElement>().relativePosition = { 0.98f, 0.f };
    entity.getComponent<UIElement>().depth = 0.2f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("start");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    bounds.left -= 3.f;
    bounds.width += 6.f;
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerStart);
    entity.getComponent<cro::UIInput>().setNextIndex(CareerQuit, CareerOptions);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerLeagueBrowser, CareerInfo);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectCursor;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectCursor;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_scene.getSystem<cro::UISystem>()->addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    saveConfig();

                    //show confirmation
                    enterConfirmCallback();
                }
            });
    bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    auto updateView = [&, rootNode, bannerEnt](cro::Camera& cam) mutable
    {
        glm::vec2 size(GolfGame::getActiveTarget()->getSize());

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        m_viewScale = glm::vec2(getViewScale());
        rootNode.getComponent<cro::Transform>().setScale(m_viewScale);
        //rootNode.getComponent<cro::Transform>().setPosition(size / 2.f);

        //updates any text objects / buttons with a relative position
        cro::Command cmd;
        cmd.targetFlags = CommandID::Menu::UIElement;
        cmd.action =
            [&, size, rootNode](cro::Entity e, float)
        {
            const auto& element = e.getComponent<UIElement>();
            auto pos = (element.relativePosition * size) / m_viewScale;
            pos -= glm::vec2(rootNode.getComponent<cro::Transform>().getPosition()) / m_viewScale;
            pos += element.absolutePosition;

            pos.x = std::floor(pos.x);
            pos.y = std::floor(pos.y);

            e.getComponent<cro::Transform>().setPosition(glm::vec3(pos, element.depth));
        };
        m_scene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

        bannerEnt.getComponent<cro::Callback>().active = true;
    };

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().resizeCallback = updateView;
    m_scene.setActiveCamera(entity);
    updateView(entity.getComponent<cro::Camera>());
}

void CareerState::createConfirmMenu(cro::Entity parent)
{
    auto& menuTransform = parent.getComponent<cro::Transform>();

    auto enter = m_scene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e) mutable
        {
            e.getComponent<cro::Text>().setFillColour(TextGoldColour);
            e.getComponent<cro::AudioEmitter>().play();
        });
    auto exit = m_scene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Text>().setFillColour(TextNormalColour);
        });


    //quit confirmation
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/ui.spt", m_sharedData.sharedResources->textures);


    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("message_board");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().depth = 1.8f;
    entity.addComponent<cro::Callback>().setUserData<ConfirmationData>();
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& data = e.getComponent<cro::Callback>().getUserData<ConfirmationData>();
            float scale = 0.f;
            if (data.dir == ConfirmationData::In)
            {
                data.progress = std::min(1.f, data.progress + (dt * 2.f));
                scale = cro::Util::Easing::easeOutBack(data.progress);

                if (data.progress == 1)
                {
                    e.getComponent<cro::Callback>().active = false;
                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::ConfirmQuit);

                    m_currentMenu = MenuID::ConfirmQuit;
                    m_scene.getSystem<cro::UISystem>()->selectAt(1);
                }
            }
            else
            {
                data.progress = std::max(0.f, data.progress - (dt * 4.f));
                scale = cro::Util::Easing::easeOutQuint(data.progress);
                if (data.progress == 0)
                {
                    e.getComponent<cro::Callback>().active = false;
                    m_currentMenu = MenuID::Career;

                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Career);
                }
            }

            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    auto confirmEnt = entity;


    //quad to darken the screen
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, bounds.height / 2.f, -0.1f });
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-0.5f, 0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(-0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.5f, -0.5f), cro::Colour::Black)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().function =
        [&, confirmEnt](cro::Entity e, float)
        {
            auto scale = confirmEnt.getComponent<cro::Transform>().getScale().x;
            scale = std::min(1.f, scale);

            if (scale > 0)
            {
                auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
                e.getComponent<cro::Transform>().setScale(size / scale);
            }

            auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
            for (auto& v : verts)
            {
                v.colour.setAlpha(BackgroundAlpha * confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().progress);
            }

            e.getComponent<cro::Callback>().active = confirmEnt.getComponent<cro::Callback>().active;
            //m_scene.getActiveCamera().getComponent<cro::Camera>().active = confirmEnt.getComponent<cro::Callback>().active;
        };
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto shadeEnt = entity;



    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);


    //confirmation text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 56.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Start Game?");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    centreText(entity);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //stash this so we can access it from the event handler (escape to ignore etc)
    quitConfirmCallback = [&, confirmEnt, shadeEnt]() mutable
    {
        confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().dir = ConfirmationData::Out;
        confirmEnt.getComponent<cro::Callback>().active = true;
        shadeEnt.getComponent<cro::Callback>().active = true;
        m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
    };

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f) - 20.f, 26.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Text>(font).setString("No");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().setGroup(MenuID::ConfirmQuit);
    entity.getComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = enter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = exit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    quitConfirmCallback();
                }
            });
    centreText(entity);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f) + 20.f, 26.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Text>(font).setString("Yes");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().setGroup(MenuID::ConfirmQuit);
    entity.getComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = enter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = exit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    m_sharedData.hosting = true;
                    m_sharedData.gameMode = GameMode::Career;
                    m_sharedData.localConnectionData.playerCount = 1;
                    m_sharedData.localConnectionData.playerData[0].isCPU = false;

                    //start a local server and connect
                    if (!m_sharedData.clientConnection.connected)
                    {
                        m_sharedData.serverInstance.launch(1, Server::GameMode::Golf, m_sharedData.fastCPU);

                        //small delay for server to get ready
                        cro::Clock clock;
                        while (clock.elapsed().asMilliseconds() < 500) {}

#ifdef USE_GNS
                        m_sharedData.clientConnection.connected = m_sharedData.serverInstance.addLocalConnection(m_sharedData.clientConnection.netClient);
#else
                        m_sharedData.clientConnection.connected = m_sharedData.clientConnection.netClient.connect("255.255.255.255", ConstVal::GamePort);
#endif

                        if (!m_sharedData.clientConnection.connected)
                        {
                            m_sharedData.serverInstance.stop();
                            m_sharedData.errorMessage = "Failed to connect to local server.\nPlease make sure port "
                                + std::to_string(ConstVal::GamePort)
                                + " is allowed through\nany firewalls or NAT";
                            requestStackPush(StateID::Error); //error makes sure to reset any connection
                        }
                        else
                        {
                            m_sharedData.serverInstance.setHostID(m_sharedData.clientConnection.netClient.getPeer().getID());
                            m_sharedData.serverInstance.setLeagueID(m_sharedData.leagueRoundID);
                            
                            //set the course - map directory and hole count is set in selectLeague()
                            auto data = serialiseString(m_sharedData.mapDirectory);
                            m_sharedData.clientConnection.netClient.sendPacket(PacketID::MapInfo, data.data(), data.size(), net::NetFlag::Reliable, ConstVal::NetChannelStrings);

                            //now we wait for the server to send us the map name so we know the
                            //know the course has been set. Then the network event handler 
                            //sends the game rules and launches the game.
                        }
                    }
                }
            });
    centreText(entity);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //displays the message
    enterConfirmCallback = [&, confirmEnt, shadeEnt]() mutable
    {
        m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
        confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().dir = ConfirmationData::In;
        confirmEnt.getComponent<cro::Callback>().active = true;
        shadeEnt.getComponent<cro::Callback>().active = true;

        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
    };
}

void CareerState::createInfoMenu(cro::Entity parent)
{
    auto& menuTransform = parent.getComponent<cro::Transform>();

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/avatar_browser.spt", m_sharedData.sharedResources->textures);

    //background sprite
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().depth = 1.8f;
    entity.addComponent<cro::Callback>().setUserData<ConfirmationData>();
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& data = e.getComponent<cro::Callback>().getUserData<ConfirmationData>();
            float scale = 0.f;
            if (data.dir == ConfirmationData::In)
            {
                data.progress = std::min(1.f, data.progress + (dt * 2.f));
                scale = cro::Util::Easing::easeOutBack(data.progress);

                if (data.progress == 1)
                {
                    e.getComponent<cro::Callback>().active = false;
                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Info);

                    m_currentMenu = MenuID::Info;
                    m_scene.getSystem<cro::UISystem>()->selectAt(1);
                }
            }
            else
            {
                data.progress = std::max(0.f, data.progress - (dt * 2.f));
                scale = cro::Util::Easing::easeOutBack(data.progress);
                if (data.progress == 0)
                {
                    e.getComponent<cro::Callback>().active = false;
                    m_currentMenu = MenuID::Career;

                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Career);
                }
            }

            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    auto confirmEnt = entity;


    //close button
    auto buttonEnt = m_scene.createEntity();
    buttonEnt.addComponent<cro::Transform>().setPosition({ 468.f, 331.f, 0.1f });
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("close_button");
    buttonEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    buttonEnt.addComponent<cro::UIInput>().setGroup(MenuID::Info);
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.getComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [](cro::Entity e) 
            {
                e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
                e.getComponent<cro::AudioEmitter>().play();
            });
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [](cro::Entity e)
            {
                e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
            });
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [&, entity](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    quitInfoCallback();
                }
            });
    buttonEnt.addComponent<cro::Callback>().function = MenuTextCallback();
    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    confirmEnt.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());




    //quad to darken the screen (idk we could probably recycle this from confirm menu, but meh)
    bounds = confirmEnt.getComponent<cro::Sprite>().getTextureBounds();
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, bounds.height / 2.f, -0.1f });
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-0.5f, 0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(-0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.5f, -0.5f), cro::Colour::Black)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().function =
        [&, confirmEnt](cro::Entity e, float)
        {
            auto scale = confirmEnt.getComponent<cro::Transform>().getScale().x;
            scale = std::min(1.f, scale);

            if (scale > 0)
            {
                auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
                e.getComponent<cro::Transform>().setScale(size / scale);
            }

            auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
            for (auto& v : verts)
            {
                v.colour.setAlpha(BackgroundAlpha * confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().progress);
            }

            e.getComponent<cro::Callback>().active = confirmEnt.getComponent<cro::Callback>().active;
        };
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto shadeEnt = entity;


    quitInfoCallback = [&, confirmEnt, shadeEnt]() mutable
    {
        confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().dir = ConfirmationData::Out;
        confirmEnt.getComponent<cro::Callback>().active = true;
        shadeEnt.getComponent<cro::Callback>().active = true;
        m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
    };

    enterInfoCallback = [&, confirmEnt, shadeEnt]() mutable
    {
        m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
        confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().dir = ConfirmationData::In;
        confirmEnt.getComponent<cro::Callback>().active = true;
        shadeEnt.getComponent<cro::Callback>().active = true;

        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
    };

    const auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 298.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("Welcome To Career Mode!");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    std::string desc = R"(
Your career is split across 6 leagues, spanning 36 rounds over 12 courses. Each league
consists of 6 rounds - two on the front 9, two on the back 9 and two rounds played over
18 holes.

Unlike Free Play mode you can leave a Career round at any time, and resume it again when
you're ready. Career mode will even remember which hole you're on!

Standard league rules apply (see the league table for more information). Finish the current
league in the top 5 to unlock the next stage. You can return to an unlocked league at any
time if you want to try and improve your existing score.

Finish a league in the top 3 to unlock a new ball, in the top 2 to unlock a new piece of
headwear and placing number one unlocks a new avatar! Each of these items will also be
available to use in Free Play.

You can reset your Career at any time from the Stats page of the Options menu.

Good Luck!
)";
    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 272.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString(desc);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //animated decorations
    spriteSheet.loadFromFile("assets/golf/sprites/main_menu.spt", m_sharedData.sharedResources->textures);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f) - 19.f, 22.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("flag_base");
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f) + 1.f, 27.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("flag");
    entity.addComponent<cro::SpriteAnimation>().play(0);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    spriteSheet.loadFromFile("assets/golf/sprites/bounce.spt", m_sharedData.sharedResources->textures);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 95.f, 22.f, 0.05f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("bounce");
    entity.getComponent<cro::Sprite>().getAnimations()[0].looped = false;
    entity.addComponent<cro::SpriteAnimation>();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(10.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
            currTime -= dt;
            if (currTime < 0.f
                && m_scene.getSystem<cro::UISystem>()->getActiveGroup() == MenuID::Info)
            {
                currTime += 22.f + cro::Util::Random::value(-4, 3);
                e.getComponent<cro::SpriteAnimation>().play(0);
            }
        };
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void CareerState::createProfileLayout(cro::Entity bgEnt, const cro::SpriteSheet& spriteSheet)
{
    //XP info
    const float xPos = bgEnt.getComponent<cro::Sprite>().getTextureRect().width / 2.f;
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ xPos, 10.f, 0.1f });

    const auto progress = Social::getLevelProgress();
    constexpr float BarWidth = 80.f;

    entity.addComponent<cro::Drawable2D>().setVertexData(createXPBar(BarWidth, progress.progress));
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);
    auto labelEnt = m_scene.createEntity();
    labelEnt.addComponent<cro::Transform>().setPosition({ (-BarWidth / 2.f) + 2.f, 4.f, 0.1f });
    labelEnt.addComponent<cro::Drawable2D>();
    labelEnt.addComponent<cro::Text>(font).setString("Level " + std::to_string(Social::getLevel()));
    labelEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    labelEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    labelEnt.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    labelEnt.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());

    labelEnt = m_scene.createEntity();
    labelEnt.addComponent<cro::Transform>().setPosition({ std::floor(-xPos * 0.6f), 4.f, 0.1f });
    labelEnt.addComponent<cro::Drawable2D>();
    labelEnt.addComponent<cro::Text>(font).setString(std::to_string(progress.currentXP) + "/" + std::to_string(progress.levelXP) + " XP");
    labelEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    labelEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    labelEnt.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    labelEnt.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    centreText(labelEnt);
    entity.getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());

    labelEnt = m_scene.createEntity();
    labelEnt.addComponent<cro::Transform>().setPosition({ std::floor(xPos * 0.6f), 4.f, 0.1f });
    labelEnt.addComponent<cro::Drawable2D>();
    labelEnt.addComponent<cro::Text>(font).setString("Total: " + std::to_string(Social::getXP()) + " XP");
    labelEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    labelEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    labelEnt.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    labelEnt.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    centreText(labelEnt);
    entity.getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());

    //club status - render to a texture to save rendering
    //a whole bunch of different text entities
    if (m_clubTexture.create(210, 36, false))
    {
        auto clubEnt = m_scene.createEntity();
        clubEnt.addComponent<cro::Transform>().setPosition({ 8.f, 26.f, 0.1f });
        clubEnt.addComponent<cro::Drawable2D>();
        clubEnt.addComponent<cro::Sprite>(m_clubTexture.getTexture());
        bgEnt.getComponent<cro::Transform>().addChild(clubEnt.getComponent<cro::Transform>());

        cro::SimpleText text(font);
        text.setFillColour(TextNormalColour);
        text.setShadowColour(LeaderboardTextDark);
        text.setShadowOffset({ 1.f, -1.f });
        text.setCharacterSize(InfoTextSize);

        glm::vec2 position(2.f, 28.f);

        m_clubTexture.clear(cro::Colour::Transparent);

        static constexpr std::int32_t Col = 4;
        static constexpr std::int32_t Row = 3;

        const auto flags = Social::getUnlockStatus(Social::UnlockType::Club);

        for (auto i = 0; i < Row; ++i)
        {
            for (auto j = 0; j < Col; ++j)
            {
                auto idx = i + (j * Row);
                text.setString(Clubs[idx].getLabel() + " ");
                text.setPosition(position);

                if (flags & (1 << idx))
                {
                    if (ClubID::DefaultSet & (1 << idx))
                    {
                        text.setFillColour(TextNormalColour);
                    }
                    else
                    {
                        text.setFillColour(TextGoldColour);
                    }
                }
                else
                {
                    text.setFillColour(TextHighlightColour);
                }

                text.draw();
                position.x += 49.f;
            }
            position.x = 2.f;
            position.y -= 12.f;
        }

        m_clubTexture.display();


        //TODO make this a button which deactivates the scene camera
        //and pushes the stats state on top so we can view the club data
    }
}

void CareerState::selectLeague(std::size_t idx)
{
    const auto& leagueData = Career::instance(m_sharedData).getLeagueData()[idx];
    const auto& league = Career::instance(m_sharedData).getLeagueTables()[idx];

    const auto playlistIdx = league.getCurrentIteration() % Career::MaxLeagues;
    const auto& courseData = m_sharedData.courseData->courseData[leagueData.playlist[playlistIdx].courseID];

    //course title, description, hole count, round number and current league position and best finishing position, course thumbnail
    m_leagueDetails.courseTitle.getComponent<cro::Text>().setString(courseData.title);
    m_leagueDetails.courseDescription.getComponent<cro::Text>().setString(courseData.description);
    
    cro::String str = "All 18 Holes";
    switch (leagueData.playlist[playlistIdx].holeCount)
    {
    default: 
        m_sharedData.holeCount = 0;
        break;
    case 1:
        m_sharedData.holeCount = 1;
        str = "Front 9";
        break;
    case 2:
        m_sharedData.holeCount = 2;
        str = "Back 9";
        break;
    }
    m_leagueDetails.holeCount.getComponent<cro::Text>().setString(str);


    str = "Round " + std::to_string(playlistIdx + 1) + "/6";
    if (m_progressPositions[idx] != 0)
    {
        str += " (Hole " + std::to_string(m_progressPositions[idx]) + ")";
    }

    if (league.getCurrentScore() == 0)
    {
        str += "\n";
    }
    else
    {
        str += "\nCurrent Position: " + std::to_string(league.getCurrentPosition());
        switch (league.getCurrentPosition())
        {
        default:
            str += "th";
            break;
        case 1:
            str += "st";
            break;
        case 2:
            str += "nd";
            break;
        case 3:
            str += "rd";
            break;
        }
    }

    if (league.getCurrentSeason() > 1)
    {
        str += "\nPrevious Best: " + std::to_string(league.getPreviousPosition());

        switch (league.getCurrentBest())
        {
        default:
            str += "th";
            break;
        case 1:
            str += "st";
            break;
        case 2:
            str += "nd";
            break;
        case 3:
            str += "rd";
            break;
        }

        str += " - Completed " + std::to_string(league.getCurrentSeason() - 1) + " time";

        if (league.getCurrentSeason() > 2)
        {
            str += "s";
        }
    }
    m_leagueDetails.leagueDetails.getComponent<cro::Text>().setString(str);

    auto& sharedCourseData = *m_sharedData.courseData;
    if (sharedCourseData.videoPaths.count(courseData.directory) != 0
        && sharedCourseData.videoPlayer.loadFromFile(sharedCourseData.videoPaths.at(courseData.directory)))
    {
        sharedCourseData.videoPlayer.setLooped(true);
        sharedCourseData.videoPlayer.play();
        sharedCourseData.videoPlayer.update(1.f / 30.f);

        m_leagueDetails.thumbnail.getComponent<cro::Sprite>().setTexture(sharedCourseData.videoPlayer.getTexture());
        auto scale = CourseThumbnailSize / glm::vec2(sharedCourseData.videoPlayer.getTexture().getSize());
        m_leagueDetails.thumbnail.getComponent<cro::Transform>().setScale(scale);
    }
    else if (sharedCourseData.courseThumbs.count(courseData.directory) != 0)
    {
        m_leagueDetails.thumbnail.getComponent<cro::Sprite>().setTexture(*m_sharedData.courseData->courseThumbs.at(courseData.directory));
        auto scale = CourseThumbnailSize / glm::vec2(m_sharedData.courseData->courseThumbs.at(courseData.directory)->getSize());
        m_leagueDetails.thumbnail.getComponent<cro::Transform>().setScale(scale);
    }
    else
    {
        m_leagueDetails.thumbnail.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    }

    auto pos = LeagueListPosition;
    pos.y -= idx * LeagueLineSpacing;
    m_leagueDetails.highlight.getComponent<cro::Transform>().setPosition(pos);


    //set this in shared data so it gets sent to the server when we start
    m_sharedData.mapDirectory = courseData.directory;
    m_sharedData.leagueRoundID = LeagueRoundID::RoundOne + idx;
}

void CareerState::applySettingsValues()
{
    if (m_settingsDetails.clubset.isValid())
    {
        m_settingsDetails.clubset.getComponent<cro::SpriteAnimation>().play(m_sharedData.preferredClubSet);
    }
    m_settingsDetails.gimme.getComponent<cro::Text>().setString(GimmeString[m_sharedData.gimmeRadius]);
    const float scale = m_sharedData.nightTime ? 1.f : 0.f;
    m_settingsDetails.night.getComponent<cro::Transform>().setScale(glm::vec2(scale));
}

void CareerState::quitState()
{
    saveConfig();

    if (m_currentMenu == MenuID::ConfirmQuit)
    {
        quitConfirmCallback();
    }
    else if (m_currentMenu == MenuID::Info)
    {
        quitInfoCallback();
    }
    else if (m_currentMenu == MenuID::Career)
    {
        Social::setStatus(Social::InfoID::Menu, { "Main Menu" });

        m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
        m_scene.setSystemActive<cro::UISystem>(false);

        m_rootNode.getComponent<cro::Callback>().active = true;
        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

        auto* msg = postMessage<SystemEvent>(cl::MessageID::SystemMessage);
        msg->type = SystemEvent::MenuRequest;
        msg->data = StateID::Career;
    }
}

void CareerState::loadConfig()
{
    const auto path = Social::getUserContentPath(Social::UserContent::Career) + ConfigFile;
    if (cro::FileSystem::fileExists(path))
    {
        cro::ConfigFile cfg;
        cfg.loadFromFile(path, false);

        for (const auto& prop : cfg.getProperties())
        {
            const auto& name = prop.getName();
            if (name == "gimme")
            {
                m_sharedData.gimmeRadius = static_cast<std::uint8_t>(std::clamp(prop.getValue<std::int32_t>(), 0, 2));
            }
            else if (name == "night")
            {
                m_sharedData.nightTime = static_cast<std::uint8_t>(std::clamp(prop.getValue<std::int32_t>(), 0, 1));
            }
            else if (name == "weather")
            {
                m_sharedData.weatherType = std::clamp(prop.getValue<std::int32_t>(), 0, static_cast<std::int32_t>(WeatherType::Count));
            }
        }

        applySettingsValues();
    }
}

void CareerState::saveConfig() const
{
    cro::ConfigFile cfg;
    cfg.addProperty("gimme").setValue(m_sharedData.gimmeRadius);
    cfg.addProperty("night").setValue(m_sharedData.nightTime);
    cfg.addProperty("weather").setValue(m_sharedData.weatherType);
    cfg.save(Social::getUserContentPath(Social::UserContent::Career) + ConfigFile);
}