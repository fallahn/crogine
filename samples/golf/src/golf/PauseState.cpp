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

#include "PauseState.hpp"
#include "SharedStateData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "TextAnimCallback.hpp"
#include "MessageIDs.hpp"
#include "../GolfGame.hpp"

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

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

using namespace cl;

namespace
{
    struct MenuID final
    {
        enum
        {
            Main, Confirm
        };
    };

    std::function<void()> resetConfirmation;
}

PauseState::PauseState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State        (ss, ctx),
    m_scene             (ctx.appInstance.getMessageBus()),
    m_sharedData        (sd),
    m_viewScale         (2.f),
    m_requestRestart    (false),
    m_confirmationType  (ConfirmType::Quit)
{
    ctx.mainWindow.setMouseCaptured(false);

    buildScene();

    cacheState(StateID::Options);
    cacheState(StateID::PlayerManagement); //TODO don't need to do this in the driving range
}

//public
bool PauseState::handleEvent(const cro::Event& evt)
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
        if (evt.cbutton.button == cro::GameController::ButtonB
            || evt.cbutton.button == cro::GameController::ButtonStart)
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

void PauseState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool PauseState::simulate(float dt)
{
    m_scene.simulate(dt);
    return /*true*/m_sharedData.baseState != StateID::DrivingRange;
}

void PauseState::render()
{
    m_scene.render();
}

