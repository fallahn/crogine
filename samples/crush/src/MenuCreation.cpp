/*-----------------------------------------------------------------------

Matt Marchant 2020
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include "MenuState.hpp"
#include "SharedStateData.hpp"
#include "PacketIDs.hpp"
#include "MenuConsts.hpp"
#include "ServerPacketData.hpp"

#include <crogine/detail/GlobalConsts.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>

#include <crogine/ecs/systems/UISystem.hpp>

#include <cstring>

namespace
{
    bool activated(const cro::ButtonEvent& evt)
    {
        switch (evt.type)
        {
        default: return false;
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEBUTTONDOWN:
            return evt.button.button == SDL_BUTTON_LEFT;
        case SDL_CONTROLLERBUTTONUP:
        case SDL_CONTROLLERBUTTONDOWN:
            return evt.cbutton.button == SDL_CONTROLLER_BUTTON_A;
        case SDL_FINGERUP:
        case SDL_FINGERDOWN:
            return true;
        case SDL_KEYUP:
        case SDL_KEYDOWN:
            return (evt.key.keysym.sym == SDLK_SPACE || evt.key.keysym.sym == SDLK_RETURN);
        }
    }
}

const std::array<glm::vec2, MenuState::MenuID::Count> MenuState::m_menuPositions =
{
    glm::vec2(0.f, 0.f),
    glm::vec2(0.f, cro::DefaultSceneSize.y),
    glm::vec2(-static_cast<float>(cro::DefaultSceneSize.x), cro::DefaultSceneSize.y),
    glm::vec2(-static_cast<float>(cro::DefaultSceneSize.x), 0.f),
    glm::vec2(0.f, 0.f),
    glm::vec2(static_cast<float>(cro::DefaultSceneSize.x), cro::DefaultSceneSize.y)
};

void MenuState::createMainMenu(cro::Entity parent, std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    auto menuEntity = m_scene.createEntity();
    menuEntity.addComponent<cro::Transform>();
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();
    auto& font = m_sharedData.fonts.get(m_sharedData.defaultFontID);

    //title
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 900.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("CRUSH!");
    entity.getComponent<cro::Text>().setCharacterSize(LargeTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //host
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 540.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Host");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Main);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>()->addCallback([&,parent](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.hostState = SharedStateData::HostState::Network;
                    parent.getComponent<cro::Transform>().setPosition(m_menuPositions[MenuID::Avatar]);
                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(GroupID::Avatar);
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //join
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 480.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Join");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Main);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>()->addCallback([&, parent](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.hostState = SharedStateData::HostState::None;
                    parent.getComponent<cro::Transform>().setPosition(m_menuPositions[MenuID::Avatar]);
                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(GroupID::Avatar);
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //local
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 420.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Split Screen");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Main);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>()->addCallback([&, parent](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    //start the local server
                    if (!m_sharedData.clientConnection.connected)
                    {
                        m_sharedData.serverInstance.launch();

                        //small delay for server to get ready
                        cro::Clock clock;
                        while (clock.elapsed().asMilliseconds() < 500) {}

                        m_sharedData.clientConnection.connected = m_sharedData.clientConnection.netClient.connect("255.255.255.255", ConstVal::GamePort);

                        if (!m_sharedData.clientConnection.connected)
                        {
                            m_sharedData.serverInstance.stop();
                            m_sharedData.errorMessage = "Failed to connect to local server.";
                            requestStackPush(States::Error);
                        }
                        else
                        {
                            m_sharedData.hostState = SharedStateData::HostState::Local;
                            parent.getComponent<cro::Transform>().setPosition(m_menuPositions[MenuID::LocalPlay]);
                            m_scene.getSystem<cro::UISystem>()->setActiveGroup(GroupID::LocalPlay);

                            //this is a hack when starting a split screen after quitting a lobby
                            //ideally we'll get rid if this once the UI is built for real (assuming we get that far)
                            cro::Command cmd;
                            cmd.targetFlags = MenuCommandID::ReadyButton;
                            cmd.action = [](cro::Entity e, float)
                            {
                                e.getComponent<cro::Text>().setString("Start");
                            };
                            m_scene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
                        }
                    }
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //options
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 360.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Options");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Main);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>()->addCallback([](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    cro::Console::show();
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //quit
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 200.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Quit");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Main);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>()->addCallback([](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    cro::App::quit();
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());
}

void MenuState::createAvatarMenu(cro::Entity parent, std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    auto menuEntity = m_scene.createEntity();
    menuEntity.addComponent<cro::Transform>();
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();
    menuTransform.setPosition({ 0.f, -static_cast<float>(cro::DefaultSceneSize.y) });

    auto& font = m_sharedData.fonts.get(m_sharedData.defaultFontID);

    //title
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 900.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Player Details");
    entity.getComponent<cro::Text>().setCharacterSize(LargeTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //name text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ glm::vec2(cro::DefaultSceneSize) / 2.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(m_sharedData.localPlayer.name);
    entity.getComponent<cro::Text>().setCharacterSize(SmallTextSize);
    auto bounds = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, -bounds.height / 2.f });
    entity.addComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        //add a cursor to the end of the string when active
        cro::String str = m_sharedData.localPlayer.name + "_";
        e.getComponent<cro::Text>().setString(str);
    };
    menuTransform.addChild(entity.getComponent<cro::Transform>());
    auto textEnt = entity;

    //box background
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ glm::vec2(cro::DefaultSceneSize) / 2.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>().setTexture(m_textureResource.get("assets/images/textbox.png"));
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Avatar);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [&, textEnt](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    auto& callback = textEnt.getComponent<cro::Callback>();
                    callback.active = !callback.active;
                    if (callback.active)
                    {
                        textEnt.getComponent<cro::Text>().setFillColour(TextHighlightColour);
                        m_textEdit.string = &m_sharedData.localPlayer.name;
                        m_textEdit.entity = textEnt;
                        SDL_StartTextInput();
                    }
                    else
                    {
                        applyTextEdit();
                    }
                }
            });

    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //back
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 80.f, 120.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Back");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Avatar);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [&,parent](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    parent.getComponent<cro::Transform>().setPosition(m_menuPositions[MenuID::Main]);
                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(GroupID::Main);
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //continue
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 1920.f - 80.f, 120.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Continue");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    //entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Right);
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Avatar);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [&,parent](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    if (m_sharedData.hostState == SharedStateData::HostState::Network)
                    {
                        if (!m_sharedData.clientConnection.connected)
                        {
                            m_sharedData.serverInstance.launch();

                            //small delay for server to get ready
                            cro::Clock clock;
                            while (clock.elapsed().asMilliseconds() < 500) {}

                            m_sharedData.clientConnection.connected = m_sharedData.clientConnection.netClient.connect("255.255.255.255", ConstVal::GamePort);

                            if (!m_sharedData.clientConnection.connected)
                            {
                                m_sharedData.serverInstance.stop();
                                m_sharedData.errorMessage = "Failed to connect to local server.";
                                requestStackPush(States::Error);
                            }
                            else
                            {
                                cro::Command cmd;
                                cmd.targetFlags = MenuCommandID::ReadyButton;
                                cmd.action = [](cro::Entity e, float)
                                {
                                    e.getComponent<cro::Text>().setString("Start");
                                };
                                m_scene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
                            }
                        }
                    }
                    else
                    {
                        parent.getComponent<cro::Transform>().setPosition(m_menuPositions[MenuID::Join]);
                        m_scene.getSystem<cro::UISystem>()->setActiveGroup(GroupID::Join);
                    }
                }
            });
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f, 0.f });
    menuTransform.addChild(entity.getComponent<cro::Transform>());
}

void MenuState::createJoinMenu(cro::Entity parent, std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    auto menuEntity = m_scene.createEntity();
    menuEntity.addComponent<cro::Transform>();
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();
    menuTransform.setPosition({ cro::DefaultSceneSize.x, -static_cast<float>(cro::DefaultSceneSize.y) });

    auto& font = m_sharedData.fonts.get(m_sharedData.defaultFontID);

    //title
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 900.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Join Game");
    entity.getComponent<cro::Text>().setCharacterSize(LargeTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //ip text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ glm::vec2(cro::DefaultSceneSize) / 2.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(m_sharedData.targetIP);
    entity.getComponent<cro::Text>().setCharacterSize(SmallTextSize);
    auto bounds = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, -bounds.height / 2.f });
    entity.addComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        //add a cursor to the end of the string when active
        cro::String str = m_sharedData.targetIP + "_";
        e.getComponent<cro::Text>().setString(str);
    };
    menuTransform.addChild(entity.getComponent<cro::Transform>());
    auto textEnt = entity;

    //box background
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(cro::DefaultSceneSize.x / 2.f, cro::DefaultSceneSize.y / 2.f, -0.01f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>().setTexture(m_textureResource.get("assets/images/textbox.png"));
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Join);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [&, textEnt](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    auto& callback = textEnt.getComponent<cro::Callback>();
                    callback.active = !callback.active;
                    if (callback.active)
                    {
                        textEnt.getComponent<cro::Text>().setFillColour(TextHighlightColour);
                        m_textEdit.string = &m_sharedData.targetIP;
                        m_textEdit.entity = textEnt;
                        SDL_StartTextInput();
                    }
                    else
                    {
                        applyTextEdit();
                    }
                }
            });

    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //back
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 80.f, 120.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Back");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Join);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>()->addCallback([&,parent](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    parent.getComponent<cro::Transform>().setPosition(m_menuPositions[MenuID::Main]);
                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(GroupID::Main);
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //join
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 1920.f - 80.f, 120.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Join");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Join);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_scene.getSystem<cro::UISystem>()->addCallback([&, parent](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit(); //finish any pending changes
                    if (!m_sharedData.clientConnection.connected)
                    {
                        m_sharedData.clientConnection.connected = m_sharedData.clientConnection.netClient.connect(m_sharedData.targetIP.toAnsiString(), ConstVal::GamePort);
                        if (!m_sharedData.clientConnection.connected)
                        {
                            m_sharedData.errorMessage = "Could not connect to server";
                            requestStackPush(States::Error);
                        }

                        cro::Command cmd;
                        cmd.targetFlags = MenuCommandID::ReadyButton;
                        cmd.action = [](cro::Entity e, float)
                        {
                            e.getComponent<cro::Text>().setString("Ready");
                        };
                        m_scene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                        //reset the map name and wait for the name from the server
                        m_sharedData.mapName = "Has cheeze";
                    }
                }
            });
    
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f, 0.f });
    menuTransform.addChild(entity.getComponent<cro::Transform>());
}

void MenuState::createLobbyMenu(cro::Entity parent, std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    auto menuEntity = m_scene.createEntity();
    menuEntity.addComponent<cro::Transform>();
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();
    menuTransform.setPosition({ cro::DefaultSceneSize.x, 0.f });

    auto& font = m_sharedData.fonts.get(m_sharedData.defaultFontID);

    //title
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 900.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Lobby");
    entity.getComponent<cro::Text>().setCharacterSize(LargeTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //display lobby members
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 400.f, 700.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("No Players...");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::CommandTarget>().ID = MenuCommandID::LobbyList;
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    for (auto i = 0u; i < m_readyState.size(); ++i)
    {
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 360.f, 668.f + (i * -58.f) });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&, i](cro::Entity e, float) 
        {
            cro::Colour c = m_readyState[i] ? cro::Colour::Green : cro::Colour::Red;
            auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
            verts = 
            {
                cro::Vertex2D(glm::vec2(0.f), c),
                cro::Vertex2D(glm::vec2(20.f, 0.f), c),
                cro::Vertex2D(glm::vec2(0.f, 20.f), c),
                cro::Vertex2D(glm::vec2(20.f), c)            
            };
            e.getComponent<cro::Drawable2D>().updateLocalBounds();
        };

        menuTransform.addChild(entity.getComponent<cro::Transform>());
    }


    //display the map name - TODO make this selectable if hosting
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ cro::DefaultSceneSize.x / 2.f, 100.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::Text>().setString(m_sharedData.mapName);
    };
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //back
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 80.f, 120.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Back");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Lobby);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>()->addCallback([&,parent](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.clientConnection.connected = false;
                    m_sharedData.clientConnection.netClient.disconnect();

                    parent.getComponent<cro::Transform>().setPosition(m_menuPositions[MenuID::Main]);
                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(GroupID::Main);
                    if (m_sharedData.hostState == SharedStateData::HostState::Network)
                    {
                        m_sharedData.serverInstance.stop();
                        m_sharedData.hostState = SharedStateData::HostState::None;
                    }
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //start
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 1920.f - 80.f, 120.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = MenuCommandID::ReadyButton;
    entity.addComponent<cro::Text>(font).setString("Start");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    auto bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(GroupID::Lobby);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_scene.getSystem<cro::UISystem>()->addCallback([&, parent](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    if (m_sharedData.hostState == SharedStateData::HostState::Network)
                    {
                        //check all members ready
                        bool ready = true;
                        for (auto i = 0u; i < ConstVal::MaxClients; ++i)
                        {
                            if (!m_sharedData.playerData[i].name.empty()
                                && !m_readyState[i])
                            {
                                ready = false;
                                break;
                            }
                        }

                        if (ready && m_sharedData.clientConnection.connected
                            && m_sharedData.serverInstance.running()) //not running if we're not hosting :)
                        {
                            m_sharedData.clientConnection.netClient.sendPacket(PacketID::RequestGame, std::uint8_t(1), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
                        }
                    }
                    else
                    {
                        //toggle readyness
                        std::uint8_t ready = m_readyState[m_sharedData.clientConnection.connectionID] ? 0 : 1;
                        m_sharedData.clientConnection.netClient.sendPacket(PacketID::LobbyReady, std::uint16_t(m_sharedData.clientConnection.connectionID << 8 | ready),
                            cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
                    }
                }
            });
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f, 0.f });
    menuTransform.addChild(entity.getComponent<cro::Transform>());


    //server info
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 80.f, 1000.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Hosting on port 16002");
    menuTransform.addChild(entity.getComponent<cro::Transform>());
}

void MenuState::createOptionsMenu(cro::Entity parent, std::uint32_t, std::uint32_t)
{
    auto menuEntity = m_scene.createEntity();
    menuEntity.addComponent<cro::Transform>();
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();
}

void MenuState::createLocalMenu(cro::Entity parent, std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    auto menuEntity = m_scene.createEntity();
    menuEntity.addComponent<cro::Transform>();
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();
    menuTransform.setPosition(-m_menuPositions[MenuID::LocalPlay]);

    auto& font = m_sharedData.fonts.get(m_sharedData.defaultFontID);

    //title
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 900.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Local Play");
    entity.getComponent<cro::Text>().setCharacterSize(LargeTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //TODO display number of active players / avatar selection
    //TODO update server with player count/avatar data
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ cro::DefaultSceneSize.x / 2, cro::DefaultSceneSize.y / 2 });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("2 Player");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextHighlightColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<cro::CommandTarget>().ID = MenuCommandID::PlayerIndicator;
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //increase button
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ cro::DefaultSceneSize.x / 2, cro::DefaultSceneSize.y / 2 });
    entity.getComponent<cro::Transform>().move({ 200.f, -10.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(">");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().setGroup(GroupID::LocalPlay);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>()->addCallback([&](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    auto nextCount = std::min(4, m_sharedData.localPlayerCount + 1);
                    if (nextCount != m_sharedData.localPlayerCount)
                    {
                        setPlayerCount(nextCount);
                    }
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //decrease button
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ cro::DefaultSceneSize.x / 2, cro::DefaultSceneSize.y / 2 });
    entity.getComponent<cro::Transform>().move({ -200.f, -10.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("<");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().setGroup(GroupID::LocalPlay);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>()->addCallback([&](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    auto nextCount = std::max(2, m_sharedData.localPlayerCount - 1);
                    if (nextCount != m_sharedData.localPlayerCount)
                    {
                        setPlayerCount(nextCount);
                    }
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //display the map name - TODO make this selectable
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ cro::DefaultSceneSize.x / 2.f, 100.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::Text>().setString(m_sharedData.mapName);
    };
    menuTransform.addChild(entity.getComponent<cro::Transform>());


    //back
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 80.f, 120.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Back");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().setGroup(GroupID::LocalPlay);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>()->addCallback([&, parent](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.clientConnection.connected = false;
                    m_sharedData.clientConnection.netClient.disconnect();

                    parent.getComponent<cro::Transform>().setPosition(m_menuPositions[MenuID::Main]);
                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(GroupID::Main);
                    if (m_sharedData.hostState == SharedStateData::HostState::Local)
                    {
                        m_sharedData.serverInstance.stop();
                        m_sharedData.hostState = SharedStateData::HostState::None;
                    }
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //start
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 1920.f - 80.f, 120.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = MenuCommandID::ReadyButton;
    entity.addComponent<cro::Text>(font).setString("Start");
    entity.getComponent<cro::Text>().setCharacterSize(MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    auto bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(GroupID::LocalPlay);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_scene.getSystem<cro::UISystem>()->addCallback([&, parent](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    if (m_sharedData.hostState == SharedStateData::HostState::Local)
                    {
                        if (m_sharedData.clientConnection.connected
                            && m_sharedData.serverInstance.running()) //not running if we're not hosting :)
                        {
                            m_sharedData.clientConnection.netClient.sendPacket(PacketID::RequestGame, std::uint8_t(1), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
                        }
                    }
                }
            });
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f, 0.f });
    menuTransform.addChild(entity.getComponent<cro::Transform>());
}

void MenuState::updateLobbyData(const cro::NetEvent& evt)
{
    LobbyData data;
    std::memcpy(&data, evt.packet.getData(), sizeof(data));

    auto size = std::min(data.stringSize, static_cast<std::uint8_t>(ConstVal::MaxStringDataSize));
    if (size % sizeof(std::uint32_t) == 0
        && data.playerID < ConstVal::MaxClients)
    {
        std::vector<std::uint32_t> buffer(size / sizeof(std::uint32_t));
        std::memcpy(buffer.data(), static_cast<const std::uint8_t*>(evt.packet.getData()) + sizeof(data), size);

        m_sharedData.playerData[data.playerID].name = cro::String::fromUtf32(buffer.begin(), buffer.end());
    }

    updateLobbyStrings();
}

void MenuState::updateLobbyStrings()
{
    cro::Command cmd;
    cmd.targetFlags = MenuCommandID::LobbyList;
    cmd.action = [&](cro::Entity e, float)
    {
        cro::String str;
        for (const auto& c : m_sharedData.playerData)
        {
            if (!c.name.empty())
            {
                str += c.name + "\n";
            }
        }

        if (!str.empty())
        {
            e.getComponent<cro::Text>().setString(str);
        }
        else
        {
            e.getComponent<cro::Text>().setString("No Players...");
        }
    };
    m_scene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
}

void MenuState::updateReadyDisplay()
{
    for (auto b : m_readyState)
    {
        //these all have their own callback which
        //update the entity state immediate mode
    }
}