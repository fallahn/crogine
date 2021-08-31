/*-----------------------------------------------------------------------

Matt Marchant 2021
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
#include "Utility.hpp"
#include "CommandIDs.hpp"
#include "../ErrorCheck.hpp"
#include "server/ServerPacketData.hpp"

#include <crogine/detail/GlobalConsts.hpp>
#include <crogine/core/ConfigFile.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/ecs/systems/UISystem.hpp>

#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/util/Easings.hpp>

#include <crogine/detail/OpenGL.hpp>

#include <cstring>

namespace
{

}

void MenuCallback::operator()(cro::Entity e, float dt)
{
    static constexpr float Speed = 2.f;
    auto& menuData = e.getComponent<cro::Callback>().getUserData<MenuData>();
    if (menuData.direction == MenuData::In)
    {
        //expand vertically
        menuData.currentTime = std::min(1.f, menuData.currentTime + (dt * Speed));
        e.getComponent<cro::Transform>().setScale({ 1.f, cro::Util::Easing::easeInQuint(menuData.currentTime) });

        if (menuData.currentTime == 1)
        {
            //stop here
            menuData.direction = MenuData::Out;
            e.getComponent<cro::Callback>().active = false;
            menuState.m_uiScene.getSystem<cro::UISystem>().setActiveGroup(menuData.targetMenu);
            menuState.m_currentMenu = menuData.targetMenu;
        }
    }
    else
    {
        //contract horizontally
        menuData.currentTime = std::max(0.f, menuData.currentTime - (dt * Speed));
        e.getComponent<cro::Transform>().setScale({ cro::Util::Easing::easeInQuint(menuData.currentTime), 1.f });

        if (menuData.currentTime == 0)
        {
            //stop this callback
            menuData.direction = MenuData::In;
            e.getComponent<cro::Callback>().active = false;

            //set target position and init target animation
            auto* positions = &menuState.m_menuPositions;
            auto viewScale = menuState.m_viewScale;

            cro::Command cmd;
            cmd.targetFlags = CommandID::Menu::RootNode;
            cmd.action = [positions, menuData, viewScale](cro::Entity e, float)
            {
                e.getComponent<cro::Transform>().setPosition(positions->at(menuData.targetMenu) * viewScale);
            };
            menuState.m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

            menuState.m_menuEntities[menuData.targetMenu].getComponent<cro::Callback>().active = true;
            menuState.m_menuEntities[menuData.targetMenu].getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = menuData.targetMenu;
        }
    }
}


constexpr std::array<glm::vec2, MenuState::MenuID::Count> MenuState::m_menuPositions =
{
    glm::vec2(0.f, 0.f),
    glm::vec2(0.f, MenuSpacing.y),
    glm::vec2(-MenuSpacing.x, MenuSpacing.y),
    glm::vec2(-MenuSpacing.x, 0.f),
    glm::vec2(0.f, 0.f),
    glm::vec2(0.f, 0.f)
};

void MenuState::createUI()
{
    auto mouseEnterCallback = m_uiScene.getSystem<cro::UISystem>().addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Text>().setFillColour(TextHighlightColour);
        });
    auto mouseExitCallback = m_uiScene.getSystem<cro::UISystem>().addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Text>().setFillColour(TextNormalColour);
        });

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_backgroundTexture.getTexture());
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    auto courseEnt = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::RootNode;
    auto rootNode = entity;

    //consumes input during menu animation.
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Dummy);

    createPlayerConfigMenu(mouseEnterCallback, mouseExitCallback);
    createMainMenu(rootNode, mouseEnterCallback, mouseExitCallback);
    createAvatarMenu(rootNode, mouseEnterCallback, mouseExitCallback);
    createJoinMenu(rootNode, mouseEnterCallback, mouseExitCallback);
    createLobbyMenu(rootNode, mouseEnterCallback, mouseExitCallback);

    //ui viewport is set 1:1 with window, then the scene
    //is scaled to best-fit to maintain pixel accuracy of text.
    auto updateView = [&, rootNode, courseEnt](cro::Camera& cam) mutable
    {
        auto windowSize = cro::App::getWindow().getSize();
        glm::vec2 size(windowSize);

        if (m_sharedData.usePostProcess)
        {
            m_postBuffer.create(windowSize.x, windowSize.y, false);
            m_postQuad.setTexture(m_postBuffer.getTexture());
            auto shaderRes = size;// / 1.f;
            glCheck(glUseProgram(m_postShader.getGLHandle()));
            glCheck(glUniform2f(m_postShader.getUniformID("u_resolution"), shaderRes.x, shaderRes.y));
        }

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        auto vpSize = calcVPSize();

        m_viewScale = glm::vec2(std::floor(size.y / vpSize.y));
        rootNode.getComponent<cro::Transform>().setScale(m_viewScale);
        rootNode.getComponent<cro::Transform>().setPosition(m_menuPositions[m_currentMenu] * m_viewScale);

        courseEnt.getComponent<cro::Transform>().setScale(m_viewScale);
        courseEnt.getComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, -1.f));
        courseEnt.getComponent<cro::Transform>().setOrigin(vpSize / 2.f);
        courseEnt.getComponent<cro::Sprite>().setTextureRect({ 0.f, 0.f, vpSize.x, vpSize.y });

        //updates any text objects / buttons with a relative position
        cro::Command cmd;
        cmd.targetFlags = CommandID::Menu::UIElement;
        cmd.action =
            [&,size](cro::Entity e, float)
        {
            const auto& element = e.getComponent<UIElement>();
            auto pos = element.absolutePosition;
            pos += element.relativePosition * size / m_viewScale;

            pos.x = std::floor(pos.x);
            pos.y = std::floor(pos.y);

            e.getComponent<cro::Transform>().setPosition(glm::vec3(pos, element.depth));
        };
        m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
    };

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().resizeCallback = updateView;
    m_uiScene.setActiveCamera(entity);
    updateView(entity.getComponent<cro::Camera>());
}

void MenuState::createMainMenu(cro::Entity parent, std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    auto menuEntity = m_uiScene.createEntity();
    menuEntity.addComponent<cro::Transform>();
    menuEntity.addComponent<cro::Callback>().setUserData<MenuData>();
    menuEntity.getComponent<cro::Callback>().getUserData<MenuData>().direction = MenuData::Out;
    menuEntity.getComponent<cro::Callback>().getUserData<MenuData>().currentTime = 1.f;
    menuEntity.getComponent<cro::Callback>().function = MenuCallback(*this);
    m_menuEntities[MenuID::Main] = menuEntity;
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();
    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);

    //title
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<UIElement>().absolutePosition = { 10.f, 0.f };
    entity.getComponent<UIElement>().relativePosition = { 0.f, 0.9f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Golf!");
    entity.getComponent<cro::Text>().setCharacterSize(SmallTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //host
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 10.f, 80.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Create Game");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextHighlightColour);
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>().addCallback([&, menuEntity](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.hosting = true;

                    m_uiScene.getSystem<cro::UISystem>().setActiveGroup(MenuID::Dummy);
                    menuEntity.getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Avatar;
                    menuEntity.getComponent<cro::Callback>().active = true;
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //join
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 10.f, 70.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Join Game");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>().addCallback([&, menuEntity](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.hosting = false;

                    m_uiScene.getSystem<cro::UISystem>().setActiveGroup(MenuID::Dummy);
                    menuEntity.getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Avatar;
                    menuEntity.getComponent<cro::Callback>().active = true;
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //driving range
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 10.f, 60.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Driving Range (Tutorial)");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>().addCallback([](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //options
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 10.f, 50.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Options");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>().addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    requestStackPush(StateID::Options);
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //quit
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 10.f, 40.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Quit");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>().addCallback([](cro::Entity, const cro::ButtonEvent& evt)
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
    auto menuEntity = m_uiScene.createEntity();
    menuEntity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    menuEntity.addComponent<cro::Callback>().setUserData<MenuData>();
    menuEntity.getComponent<cro::Callback>().function = MenuCallback(*this);
    m_menuEntities[MenuID::Avatar] = menuEntity;
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());
    

    //background
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/player_menu.spt", m_resources.textures);

    m_sprites[SpriteID::ArrowLeft] = spriteSheet.getSprite("arrow_l");
    m_sprites[SpriteID::ArrowLeftHighlight] = spriteSheet.getSprite("arrow_l_h");
    m_sprites[SpriteID::ArrowRight] = spriteSheet.getSprite("arrow_r");
    m_sprites[SpriteID::ArrowRightHighlight] = spriteSheet.getSprite("arrow_r_h");
    m_sprites[SpriteID::Controller] = spriteSheet.getSprite("controller");
    m_sprites[SpriteID::Keyboard] = spriteSheet.getSprite("keyboard");

    //this entity has the player edit text ents added to it by updateLocalAvatars
    auto avatarEnt = m_uiScene.createEntity();
    avatarEnt.addComponent<cro::Transform>();
    avatarEnt.addComponent<cro::Drawable2D>();
    avatarEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    avatarEnt.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    avatarEnt.getComponent<UIElement>().depth = -0.2f;
    avatarEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    auto bounds = avatarEnt.getComponent<cro::Sprite>().getTextureBounds();
    avatarEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

    menuEntity.getComponent<cro::Transform>().addChild(avatarEnt.getComponent<cro::Transform>());
    m_avatarMenu = avatarEnt;

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();
    menuTransform.setPosition(-m_menuPositions[MenuID::Avatar]);

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);

    //title
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<UIElement>().absolutePosition = { 10.f, 0.f };
    entity.getComponent<UIElement>().relativePosition = { 0.f, 0.9f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Player Details");
    entity.getComponent<cro::Text>().setCharacterSize(SmallTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    menuTransform.addChild(entity.getComponent<cro::Transform>());


    //back
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 10.f, MenuBottomBorder });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Back");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>().addCallback(
            [&, menuEntity](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    m_uiScene.getSystem<cro::UISystem>().setActiveGroup(MenuID::Dummy);
                    menuEntity.getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Main;
                    menuEntity.getComponent<cro::Callback>().active = true;
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());



    //add player button
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 0.f, MenuBottomBorder };
    entity.getComponent<UIElement>().relativePosition = { 0.2f, 0.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Text>(font).setString("Add Player");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bounds = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::Transform>().move({ -360.f, 0.f });
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>().addCallback(
            [&, mouseEnter, mouseExit](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    if (m_sharedData.localConnectionData.playerCount < m_sharedData.localConnectionData.MaxPlayers)
                    {
                        auto index = m_sharedData.localConnectionData.playerCount;
                        
                        if (m_sharedData.localConnectionData.playerData[index].name.empty())
                        {
                            m_sharedData.localConnectionData.playerData[index].name = "Player " + std::to_string(index + 1);
                        }
                        m_sharedData.localConnectionData.playerCount++;

                        updateLocalAvatars(mouseEnter, mouseExit);
                    }
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //remove player button
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 0.f, MenuBottomBorder };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 0.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Text>(font).setString("Remove Player");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bounds = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::Transform>().move({ 10.f, 0.f });
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>().addCallback(
            [&, mouseEnter, mouseExit](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    
                    if (m_sharedData.localConnectionData.playerCount > 1)
                    {
                        m_sharedData.localConnectionData.playerCount--;
                        updateLocalAvatars(mouseEnter, mouseExit);
                    }
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());



    //continue
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 0.f, MenuBottomBorder };
    entity.getComponent<UIElement>().relativePosition = { 0.98f, 0.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Text>(font).setString("Continue");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>().addCallback(
            [&, menuEntity](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    saveAvatars();

                    if (m_sharedData.hosting)
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
                                requestStackPush(StateID::Error);
                            }
                            else
                            {
                                cro::Command cmd;
                                cmd.targetFlags = CommandID::Menu::ReadyButton;
                                cmd.action = [](cro::Entity e, float)
                                {
                                    e.getComponent<cro::Text>().setString("Start");
                                };
                                m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

                                cmd.targetFlags = CommandID::Menu::ServerInfo;
                                cmd.action = [](cro::Entity e, float)
                                {
                                    e.getComponent<cro::Text>().setString("Hosting on: localhost:" + std::to_string(ConstVal::GamePort));
                                };
                                m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

                                //send the selected map/course
                                //TODO this should be sent from lobby menu so host can change the selected course
                                auto data = serialiseString(m_sharedData.mapDirectory);
                                m_sharedData.clientConnection.netClient.sendPacket(PacketID::MapInfo, data.data(), data.size(), cro::NetFlag::Reliable, ConstVal::NetChannelStrings);
                            }
                        }
                    }
                    else
                    {
                        m_uiScene.getSystem<cro::UISystem>().setActiveGroup(MenuID::Dummy);
                        menuEntity.getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Join;
                        menuEntity.getComponent<cro::Callback>().active = true;
                    }
                }
            });
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f, 0.f });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    updateLocalAvatars(mouseEnter, mouseExit);
}

void MenuState::createJoinMenu(cro::Entity parent, std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    auto menuEntity = m_uiScene.createEntity();
    menuEntity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    menuEntity.addComponent<cro::Callback>().setUserData<MenuData>();
    menuEntity.getComponent<cro::Callback>().function = MenuCallback(*this);
    m_menuEntities[MenuID::Join] = menuEntity;
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();
    menuTransform.setPosition(-m_menuPositions[MenuID::Join]);

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);

    //title
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<UIElement>().absolutePosition = { 10.f, 0.f };
    entity.getComponent<UIElement>().relativePosition = { 0.f, 0.9f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Join Game");
    entity.getComponent<cro::Text>().setCharacterSize(SmallTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //ip text
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(m_sharedData.targetIP);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
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
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().depth = -0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>().setTexture(m_resources.textures.get("assets/golf/images/textbox.png"));
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Join);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>().addCallback(
            [&, textEnt](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    auto& callback = textEnt.getComponent<cro::Callback>();
                    callback.active = !callback.active;
                    if (callback.active)
                    {
                        textEnt.getComponent<cro::Text>().setFillColour(TextEditColour);
                        m_textEdit.string = &m_sharedData.targetIP;
                        m_textEdit.entity = textEnt;
                        m_textEdit.maxLen = ConstVal::MaxStringChars;
                        SDL_StartTextInput();
                    }
                    else
                    {
                        applyTextEdit();
                    }
                }
            });
    textEnt.getComponent<cro::Transform>().setPosition(entity.getComponent<cro::Transform>().getOrigin());
    textEnt.getComponent<cro::Transform>().move({ 0.f, 0.f, 0.1f });
    entity.getComponent<cro::Transform>().addChild(textEnt.getComponent<cro::Transform>());
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //back
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 10.f, MenuBottomBorder };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Text>(font).setString("Back");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Join);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>().addCallback([&, menuEntity](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    m_uiScene.getSystem<cro::UISystem>().setActiveGroup(MenuID::Dummy);
                    menuEntity.getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Avatar;
                    menuEntity.getComponent<cro::Callback>().active = true;
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //join
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 0.f, MenuBottomBorder };
    entity.getComponent<UIElement>().relativePosition = { 0.98f, 0.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Text>(font).setString("Join");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Join);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>().addCallback([&, parent](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit(); //finish any pending changes
                    if (!m_sharedData.targetIP.empty() &&
                        !m_sharedData.clientConnection.connected)
                    {
                        m_sharedData.clientConnection.connected = m_sharedData.clientConnection.netClient.connect(m_sharedData.targetIP.toAnsiString(), ConstVal::GamePort);
                        if (!m_sharedData.clientConnection.connected)
                        {
                            m_sharedData.errorMessage = "Could not connect to server";
                            requestStackPush(StateID::Error);
                        }

                        cro::Command cmd;
                        cmd.targetFlags = CommandID::Menu::ReadyButton;
                        cmd.action = [](cro::Entity e, float)
                        {
                            e.getComponent<cro::Text>().setString("Ready");
                        };
                        m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

                        cmd.targetFlags = CommandID::Menu::ServerInfo;
                        cmd.action = [&](cro::Entity e, float)
                        {
                            e.getComponent<cro::Text>().setString("Connected to: " + m_sharedData.targetIP + ":" + std::to_string(ConstVal::GamePort));
                        };
                        m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
                    }
                }
            });
    
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f, 0.f });
    menuTransform.addChild(entity.getComponent<cro::Transform>());
}

void MenuState::createLobbyMenu(cro::Entity parent, std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    auto menuEntity = m_uiScene.createEntity();
    menuEntity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    menuEntity.addComponent<cro::Callback>().setUserData<MenuData>();
    menuEntity.getComponent<cro::Callback>().function = MenuCallback(*this);
    m_menuEntities[MenuID::Lobby] = menuEntity;
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();
    menuTransform.setPosition(-m_menuPositions[MenuID::Lobby]);

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);

    //title
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<UIElement>().absolutePosition = { 10.f, 0.f };
    entity.getComponent<UIElement>().relativePosition = { 0.f, 0.9f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Lobby");
    entity.getComponent<cro::Text>().setCharacterSize(SmallTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //display lobby members
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().relativePosition = { 0.2f, 0.65f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::LobbyList;
    entity.addComponent<cro::Text>(font).setString("No Players...");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setVerticalSpacing(2.f);
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    auto textEnt = entity;
    for (auto i = 0u; i < m_readyState.size(); ++i)
    {
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ -12.f, (i * -10.f/*TODO this should be line spacing*/) - 7.f});
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&, i](cro::Entity e, float)
        {
            cro::Colour c =
                m_sharedData.connectionData[i].playerCount == 0 ? cro::Colour::Transparent :
                m_readyState[i] ? cro::Colour::Green : cro::Colour::Red;

            auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
            verts =
            {
                cro::Vertex2D(glm::vec2(0.f), c),
                cro::Vertex2D(glm::vec2(8.f, 0.f), c),
                cro::Vertex2D(glm::vec2(0.f, 8.f), c),
                cro::Vertex2D(glm::vec2(8.f), c)
            };
            e.getComponent<cro::Drawable2D>().updateLocalBounds();
        };

        textEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }

    //back
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 10.f, MenuBottomBorder };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Text>(font).setString("Leave");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>().addCallback([&, menuEntity](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.clientConnection.connected = false;
                    m_sharedData.clientConnection.netClient.disconnect();

                    m_uiScene.getSystem<cro::UISystem>().setActiveGroup(MenuID::Dummy);
                    menuEntity.getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Main;
                    menuEntity.getComponent<cro::Callback>().active = true;

                    if (m_sharedData.hosting)
                    {
                        m_sharedData.serverInstance.stop();
                        m_sharedData.hosting = false;
                    }
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //start
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 0.f, MenuBottomBorder };
    entity.getComponent<UIElement>().relativePosition = { 0.98f, 0.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::ReadyButton;
    entity.addComponent<cro::Text>(font).setString("Start");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    auto bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>().addCallback([&, parent](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    if (m_sharedData.hosting)
                    {
                        //check all members ready
                        bool ready = true;
                        for (auto i = 0u; i < ConstVal::MaxClients; ++i)
                        {
                            if (m_sharedData.connectionData[i].playerCount != 0
                                && !m_readyState[i])
                            {
                                ready = false;
                                break;
                            }
                        }

                        if (ready && m_sharedData.clientConnection.connected
                            && m_sharedData.serverInstance.running()) //not running if we're not hosting :)
                        {
                            m_sharedData.clientConnection.netClient.sendPacket(PacketID::RequestGameStart, std::uint8_t(0), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
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

    //server info message
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<UIElement>().absolutePosition = { 10.f, 0.f };
    entity.getComponent<UIElement>().relativePosition = { 0.f, 0.98f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::ServerInfo;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_sharedData.sharedResources->fonts.get(FontID::Info)).setString("Connected to");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(cro::Colour::White);
    menuTransform.addChild(entity.getComponent<cro::Transform>());
}

void MenuState::createPlayerConfigMenu(std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    cro::Colour c(0.f, 0.f, 0.f, BackgroundAlpha);
    auto fadeNode = m_uiScene.createEntity();
    fadeNode.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.2f }); //relative to bgNode!
    fadeNode.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-0.5f, 0.5f), c),
        cro::Vertex2D(glm::vec2(-0.5f), c),
        cro::Vertex2D(glm::vec2(0.5f), c),
        cro::Vertex2D(glm::vec2(0.5f, -0.5f), c)
    };
    fadeNode.getComponent<cro::Drawable2D>().updateLocalBounds();


    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/player_selection.spt", m_resources.textures);

    auto bgNode = m_uiScene.createEntity();
    bgNode.addComponent<cro::Transform>();
    bgNode.addComponent<cro::Drawable2D>();
    bgNode.addComponent<cro::CommandTarget>().ID = CommandID::Menu::PlayerConfig;
    bgNode.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    auto bounds = bgNode.getComponent<cro::Sprite>().getTextureBounds();
    bgNode.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

    bgNode.addComponent<cro::Callback>().active = true;
    bgNode.getComponent<cro::Callback>().setUserData<std::pair<float, float>>(0.f, 0.f);
    bgNode.getComponent<cro::Callback>().function =
        [&, fadeNode](cro::Entity e, float dt) mutable
    {
        auto& [current, target] = e.getComponent<cro::Callback>().getUserData<std::pair<float, float >>();
        float scale = current;

        if (current < target)
        {
            current = std::min(current + dt * 2.f, target);
            scale = cro::Util::Easing::easeOutBounce(current);
        }
        else if (current > target)
        {
            current = std::max(current - dt * 3.f, target);
            scale = cro::Util::Easing::easeOutQuint(current);
        }

        auto size = glm::vec2(cro::App::getWindow().getSize());
        e.getComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, 1.f));
        e.getComponent<cro::Transform>().setScale(m_viewScale * scale);

        fadeNode.getComponent<cro::Transform>().setScale(size * current);
        float alpha = cro::Util::Easing::easeInQuint(current) * BackgroundAlpha;
        auto& verts = fadeNode.getComponent<cro::Drawable2D>().getVertexData();
        for (auto& v : verts)
        {
            v.colour.setAlpha(alpha);
        }
    };

    fadeNode.getComponent<cro::Transform>().setPosition(bgNode.getComponent<cro::Transform>().getOrigin());
    bgNode.getComponent<cro::Transform>().addChild(fadeNode.getComponent<cro::Transform>());

    auto selected = m_uiScene.getSystem<cro::UISystem>().addCallback(
        [](cro::Entity e) 
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
        });
    auto unselected = m_uiScene.getSystem<cro::UISystem>().addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });


    static constexpr float ButtonDepth = 0.1f;
    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);

    //player name text
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 79.f, 171.f, ButtonDepth });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::PlayerName;

    entity.addComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        if (m_textEdit.string != nullptr)
        {
            auto str = *m_textEdit.string;
            if (str.size() < ConstVal::MaxNameChars)
            {
                str += "_";
            }
            e.getComponent<cro::Text>().setString(str);
        }
    };

    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto textEnt = entity;

    auto createButton = [&](glm::vec2 pos, const std::string& sprite)
    {
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(pos, ButtonDepth));
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite(sprite);
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().setGroup(MenuID::PlayerConfig);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selected;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselected;
        bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        return entity;
    };

    //I need to finish the layout editor :3
    entity = createButton({ 74.f, 159.f }, "name_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::White);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>().addCallback(
            [&, textEnt](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    auto& callback = textEnt.getComponent<cro::Callback>();
                    callback.active = !callback.active;
                    if (callback.active)
                    {
                        textEnt.getComponent<cro::Text>().setFillColour(TextEditColour);
                        m_textEdit.string = &m_sharedData.localConnectionData.playerData[callback.getUserData<std::uint8_t>()].name;
                        m_textEdit.entity = textEnt;
                        m_textEdit.maxLen = ConstVal::MaxNameChars;
                        SDL_StartTextInput();
                    }
                    else
                    {
                        applyTextEdit();
                    }
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //callbacks for colour swap arrows
    auto arrowLeftCallback = [&](const cro::ButtonEvent& evt, pc::ColourKey::Index idx)
    {
        if (activated(evt))
        {
            applyTextEdit();

            auto paletteIdx = m_sharedData.localConnectionData.playerData[m_playerAvatar.activePlayer].avatarFlags[idx];
            paletteIdx = (paletteIdx + (pc::ColourID::Count - 1)) % pc::ColourID::Count;
            m_playerAvatar.setColour(idx, paletteIdx);

            m_sharedData.localConnectionData.playerData[m_playerAvatar.activePlayer].avatarFlags[idx] = paletteIdx;
        }
    };

    auto arrowRightCallback = [&](const cro::ButtonEvent& evt, pc::ColourKey::Index idx)
    {
        if (activated(evt))
        {
            applyTextEdit();

            auto paletteIdx = m_sharedData.localConnectionData.playerData[m_playerAvatar.activePlayer].avatarFlags[idx];
            paletteIdx = (paletteIdx + 1) % pc::ColourID::Count;
            m_playerAvatar.setColour(idx, paletteIdx);

            m_sharedData.localConnectionData.playerData[m_playerAvatar.activePlayer].avatarFlags[idx] = paletteIdx;
        }
    };

    //c1 left
    entity = createButton({ 19.f, 127.f }, "arrow_left");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>().addCallback([&, arrowLeftCallback](cro::Entity, const cro::ButtonEvent& evt)
            {
                arrowLeftCallback(evt, pc::ColourKey::Hair);
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //c1 right
    entity = createButton({ 52.f, 127.f }, "arrow_right");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>().addCallback([&, arrowRightCallback](cro::Entity, const cro::ButtonEvent& evt)
            {
                arrowRightCallback(evt, pc::ColourKey::Hair);
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //c2 left
    entity = createButton({ 19.f, 102.f }, "arrow_left");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>().addCallback([&, arrowLeftCallback](cro::Entity, const cro::ButtonEvent& evt)
            {
                arrowLeftCallback(evt, pc::ColourKey::Skin);
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //c2 right
    entity = createButton({ 52.f, 102.f }, "arrow_right");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>().addCallback([&, arrowRightCallback](cro::Entity, const cro::ButtonEvent& evt)
            {
                arrowRightCallback(evt, pc::ColourKey::Skin);
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //c3 left
    entity = createButton({ 19.f, 77.f }, "arrow_left");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>().addCallback([&, arrowLeftCallback](cro::Entity, const cro::ButtonEvent& evt)
            {
                arrowLeftCallback(evt, pc::ColourKey::Top);
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //c3 right
    entity = createButton({ 52.f, 77.f }, "arrow_right");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>().addCallback([&, arrowRightCallback](cro::Entity, const cro::ButtonEvent& evt)
            {
                arrowRightCallback(evt, pc::ColourKey::Top);
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //c4 left
    entity = createButton({ 19.f, 52.f }, "arrow_left");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>().addCallback([&, arrowLeftCallback](cro::Entity, const cro::ButtonEvent& evt)
            {
                arrowLeftCallback(evt, pc::ColourKey::Bottom);
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //c4 right
    entity = createButton({ 52.f, 52.f }, "arrow_right");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>().addCallback([&, arrowRightCallback](cro::Entity, const cro::ButtonEvent& evt)
            {
                arrowRightCallback(evt, pc::ColourKey::Bottom);
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //colour preview
    auto position = glm::vec2(36.f, 52.f);
    for (auto i = 0u; i < 4u; ++i)
    {
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(position, 0.1f));
        entity.addComponent<cro::Drawable2D>().getVertexData() =
        {
            cro::Vertex2D(glm::vec2(0.f, 15.f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(0.f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(15.f), cro::Colour::White),
            
            cro::Vertex2D(glm::vec2(15.f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(0.f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(15, 0.f), cro::Colour::White)
        };
        entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
        entity.getComponent<cro::Drawable2D>().updateLocalBounds();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&, i](cro::Entity e, float)
        {
            auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
            for (auto j = 0u; j < verts.size(); ++j)
            {
                auto idx = m_sharedData.localConnectionData.playerData[m_playerAvatar.activePlayer].avatarFlags[i];
                verts[j].colour = j < 3 ? pc::Palette[idx].light : pc::Palette[idx].dark;
            }
        };

        bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        position.y += 25.f;
    }


    //skin left
    entity = createButton({ 90.f, 92.f }, "arrow_left");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>().addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    auto skinID = m_sharedData.localConnectionData.playerData[m_playerAvatar.activePlayer].skinID;
                    skinID = (skinID + (PlayerAvatar::MaxSkins - 1)) % PlayerAvatar::MaxSkins;
                    m_sharedData.localConnectionData.playerData[m_playerAvatar.activePlayer].skinID = skinID;

                    cro::Command cmd;
                    cmd.targetFlags = CommandID::Menu::PlayerAvatar;
                    cmd.action = [&, skinID](cro::Entity en, float)
                    {
                        en.getComponent<cro::Sprite>().setTextureRect(m_playerAvatar.previewRects[skinID]);
                    };
                    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //skin right
    entity = createButton({ 172.f, 92.f }, "arrow_right");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>().addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    auto skinID = m_sharedData.localConnectionData.playerData[m_playerAvatar.activePlayer].skinID;
                    skinID = (skinID + 1) % PlayerAvatar::MaxSkins;
                    m_sharedData.localConnectionData.playerData[m_playerAvatar.activePlayer].skinID = skinID;

                    cro::Command cmd;
                    cmd.targetFlags = CommandID::Menu::PlayerAvatar;
                    cmd.action = [&, skinID](cro::Entity en, float)
                    {
                        en.getComponent<cro::Sprite>().setTextureRect(m_playerAvatar.previewRects[skinID]);
                    };
                    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //done
    entity = createButton({ 75.f, 15.f }, "button_highlight");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>().addCallback(
            [&, mouseEnter, mouseExit](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    showPlayerConfig(false, m_playerAvatar.activePlayer);
                    updateLocalAvatars(mouseEnter, mouseExit);
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //player preview
    spriteSheet.loadFromFile("assets/golf/sprites/player.spt", m_resources.textures);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 113.f, 68.f, ButtonDepth });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::PlayerAvatar;
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("female_wood");
    entity.getComponent<cro::Sprite>().setTexture(m_playerAvatar.getTexture(), false);
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    m_playerAvatar.previewRects[0] = spriteSheet.getSprite("female_wood").getTextureRect();
    m_playerAvatar.previewRects[1] = spriteSheet.getSprite("male_wood").getTextureRect();
    //m_playerAvatar.previewRects[2] = spriteSheet.getSprite("female_wood_l").getTextureRect();
    //m_playerAvatar.previewRects[3] = spriteSheet.getSprite("male_wood_l").getTextureRect();
}

void MenuState::updateLocalAvatars(std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    //these can have fixed positions as they are attached to a menuEntity[] which is UI scaled
    static constexpr glm::vec3 EditButtonOffset(-47.f, -57.f, 0.f);
    static constexpr glm::vec3 AvatarOffset = EditButtonOffset + glm::vec3(-68.f, -10.f, 0.f);
    static constexpr glm::vec3 ControlIconOffset = AvatarOffset + glm::vec3(115.f, 34.f, 0.f);
    static constexpr float LineHeight = -8.f;

    for (auto e : m_avatarListEntities)
    {
        m_uiScene.destroyEntity(e);
    }
    m_avatarListEntities.clear();

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);

    static constexpr glm::vec3 RootPos(131.f, 174.f, 0.f);
    for (auto i = 0u; i < m_sharedData.localConnectionData.playerCount; ++i)
    {
        auto localPos = glm::vec3(
            173.f * static_cast<float>(i % 2),
            -(LineHeight + m_playerAvatar.previewRects[0].height) * static_cast<float>(i / 2),
            0.1f);
        localPos += RootPos;

        //player name
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(localPos);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setString(m_sharedData.localConnectionData.playerData[i].name);
        entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
        entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
        entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);

        m_avatarMenu.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_avatarListEntities.push_back(entity);

        //add avatar preview
        if (m_sharedData.avatarTextures[0][i].getSize().x == 0)
        {
            //this is the first time the texture was displayed so
            //updated it from the current (loaded from disk) settings
            const auto& flags = m_sharedData.localConnectionData.playerData[i].avatarFlags;
            m_playerAvatar.setColour(pc::ColourKey::Bottom, flags[0]);
            m_playerAvatar.setColour(pc::ColourKey::Top, flags[1]);
            m_playerAvatar.setColour(pc::ColourKey::Skin, flags[2]);
            m_playerAvatar.setColour(pc::ColourKey::Hair, flags[3]);

            m_playerAvatar.setTarget(m_sharedData.avatarTextures[0][i]);
            m_playerAvatar.apply();
        }

        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(localPos + AvatarOffset);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>(m_sharedData.avatarTextures[0][i]);
        entity.getComponent<cro::Sprite>().setTextureRect(m_playerAvatar.previewRects[m_sharedData.localConnectionData.playerData[i].skinID]);
        m_avatarMenu.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_avatarListEntities.push_back(entity);

        //add edit button
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(localPos + EditButtonOffset);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setString("EDIT");
        entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        auto bounds = cro::Text::getLocalBounds(entity);
        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
            m_uiScene.getSystem<cro::UISystem>().addCallback(
                [&, i](cro::Entity, const cro::ButtonEvent& evt) mutable
                {
                    if (activated(evt))
                    {
                        showPlayerConfig(true, i);
                    }
                });
        m_avatarMenu.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_avatarListEntities.push_back(entity);

        //input type icon
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(localPos + ControlIconOffset);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::Keyboard];
        bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

        struct ControlUserData final
        {
            std::size_t prevControllerCount = 0;
            std::array<cro::Entity, 2u> arrowEntities = {};
        };

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<ControlUserData>();
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float)
        {
            auto& controlData = e.getComponent<cro::Callback>().getUserData<ControlUserData>();
            auto controllerCount = 2;// cro::GameController::getControllerCount();
            if (controllerCount != controlData.prevControllerCount)
            {
                //we need to delete specifically the button
                //arrows should we be going to < 2 controllers
                if (controllerCount < 2 && controlData.arrowEntities[0].isValid())
                {
                    //remove from main list first
                    m_avatarListEntities.erase(std::remove_if(m_avatarListEntities.begin(), m_avatarListEntities.end(),
                        [&controlData](const cro::Entity a)
                        {
                            return (a == controlData.arrowEntities[0] || a == controlData.arrowEntities[1]);
                        }), m_avatarListEntities.end());

                    m_uiScene.destroyEntity(controlData.arrowEntities[0]);
                    m_uiScene.destroyEntity(controlData.arrowEntities[1]);
                    controlData.arrowEntities = {};
                }


                if (controllerCount == 0)
                {
                    e.getComponent<cro::Sprite>() = m_sprites[SpriteID::Keyboard];
                }
                else
                {
                    e.getComponent<cro::Sprite>() = m_sprites[SpriteID::Controller];

                    //add buttons for selecting controller ID
                    if (controllerCount > 1)
                    {
                        auto ent = m_uiScene.createEntity();
                        ent.addComponent<cro::Transform>().setPosition({ -22.f, 7.f, 0.f });
                        ent.addComponent<cro::Drawable2D>();
                        ent.addComponent<cro::Sprite>() = m_sprites[SpriteID::ArrowLeft];
                        e.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
                        m_avatarListEntities.push_back(ent);
                        controlData.arrowEntities[0] = ent;

                        ent = m_uiScene.createEntity();
                        ent.addComponent<cro::Transform>().setPosition({ 55.f, 7.f, 0.f });
                        ent.addComponent<cro::Drawable2D>();
                        ent.addComponent<cro::Sprite>() = m_sprites[SpriteID::ArrowRight];
                        e.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
                        m_avatarListEntities.push_back(ent);
                        controlData.arrowEntities[1] = ent;

                        //TODO add UI input to above
                        //TODO apply controller ID to player
                        //TODO add indicator for current controller ID
                    }
                }

                auto bb = e.getComponent<cro::Sprite>().getTextureBounds();
                e.getComponent<cro::Transform>().setOrigin({ bb.width / 2.f, bb.height / 2.f });
            }
            controlData.prevControllerCount = controllerCount;
        };

        m_avatarMenu.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_avatarListEntities.push_back(entity);
    }
}

void MenuState::updateLobbyData(const cro::NetEvent& evt)
{
    ConnectionData cd;
    if (cd.deserialise(evt.packet))
    {
        m_sharedData.connectionData[cd.connectionID] = cd;
    }

    updateLobbyAvatars();
}

void MenuState::updateLobbyAvatars()
{
    //TODO detect only the avatars which changed
    //so we don't needlessly update textures

    cro::Command cmd;
    cmd.targetFlags = CommandID::Menu::LobbyList;
    cmd.action = [&](cro::Entity e, float)
    {
        const auto applyTexture = [&](cro::Texture& targetTexture, const std::array<uint8_t, 4u>& flags)
        {
            m_playerAvatar.setTarget(targetTexture);
            for (auto j = 0u; j < flags.size(); ++j)
            {
                m_playerAvatar.setColour(pc::ColourKey::Index(j), flags[j]);
            }
            m_playerAvatar.apply();
        };

        cro::String str;
        for (const auto& c : m_sharedData.connectionData)
        {
            if (c.playerCount > 0)
            {
                str += c.playerData[0].name;

                applyTexture(m_sharedData.avatarTextures[c.connectionID][0], c.playerData[0].avatarFlags);

                for (auto i = 1u; i < c.playerCount; ++i)
                {
                    str += ", " + c.playerData[i].name;

                    applyTexture(m_sharedData.avatarTextures[c.connectionID][i], c.playerData[i].avatarFlags);
                }
                str += "\n";
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
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
}

void MenuState::showPlayerConfig(bool visible, std::uint8_t playerIndex)
{
    m_playerAvatar.activePlayer = playerIndex;

    cro::Command cmd;
    cmd.targetFlags = CommandID::Menu::PlayerConfig;
    cmd.action = [&,visible](cro::Entity e, float)
    {
        float target = visible ? 1.f : 0.f;
        std::size_t menu = visible ? MenuID::PlayerConfig : MenuID::Avatar;

        e.getComponent<cro::Callback>().getUserData<std::pair<float, float>>().second = target;
        m_uiScene.getSystem<cro::UISystem>().setActiveGroup(menu);
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

    cmd.targetFlags = CommandID::Menu::PlayerName;
    cmd.action = [&](cro::Entity e, float)
    {
        e.getComponent<cro::Text>().setString(m_sharedData.localConnectionData.playerData[m_playerAvatar.activePlayer].name);
        e.getComponent<cro::Callback>().setUserData<std::uint8_t>(m_playerAvatar.activePlayer);
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

    cmd.targetFlags = CommandID::Menu::PlayerAvatar;
    cmd.action = [&](cro::Entity e, float)
    {
        e.getComponent<cro::Sprite>().setTextureRect(m_playerAvatar.previewRects[m_sharedData.localConnectionData.playerData[m_playerAvatar.activePlayer].skinID]);
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

    //apply all the current indices to the player avatar
    if (visible)
    {
        m_playerAvatar.setColour(pc::ColourKey::Bottom, m_sharedData.localConnectionData.playerData[playerIndex].avatarFlags[0]);
        m_playerAvatar.setColour(pc::ColourKey::Top, m_sharedData.localConnectionData.playerData[playerIndex].avatarFlags[1]);
        m_playerAvatar.setColour(pc::ColourKey::Skin, m_sharedData.localConnectionData.playerData[playerIndex].avatarFlags[2]);
        m_playerAvatar.setColour(pc::ColourKey::Hair, m_sharedData.localConnectionData.playerData[playerIndex].avatarFlags[3]);
    }
    else
    {
        //apply this to textures in the first slot just for preview
        //these will be updated in the correct positions once we join the lobby
        m_playerAvatar.setTarget(m_sharedData.avatarTextures[0][m_playerAvatar.activePlayer]);
        m_playerAvatar.apply();
    }
}

void MenuState::saveAvatars()
{
    cro::ConfigFile cfg("avatars");
    for (const auto& player : m_sharedData.localConnectionData.playerData)
    {
        auto* avatar = cfg.addObject("avatar");
        avatar->addProperty("name", player.name.empty() ? "Player" : player.name.toAnsiString()); //hmmm shame we can't save the encoding here
        avatar->addProperty("skin_id").setValue(player.skinID);
        avatar->addProperty("flags0").setValue(player.avatarFlags[0]);
        avatar->addProperty("flags1").setValue(player.avatarFlags[1]);
        avatar->addProperty("flags2").setValue(player.avatarFlags[2]);
        avatar->addProperty("flags3").setValue(player.avatarFlags[3]);
    }

    auto path = cro::App::getPreferencePath() + "avatars.cfg";
    cfg.save(path);
}

void MenuState::loadAvatars()
{
    auto path = cro::App::getPreferencePath() + "avatars.cfg";
    cro::ConfigFile cfg;
    if (cfg.loadFromFile(path))
    {
        std::uint32_t i = 0;

        const auto& objects = cfg.getObjects();
        for (const auto& obj : objects)
        {
            if (obj.getName() == "avatar"
                && i < m_sharedData.localConnectionData.MaxPlayers)
            {
                const auto& props = obj.getProperties();
                for (const auto& prop : props)
                {
                    const auto& name = prop.getName();
                    if (name == "name")
                    {
                        //TODO try running this through unicode parser
                        m_sharedData.localConnectionData.playerData[i].name = prop.getValue<std::string>();
                    }
                    else if (name == "skin_id")
                    {
                        auto id = prop.getValue<std::int32_t>();
                        id = std::min(PlayerAvatar::MaxSkins - 1, std::max(0, id));
                        m_sharedData.localConnectionData.playerData[i].skinID = id;
                    }
                    else if (name == "flags0")
                    {
                        auto flag = prop.getValue<std::int32_t>();
                        flag = std::min(pc::ColourID::Count - 1, std::max(0, flag));
                        m_sharedData.localConnectionData.playerData[i].avatarFlags[0] = static_cast<std::uint8_t>(flag);
                    }
                    else if (name == "flags1")
                    {
                        auto flag = prop.getValue<std::int32_t>();
                        flag = std::min(pc::ColourID::Count - 1, std::max(0, flag));
                        m_sharedData.localConnectionData.playerData[i].avatarFlags[1] = static_cast<std::uint8_t>(flag);
                    }
                    else if (name == "flags2")
                    {
                        auto flag = prop.getValue<std::int32_t>();
                        flag = std::min(pc::ColourID::Count - 1, std::max(0, flag));
                        m_sharedData.localConnectionData.playerData[i].avatarFlags[2] = static_cast<std::uint8_t>(flag);
                    }
                    else if (name == "flags3")
                    {
                        auto flag = prop.getValue<std::int32_t>();
                        flag = std::min(pc::ColourID::Count - 1, std::max(0, flag));
                        m_sharedData.localConnectionData.playerData[i].avatarFlags[3] = static_cast<std::uint8_t>(flag);
                    }
                }
                i++;
            }
        }
    }

    if (m_sharedData.localConnectionData.playerData[0].name.empty())
    {
        m_sharedData.localConnectionData.playerData[0].name = "Player 1";
    }
}