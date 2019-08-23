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

void MainState::createScoreMenu()
{
    auto& testFont = m_fontResource.get(FontID::MenuFont);

    //TODO centralise UI dimensions
    cro::FloatRect buttonArea(0.f, 0.f, 256.f, 64.f);

    //TODO not duplicate these
    auto mouseEnterCallback = m_uiSystem->addCallback([buttonArea](cro::Entity e, cro::uint64)
    {
        auto area = buttonArea;
        area.left = buttonArea.width;
        e.getComponent<cro::Sprite>().setTextureRect(area);
    });
    auto mouseExitCallback = m_uiSystem->addCallback([buttonArea](cro::Entity e, cro::uint64)
    {
        e.getComponent<cro::Sprite>().setTextureRect(buttonArea);
    });

    //create an entity to move the menu
    auto controlEntity = m_menuScene.createEntity();
    auto& controlTx = controlEntity.addComponent<cro::Transform>();
    controlTx.setPosition({ 2880.f, 624.f, 0.f });
    controlEntity.addComponent<cro::CommandTarget>().ID = CommandID::MenuController;
    controlEntity.addComponent<Slider>();

    auto textEnt = m_menuScene.createEntity();
    auto& titleText = textEnt.addComponent<cro::Text>(testFont);
    titleText.setString("Scores");
    titleText.setColour(cro::Colour::White());
    //TODO set font size
    auto& titleTextTx = textEnt.addComponent<cro::Transform>();
    titleTextTx.setPosition({ -60.f, 80.f, 0.f });
    titleTextTx.setParent(controlEntity);

    auto& uiTexture = m_textureResource.get("assets/sprites/menu.png");
    uiTexture.setSmooth(true);
    auto entity = m_menuScene.createEntity();
    auto& backSprite = entity.addComponent<cro::Sprite>();
    backSprite.setTexture(uiTexture);
    backSprite.setTextureRect(buttonArea);
    auto& backTx = entity.addComponent<cro::Transform>();
    backTx.setPosition({ 0.f, -480.f, 0.f });
    backTx.setScale({ 2.f, 2.f, 2.f });
    backTx.setParent(controlEntity);
    backTx.setOrigin({ buttonArea.width, buttonArea.height, 0.f });

    textEnt = m_menuScene.createEntity();
    auto& backText = textEnt.addComponent<cro::Text>(testFont);
    backText.setString("Back");
    backText.setColour(cro::Colour::Red());
    auto& backTexTx = textEnt.addComponent<cro::Transform>();
    backTexTx.setParent(entity);
    backTexTx.move({ 88.f, 50.f, 0.f });

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
                slider.destination = e.getComponent<cro::Transform>().getPosition() + glm::vec3(static_cast<float>(cro::DefaultSceneSize.x), 0.f, 0.f);
            };
            m_commandSystem->sendCommand(cmd);
        }
    });
    auto& backControl = entity.addComponent<cro::UIInput>();
    backControl.callbacks[cro::UIInput::MouseUp] = backCallback;
    backControl.callbacks[cro::UIInput::MouseEnter] = mouseEnterCallback;
    backControl.callbacks[cro::UIInput::MouseExit] = mouseExitCallback;
    backControl.area = buttonArea;
}