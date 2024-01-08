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

#include "PlayerManagementState.hpp"
#include "PacketIDs.hpp"
#include "SharedStateData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "TextAnimCallback.hpp"
#include "MessageIDs.hpp"
#include "../GolfGame.hpp"

#include <Social.hpp>

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

    constexpr std::size_t BaseSelectionIndex = 50;

    constexpr std::size_t PokeIndex    = 100;
    constexpr std::size_t ForfeitIndex = 101;
    constexpr std::size_t KickIndex    = 102;
    constexpr std::size_t QuitIndex    = 103;

    constexpr glm::vec2 MenuNodePosition(112.f, -76.f);
    constexpr glm::vec2 MenuHiddenPosition(-10000.f);

    const cro::Time CooldownTime = cro::seconds(10.f);
}

PlayerManagementState::PlayerManagementState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State        (ss, ctx),
    m_scene             (ctx.appInstance.getMessageBus()),
    m_sharedData        (sd),
    m_viewScale         (2.f),
    m_confirmType       (0)
{
    ctx.mainWindow.setMouseCaptured(false);

    buildScene();
}

//public
bool PlayerManagementState::handleEvent(const cro::Event& evt)
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

void PlayerManagementState::handleMessage(const cro::Message& msg)
{
    const auto updateUI =
        [&]()
        {
            m_scene.getSystem<cro::UISystem>()->selectByIndex(QuitIndex);
            refreshPlayerList();
        };

    if (msg.id == MessageID::GolfMessage)
    {
        const auto& data = msg.getData<GolfEvent>();
        if (data.type == GolfEvent::ClientDisconnect)
        {
            updateUI();
        }
    }
    else if (msg.id == Social::MessageID::SocialMessage)
    {
        const auto& data = msg.getData<Social::SocialEvent>();
        if (data.type == Social::SocialEvent::LobbyUpdated)
        {
            updateUI();
        }
    }

    m_scene.forwardMessage(msg);
}

bool PlayerManagementState::simulate(float dt)
{
    m_scene.simulate(dt);
    return true;
}

void PlayerManagementState::render()
{
    m_scene.render();
}

