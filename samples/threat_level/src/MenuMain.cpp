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
#include "RotateSystem.hpp"
#include "MyApp.hpp"

#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/DebugInfo.hpp>

#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/Font.hpp>
#include <crogine/graphics/SpriteSheet.hpp>

#include <crogine/core/App.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/detail/GlobalConsts.hpp>

#include <crogine/core/FileSystem.hpp>
namespace
{
#include "MenuConsts.inl"
}

void MainState::createMainMenu(std::uint32_t mouseEnterCallback, std::uint32_t mouseExitCallback, 
    const cro::SpriteSheet& spriteSheetButtons, const cro::SpriteSheet& spriteSheetIcons)
{
    const auto buttonNormalArea = spriteSheetButtons.getSprite("button_inactive").getTextureRect();

    //create an entity to move the menu
    auto controlEntity = m_menuScene.createEntity();
    auto& controlTx = controlEntity.addComponent<cro::Transform>();
    controlTx.setPosition({ cro::DefaultSceneSize.x / 2.f, 624.f, 0.f });
    controlEntity.addComponent<cro::CommandTarget>().ID = CommandID::MenuController;
    controlEntity.addComponent<Slider>();

    //title image
    auto entity = m_menuScene.createEntity();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheetButtons.getSprite("title");
    auto size = entity.getComponent<cro::Sprite>().getSize();
    auto& titleTx = entity.addComponent<cro::Transform>();
    titleTx.setOrigin({ size.x / 2.f, size.y / 2.f, 0.f });
    titleTx.setPosition({ 960.f, 920.f, -20.f });


    //start game
    entity = m_menuScene.createEntity();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheetButtons.getSprite("button_inactive");
    auto& gameTx = entity.addComponent<cro::Transform>();
    gameTx.setOrigin({ buttonNormalArea.width / 2.f, buttonNormalArea.height / 2.f, 0.f });
    controlEntity.getComponent<cro::Transform>().addChild(gameTx);
    gameTx.setPosition({ 0.f, 60.f, 0.f });
    auto& gameControl = entity.addComponent<cro::UIInput>();
    gameControl.setGroup(GroupID::Main);
    gameControl.callbacks[cro::UIInput::Selected] = mouseEnterCallback;
    gameControl.callbacks[cro::UIInput::Unselected] = mouseExitCallback;
    gameControl.callbacks[cro::UIInput::ButtonUp] = m_uiSystem->addCallback([this]
    (cro::Entity, const cro::ButtonEvent& evt)
    {
        if (activated(evt))
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::MenuController;
            cmd.action = [](cro::Entity e, float)
            {
                auto& slider = e.getComponent<Slider>();
                slider.active = true;
                slider.destination = e.getComponent<cro::Transform>().getPosition() + glm::vec3(0.f, -static_cast<float>(cro::DefaultSceneSize.y), 0.f);
            };
            m_commandSystem->sendCommand(cmd);

            m_menuScene.getSystem<cro::UISystem>().setActiveGroup(GroupID::MapSelect);
        }
    });
    gameControl.area.width = buttonNormalArea.width;
    gameControl.area.height = buttonNormalArea.height;

    auto& menuFont = m_sharedResources.fonts.get(FontID::MenuFont);
    auto textEnt = m_menuScene.createEntity();
    textEnt.addComponent<cro::Drawable2D>();
    textEnt.addComponent<cro::Callback>().active = true;
    textEnt.getComponent<cro::Callback>().function = 
        [](cro::Entity e, float dt)
    {
        static float t = 0.f;
        t += dt;
        if (t > 1)
        {
            t -= 1.f;
            static bool o = false;
            o = !o;

            if (o)
            {
                e.getComponent<cro::Text>().setOutlineThickness(8.f);
            }
            else
            {
                e.getComponent<cro::Text>().setOutlineThickness(0.f);
            }
        }
    };
    
    auto& gameText = textEnt.addComponent<cro::Text>(menuFont);
    gameText.setString("Play");
    gameText.setFillColour(textColourNormal);
    gameText.setCharacterSize(TextLarge);
    //gameText.setBlendMode(cro::Material::BlendMode::Additive);
    auto& gameTextTx = textEnt.addComponent<cro::Transform>();
    gameTextTx.setPosition({ 20.f, 80.f, 1.f });
    entity.getComponent<cro::Transform>().addChild(gameTextTx);
    auto iconEnt = m_menuScene.createEntity();
    entity.getComponent<cro::Transform>().addChild(iconEnt.addComponent<cro::Transform>());
    iconEnt.getComponent<cro::Transform>().setPosition({ buttonNormalArea.width - buttonIconOffset, 0.f, 0.2f });
    iconEnt.addComponent<cro::Drawable2D>();
    iconEnt.addComponent<cro::Sprite>() = spriteSheetIcons.getSprite("play");

    //textEnt.addComponent<cro::Callback>().active = true;
    //textEnt.getComponent<cro::Callback>().function = [](cro::Entity e, cro::Time dt) {e.getComponent<cro::Transform>().rotate({ 0.f, 0.f, 1.f },dt.asSeconds()); };

    //options
    entity = m_menuScene.createEntity();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheetButtons.getSprite("button_inactive");
    auto& optionTx = entity.addComponent<cro::Transform>();
    optionTx.setOrigin({ buttonNormalArea.width / 2.f, buttonNormalArea.height / 2.f, 0.f });
    controlEntity.getComponent<cro::Transform>().addChild(optionTx);
    optionTx.setPosition({ 0.f, -120.f, -1.f });
    auto& optionControl = entity.addComponent<cro::UIInput>();
    optionControl.setGroup(GroupID::Main);
    optionControl.callbacks[cro::UIInput::Selected] = mouseEnterCallback;
    optionControl.callbacks[cro::UIInput::Unselected] = mouseExitCallback;
    optionControl.area.width = buttonNormalArea.width;
    optionControl.area.height = buttonNormalArea.height;

    optionControl.callbacks[cro::UIInput::ButtonUp] = m_uiSystem->addCallback([this]
    (cro::Entity, const cro::ButtonEvent& evt)
    {
        if (activated(evt))
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::MenuController;
            cmd.action = [](cro::Entity e, float)
            {
                auto& slider = e.getComponent<Slider>();
                slider.active = true;
                slider.destination = e.getComponent<cro::Transform>().getPosition() + glm::vec3(cro::DefaultSceneSize.x, 0.f, 0.f);
            };
            m_commandSystem->sendCommand(cmd);

            m_menuScene.getSystem<cro::UISystem>().setActiveGroup(GroupID::Options);
        }
    });

    textEnt = m_menuScene.createEntity();
    textEnt.addComponent<cro::Drawable2D>();
    auto& optionText = textEnt.addComponent<cro::Text>(menuFont);
    optionText.setString("Options");
    optionText.setFillColour(textColourNormal);
    optionText.setCharacterSize(TextLarge);
    //optionText.setBlendMode(cro::Material::BlendMode::Additive);
    auto& texTx = textEnt.addComponent<cro::Transform>();
    entity.getComponent<cro::Transform>().addChild(texTx);
    texTx.move({ 20.f, 80.f, 1.f });
    iconEnt = m_menuScene.createEntity();
    entity.getComponent<cro::Transform>().addChild(iconEnt.addComponent<cro::Transform>());
    iconEnt.getComponent<cro::Transform>().setPosition({ buttonNormalArea.width - buttonIconOffset, 0.f, 0.2f });
    iconEnt.addComponent<cro::Drawable2D>();
    iconEnt.addComponent<cro::Sprite>() = spriteSheetIcons.getSprite("settings");

    //high scores
    entity = m_menuScene.createEntity();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheetButtons.getSprite("button_inactive");
    auto& scoreTx = entity.addComponent<cro::Transform>();
    scoreTx.setPosition({ 0.f, -300.f, 0.f });
    scoreTx.setOrigin({ buttonNormalArea.width / 2.f, buttonNormalArea.height / 2.f, 0.f });
    controlEntity.getComponent<cro::Transform>().addChild(scoreTx);
    auto& scoreControl = entity.addComponent<cro::UIInput>();
    scoreControl.setGroup(GroupID::Main);
    scoreControl.callbacks[cro::UIInput::Selected] = mouseEnterCallback;
    scoreControl.callbacks[cro::UIInput::Unselected] = mouseExitCallback;
    scoreControl.area.width = buttonNormalArea.width;
    scoreControl.area.height = buttonNormalArea.height;

    auto scoreCallback = m_uiSystem->addCallback([this]
    (cro::Entity e, const cro::ButtonEvent& evt)
    {
        if (activated(evt))
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::MenuController;
            cmd.action = [](cro::Entity e, float)
            {
                auto& slider = e.getComponent<Slider>();
                slider.active = true;
                slider.destination = e.getComponent<cro::Transform>().getPosition() + glm::vec3(-static_cast<float>(cro::DefaultSceneSize.x), 0.f, 0.f);
            };
            m_commandSystem->sendCommand(cmd);

            m_menuScene.getSystem<cro::UISystem>().setActiveGroup(GroupID::Scores);
        }
    });
    scoreControl.callbacks[cro::UIInput::ButtonUp] = scoreCallback;

    textEnt = m_menuScene.createEntity();
    textEnt.addComponent<cro::Drawable2D>();
    auto& scoreText = textEnt.addComponent<cro::Text>(menuFont);
    scoreText.setString("Scores");
    scoreText.setFillColour(textColourNormal);
    scoreText.setCharacterSize(TextLarge);
    //scoreText.setBlendMode(cro::Material::BlendMode::Additive);
    auto& scoreTexTx = textEnt.addComponent<cro::Transform>();
    entity.getComponent<cro::Transform>().addChild(scoreTexTx);
    scoreTexTx.move({ 20.f, 80.f, 1.f });
    iconEnt = m_menuScene.createEntity();
    entity.getComponent<cro::Transform>().addChild(iconEnt.addComponent<cro::Transform>());
    iconEnt.getComponent<cro::Transform>().setPosition({ buttonNormalArea.width - buttonIconOffset, 0.f, 0.2f });
    iconEnt.addComponent<cro::Drawable2D>();
    iconEnt.addComponent<cro::Sprite>() = spriteSheetIcons.getSprite("scores");

    //quit button
    entity = m_menuScene.createEntity();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheetButtons.getSprite("button_inactive");
    auto& quitTx = entity.addComponent<cro::Transform>();
    quitTx.setPosition({ 0.f, -480.f, 0.f });
    controlEntity.getComponent<cro::Transform>().addChild(quitTx);
    quitTx.setOrigin({ buttonNormalArea.width / 2.f, buttonNormalArea.height / 2.f, 0.f });

    textEnt = m_menuScene.createEntity();
    textEnt.addComponent<cro::Drawable2D>();
    auto& quitText = textEnt.addComponent<cro::Text>(menuFont);
    quitText.setString("Quit");
    quitText.setFillColour(textColourNormal);
    quitText.setCharacterSize(TextLarge);
    //quitText.setBlendMode(cro::Material::BlendMode::Additive);
    auto& quitTexTx = textEnt.addComponent<cro::Transform>();
    entity.getComponent<cro::Transform>().addChild(quitTexTx);
    quitTexTx.move({ 20.f, 80.f, 1.f });
    iconEnt = m_menuScene.createEntity();
    entity.getComponent<cro::Transform>().addChild(iconEnt.addComponent<cro::Transform>());
    iconEnt.getComponent<cro::Transform>().setPosition({ buttonNormalArea.width - buttonIconOffset, 0.f, 0.2f });
    iconEnt.addComponent<cro::Drawable2D>();
    iconEnt.addComponent<cro::Sprite>() = spriteSheetIcons.getSprite("exit");

    auto quitCallback = m_uiSystem->addCallback([this](cro::Entity, const cro::ButtonEvent& evt)
    {
        if (activated(evt))
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::MenuController;
            cmd.action = [](cro::Entity e, float)
            {
                auto& slider = e.getComponent<Slider>();
                slider.active = true;
                slider.destination = e.getComponent<cro::Transform>().getPosition() + glm::vec3(0.f, static_cast<float>(cro::DefaultSceneSize.y), 0.f);
            };
            m_commandSystem->sendCommand(cmd);

            m_menuScene.getSystem<cro::UISystem>().setActiveGroup(GroupID::QuitConfirm);
        }
    });
    auto& quitControl = entity.addComponent<cro::UIInput>();
    quitControl.setGroup(GroupID::Main);
    quitControl.callbacks[cro::UIInput::ButtonUp] = quitCallback;
    quitControl.callbacks[cro::UIInput::Selected ] = mouseEnterCallback;
    quitControl.callbacks[cro::UIInput::Unselected] = mouseExitCallback;
    quitControl.area.width = buttonNormalArea.width;
    quitControl.area.height = buttonNormalArea.height;

    //quit confirmation
    entity = m_menuScene.createEntity();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheetButtons.getSprite("menu");
    size = entity.getComponent<cro::Sprite>().getSize();
    auto& confTx = entity.addComponent<cro::Transform>();
    controlEntity.getComponent<cro::Transform>().addChild(confTx);
    confTx.setPosition({ 0.f, -1080.f, -10.f });
    confTx.setOrigin({size.x / 2.f, size.y, 0.f });
    

    textEnt = m_menuScene.createEntity();
    textEnt.addComponent<cro::Drawable2D>();
    auto& confText = textEnt.addComponent<cro::Text>(menuFont);
    confText.setString("Exit?");
    confText.setFillColour(textColourSelected);
    //confText.setBlendMode(cro::Material::BlendMode::Additive);
    confText.setCharacterSize(TextMedium);
    auto& confTexTx = textEnt.addComponent<cro::Transform>();
    entity.getComponent<cro::Transform>().addChild(confTexTx);
    confTexTx.move({ (size.x / 2.f) - 56.f, size.y - 30.f, 0.f });


    //OK button
    auto buttonEnt = m_menuScene.createEntity();
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = spriteSheetButtons.getSprite("button_inactive");
    auto& buttonTx0 = buttonEnt.addComponent<cro::Transform>();
    entity.getComponent<cro::Transform>().addChild(buttonTx0);
    buttonTx0.setPosition({ size.x / 4.f, 82.f, 1.f });
    buttonTx0.setOrigin({ buttonNormalArea.width / 2.f, 0.f, 0.f });

    textEnt = m_menuScene.createEntity();
    textEnt.addComponent<cro::Drawable2D>();
    auto& buttonText0 = textEnt.addComponent<cro::Text>(menuFont);
    buttonText0.setString("OK");
    buttonText0.setFillColour(textColourNormal);
    //buttonText0.setBlendMode(cro::Material::BlendMode::Additive);
    buttonText0.setCharacterSize(TextXL);
    auto& buttonTextTx0 = textEnt.addComponent<cro::Transform>();
    buttonEnt.getComponent<cro::Transform>().addChild(buttonTextTx0);
    buttonTextTx0.setPosition({ 40.f, 80.f, 0.f });
    iconEnt = m_menuScene.createEntity();
    buttonEnt.getComponent<cro::Transform>().addChild(iconEnt.addComponent<cro::Transform>());
    iconEnt.getComponent<cro::Transform>().setPosition({ buttonNormalArea.width - buttonIconOffset, 0.f, 0.2f });
    iconEnt.addComponent<cro::Drawable2D>();
    iconEnt.addComponent<cro::Sprite>() = spriteSheetIcons.getSprite("exit");

    auto okCallback = m_uiSystem->addCallback([this](cro::Entity, const cro::ButtonEvent& evt)
    {
        if (activated(evt))
        {
            cro::App::quit();
        }
    });
    auto& okControl = buttonEnt.addComponent<cro::UIInput>();
    okControl.setGroup(GroupID::QuitConfirm);
    okControl.callbacks[cro::UIInput::ButtonUp] = okCallback;
    okControl.callbacks[cro::UIInput::Selected] = mouseEnterCallback;
    okControl.callbacks[cro::UIInput::Unselected] = mouseExitCallback;
    okControl.area.width = buttonNormalArea.width;
    okControl.area.height = buttonNormalArea.height;


    //cancel button
    buttonEnt = m_menuScene.createEntity();
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = spriteSheetButtons.getSprite("button_inactive");
    auto& buttonTx1 = buttonEnt.addComponent<cro::Transform>();
    entity.getComponent<cro::Transform>().addChild(buttonTx1);
    buttonTx1.setPosition({ (size.x / 4.f) + (size.x / 2.f), 82.f, 1.f });
    buttonTx1.setOrigin({ buttonNormalArea.width / 2.f, 0.f, 0.f });

    textEnt = m_menuScene.createEntity();
    textEnt.addComponent<cro::Drawable2D>();
    auto& buttonText1 = textEnt.addComponent<cro::Text>(menuFont);
    buttonText1.setString("Cancel");
    buttonText1.setFillColour(textColourNormal);
    //buttonText1.setBlendMode(cro::Material::BlendMode::Additive);
    buttonText1.setCharacterSize(TextXL);
    auto& buttonTextTx1 = textEnt.addComponent<cro::Transform>();
    buttonEnt.getComponent<cro::Transform>().addChild(buttonTextTx1);
    buttonTextTx1.setPosition({ 40.f, 80.f, 0.f });
    iconEnt = m_menuScene.createEntity();
    buttonEnt.getComponent<cro::Transform>().addChild(iconEnt.addComponent<cro::Transform>());
    iconEnt.getComponent<cro::Transform>().setPosition({ buttonNormalArea.width - buttonIconOffset, 0.f, 0.2f });
    iconEnt.addComponent<cro::Drawable2D>();
    iconEnt.addComponent<cro::Sprite>() = spriteSheetIcons.getSprite("back");

    auto cancelCallback = m_uiSystem->addCallback([this](cro::Entity, const cro::ButtonEvent& evt)
    {
        if (activated(evt))
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::MenuController;
            cmd.action = [](cro::Entity e, float)
            {
                auto& slider = e.getComponent<Slider>();
                slider.active = true;
                slider.destination = e.getComponent<cro::Transform>().getPosition() + glm::vec3(0.f, -static_cast<float>(cro::DefaultSceneSize.y), 0.f);
            };
            m_commandSystem->sendCommand(cmd);

            m_menuScene.getSystem<cro::UISystem>().setActiveGroup(GroupID::Main);
        }
    });
    auto& cancelControl = buttonEnt.addComponent<cro::UIInput>();
    cancelControl.setGroup(GroupID::QuitConfirm);
    cancelControl.callbacks[cro::UIInput::ButtonUp] = cancelCallback;
    cancelControl.callbacks[cro::UIInput::Selected] = mouseEnterCallback;
    cancelControl.callbacks[cro::UIInput::Unselected] = mouseExitCallback;
    cancelControl.area.width = buttonNormalArea.width;
    cancelControl.area.height = buttonNormalArea.height;

    createMapSelect(controlEntity, spriteSheetButtons, spriteSheetIcons);
}