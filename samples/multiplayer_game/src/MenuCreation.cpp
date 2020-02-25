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

#include <crogine/detail/GlobalConsts.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>

#include <crogine/ecs/systems/UISystem.hpp>

namespace
{
    const std::uint32_t LargeTextSize = 120;
    const std::uint32_t MediumTextSize = 50;
}

const std::array<glm::vec2, MenuState::MenuID::Count> MenuState::m_menuPositions =
{
    glm::vec2(0.f, 0.f),
    glm::vec2(0.f, cro::DefaultSceneSize.y),
    glm::vec2(-static_cast<float>(cro::DefaultSceneSize.x), cro::DefaultSceneSize.y),
    glm::vec2(-static_cast<float>(cro::DefaultSceneSize.x), 0.f),
    glm::vec2(0.f, 0.f)
};

void MenuState::createMainMenu(cro::Entity parent, std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    auto menuEntity = m_scene.createEntity();
    menuEntity.addComponent<cro::Transform>();
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();

    //title
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 900.f });
    entity.addComponent<cro::Text>(m_font).setString("Title!");
    entity.getComponent<cro::Text>().setCharSize(LargeTextSize);
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //host
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 540.f });
    entity.addComponent<cro::Text>(m_font).setString("Host");
    entity.getComponent<cro::Text>().setCharSize(MediumTextSize);
    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Text>().getLocalBounds();
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseEnter] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseExit] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseUp] =
        m_scene.getSystem<cro::UISystem>().addCallback([&,parent](cro::Entity, std::uint64_t flags) mutable
            {
                if (flags & cro::UISystem::LeftMouse)
                {
                    m_hosting = true;
                    parent.getComponent<cro::Transform>().setPosition(m_menuPositions[MenuID::Avatar]);
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //join
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 480.f });
    entity.addComponent<cro::Text>(m_font).setString("Join");
    entity.getComponent<cro::Text>().setCharSize(MediumTextSize);
    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Text>().getLocalBounds();
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseEnter] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseExit] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseUp] =
        m_scene.getSystem<cro::UISystem>().addCallback([&, parent](cro::Entity, std::uint64_t flags) mutable
            {
                if (flags & cro::UISystem::LeftMouse)
                {
                    m_hosting = false;
                    parent.getComponent<cro::Transform>().setPosition(m_menuPositions[MenuID::Avatar]);
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //options
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 420.f });
    entity.addComponent<cro::Text>(m_font).setString("Options");
    entity.getComponent<cro::Text>().setCharSize(MediumTextSize);
    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Text>().getLocalBounds();
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseEnter] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseExit] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseUp] =
        m_scene.getSystem<cro::UISystem>().addCallback([](cro::Entity, std::uint64_t flags)
            {
                if (flags & cro::UISystem::LeftMouse)
                {
                    cro::Console::show();
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //quit
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 360.f });
    entity.addComponent<cro::Text>(m_font).setString("Quit");
    entity.getComponent<cro::Text>().setCharSize(MediumTextSize);
    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Text>().getLocalBounds();
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseEnter] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseExit] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseUp] =
        m_scene.getSystem<cro::UISystem>().addCallback([](cro::Entity, std::uint64_t flags)
            {
                if (flags & cro::UISystem::LeftMouse)
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

    //title
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 900.f });
    entity.addComponent<cro::Text>(m_font).setString("Player Details");
    entity.getComponent<cro::Text>().setCharSize(LargeTextSize);
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //TODO enter player name


    //back
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 80.f, 120.f });
    entity.addComponent<cro::Text>(m_font).setString("Back");
    entity.getComponent<cro::Text>().setCharSize(MediumTextSize);
    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Text>().getLocalBounds();
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseEnter] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseExit] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseUp] =
        m_scene.getSystem<cro::UISystem>().addCallback([parent](cro::Entity, std::uint64_t flags) mutable
            {
                if (flags & cro::UISystem::LeftMouse)
                {
                    parent.getComponent<cro::Transform>().setPosition(m_menuPositions[MenuID::Main]);
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //continue
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 1920.f - 80.f, 120.f });
    entity.addComponent<cro::Text>(m_font).setString("Continue");
    entity.getComponent<cro::Text>().setCharSize(MediumTextSize);
    //entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Right);
    auto bounds = entity.getComponent<cro::Text>().getLocalBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseEnter] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseExit] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseUp] =
        m_scene.getSystem<cro::UISystem>().addCallback([&,parent](cro::Entity, std::uint64_t flags) mutable
            {
                if (flags & cro::UISystem::LeftMouse)
                {
                    if (m_hosting)
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
                                m_scene.getSystem<cro::CommandSystem>().sendCommand(cmd);
                            }
                        }
                    }
                    else
                    {
                        parent.getComponent<cro::Transform>().setPosition(m_menuPositions[MenuID::Join]);
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

    //title
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 900.f });
    entity.addComponent<cro::Text>(m_font).setString("Join Game");
    entity.getComponent<cro::Text>().setCharSize(LargeTextSize);
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //TODO enter server address

    //back
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 80.f, 120.f });
    entity.addComponent<cro::Text>(m_font).setString("Back");
    entity.getComponent<cro::Text>().setCharSize(MediumTextSize);
    auto bounds = entity.getComponent<cro::Text>().getLocalBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseEnter] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseExit] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseUp] =
        m_scene.getSystem<cro::UISystem>().addCallback([parent](cro::Entity, std::uint64_t flags) mutable
            {
                if (flags & cro::UISystem::LeftMouse)
                {
                    parent.getComponent<cro::Transform>().setPosition(m_menuPositions[MenuID::Main]);
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //join
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 1920.f - 80.f, 120.f });
    entity.addComponent<cro::Text>(m_font).setString("Join");
    entity.getComponent<cro::Text>().setCharSize(MediumTextSize);
    bounds = entity.getComponent<cro::Text>().getLocalBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseEnter] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseExit] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseDown] =
        m_scene.getSystem<cro::UISystem>().addCallback([&, parent](cro::Entity, std::uint64_t flags) mutable
            {
                if (flags & cro::UISystem::LeftMouse)
                {
                    if (!m_sharedData.clientConnection.connected)
                    {
                        m_sharedData.clientConnection.connected = m_sharedData.clientConnection.netClient.connect("255.255.255.255", ConstVal::GamePort);
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
                        m_scene.getSystem<cro::CommandSystem>().sendCommand(cmd);
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

    //title
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 900.f });
    entity.addComponent<cro::Text>(m_font).setString("Lobby");
    entity.getComponent<cro::Text>().setCharSize(LargeTextSize);
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //TODO display lobby members

    //back
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 80.f, 120.f });
    entity.addComponent<cro::Text>(m_font).setString("Back");
    entity.getComponent<cro::Text>().setCharSize(MediumTextSize);
    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Text>().getLocalBounds();
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseEnter] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseExit] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseUp] =
        m_scene.getSystem<cro::UISystem>().addCallback([&,parent](cro::Entity, std::uint64_t flags) mutable
            {
                if (flags & cro::UISystem::LeftMouse)
                {
                    m_sharedData.clientConnection.connected = false;
                    m_sharedData.clientConnection.netClient.disconnect();

                    parent.getComponent<cro::Transform>().setPosition(m_menuPositions[MenuID::Main]);
                    if (m_hosting)
                    {
                        m_sharedData.serverInstance.stop();
                    }
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //start
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 1920.f - 80.f, 120.f });
    entity.addComponent<cro::CommandTarget>().ID = MenuCommandID::ReadyButton;
    entity.addComponent<cro::Text>(m_font).setString("Start");
    entity.getComponent<cro::Text>().setCharSize(MediumTextSize);
    auto bounds = entity.getComponent<cro::Text>().getLocalBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseEnter] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseExit] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseDown] =
        m_scene.getSystem<cro::UISystem>().addCallback([&, parent](cro::Entity, std::uint64_t flags) mutable
            {
                if (flags & cro::UISystem::LeftMouse)
                {
                    if (m_hosting)
                    {
                        //TODO check all members ready
                        if (m_sharedData.clientConnection.connected
                            && m_sharedData.serverInstance.running()) //not running if we're not hosting :)
                        {
                            m_sharedData.clientConnection.netClient.sendPacket(PacketID::RequestGameStart, std::uint8_t(0), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
                        }
                    }
                    else
                    {
                        //toggle readyness
                    }
                }
            });
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f, 0.f });
    menuTransform.addChild(entity.getComponent<cro::Transform>());
}

void MenuState::createOptionsMenu(cro::Entity parent, std::uint32_t, std::uint32_t)
{
    auto menuEntity = m_scene.createEntity();
    menuEntity.addComponent<cro::Transform>();
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();
}