//private
void PlayerManagementState::buildScene()
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
    m_audioEnts[AudioID::Denied] = m_scene.createEntity();
    m_audioEnts[AudioID::Denied].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("nope");

    struct RootCallbackData final
    {
        enum
        {
            FadeIn, FadeOut
        }state = FadeIn;
        float currTime = 0.f;
    };

    auto rootNode = m_scene.createEntity();
    rootNode.addComponent<cro::Transform>().setOrigin({ 0.f, -10.f, 0.f });
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
                refreshPlayerList();

                m_scene.getSystem<cro::UISystem>()->selectByIndex(BaseSelectionIndex);
            }
            break;
        case RootCallbackData::FadeOut:
            currTime = std::max(0.f, currTime - (dt * 2.f));
            e.getComponent<cro::Transform>().setScale(m_viewScale * cro::Util::Easing::easeOutQuint(currTime));
            if (currTime == 0)
            {
                requestStackPop();

                m_scene.setSystemActive<cro::AudioPlayerSystem>(false);

                state = RootCallbackData::FadeIn;
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
    spriteSheet.loadFromFile("assets/golf/sprites/player_management.spt", m_resources.textures);

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    rootNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());


    auto menuEntity = m_scene.createEntity();
    menuEntity.addComponent<cro::Transform>().setPosition(MenuNodePosition);
    rootNode.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());

    auto confirmEntity = m_scene.createEntity();
    confirmEntity.addComponent<cro::Transform>().setPosition(MenuHiddenPosition);
    rootNode.getComponent<cro::Transform>().addChild(confirmEntity.getComponent<cro::Transform>());

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Label);

    //contains the player preview/icon - updated by refreshPreview()
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 80.f, 30.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>();
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_playerList.previewAvatar = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 20.f, 10.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont);
    entity.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_playerList.previewText = entity;

    //contains the player name list - updated by refreshPlayerList()
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -202.f, 101.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setVerticalSpacing(6.f);
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_playerList.text = entity;

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

    auto activatedID = uiSystem.addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                //read button indices and set as active
                auto [client, player] = e.getComponent<cro::Callback>().getUserData<std::pair<std::uint8_t, std::uint8_t>>();
                m_playerList.selectedClient = client;
                m_playerList.selectedPlayer = player;

                refreshPreview();
            }
        });

    //create the button highlights - these will be hidden as needed by refreshPlayerList()
    glm::vec3 highlightPos = glm::vec3(95.f, -5.f, 0.1f);
    for (auto i = 0u; i < 16u; ++i) //TODO shouldn't there be a constval for max total players?
    {
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(highlightPos);
        entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("name_highlight");
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);

        bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        bounds.left += 2.f;
        bounds.width -= 4.f;
        bounds.bottom += 2.f;
        bounds.height -= 4.f;

        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = activatedID;
        entity.getComponent<cro::UIInput>().setSelectionIndex(BaseSelectionIndex + i);
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);

        entity.addComponent<cro::Callback>().function = MenuTextCallback();
        entity.getComponent<cro::Callback>().setUserData<std::pair<std::uint8_t, std::uint8_t>>(0, 0);

        entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
        m_playerList.text.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_playerList.buttons.push_back(entity);
        highlightPos.y -= 14.f;
    }



    selectedID = uiSystem.addCallback(
        [](cro::Entity e) mutable
        {
            e.getComponent<cro::Text>().setFillColour(TextGoldColour); 
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Callback>().active = true;
        });
    unselectedID = uiSystem.addCallback(
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
    entity = createItem(glm::vec2(0.f, -6.f), "Return", menuEntity);
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);
    entity.getComponent<cro::UIInput>().setSelectionIndex(QuitIndex);
    entity.getComponent<cro::UIInput>().setNextIndex(BaseSelectionIndex, PokeIndex);
    entity.getComponent<cro::UIInput>().setPrevIndex(BaseSelectionIndex, KickIndex);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    quitState();
                }
            });


    //confirm message
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(0.f, 2.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(LabelTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    confirmEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto confirmMessage = entity;

    auto setConfirmMessage = 
        [&, menuEntity, confirmEntity, confirmMessage](const cro::String& str, std::int32_t confirmType) mutable
        {
            confirmMessage.getComponent<cro::Text>().setString(str);
            centreText(confirmMessage);

            confirmEntity.getComponent<cro::Transform>().setPosition(MenuNodePosition);
            menuEntity.getComponent<cro::Transform>().setPosition(MenuHiddenPosition);

            m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Confirm);
            m_confirmType = confirmType;
        };


    if (m_sharedData.baseState != StateID::Menu)
    {
        //poke button
        entity = createItem(glm::vec2(0.f, 30.f), "Poke Selected Player", menuEntity);
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);
        entity.getComponent<cro::UIInput>().setSelectionIndex(PokeIndex);
        entity.getComponent<cro::UIInput>().setNextIndex(BaseSelectionIndex, ForfeitIndex);
        entity.getComponent<cro::UIInput>().setPrevIndex(BaseSelectionIndex, QuitIndex);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            uiSystem.addCallback([&, setConfirmMessage](cro::Entity e, cro::ButtonEvent evt) mutable
                {
                    if (activated(evt))
                    {
                        if (m_cooldownTimer.elapsed() > CooldownTime)
                        {
                            m_cooldownTimer.restart();
                            setConfirmMessage("Poke " + m_sharedData.connectionData[m_playerList.selectedClient].playerData[m_playerList.selectedPlayer].name + " ?", ConfirmType::Poke);
                        }
                        else
                        {
                            m_audioEnts[AudioID::Denied].getComponent<cro::AudioEmitter>().play();
                        }
                    }
                });

        //forfeit button
        entity = createItem(glm::vec2(0.f, 18.f), "Forfeit Current Player", menuEntity);
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);
        entity.getComponent<cro::UIInput>().setSelectionIndex(ForfeitIndex);
        entity.getComponent<cro::UIInput>().setNextIndex(BaseSelectionIndex, KickIndex);
        entity.getComponent<cro::UIInput>().setPrevIndex(BaseSelectionIndex, PokeIndex);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            uiSystem.addCallback([&, setConfirmMessage](cro::Entity e, cro::ButtonEvent evt) mutable
                {
                    if (activated(evt))
                    {
                        if (m_cooldownTimer.elapsed() > CooldownTime)
                        {
                            m_cooldownTimer.restart();
                            setConfirmMessage("Forfeit current player's hole?", ConfirmType::Forfeit);
                        }
                        else
                        {
                            m_audioEnts[AudioID::Denied].getComponent<cro::AudioEmitter>().play();
                        }
                    }
                });
    }
    else
    {
        //poke button
        entity = createItem(glm::vec2(0.f, 18.f), "Poke Selected Player", menuEntity);
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);
        entity.getComponent<cro::UIInput>().setSelectionIndex(PokeIndex);
        entity.getComponent<cro::UIInput>().setNextIndex(BaseSelectionIndex, KickIndex);
        entity.getComponent<cro::UIInput>().setPrevIndex(BaseSelectionIndex, QuitIndex);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            uiSystem.addCallback([&, setConfirmMessage](cro::Entity e, cro::ButtonEvent evt) mutable
                {
                    if (activated(evt))
                    {
                        if (m_cooldownTimer.elapsed() > CooldownTime)
                        {
                            m_cooldownTimer.restart();
                            setConfirmMessage("Poke " + m_sharedData.connectionData[m_playerList.selectedClient].playerData[m_playerList.selectedPlayer].name + " ?", ConfirmType::Poke);
                        }
                        else
                        {
                            m_audioEnts[AudioID::Denied].getComponent<cro::AudioEmitter>().play();
                        }
                    }
                });
    }

    //kick button
    entity = createItem(glm::vec2(0.f, 6.f), "Kick Selected Player", menuEntity);
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);
    entity.getComponent<cro::UIInput>().setSelectionIndex(KickIndex);
    entity.getComponent<cro::UIInput>().setNextIndex(BaseSelectionIndex, QuitIndex);
    entity.getComponent<cro::UIInput>().setPrevIndex(BaseSelectionIndex, m_sharedData.baseState == StateID::Menu ? PokeIndex : ForfeitIndex);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&, setConfirmMessage](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    setConfirmMessage("Kick " + m_sharedData.connectionData[m_playerList.selectedClient].playerData[m_playerList.selectedPlayer].name + " ?", ConfirmType::Kick);
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
                    menuEntity.getComponent<cro::Transform>().setPosition(MenuNodePosition);
                    confirmEntity.getComponent<cro::Transform>().setPosition(MenuHiddenPosition);

                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Main);

                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });


    entity = createItem(glm::vec2(20.f, -12.f), "Yes", confirmEntity);
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Confirm);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&,menuEntity,confirmEntity](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    std::uint16_t data = 0;

                    switch (m_confirmType)
                    {
                    default: return;
                    case ConfirmType::Forfeit:
                        data = std::uint16_t(ServerCommand::ForfeitClient) | ((m_playerList.selectedClient) << 8);
                        break;
                    case ConfirmType::Poke:
                        data = std::uint16_t(ServerCommand::PokeClient) | ((m_playerList.selectedClient) << 8);
                        break;
                    case ConfirmType::Kick:
                        data = std::uint16_t(ServerCommand::KickClient) | ((m_playerList.selectedClient) << 8);
                        break;
                    }

                    m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, data, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

                    menuEntity.getComponent<cro::Transform>().setPosition(MenuNodePosition);
                    confirmEntity.getComponent<cro::Transform>().setPosition(MenuHiddenPosition);

                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Main);

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

    refreshPlayerList();

    m_scene.simulate(0.f);
}

