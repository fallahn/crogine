/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2026
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

#include "ProLeagueState.hpp"
#include "SharedStateData.hpp"
#include "SharedCourseData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "MessageIDs.hpp"
#include "Utility.hpp"
#include "CallbackData.hpp"
#include "MenuCallbacks.hpp"
#include "TextAnimCallback.hpp"
#include "PacketIDs.hpp"
#include "Clubs.hpp"
#include "../GolfGame.hpp"
#include "../Colordome-32.hpp"
#include "../WebsocketServer.hpp"

#include <Achievements.hpp>
#include <AchievementStrings.hpp>
#include <CompetitionLeague.hpp>
#include <Input.hpp>
#include <Content.hpp>

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

    //constexpr glm::vec3 LeagueListPosition = glm::vec3(119.f, 216.f, 0.2f);
    //constexpr float LeagueLineSpacing = 14.f;

    //const std::string ConfigFile("career.cfg");
}

ProLeagueState::ProLeagueState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
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

//public
bool ProLeagueState::handleEvent(const cro::Event& evt)
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
        if (evt.caxis.value > cro::GameController::LeftThumbDeadZone)
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

void ProLeagueState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::WindowMessage)
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
    else if (msg.id == Social::MessageID::StatsMessage)
    {
        const auto& data = msg.getData<Social::StatEvent>();
        if (data.type == Social::StatEvent::CompetitionLeagueReceived)
        {
            refreshProgressText();
        }
    }

    m_scene.forwardMessage(msg);
}

bool ProLeagueState::simulate(float dt)
{
    m_scene.simulate(dt);
    return true;
}

void ProLeagueState::render()
{
    m_scene.render();
}

//private
void ProLeagueState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::UISystem>(mb);
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

void ProLeagueState::buildScene()
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
                    WebSock::broadcastPacket(Social::setStatus(Social::InfoID::Menu, { "Pondering the Pro League" }));


                    if (!m_sharedData.unlockedItems.empty())
                    {
                        requestStackPush(StateID::Unlock);
                    }
                    //else
                    //{
                    //    auto idx = m_sharedData.leagueRoundID - LeagueRoundID::RoundOne;
                    //    if (idx == 0 && m_progressPositions[idx] == 0 //no completed holes
                    //        && Career::instance(m_sharedData).getLeagueTables()[idx].getCurrentIteration() == 0)
                    //    {
                    //        //if we're on the first league/season show the info
                    //        enterInfoCallback();
                    //    }
                    //}

                    //start title animation
                    cro::Command cmd;
                    cmd.targetFlags = CommandID::Menu::TitleText;
                    cmd.action = [](cro::Entity t, float)
                        {
                            t.getComponent<cro::Callback>().setUserData<float>(0.f);
                            t.getComponent<cro::Callback>().active = true;
                        };
                    m_scene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                    auto* msg = cro::App::getInstance().getMessageBus().post<SystemEvent>(cl::MessageID::SystemMessage);
                    msg->type = SystemEvent::MenuChanged;
                    msg->data = -1;
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
    spriteSheet.loadFromFile("assets/golf/sprites/pro_league_menu.spt", m_sharedData.sharedResources->textures);

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
    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    const auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);

    //title text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ std::floor(bounds.width / 2.f), 224.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("Welcome to the Pro League!");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    const std::string info = 
        R"(
