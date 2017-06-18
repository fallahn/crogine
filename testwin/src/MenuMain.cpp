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

#include <crogine/core/App.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/detail/GlobalConsts.hpp>

namespace
{
    const cro::Colour textColourSelected(1.f, 0.77f, 0.f);
    const cro::Colour textColourNormal = cro::Colour::White();
    const cro::FloatRect buttonArea(0.f, 0.f, 240.f, 64.f);
}

void MainState::createMainMenu()
{
    auto mouseEnterCallback = m_uiSystem->addCallback([&](cro::Entity e, cro::uint64)
    {
        auto area = buttonArea;
        area.left = buttonArea.width + 16;
        e.getComponent<cro::Sprite>().setTextureRect(area);

        auto textEnt = m_menuScene.getEntity(e.getComponent<cro::Transform>().getChildIDs()[0]);
        textEnt.getComponent<cro::Text>().setColour(textColourSelected);
    });
    auto mouseExitCallback = m_uiSystem->addCallback([&](cro::Entity e, cro::uint64)
    {
        e.getComponent<cro::Sprite>().setTextureRect(buttonArea);
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
    auto& uiTexture = m_resources.textures.get("assets/sprites/menu.png");
    auto entity = m_menuScene.createEntity();
    auto& titleSprite = entity.addComponent<cro::Sprite>();
    titleSprite.setTexture(uiTexture);
    titleSprite.setTextureRect({ 0.f, 65.f, 1024.f, 319.f });
    auto& titleTx = entity.addComponent<cro::Transform>();
    titleTx.setOrigin({ 1024.f, 160.f, 0.f });
    titleTx.setPosition({ 960.f - 274.f, 840.f, -20.f });
    titleTx.setScale({ 1.5f, 1.5f, 1.5f });

    //start game
    entity = m_menuScene.createEntity();
    auto& gameSprite = entity.addComponent<cro::Sprite>();
    gameSprite.setTexture(uiTexture);
    gameSprite.setTextureRect(buttonArea);
    auto& gameTx = entity.addComponent<cro::Transform>();
    gameTx.setOrigin({ buttonArea.width, buttonArea.height, 0.f });
    gameTx.setScale({ 2.f, 2.f, 2.f });
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
    gameControl.area = buttonArea;

    auto& testFont = m_resources.fonts.get(FontID::MenuFont);
    auto textEnt = m_menuScene.createEntity();
    auto& gameText = textEnt.addComponent<cro::Text>(testFont);
    gameText.setString("Play");
    gameText.setColour(textColourNormal);
    auto& gameTextTx = textEnt.addComponent<cro::Transform>();
    gameTextTx.setPosition({ 90.f, 50.f, 0.f });
    gameTextTx.setParent(entity);

    //options
    entity = m_menuScene.createEntity();
    auto& optionSprite = entity.addComponent<cro::Sprite>();
    optionSprite.setTexture(uiTexture);
    optionSprite.setTextureRect(buttonArea);
    auto& optionTx = entity.addComponent<cro::Transform>();
    optionTx.setOrigin({ buttonArea.width, buttonArea.height, 0.f });
    optionTx.setScale({ 2.f, 2.f, 2.f });
    optionTx.setParent(controlEntity);
    optionTx.setPosition({ 0.f, -160.f, -2.f });
    auto& optionControl = entity.addComponent<cro::UIInput>();
    optionControl.callbacks[cro::UIInput::MouseEnter] = mouseEnterCallback;
    optionControl.callbacks[cro::UIInput::MouseExit] = mouseExitCallback;
    optionControl.area = buttonArea;

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
    auto& texTx = textEnt.addComponent<cro::Transform>();
    texTx.setParent(entity);
    texTx.move({ 64.f, 50.f, 0.f });

    //high scores
    entity = m_menuScene.createEntity();
    auto& scoreSprite = entity.addComponent<cro::Sprite>();
    scoreSprite.setTexture(uiTexture);
    scoreSprite.setTextureRect(buttonArea);
    auto& scoreTx = entity.addComponent<cro::Transform>();
    scoreTx.setPosition({ 0.f, -320.f, 0.f });
    scoreTx.setScale({ 2.f, 2.f, 2.f });
    scoreTx.setOrigin({ buttonArea.width, buttonArea.height, 0.f });
    scoreTx.setParent(controlEntity);
    auto& scoreControl = entity.addComponent<cro::UIInput>();
    scoreControl.callbacks[cro::UIInput::MouseEnter] = mouseEnterCallback;
    scoreControl.callbacks[cro::UIInput::MouseExit] = mouseExitCallback;
    scoreControl.area = buttonArea;

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
    auto& scoreTexTx = textEnt.addComponent<cro::Transform>();
    scoreTexTx.setParent(entity);
    scoreTexTx.move({ 74.f, 50.f, 0.f });

    //quit button
    entity = m_menuScene.createEntity();
    auto& quitSprite = entity.addComponent<cro::Sprite>();
    quitSprite.setTexture(uiTexture);
    quitSprite.setTextureRect(buttonArea);
    auto& quitTx = entity.addComponent<cro::Transform>();
    quitTx.setPosition({ 0.f, -480.f, 0.f });
    quitTx.setScale({ 2.f, 2.f, 2.f });
    quitTx.setParent(controlEntity);
    quitTx.setOrigin({ buttonArea.width, buttonArea.height, 0.f });

    textEnt = m_menuScene.createEntity();
    auto& quitText = textEnt.addComponent<cro::Text>(testFont);
    quitText.setString("Quit");
    quitText.setColour(textColourNormal);
    auto& quitTexTx = textEnt.addComponent<cro::Transform>();
    quitTexTx.setParent(entity);
    quitTexTx.move({ 88.f, 50.f, 0.f });

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
    quitControl.area = buttonArea;

    //quit confirmation
    entity = m_menuScene.createEntity();
    auto& confTx = entity.addComponent<cro::Transform>();
    confTx.setParent(controlEntity);
    confTx.setPosition({ 0.f, -1080.f, -10.f });
    confTx.setScale({ 4.f, 6.f, 1.f });
    confTx.setOrigin({ buttonArea.width * 0.667f, buttonArea.height, 0.f });

    auto& confSprite = entity.addComponent<cro::Sprite>();
    confSprite.setTexture(uiTexture);
    confSprite.setTextureRect(buttonArea);


    textEnt = m_menuScene.createEntity();
    auto& confText = textEnt.addComponent<cro::Text>(testFont);
    confText.setString("Exit Game?");
    confText.setColour(textColourSelected);
    auto& confTexTx = textEnt.addComponent<cro::Transform>();
    confTexTx.setParent(entity);
    confTexTx.move({ 73.f, 54.f, 0.f });
    confTexTx.setScale({ 0.667f, 0.334f, 1.f });

    //OK button
    auto buttonEnt = m_menuScene.createEntity();
    auto& buttonTx0 = buttonEnt.addComponent<cro::Transform>();
    buttonTx0.setParent(entity);
    buttonTx0.setPosition({ 20.f, 12.f, 1.f });
    buttonTx0.setScale({ 0.4f, 0.4f, 1.f });

    auto& buttonSprite0 = buttonEnt.addComponent<cro::Sprite>();
    buttonSprite0.setTexture(uiTexture);
    buttonSprite0.setTextureRect(buttonArea);

    textEnt = m_menuScene.createEntity();
    auto& buttonText0 = textEnt.addComponent<cro::Text>(testFont);
    buttonText0.setString("OK");
    buttonText0.setColour(textColourNormal);
    auto& buttonTextTx0 = textEnt.addComponent<cro::Transform>();
    buttonTextTx0.setParent(buttonEnt);
    buttonTextTx0.setPosition({ 107.f, 44.f, 0.f });
    buttonTextTx0.setScale({ 1.f, 0.67f, 1.f });

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
    okControl.area = buttonArea;



    //cancel button
    buttonEnt = m_menuScene.createEntity();
    auto& buttonTx1 = buttonEnt.addComponent<cro::Transform>();
    buttonTx1.setParent(entity);
    buttonTx1.setPosition({ 132.f, 12.f, 1.f });
    buttonTx1.setScale({ 0.4f, 0.4f, 1.f });

    auto& buttonSprite1 = buttonEnt.addComponent<cro::Sprite>();
    buttonSprite1.setTexture(uiTexture);
    buttonSprite1.setTextureRect(buttonArea);

    textEnt = m_menuScene.createEntity();
    auto& buttonText1 = textEnt.addComponent<cro::Text>(testFont);
    buttonText1.setString("Cancel");
    buttonText1.setColour(textColourNormal);
    auto& buttonTextTx1 = textEnt.addComponent<cro::Transform>();
    buttonTextTx1.setParent(buttonEnt);
    buttonTextTx1.setPosition({ 81.f, 44.f, 0.f });
    buttonTextTx1.setScale({ 1.f, 0.67f, 1.f });

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
    cancelControl.area = buttonArea;


    //entity = m_mainMenuScene.createEntity();
    //auto& buns = entity.addComponent<cro::Sprite>();
    //buns.setTexture(testFont.getTexture());
    ////buns.setTextureRect(testFont.getGlyph('@'));
    //entity.addComponent<cro::Transform>().move({ 30.f, 0.f, 0.f });
}