void PlayerManagementState::refreshPlayerList()
{
    std::size_t playerCount = 0;

    cro::String str;
    for (auto i = 0; i < ConstVal::MaxClients; ++i)
    {
        for (auto j = 0; j < m_sharedData.connectionData[i].playerCount; ++j)
        {
            str += m_sharedData.connectionData[i].playerData[j].name + "\n";
            m_playerList.buttons[playerCount].getComponent<cro::Callback>().setUserData<std::pair<std::uint8_t, std::uint8_t>>(i, j);
            
            const auto nextUp = (playerCount + (m_playerList.buttons.size() - 1)) % m_playerList.buttons.size();
            const auto nextDown = (playerCount + 1) % m_playerList.buttons.size();

            auto& ip = m_playerList.buttons[playerCount].getComponent<cro::UIInput>();
            ip.enabled = true;
            ip.setNextIndex(KickIndex, BaseSelectionIndex + nextDown);
            ip.setPrevIndex(QuitIndex, BaseSelectionIndex + nextUp);

            playerCount++;
        }
    }
    m_playerList.text.getComponent<cro::Text>().setString(str);

    //set the selection index of the last active button to wrap to 0
    if (playerCount > 1)
    {
        auto& ip = m_playerList.buttons[playerCount - 1].getComponent<cro::UIInput>();
        ip.setNextIndex(KickIndex, BaseSelectionIndex);

        m_playerList.buttons[0].getComponent<cro::UIInput>().setPrevIndex(QuitIndex, BaseSelectionIndex + (playerCount - 1));
    }
    else
    {
        auto& ip = m_playerList.buttons[0].getComponent<cro::UIInput>();
        ip.setNextIndex(PokeIndex, PokeIndex);
        ip.setPrevIndex(QuitIndex, QuitIndex);
    }

    //disable and hide unused highlights
    for (auto i = playerCount; i < m_playerList.buttons.size(); ++i)
    {
        m_playerList.buttons[i].getComponent<cro::UIInput>().enabled = false;
    }

    m_playerList.selectedClient = 0;
    m_playerList.selectedPlayer = 0;
    m_playerList.buttons[0].getComponent<cro::Sprite>().setColour(cro::Colour::White);

    refreshPreview();
}