Rules:

    The league lasts one calendar month.
    There are 12 rounds, one on each course, made up of 18 holes.
    Each round can only be played once each month - so bring your A-Game!
    All assists are off - there's no putt assist, and the range indicator is set to Estimated.
    Pro clubs only! Your profile loadout is still applied, so make sure to upgrade your kit.

    Current progress can be viewed from the League Browser, as with other leagues.
)";

    //menu text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 16.f, 216.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString(info);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //progress info (score, current position etc)
    const auto& labelFont = m_sharedData.sharedResources->fonts.get(FontID::Label);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ std::floor(bounds.width / 2.f), 15.f, 0.1f});
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(labelFont).setString(*CompetitionLeague::getCurrentLeaderboard().second);
    entity.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_progressText = entity;

    refreshProgressText();

    //TODO display course info for next round


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



    //entity with confirmation for starting round
    createConfirmMenu(rootNode);


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
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerStart, CareerProfile);
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

    
    auto labelEnt = m_scene.createEntity();
    labelEnt.addComponent<cro::Transform>().setPosition({ 16.f, 13.f });
    labelEnt.addComponent<cro::Drawable2D>();
    labelEnt.addComponent<cro::Text>(smallFont).setString("Back");
    labelEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    labelEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    labelEnt.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    labelEnt.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());
    entity.getComponent<cro::UIInput>().area.width += cro::Text::getLocalBounds(labelEnt).width;


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
    entity.getComponent<cro::UIInput>().setNextIndex(CareerLeagueBrowser, CareerQuit);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerQuit, CareerLeagueBrowser);
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
    auto iconEnt = entity;

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
    entity.getComponent<cro::UIInput>().setNextIndex(CareerStart, CareerProfile);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerProfile, CareerStart);
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
    entity.getComponent<cro::UIInput>().setNextIndex(CareerQuit, CareerLeagueBrowser);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerLeagueBrowser, CareerQuit);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectCursor;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectCursor;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_scene.getSystem<cro::UISystem>()->addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (CompetitionLeague::getCourseIndex() > -1
                    && activated(evt))
                {
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

void ProLeagueState::createConfirmMenu(cro::Entity parent)
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
                //actually we should never get this far if the index is invalid.
                const auto currCourse = CompetitionLeague::getCourseIndex();

                if (currCourse > -1
                    && activated(evt))
                {
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    auto* msg = postMessage<SystemEvent>(cl::MessageID::SystemMessage);
                    msg->type = SystemEvent::MenuRequest;
                    msg->data = RequestID::ProLeague;
                }
            });
    centreText(entity);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    /*const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 44.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("Pro clubs can be tough to use!");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    centreText(entity);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto messageEnt = entity;*/


    //displays the message
    enterConfirmCallback = [&, confirmEnt, /*messageEnt, */shadeEnt]() mutable
    {
        /*if (m_sharedData.preferredClubSet == 2
            && m_sharedData.showInGameTips)
        {
            messageEnt.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        }
        else
        {
            messageEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        }*/

        m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
        confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().dir = ConfirmationData::In;
        confirmEnt.getComponent<cro::Callback>().active = true;
        shadeEnt.getComponent<cro::Callback>().active = true;

        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
    };
}

void ProLeagueState::refreshProgressText()
{
    m_progressText.getComponent<cro::Text>().setString(*CompetitionLeague::getCurrentLeaderboard().second);

    /*const auto currentRound = CompetitionLeague::getCourseIndex();
    if (currentRound == -1)
    {
        m_progressText.getComponent<cro::Text>().setString(*CompetitionLeague::getCurrentLeaderboard().second);
    }
    else
    {
        cro::String str = "Current Round: " + std::to_string(currentRound+1) + "/12 - ";
        str += *CompetitionLeague::getCurrentLeaderboard().second;
        m_progressText.getComponent<cro::Text>().setString(str);
    }*/
}

void ProLeagueState::onCachedPush()
{
    CompetitionLeague::refreshCurrentLeaderboard();
}

void ProLeagueState::quitState()
{
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
        WebSock::broadcastPacket(Social::setStatus(Social::InfoID::Menu, { "Main Menu" }));

        m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
        m_scene.setSystemActive<cro::UISystem>(false);

        m_rootNode.getComponent<cro::Callback>().active = true;
        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

        auto* msg = postMessage<SystemEvent>(cl::MessageID::SystemMessage);
        msg->type = SystemEvent::MenuRequest;
        msg->data = StateID::Career;
    }
}