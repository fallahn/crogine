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
#include "Slider.hpp"
#include "StateIDs.hpp"

#include <crogine/ecs/systems/SpriteRenderer.hpp>
#include <crogine/ecs/systems/TextRenderer.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/SceneGraph.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/DebugInfo.hpp>

#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/CommandID.hpp>

#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/Font.hpp>
#include <crogine/graphics/SpriteSheet.hpp>

#include <crogine/core/App.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/detail/GlobalConsts.hpp>

#include "RotateSystem.hpp"

namespace
{
    const cro::Colour textColourSelected(1.f, 0.77f, 0.f);
    const cro::Colour textColourNormal = cro::Colour::White();
}

void MainState::createMainMenu()
{
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/sprites/ui_menu.spt", m_resources.textures);
    const auto buttonNormalArea = spriteSheet.getSprite("button_inactive").getTextureRect();
    const auto buttonHighlightArea = spriteSheet.getSprite("button_active").getTextureRect();

    auto mouseEnterCallback = m_uiSystem->addCallback([&, buttonHighlightArea](cro::Entity e, cro::uint64)
    {
        e.getComponent<cro::Sprite>().setTextureRect(buttonHighlightArea);
        auto textEnt = m_menuScene.getEntity(e.getComponent<cro::Transform>().getChildIDs()[0]);
        textEnt.getComponent<cro::Text>().setColour(textColourSelected);
    });
    auto mouseExitCallback = m_uiSystem->addCallback([&, buttonNormalArea](cro::Entity e, cro::uint64)
    {
        e.getComponent<cro::Sprite>().setTextureRect(buttonNormalArea);
        auto textEnt = m_menuScene.getEntity(e.getComponent<cro::Transform>().getChildIDs()[0]);
        textEnt.getComponent<cro::Text>().setColour(textColourNormal);
    });

    //create an entity to move the menu
    auto controlEntity = m_menuScene.createEntity();
    auto& controlTx = controlEntity.addComponent<cro::Transform>();
    controlTx.setPosition({ 960.f, 624.f, 0.f });
    controlEntity.addComponent<cro::CommandTarget>().ID = CommandID::MenuController;
    controlEntity.addComponent<Slider>();

    //title image
    cro::SpriteSheet titleSheet;
    titleSheet.loadFromFile("assets/sprites/ui_title.spt", m_resources.textures);
    auto entity = m_menuScene.createEntity();
    entity.addComponent<cro::Sprite>() = titleSheet.getSprite("header");
    auto size = entity.getComponent<cro::Sprite>().getSize();
    auto& titleTx = entity.addComponent<cro::Transform>();
    titleTx.setOrigin({ size.x / 2.f, size.y / 2.f, 0.f });
    titleTx.setPosition({ 960.f, 960.f, -20.f });
    titleTx.setScale({ 1.5f, 1.5f, 1.5f });


    //start game
    entity = m_menuScene.createEntity();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_inactive");
    auto& gameTx = entity.addComponent<cro::Transform>();
    gameTx.setOrigin({ buttonNormalArea.width / 2.f, buttonNormalArea.height / 2.f, 0.f });
    gameTx.setParent(controlEntity);
    auto& gameControl = entity.addComponent<cro::UIInput>();
    gameControl.callbacks[cro::UIInput::MouseEnter] = mouseEnterCallback;
    gameControl.callbacks[cro::UIInput::MouseExit] = mouseExitCallback;
    gameControl.callbacks[cro::UIInput::MouseUp] = m_uiSystem->addCallback([this]
    (cro::Entity, cro::uint64)
    {
        requestStackClear();
        requestStackPush(States::ID::GamePlaying);
    });
    gameControl.area.width = buttonNormalArea.width;
    gameControl.area.height = buttonNormalArea.height;

    auto& testFont = m_resources.fonts.get(FontID::MenuFont);
    auto textEnt = m_menuScene.createEntity();
    auto& gameText = textEnt.addComponent<cro::Text>(testFont);
    gameText.setString("Play");
    gameText.setColour(textColourNormal);
    gameText.setCharSize(60);
    gameText.setBlendMode(cro::Material::BlendMode::Additive);
    auto& gameTextTx = textEnt.addComponent<cro::Transform>();
    gameTextTx.setPosition({ 40.f, 100.f, 0.f });
    gameTextTx.setParent(entity);

    //options
    entity = m_menuScene.createEntity();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_inactive");
    auto& optionTx = entity.addComponent<cro::Transform>();
    optionTx.setOrigin({ buttonNormalArea.width / 2.f, buttonNormalArea.height / 2.f, 0.f });
    optionTx.setParent(controlEntity);
    optionTx.setPosition({ 0.f, -160.f, -2.f });
    auto& optionControl = entity.addComponent<cro::UIInput>();
    optionControl.callbacks[cro::UIInput::MouseEnter] = mouseEnterCallback;
    optionControl.callbacks[cro::UIInput::MouseExit] = mouseExitCallback;
    optionControl.area.width = buttonNormalArea.width;
    optionControl.area.height = buttonNormalArea.height;

    optionControl.callbacks[cro::UIInput::MouseUp] = m_uiSystem->addCallback([this]
    (cro::Entity, cro::uint64 flags)
    {
        if ((flags & cro::UISystem::LeftMouse)
            || (flags & cro::UISystem::Finger))
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::MenuController;
            cmd.action = [](cro::Entity e, cro::Time)
            {
                auto& slider = e.getComponent<Slider>();
                slider.active = true;
                slider.destination = e.getComponent<cro::Transform>().getPosition() + glm::vec3(cro::DefaultSceneSize.x, 0.f, 0.f);
            };
            m_commandSystem->sendCommand(cmd);
        }
    });

    textEnt = m_menuScene.createEntity();
    auto& optionText = textEnt.addComponent<cro::Text>(testFont);
    optionText.setString("Options");
    optionText.setColour(textColourNormal);
    optionText.setCharSize(60);
    optionText.setBlendMode(cro::Material::BlendMode::Additive);
    auto& texTx = textEnt.addComponent<cro::Transform>();
    texTx.setParent(entity);
    texTx.move({ 40.f, 100.f, 0.f });

    //high scores
    entity = m_menuScene.createEntity();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_inactive");
    auto& scoreTx = entity.addComponent<cro::Transform>();
    scoreTx.setPosition({ 0.f, -320.f, 0.f });
    scoreTx.setOrigin({ buttonNormalArea.width / 2.f, buttonNormalArea.height / 2.f, 0.f });
    scoreTx.setParent(controlEntity);
    auto& scoreControl = entity.addComponent<cro::UIInput>();
    scoreControl.callbacks[cro::UIInput::MouseEnter] = mouseEnterCallback;
    scoreControl.callbacks[cro::UIInput::MouseExit] = mouseExitCallback;
    scoreControl.area.width = buttonNormalArea.width;
    scoreControl.area.height = buttonNormalArea.height;

    auto scoreCallback = m_uiSystem->addCallback([this]
    (cro::Entity e, cro::uint64 flags)
    {
        if ((flags & cro::UISystem::LeftMouse)
            || (flags & cro::UISystem::Finger))
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::MenuController;
            cmd.action = [](cro::Entity e, cro::Time)
            {
                auto& slider = e.getComponent<Slider>();
                slider.active = true;
                slider.destination = e.getComponent<cro::Transform>().getPosition() + glm::vec3(-static_cast<float>(cro::DefaultSceneSize.x), 0.f, 0.f);
            };
            m_commandSystem->sendCommand(cmd);
        }
    });
    scoreControl.callbacks[cro::UIInput::MouseUp] = scoreCallback;

    textEnt = m_menuScene.createEntity();
    auto& scoreText = textEnt.addComponent<cro::Text>(testFont);
    scoreText.setString("Scores");
    scoreText.setColour(textColourNormal);
    scoreText.setCharSize(60);
    scoreText.setBlendMode(cro::Material::BlendMode::Additive);
    auto& scoreTexTx = textEnt.addComponent<cro::Transform>();
    scoreTexTx.setParent(entity);
    scoreTexTx.move({ 40.f, 100.f, 0.f });

    //quit button
    entity = m_menuScene.createEntity();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_inactive");
    auto& quitTx = entity.addComponent<cro::Transform>();
    quitTx.setPosition({ 0.f, -480.f, 0.f });
    quitTx.setParent(controlEntity);
    quitTx.setOrigin({ buttonNormalArea.width / 2.f, buttonNormalArea.height / 2.f, 0.f });

    textEnt = m_menuScene.createEntity();
    auto& quitText = textEnt.addComponent<cro::Text>(testFont);
    quitText.setString("Quit");
    quitText.setColour(textColourNormal);
    quitText.setCharSize(60);
    quitText.setBlendMode(cro::Material::BlendMode::Additive);
    auto& quitTexTx = textEnt.addComponent<cro::Transform>();
    quitTexTx.setParent(entity);
    quitTexTx.move({ 40.f, 100.f, 0.f });

    auto quitCallback = m_uiSystem->addCallback([this](cro::Entity, cro::uint64 flags)
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
    auto& quitControl = entity.addComponent<cro::UIInput>();
    quitControl.callbacks[cro::UIInput::MouseUp] = quitCallback;
    quitControl.callbacks[cro::UIInput::MouseEnter] = mouseEnterCallback;
    quitControl.callbacks[cro::UIInput::MouseExit] = mouseExitCallback;
    quitControl.area.width = buttonNormalArea.width;
    quitControl.area.height = buttonNormalArea.height;

    //quit confirmation
    entity = m_menuScene.createEntity();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("menu");
    size = entity.getComponent<cro::Sprite>().getSize();
    auto& confTx = entity.addComponent<cro::Transform>();
    confTx.setParent(controlEntity);
    confTx.setPosition({ 0.f, -1080.f, -10.f });
    confTx.setOrigin({size.x / 2.f, size.y, 0.f });
    

    textEnt = m_menuScene.createEntity();
    auto& confText = textEnt.addComponent<cro::Text>(testFont);
    confText.setString("Exit Game?");
    confText.setColour(textColourSelected);
    confText.setBlendMode(cro::Material::BlendMode::Additive);
    confText.setCharSize(42);
    auto& confTexTx = textEnt.addComponent<cro::Transform>();
    confTexTx.setParent(entity);
    confTexTx.move({ (size.x / 2.f) - 112.f, size.y - 20.f, 0.f });


    //OK button
    auto buttonEnt = m_menuScene.createEntity();
    buttonEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_inactive");
    auto& buttonTx0 = buttonEnt.addComponent<cro::Transform>();
    buttonTx0.setParent(entity);
    buttonTx0.setPosition({ size.x / 4.f, 82.f, 1.f });
    buttonTx0.setOrigin({ buttonNormalArea.width / 2.f, 0.f, 0.f });

    textEnt = m_menuScene.createEntity();
    auto& buttonText0 = textEnt.addComponent<cro::Text>(testFont);
    buttonText0.setString("OK");
    buttonText0.setColour(textColourNormal);
    buttonText0.setBlendMode(cro::Material::BlendMode::Additive);
    buttonText0.setCharSize(80);
    auto& buttonTextTx0 = textEnt.addComponent<cro::Transform>();
    buttonTextTx0.setParent(buttonEnt);
    buttonTextTx0.setPosition({ 107.f, 104.f, 0.f });

    auto okCallback = m_uiSystem->addCallback([this](cro::Entity, cro::uint64 flags)
    {
        if ((flags & cro::UISystem::LeftMouse)
            || flags & cro::UISystem::Finger)
        {
            cro::App::quit();
        }
    });
    auto& okControl = buttonEnt.addComponent<cro::UIInput>();
    okControl.callbacks[cro::UIInput::MouseUp] = okCallback;
    okControl.callbacks[cro::UIInput::MouseEnter] = mouseEnterCallback;
    okControl.callbacks[cro::UIInput::MouseExit] = mouseExitCallback;
    okControl.area.width = buttonNormalArea.width;
    okControl.area.height = buttonNormalArea.height;


    //cancel button
    buttonEnt = m_menuScene.createEntity();
    buttonEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_inactive");
    auto& buttonTx1 = buttonEnt.addComponent<cro::Transform>();
    buttonTx1.setParent(entity);
    buttonTx1.setPosition({ (size.x / 4.f) + (size.x / 2.f), 82.f, 1.f });
    buttonTx1.setOrigin({ buttonNormalArea.width / 2.f, 0.f, 0.f });

    textEnt = m_menuScene.createEntity();
    auto& buttonText1 = textEnt.addComponent<cro::Text>(testFont);
    buttonText1.setString("Cancel");
    buttonText1.setColour(textColourNormal);
    buttonText1.setBlendMode(cro::Material::BlendMode::Additive);
    buttonText1.setCharSize(80);
    auto& buttonTextTx1 = textEnt.addComponent<cro::Transform>();
    buttonTextTx1.setParent(buttonEnt);
    buttonTextTx1.setPosition({ 81.f, 104.f, 0.f });

    auto cancelCallback = m_uiSystem->addCallback([this](cro::Entity, cro::uint64 flags)
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
            slider.destination = e.getComponent<cro::Transform>().getPosition() + glm::vec3(0.f, -static_cast<float>(cro::DefaultSceneSize.y), 0.f);
            };
            m_commandSystem->sendCommand(cmd);
        }
    });
    auto& cancelControl = buttonEnt.addComponent<cro::UIInput>();
    cancelControl.callbacks[cro::UIInput::MouseUp] = cancelCallback;
    cancelControl.callbacks[cro::UIInput::MouseEnter] = mouseEnterCallback;
    cancelControl.callbacks[cro::UIInput::MouseExit] = mouseExitCallback;
    cancelControl.area.width = buttonNormalArea.width;
    cancelControl.area.height = buttonNormalArea.height;
}