//private
void PauseState::buildScene()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::UISystem>(mb);// ->setActiveControllerID(m_sharedData.inputBinding.controllerID);
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

            {
                auto reset = (m_sharedData.baseState == StateID::DrivingRange);
                m_restartButton.getComponent<cro::UIInput>().enabled = reset;
                m_restartButton.getComponent<cro::Transform>().setScale(glm::vec2(reset ? 1.f : 0.f));
            }

            {
                auto active = m_sharedData.minimapData.active;
                m_minimapButton.getComponent<cro::UIInput>().enabled = active;
                m_minimapButton.getComponent<cro::Transform>().setScale(glm::vec2(active ? 1.f : 0.f));
            }

            {
                auto players = (m_sharedData.baseState == StateID::Golf && m_sharedData.hosting);
                m_playerButton.getComponent<cro::UIInput>().enabled = players;
                m_playerButton.getComponent<cro::Transform>().setScale(glm::vec2(players ? 1.f : 0.f));
            }

            if (currTime == 1)
            {
                state = RootCallbackData::FadeOut;
                e.getComponent<cro::Callback>().active = false;

                m_scene.setSystemActive<cro::AudioPlayerSystem>(true);
                m_scene.getSystem<cro::UISystem>()->selectAt(0);
            }
            break;
        case RootCallbackData::FadeOut:
            currTime = std::max(0.f, currTime - (dt * 2.f));
            e.getComponent<cro::Transform>().setScale(m_viewScale * cro::Util::Easing::easeOutQuint(currTime));
            if (currTime == 0)
            {
                resetConfirmation();
                requestStackPop();

                m_scene.setSystemActive<cro::AudioPlayerSystem>(false);

                state = RootCallbackData::FadeIn;

                if (m_requestRestart)
                {
                    auto* msg = postMessage<SystemEvent>(MessageID::SystemMessage);
                    msg->type = SystemEvent::RestartActiveMode;

                    m_requestRestart = false;
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
    spriteSheet.loadFromFile("assets/golf/sprites/facilities_menu.spt", m_sharedData.sharedResources->textures);

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    rootNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());

    auto menuEntity = m_scene.createEntity();
    menuEntity.addComponent<cro::Transform>();
    rootNode.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());

    auto confirmEntity = m_scene.createEntity();
    confirmEntity.addComponent<cro::Transform>().setPosition(glm::vec2(-10000.f));
    rootNode.getComponent<cro::Transform>().addChild(confirmEntity.getComponent<cro::Transform>());

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto& uiSystem = *m_scene.getSystem<cro::UISystem>();

    auto selectedID = uiSystem.addCallback(
        [](cro::Entity e) mutable
        {
            e.getComponent<cro::Text>().setFillColour(TextGoldColour); 
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Callback>().active = true;
        });
    auto unselectedID = uiSystem.addCallback(
        [](cro::Entity e) 
        { 
            e.getComponent<cro::Text>().setFillColour(TextNormalColour);
        });
    
    auto createItem = [&](glm::vec2 position, const std::string& label, cro::Entity parent) 
    {
        auto e = m_scene.createEntity();
        e.addComponent<cro::Transform>().setPosition(position);
        e.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        e.addComponent<cro::Drawable2D>();
        e.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
        e.getComponent<cro::Text>().setString(label);
        e.getComponent<cro::Text>().setFillColour(TextNormalColour);
        centreText(e);
        e.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(e);
        e.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
        e.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;

        e.addComponent<cro::Callback>().function = MenuTextCallback();

        parent.getComponent<cro::Transform>().addChild(e.getComponent<cro::Transform>());
        return e;
    };

    //return to game
    entity = createItem(glm::vec2(0.f, 28.f), "Return", menuEntity);
    entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    quitState();
                }
            });


    //options button
    entity = createItem(glm::vec2(0.f, 17.f), "Options", menuEntity);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt) 
            {
                if (activated(evt))
                {
                    requestStackPush(StateID::Options);
                }            
            });



    //restart button
    entity = createItem(glm::vec2(0.f, 6.f), "Restart Round", menuEntity);
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&, menuEntity, confirmEntity](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    confirmEntity.getComponent<cro::Transform>().setPosition(glm::vec2(0.f));
                    menuEntity.getComponent<cro::Transform>().setPosition(glm::vec2(-10000.f));

                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Confirm);
                    m_confirmationType = ConfirmType::Restart;
                }
            });
    entity.getComponent<cro::UIInput>().enabled = (m_sharedData.baseState == StateID::DrivingRange);
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    m_restartButton = entity;


    //minimap button
    entity = createItem(glm::vec2(0.f, 6.f), "Hole Overview", menuEntity);
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    requestStackPush(StateID::MapOverview);
                }
            });
    entity.getComponent<cro::UIInput>().enabled = false;
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    m_minimapButton = entity;

    //player management
    entity = createItem(glm::vec2(0.f, -5.f), "Player Management", menuEntity);
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    requestStackPush(StateID::PlayerManagement);
                }
            });
    entity.getComponent<cro::UIInput>().enabled = false;
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    m_playerButton = entity;


    //quit button
    entity = createItem(glm::vec2(0.f, -26.f), "Quit To Menu", menuEntity);
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&, menuEntity, confirmEntity](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    confirmEntity.getComponent<cro::Transform>().setPosition(glm::vec2(0.f));
                    menuEntity.getComponent<cro::Transform>().setPosition(glm::vec2(-10000.f));

                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Confirm);
                    m_confirmationType = ConfirmType::Quit;
                }
            });


    //confirmation buttons
    entity = createItem(glm::vec2(-20.f, -12.f), "No", confirmEntity);
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Confirm);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&,menuEntity, confirmEntity](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    menuEntity.getComponent<cro::Transform>().setPosition(glm::vec2(0.f));
                    confirmEntity.getComponent<cro::Transform>().setPosition(glm::vec2(-10000.f));

                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Main);

                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });


    entity = createItem(glm::vec2(20.f, -12.f), "Yes", confirmEntity);
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Confirm);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    if (m_confirmationType == ConfirmType::Quit)
                    {
                        //this is a kludge which tells the
                        //menu state to remove any existing connection/server instance
                        //rather than disconnecting here which would raise an error message
                        m_sharedData.tutorial = true;

                        requestStackClear();
                        //requestStackPush(StateID::Menu);
                        if (m_sharedData.baseState != StateID::Clubhouse)
                        {
                            requestStackPush(StateID::Menu);
                        }
                        else
                        {
                            requestStackPush(StateID::Clubhouse);
                        }
                    }
                    else
                    {
                        m_requestRestart = true;

                        //this is done by quitState()
                        /*menuEntity.getComponent<cro::Transform>().setPosition(glm::vec2(0.f));
                        confirmEntity.getComponent<cro::Transform>().setPosition(glm::vec2(-10000.f));

                        m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Main);*/

                        quitState();
                    }
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(0.f, 12.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setString("Are You Sure?");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    centreText(entity);
    confirmEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    resetConfirmation = [&, menuEntity, confirmEntity]() mutable
    {
        menuEntity.getComponent<cro::Transform>().setPosition(glm::vec2(0.f));
        confirmEntity.getComponent<cro::Transform>().setPosition(glm::vec2(-10000.f));

        m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Main);
    };



    if (m_sharedData.hosting
        && !m_sharedData.tutorial)
    {
        auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec2(0.f, 2.f));
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
        entity.getComponent<cro::Text>().setString("This Will Kick All Players");
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        centreText(entity);
        confirmEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }


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
}

void PauseState::quitState()
{
    //m_scene.setSystemActive<cro::AudioPlayerSystem>(false);
    m_rootNode.getComponent<cro::Callback>().active = true;
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}