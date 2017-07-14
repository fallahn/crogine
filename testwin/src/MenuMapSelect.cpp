/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#include "MainState.hpp"
#include "ResourceIDs.hpp"
#include "Slider.hpp"
#include "MyApp.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/CommandID.hpp>
#include <crogine/ecs/components/Text.hpp>

#include <crogine/ecs/systems/UISystem.hpp>

#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/detail/GlobalConsts.hpp>

namespace
{
    const float buttonPosition = 960.f;
#include "MenuConsts.inl"
}

void MainState::createMapSelect(cro::Entity parentEnt)
{
    auto menuController = m_menuScene.createEntity();
    menuController.addComponent<cro::Transform>().setPosition({ 0.f, buttonPosition, 0.f });
    menuController.getComponent<cro::Transform>().setParent(parentEnt);
    menuController.addComponent<Slider>();
    menuController.addComponent<cro::CommandTarget>().ID = CommandID::MapSelectController;

    auto& font = m_sharedResources.fonts.get(FontID::MenuFont);

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/sprites/ui_maps.spt", m_sharedResources.textures);

    //caves button
    auto entity = m_menuScene.createEntity();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("caves_normal");
    auto size = entity.getComponent<cro::Sprite>().getSize();
    entity.addComponent<cro::Transform>().setOrigin({ size.x / 2.f, size.y / 2.f, 0.f });
    entity.getComponent<cro::Transform>().setParent(menuController);
    entity.getComponent<cro::Transform>().move({ -720.f, 0.f, 0.f });

    auto normalRect = spriteSheet.getSprite("caves_normal").getTextureRect();
    auto activeRect = spriteSheet.getSprite("caves_active").getTextureRect();
    entity.addComponent<cro::UIInput>().callbacks[cro::UIInput::MouseEnter] = m_uiSystem->addCallback(
        [this, activeRect](cro::Entity ent, cro::uint64 flags)
    {
        ent.getComponent<cro::Sprite>().setTextureRect(activeRect);
        auto textEnt = m_menuScene.getEntity(ent.getComponent<cro::Transform>().getChildIDs()[0]);
        textEnt.getComponent<cro::Text>().setColour(textColourSelected);
    });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseExit] = m_uiSystem->addCallback(
        [this, normalRect](cro::Entity ent, cro::uint64 flags)
    {
        ent.getComponent<cro::Sprite>().setTextureRect(normalRect);
        auto textEnt = m_menuScene.getEntity(ent.getComponent<cro::Transform>().getChildIDs()[0]);
        textEnt.getComponent<cro::Text>().setColour(textColourNormal);
    });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseUp] = m_uiSystem->addCallback(
        [this](cro::Entity, cro::uint64)
    {
        requestStackClear();
        requestStackPush(States::ID::GamePlaying);
    });
    entity.getComponent<cro::UIInput>().area.width = size.x;
    entity.getComponent<cro::UIInput>().area.height = size.y;

    auto textEnt = m_menuScene.createEntity();
    textEnt.addComponent<cro::Text>(font);
    textEnt.getComponent<cro::Text>().setString("Ice Caves");
    textEnt.getComponent<cro::Text>().setColour(textColourNormal);
    textEnt.getComponent<cro::Text>().setCharSize(32);
    textEnt.addComponent<cro::Transform>();
    textEnt.getComponent<cro::Transform>().setParent(entity);
    textEnt.getComponent<cro::Transform>().move({ 25.f, 56.f, 0.f });

    //forest button
    entity = m_menuScene.createEntity();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("forest_normal");
    size = entity.getComponent<cro::Sprite>().getSize();
    entity.addComponent<cro::Transform>().setOrigin({ size.x / 2.f, size.y / 2.f, 0.f });
    entity.getComponent<cro::Transform>().setParent(menuController);
    entity.getComponent<cro::Transform>().move({ -240.f, 0.f, 0.f });

    normalRect = spriteSheet.getSprite("forest_normal").getTextureRect();
    activeRect = spriteSheet.getSprite("forest_active").getTextureRect();
    entity.addComponent<cro::UIInput>().callbacks[cro::UIInput::MouseEnter] = m_uiSystem->addCallback(
        [this, activeRect](cro::Entity ent, cro::uint64 flags)
    {
        ent.getComponent<cro::Sprite>().setTextureRect(activeRect);
        auto textEnt = m_menuScene.getEntity(ent.getComponent<cro::Transform>().getChildIDs()[0]);
        textEnt.getComponent<cro::Text>().setColour(textColourSelected);
    });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseExit] = m_uiSystem->addCallback(
        [this, normalRect](cro::Entity ent, cro::uint64 flags)
    {
        ent.getComponent<cro::Sprite>().setTextureRect(normalRect);
        auto textEnt = m_menuScene.getEntity(ent.getComponent<cro::Transform>().getChildIDs()[0]);
        textEnt.getComponent<cro::Text>().setColour(textColourNormal);
    });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseUp] = m_uiSystem->addCallback(
        [this](cro::Entity, cro::uint64)
    {
        /*requestStackClear();
        requestStackPush(States::ID::GamePlaying);*/
    });
    entity.getComponent<cro::UIInput>().area.width = size.x;
    entity.getComponent<cro::UIInput>().area.height = size.y;

    auto lockEnt = m_menuScene.createEntity();
    lockEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("padlock");
    size = lockEnt.getComponent<cro::Sprite>().getSize();
    lockEnt.addComponent<cro::Transform>().setOrigin({ size.x / 2.f, size.y / 2.f, 0.f });
    lockEnt.getComponent<cro::Transform>().setParent(entity);
    lockEnt.getComponent<cro::Transform>().setPosition({ normalRect.width / 2.f, normalRect.height / 2.f, 0.1f });

    textEnt = m_menuScene.createEntity();
    textEnt.addComponent<cro::Text>(font);
    textEnt.getComponent<cro::Text>().setString("Locked");
    textEnt.getComponent<cro::Text>().setColour(textColourNormal);
    textEnt.getComponent<cro::Text>().setCharSize(32);
    textEnt.addComponent<cro::Transform>();
    textEnt.getComponent<cro::Transform>().setParent(entity);
    textEnt.getComponent<cro::Transform>().move({ 25.f, 56.f, 0.f });

    //desert button
    entity = m_menuScene.createEntity();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("desert_normal");
    size = entity.getComponent<cro::Sprite>().getSize();
    entity.addComponent<cro::Transform>().setOrigin({ size.x / 2.f, size.y / 2.f, 0.f });
    entity.getComponent<cro::Transform>().setParent(menuController);
    entity.getComponent<cro::Transform>().move({ 240.f, 0.f, 0.f });

    normalRect = spriteSheet.getSprite("desert_normal").getTextureRect();
    activeRect = spriteSheet.getSprite("desert_active").getTextureRect();
    entity.addComponent<cro::UIInput>().callbacks[cro::UIInput::MouseEnter] = m_uiSystem->addCallback(
        [this, activeRect](cro::Entity ent, cro::uint64 flags)
    {
        ent.getComponent<cro::Sprite>().setTextureRect(activeRect);
        auto textEnt = m_menuScene.getEntity(ent.getComponent<cro::Transform>().getChildIDs()[0]);
        textEnt.getComponent<cro::Text>().setColour(textColourSelected);
    });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseExit] = m_uiSystem->addCallback(
        [this, normalRect](cro::Entity ent, cro::uint64 flags)
    {
        ent.getComponent<cro::Sprite>().setTextureRect(normalRect);
        auto textEnt = m_menuScene.getEntity(ent.getComponent<cro::Transform>().getChildIDs()[0]);
        textEnt.getComponent<cro::Text>().setColour(textColourNormal);
    });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseUp] = m_uiSystem->addCallback(
        [this](cro::Entity, cro::uint64)
    {
        /*requestStackClear();
        requestStackPush(States::ID::GamePlaying);*/
    });
    entity.getComponent<cro::UIInput>().area.width = size.x;
    entity.getComponent<cro::UIInput>().area.height = size.y;

    lockEnt = m_menuScene.createEntity();
    lockEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("padlock");
    size = lockEnt.getComponent<cro::Sprite>().getSize();
    lockEnt.addComponent<cro::Transform>().setOrigin({ size.x / 2.f, size.y / 2.f, 0.f });
    lockEnt.getComponent<cro::Transform>().setParent(entity);
    lockEnt.getComponent<cro::Transform>().setPosition({ normalRect.width / 2.f, normalRect.height / 2.f, 0.1f });

    textEnt = m_menuScene.createEntity();
    textEnt.addComponent<cro::Text>(font);
    textEnt.getComponent<cro::Text>().setString("Locked");
    textEnt.getComponent<cro::Text>().setColour(textColourNormal);
    textEnt.getComponent<cro::Text>().setCharSize(32);
    textEnt.addComponent<cro::Transform>();
    textEnt.getComponent<cro::Transform>().setParent(entity);
    textEnt.getComponent<cro::Transform>().move({ 25.f, 56.f, 0.f });

    //comin soon button
    entity = m_menuScene.createEntity();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("empty_normal");
    size = entity.getComponent<cro::Sprite>().getSize();
    entity.addComponent<cro::Transform>().setOrigin({ size.x / 2.f, size.y / 2.f, 0.f });
    entity.getComponent<cro::Transform>().setParent(menuController);
    entity.getComponent<cro::Transform>().move({ 720.f, 0.f, 0.f });

    normalRect = spriteSheet.getSprite("empty_normal").getTextureRect();
    activeRect = spriteSheet.getSprite("empty_active").getTextureRect();
    entity.addComponent<cro::UIInput>().callbacks[cro::UIInput::MouseEnter] = m_uiSystem->addCallback(
        [this, activeRect](cro::Entity ent, cro::uint64 flags)
    {
        ent.getComponent<cro::Sprite>().setTextureRect(activeRect);
        auto textEnt = m_menuScene.getEntity(ent.getComponent<cro::Transform>().getChildIDs()[0]);
        textEnt.getComponent<cro::Text>().setColour(textColourSelected);
    });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseExit] = m_uiSystem->addCallback(
        [this, normalRect](cro::Entity ent, cro::uint64 flags)
    {
        ent.getComponent<cro::Sprite>().setTextureRect(normalRect);
        auto textEnt = m_menuScene.getEntity(ent.getComponent<cro::Transform>().getChildIDs()[0]);
        textEnt.getComponent<cro::Text>().setColour(textColourNormal);
    });
    entity.getComponent<cro::UIInput>().area.width = size.x;
    entity.getComponent<cro::UIInput>().area.height = size.y;

    textEnt = m_menuScene.createEntity();
    textEnt.addComponent<cro::Text>(font);
    textEnt.getComponent<cro::Text>().setString("Coming Soon");
    textEnt.getComponent<cro::Text>().setColour(textColourNormal);
    textEnt.getComponent<cro::Text>().setCharSize(32);
    textEnt.addComponent<cro::Transform>();
    textEnt.getComponent<cro::Transform>().setParent(entity);
    textEnt.getComponent<cro::Transform>().move({ 25.f, 56.f, 0.f });

    //back button
    spriteSheet.loadFromFile("assets/sprites/ui_menu.spt", m_sharedResources.textures);
    const auto buttonNormalArea = spriteSheet.getSprite("button_inactive").getTextureRect();
    const auto buttonHighlightArea = spriteSheet.getSprite("button_active").getTextureRect();

    entity = m_menuScene.createEntity();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_inactive");
    auto& quitTx = entity.addComponent<cro::Transform>();
    quitTx.setPosition({ 0.f, 1080.f - 480.f, 0.f });
    quitTx.setParent(parentEnt);
    quitTx.setOrigin({ buttonNormalArea.width / 2.f, buttonNormalArea.height / 2.f, 0.f });

    textEnt = m_menuScene.createEntity();
    textEnt.addComponent<cro::Text>(font);
    textEnt.getComponent<cro::Text>().setString("Back");
    textEnt.getComponent<cro::Text>().setColour(textColourNormal);
    textEnt.getComponent<cro::Text>().setCharSize(60);
    textEnt.addComponent<cro::Transform>();
    textEnt.getComponent<cro::Transform>().setParent(entity);
    textEnt.getComponent<cro::Transform>().move({ 40.f, 100.f, 0.f });

    spriteSheet.loadFromFile("assets/sprites/ui_icons.spt", m_sharedResources.textures);
    auto iconEnt = m_menuScene.createEntity();
    iconEnt.addComponent<cro::Transform>().setParent(entity);
    iconEnt.getComponent<cro::Transform>().setPosition({ buttonNormalArea.width - buttonIconOffset, 0.f, 0.f });
    iconEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("back");


    auto backCallback = m_uiSystem->addCallback([this](cro::Entity, cro::uint64 flags)
    {
        if ((flags & cro::UISystem::LeftMouse)
            || flags & cro::UISystem::Finger)
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::MenuController;
            cmd.action = [](cro::Entity e, cro::Time)
            {
                auto& slider = e.getComponent<Slider>();
                slider.active = true;
                slider.destination = e.getComponent<cro::Transform>().getPosition() + glm::vec3(0.f, static_cast<float>(cro::DefaultSceneSize.y), 0.f);
            };
            m_commandSystem->sendCommand(cmd);
        }
    });

    auto mouseEnterCallback = m_uiSystem->addCallback([&, buttonHighlightArea](cro::Entity e, cro::uint64)
    {
        e.getComponent<cro::Sprite>().setTextureRect(buttonHighlightArea);
        const auto& children = e.getComponent<cro::Transform>().getChildIDs();
        std::size_t i = 0;
        while (children[i] != -1)
        {
            auto c = children[i++];
            auto child = m_menuScene.getEntity(c);
            if (child.hasComponent<cro::Text>())
            {
                child.getComponent<cro::Text>().setColour(textColourSelected);
            }
            else if (child.hasComponent<cro::Sprite>())
            {
                child.getComponent<cro::Sprite>().setColour(textColourSelected);
            }
        }
    });
    auto mouseExitCallback = m_uiSystem->addCallback([&, buttonNormalArea](cro::Entity e, cro::uint64)
    {
        e.getComponent<cro::Sprite>().setTextureRect(buttonNormalArea);
        const auto& children = e.getComponent<cro::Transform>().getChildIDs();
        std::size_t i = 0;
        while (children[i] != -1)
        {
            auto c = children[i++];
            auto child = m_menuScene.getEntity(c);
            if (child.hasComponent<cro::Text>())
            {
                child.getComponent<cro::Text>().setColour(textColourNormal);
            }
            else if (child.hasComponent<cro::Sprite>())
            {
                child.getComponent<cro::Sprite>().setColour(textColourNormal);
            }
        }
    });

    auto& backControl = entity.addComponent<cro::UIInput>();
    backControl.callbacks[cro::UIInput::MouseUp] = backCallback;
    backControl.callbacks[cro::UIInput::MouseEnter] = mouseEnterCallback;
    backControl.callbacks[cro::UIInput::MouseExit] = mouseExitCallback;
    backControl.area.width = buttonNormalArea.width;
    backControl.area.height = buttonNormalArea.height;
}