void PlayerManagementState::refreshPreview()
{
    glm::vec2 texSize(LabelTextureSize);
    texSize.y -= (LabelIconSize.y * 4.f);

    const cro::FloatRect textureRect(0.f, m_playerList.selectedPlayer * (texSize.y / ConstVal::MaxPlayers), texSize.x, texSize.y / ConstVal::MaxPlayers);

    constexpr glm::vec2 AvatarSize(16.f);
    const glm::vec2 AvatarOffset((textureRect.width - AvatarSize.x) / 2.f, textureRect.height + 2.f);
    cro::FloatRect avatarUV(0.f, texSize.y/* / static_cast<float>(LabelTextureSize.y)*/,
        LabelIconSize.x /*/ static_cast<float>(LabelTextureSize.x)*/,
        LabelIconSize.y /*/ static_cast<float>(LabelTextureSize.y)*/);

    float xOffset = (m_playerList.selectedPlayer % 2) * avatarUV.width;
    float yOffset = (m_playerList.selectedPlayer / 2) * avatarUV.height;
    avatarUV.left += xOffset;
    avatarUV.bottom += yOffset;

    
    m_playerList.previewAvatar.getComponent<cro::Sprite>().setTexture(m_sharedData.nameTextures[m_playerList.selectedClient].getTexture());
    m_playerList.previewAvatar.getComponent<cro::Sprite>().setTextureRect(avatarUV);

    cro::String str = m_sharedData.connectionData[m_playerList.selectedClient].playerData[m_playerList.selectedPlayer].name;
    str += "\nClientID: " + std::to_string(m_playerList.selectedClient);
    str += "\nWith " + std::to_string(m_sharedData.connectionData[m_playerList.selectedClient].playerCount - 1) + " other players";

    m_playerList.previewText.getComponent<cro::Text>().setString(str);
}

void PlayerManagementState::quitState()
{
    m_rootNode.getComponent<cro::Callback>().active = true;
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}