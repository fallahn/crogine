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
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "MessageIDs.hpp"
#include "Utility.hpp"
#include "TextAnimCallback.hpp"
#include "../GolfGame.hpp"
#include "../Colordome-32.hpp"

#include <Achievements.hpp>
#include <AchievementStrings.hpp>

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
    constexpr float PadlockOffset = 58.f;

    struct MenuID final
    {
        enum
        {
            Dummy,
            Career, Confirm
        };
    };

    enum
    {
        CareerOptions = 10,
        CareerQuit,
        CareerStart
    };
}

CareerState::CareerState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State(ss, ctx),
    m_scene     (ctx.appInstance.getMessageBus()),
    m_sharedData(sd),
    m_viewScale (2.f)
{
    ctx.mainWindow.setMouseCaptured(false);

    addSystems();
    buildScene();
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
            e.getComponent<cro::Transform>().setScale({ 1.f, m_viewScale.y * cro::Util::Easing::easeOutQuint(currTime) });
            if (currTime == 1)
            {
                state = RootCallbackData::FadeOut;
                e.getComponent<cro::Callback>().active = false;

                m_scene.setSystemActive<cro::UISystem>(true);
                m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Career);
                Social::setStatus(Social::InfoID::Menu, { "Making Career Decisions" });

                if (!m_sharedData.unlockedItems.empty())
                {
                    requestStackPush(StateID::Unlock);
                }
            }
            break;
        case RootCallbackData::FadeOut:
            currTime = std::max(0.f, currTime - (dt * 2.f));
            e.getComponent<cro::Transform>().setScale({ 1.f, m_viewScale.y * cro::Util::Easing::easeOutQuint(currTime) });
            if (currTime == 0)
            {
                requestStackPop();            

                state = RootCallbackData::FadeIn;

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
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto bgEnt = entity;


    //TODO Title



    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Dummy);

    auto selectHighlight = m_scene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
        });
    auto unselectHighlight = m_scene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });


    //league items
    for (auto i = 0u; i < Career::MaxLeagues; ++i)
    {
        //league titles, listed on left
        //if unlocked white text with UI input to start playing
        //else red text, probably padlock icon?

        //activating updates preview
    }
    
    //right box
    //info entities - round x/6, course title, hole count, current league position, best finishing position, lock status(?)
    //probably course thumbnail
    //maybe weather selection/gimme selection
    //probably want to be able to reset the career too
    //club set selection

    //options button
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 461.f, 241.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("options_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerOptions);
    entity.getComponent<cro::UIInput>().setNextIndex(CareerStart, CareerStart);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerStart, CareerStart);
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
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //entity with confirmation for starting round
    createConfirmMenu();



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
    entity.addComponent<UIElement>().absolutePosition = { 20.f, 5.f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("exit");
    /*entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerQuit);
    entity.getComponent<cro::UIInput>().setNextIndex(CareerStart, CareerStart);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerStart, CareerStart);*/
    //entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnterHighlight;
    //entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExitHighlight;
    //entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
    //    m_scene.getSystem<cro::UISystem>()->addCallback(
    //        [&](cro::Entity, const cro::ButtonEvent& evt) mutable
    //        {
    //            if (activated(evt))
    //            {
    //                quitState()
    //            }
    //        });
    bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //profile editor
    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("Edit Profile");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.addComponent<UIElement>().absolutePosition = { -40.f, 15.f };
    entity.getComponent<UIElement>().relativePosition = { 0.3334f, 0.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

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
        iconEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
        bannerEnt.getComponent<cro::Transform>().addChild(iconEnt.getComponent<cro::Transform>());
    }

    //league browser
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("View Leagues");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.addComponent<UIElement>().absolutePosition = { 0.f, 15.f };
    entity.getComponent<UIElement>().relativePosition = { 0.6667f, 0.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //start
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { -16.f, 5.f };
    entity.getComponent<UIElement>().relativePosition = { 0.98f, 0.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("start");
    //entity.addComponent<cro::UIInput>().area = m_sprites[SpriteID::ReadyUp].getTextureBounds();
    //entity.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    //entity.getComponent<cro::UIInput>().setSelectionIndex(LobbyStart);
    //entity.getComponent<cro::UIInput>().setNextIndex(LobbyQuit, LobbyOptions);
    //entity.getComponent<cro::UIInput>().setPrevIndex(LobbyQuit, LobbyInfoB);
    //entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    //entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    //entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
    //    m_uiScene.getSystem<cro::UISystem>()->addCallback([&](cro::Entity, const cro::ButtonEvent& evt) mutable
    //        {
    //            if (activated(evt))
    //            {
    //                //TODO show confirmation
    //            }
    //        });
    bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto startEnt = entity;

    //cursor
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -23.f, 2.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("cursor");
    entity.addComponent<cro::SpriteAnimation>().play(0);
    startEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


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

void CareerState::createConfirmMenu()
{
    //quit confirmation
    //spriteSheet.loadFromFile("assets/golf/sprites/ui.spt", m_resources.textures);

    //struct ConfirmationData final
    //{
    //    float progress = 0.f;
    //    enum
    //    {
    //        In, Out
    //    }dir = In;
    //    bool quitWhenDone = false;

    //    bool startGame = false;
    //};

    //entity = m_uiScene.createEntity();
    //entity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    //entity.addComponent<cro::Drawable2D>();
    //entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("message_board");
    //bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    //entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    //entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    //entity.getComponent<UIElement>().depth = 1.8f;
    //entity.addComponent<cro::Callback>().setUserData<ConfirmationData>();
    //entity.getComponent<cro::Callback>().function =
    //    [&](cro::Entity e, float dt)
    //    {
    //        auto& data = e.getComponent<cro::Callback>().getUserData<ConfirmationData>();
    //        float scale = 0.f;
    //        if (data.dir == ConfirmationData::In)
    //        {
    //            data.progress = std::min(1.f, data.progress + (dt * 2.f));
    //            scale = cro::Util::Easing::easeOutBack(data.progress);

    //            if (data.progress == 1)
    //            {
    //                e.getComponent<cro::Callback>().active = false;
    //                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::ConfirmQuit);
    //                //TODO this needs to be updated so the resize handler correctly
    //                //accounts for this and the scorecard menu
    //                m_currentMenu = MenuID::Lobby;// MenuID::ConfirmQuit;

    //                if (data.startGame)
    //                {
    //                    m_uiScene.getSystem<cro::UISystem>()->selectAt(1);
    //                }
    //            }
    //        }
    //        else
    //        {
    //            data.progress = std::max(0.f, data.progress - (dt * 4.f));
    //            scale = cro::Util::Easing::easeOutQuint(data.progress);
    //            if (data.progress == 0)
    //            {
    //                e.getComponent<cro::Callback>().active = false;
    //                m_currentMenu = MenuID::Lobby;

    //                if (data.quitWhenDone)
    //                {
    //                    quitLobby();
    //                }
    //                else
    //                {
    //                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Lobby);
    //                }
    //                refreshUI();
    //            }
    //        }


    //        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    //    };
    //entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    //menuTransform.addChild(entity.getComponent<cro::Transform>());

    //auto confirmEnt = entity;


    ////quad to darken the screen
    //entity = m_uiScene.createEntity();
    //entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, bounds.height / 2.f, -0.1f });
    //entity.addComponent<cro::Drawable2D>().getVertexData() =
    //{
    //    cro::Vertex2D(glm::vec2(-0.5f, 0.5f), cro::Colour::Black),
    //    cro::Vertex2D(glm::vec2(-0.5f), cro::Colour::Black),
    //    cro::Vertex2D(glm::vec2(0.5f), cro::Colour::Black),
    //    cro::Vertex2D(glm::vec2(0.5f, -0.5f), cro::Colour::Black)
    //};
    //entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    //entity.addComponent<cro::Callback>().function =
    //    [&, confirmEnt](cro::Entity e, float)
    //    {
    //        auto scale = confirmEnt.getComponent<cro::Transform>().getScale().x;
    //        scale = std::min(1.f, scale);

    //        if (scale > 0)
    //        {
    //            auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
    //            e.getComponent<cro::Transform>().setScale(size / scale);
    //        }

    //        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
    //        for (auto& v : verts)
    //        {
    //            v.colour.setAlpha(BackgroundAlpha * confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().progress);
    //        }

    //        e.getComponent<cro::Callback>().active = confirmEnt.getComponent<cro::Callback>().active;
    //        m_uiScene.getActiveCamera().getComponent<cro::Camera>().active = confirmEnt.getComponent<cro::Callback>().active;
    //    };
    //confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    //auto shadeEnt = entity;

    ////confirmation text
    //entity = m_uiScene.createEntity();
    //entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 56.f, 0.1f });
    //entity.addComponent<cro::Drawable2D>();
    //entity.addComponent<cro::Text>(font).setString("Are You Sure?");
    //entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    //entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    //centreText(entity);
    //confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    //auto messageTitleEnt = entity;

    //entity = m_uiScene.createEntity();
    //entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 44.f, 0.1f });
    //entity.addComponent<cro::Drawable2D>();
    //entity.addComponent<cro::Text>(smallFont).setString("This will kick all players.");
    //entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    //entity.getComponent<cro::Text>().setFillColour(cro::Colour::Magenta);
    //centreText(entity);
    //confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    //auto messageEnt = entity;



    ////stash this so we can access it from the event handler (escape to ignore etc)
    //quitConfirmCallback = [&, confirmEnt, shadeEnt]() mutable
    //    {
    //        confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().dir = ConfirmationData::Out;
    //        confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().quitWhenDone = false;
    //        confirmEnt.getComponent<cro::Callback>().active = true;
    //        shadeEnt.getComponent<cro::Callback>().active = true;
    //        m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
    //        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
    //    };

    //entity = m_uiScene.createEntity();
    //entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f) - 20.f, 26.f, 0.1f });
    //entity.addComponent<cro::Drawable2D>();
    //entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    //entity.addComponent<cro::Text>(font).setString("No");
    //entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    //entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    //entity.addComponent<cro::UIInput>().setGroup(MenuID::ConfirmQuit);
    //entity.getComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    //entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = enter;
    //entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = exit;
    //entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
    //    m_uiScene.getSystem<cro::UISystem>()->addCallback(
    //        [&](cro::Entity e, const cro::ButtonEvent& evt) mutable
    //        {
    //            if (activated(evt))
    //            {
    //                quitConfirmCallback();
    //            }
    //        });
    //centreText(entity);
    //confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //entity = m_uiScene.createEntity();
    //entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f) + 20.f, 26.f, 0.1f });
    //entity.addComponent<cro::Drawable2D>();
    //entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    //entity.addComponent<cro::Text>(font).setString("Yes");
    //entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    //entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    //entity.addComponent<cro::UIInput>().setGroup(MenuID::ConfirmQuit);
    //entity.getComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    //entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = enter;
    //entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = exit;
    //entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
    //    m_uiScene.getSystem<cro::UISystem>()->addCallback(
    //        [&, confirmEnt, shadeEnt](cro::Entity e, const cro::ButtonEvent& evt) mutable
    //        {
    //            if (activated(evt))
    //            {
    //                auto& data = confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>();

    //                if (!data.startGame)
    //                {
    //                    data.dir = ConfirmationData::Out;
    //                    data.quitWhenDone = true;
    //                    confirmEnt.getComponent<cro::Callback>().active = true;
    //                    shadeEnt.getComponent<cro::Callback>().active = true;
    //                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
    //                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

    //                    //restore the rules tab if necessary
    //                    float scale = m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().getScale().y;
    //                    if (scale == 0)
    //                    {
    //                        m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().setScale({ 1.f, 1.f });
    //                        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
    //                    }
    //                }
    //                else
    //                {
    //                    m_sharedData.clientConnection.netClient.sendPacket(PacketID::RequestGameStart, std::uint8_t(sv::StateID::Golf), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
    //                }
    //            }
    //        });
    //centreText(entity);
    //confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    ////back
    //enterConfirmCallback = [&, confirmEnt, shadeEnt, messageEnt, messageTitleEnt](bool quit) mutable
    //    {
    //        m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
    //        confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().dir = ConfirmationData::In;
    //        confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().quitWhenDone = false;
    //        confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().startGame = !quit;
    //        confirmEnt.getComponent<cro::Callback>().active = true;
    //        shadeEnt.getComponent<cro::Callback>().active = true;

    //        if (quit)
    //        {
    //            messageTitleEnt.getComponent<cro::Text>().setString("Are You Sure?");
    //            centreText(messageTitleEnt);
    //            messageEnt.getComponent<cro::Text>().setFillColour(m_sharedData.hosting ? TextNormalColour : cro::Colour::Transparent);
    //        }
    //        else
    //        {
    //            //continue message
    //            messageEnt.getComponent<cro::Text>().setFillColour(cro::Colour::Transparent);

    //            messageTitleEnt.getComponent<cro::Text>().setString("Start Game?");
    //            centreText(messageTitleEnt);
    //        }
    //        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
    //    };

    //entity = m_uiScene.createEntity();
    //entity.addComponent<cro::Transform>();
    //entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    //entity.addComponent<cro::Drawable2D>();
    //entity.addComponent<UIElement>().absolutePosition = { 20.f, MenuBottomBorder };
    //entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    //entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::PrevMenu];
    //entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    //entity.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    //entity.getComponent<cro::UIInput>().setSelectionIndex(LobbyQuit);
    //entity.getComponent<cro::UIInput>().setNextIndex(LobbyStart, LobbyStart);
    //entity.getComponent<cro::UIInput>().setPrevIndex(LobbyStart, LobbyRulesA); //TODO dynamically update these with active menu
    //entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnterHighlight;
    //entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExitHighlight;
    //entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
    //    m_uiScene.getSystem<cro::UISystem>()->addCallback(
    //        [&](cro::Entity, const cro::ButtonEvent& evt) mutable
    //        {
    //            if (activated(evt))
    //            {
    //                enterConfirmCallback(true);
    //            }
    //        });
    //menuTransform.addChild(entity.getComponent<cro::Transform>());
}


void CareerState::quitState()
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