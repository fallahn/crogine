/*-----------------------------------------------------------------------

Matt Marchant - 2026
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

#include "MenuState.hpp"

#include <crogine/detail/OpenGL.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/UIElement.hpp>
#include <crogine/ecs/systems/UIElementSystem.hpp>
#include <crogine/graphics/SpriteSheet.hpp>

using namespace UI;

namespace
{
    const std::array ItemLabels =
    {
        "Players", "Course", "Rules", "Scores"
    };

    //TODO this needs the down arrow icon for press/hold
    static const cro::String KeyInfo = "F4 - Open Chat   LAlt - Options   ESC - Close";

    static constexpr cro::Time RepeatTimeLong = cro::seconds(0.5f);
    static constexpr cro::Time RepeatTimeShort = cro::seconds(0.05f);
}

void MenuState::LobbyMenu::handleEvent(const cro::Event& evt)
{
    const auto setActiveInput =
        [&](bool mouse, std::int32_t controllerIndex)
        {
            if (mouse)
            {
                m_infoString.getComponent<cro::Text>().setString(KeyInfo); //garbled font bug strikes again!!
                m_infoString.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
                m_infoSprite.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
                m_sharedData.activeInput = SharedStateData::ActiveInput::Keyboard;

                m_uiLayout.tabBar.navLeft.getComponent<cro::Text>().setString("< " + cro::Keyboard::keyString(m_sharedData.inputBinding.scancodes[InputBinding::PrevClub]));
                m_uiLayout.tabBar.navRight.getComponent<cro::Text>().setString(cro::Keyboard::keyString(m_sharedData.inputBinding.scancodes[InputBinding::NextClub]) + " >");

                m_uiLayout.tabBar.navLeftSprite.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
                m_uiLayout.tabBar.navRightSprite.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);

                const auto viewScale = cro::UIElementSystem::getViewScale();
                const auto charSize = static_cast<std::uint32_t>((UITextSize)*viewScale);
                m_uiLayout.tabBar.navLeft.getComponent<cro::Text>().setCharacterSize(charSize);
                m_uiLayout.tabBar.navLeft.getComponent<cro::UIElement>().characterSize = UITextSize;
                m_uiLayout.tabBar.navLeft.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);

                m_uiLayout.tabBar.navRight.getComponent<cro::Text>().setCharacterSize(charSize);
                m_uiLayout.tabBar.navRight.getComponent<cro::UIElement>().characterSize = UITextSize;
                m_uiLayout.tabBar.navRight.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
            }
            else
            {
                m_infoString.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
                m_infoSprite.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);

                m_uiLayout.tabBar.navLeft.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
                m_uiLayout.tabBar.navRight.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);

                m_uiLayout.tabBar.navLeftSprite.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
                m_uiLayout.tabBar.navRightSprite.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);

                if (cro::GameController::hasPSLayout(controllerIndex))
                {
                    m_sharedData.activeInput = SharedStateData::ActiveInput::PS;
                    m_infoSprite.getComponent<cro::Sprite>().setTextureRect(m_infoRects[0]);

                    m_uiLayout.tabBar.navLeftSprite.getComponent<cro::Sprite>().setTextureRect(m_uiLayout.tabBar.navLeftRects[0]);
                    m_uiLayout.tabBar.navRightSprite.getComponent<cro::Sprite>().setTextureRect(m_uiLayout.tabBar.navRightRects[0]);
                }
                else
                {
                    m_sharedData.activeInput = SharedStateData::ActiveInput::XBox;
                    m_infoSprite.getComponent<cro::Sprite>().setTextureRect(m_infoRects[1]);

                    m_uiLayout.tabBar.navLeftSprite.getComponent<cro::Sprite>().setTextureRect(m_uiLayout.tabBar.navLeftRects[1]);
                    m_uiLayout.tabBar.navRightSprite.getComponent<cro::Sprite>().setTextureRect(m_uiLayout.tabBar.navRightRects[1]);
                }
            }
            cro::App::getWindow().setCursorVisible(!!mouse);
        };

    if (evt.type == SDL_EVENT_KEY_UP)
    {
        setActiveInput(true, 0);

        if (evt.key.key == SDLK_BACKSPACE
            || evt.key.key == SDLK_ESCAPE)
        {
            LogI << "Implement me!" << std::endl;
            //TODO close menu
        }

        if (evt.key.scancode == m_sharedData.inputBinding.scancodes[InputBinding::NextClub])
        {
            m_uiLayout.nextTab();
        }
        else if (evt.key.scancode == m_sharedData.inputBinding.scancodes[InputBinding::PrevClub])
        {
            m_uiLayout.prevTab();
        }
        else if (evt.key.scancode == m_sharedData.inputBinding.scancodes[InputBinding::Action]
            || evt.key.key == SDLK_RETURN)
        {
            m_uiLayout.activate();
        }

        switch (evt.key.key)
        {
        default: break;
        case SDLK_LCTRL:
            //TODO nothing?
            break;
        case SDLK_LALT:
            LogI << "Implement me!" << std::endl;
            //TODO show options
            break;
        }

    }
    else if (evt.type == SDL_EVENT_KEY_DOWN)
    {
        setActiveInput(true, 0);

        //do this here to take advantage of key repeat
        if (evt.key.key == SDLK_DOWN)
        {
            m_uiLayout.nextItem();
        }
        else if (evt.key.key == SDLK_UP)
        {
            m_uiLayout.prevItem();
        }
        else if (evt.key.key == SDLK_LEFT)
        {
            m_uiLayout.activateLeft();
        }
        else if (evt.key.key == SDLK_RIGHT)
        {
            m_uiLayout.activateRight();
        }
    }
    else if (evt.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
    {
        const auto controllerID = cro::GameController::controllerID(evt.gbutton.which);
        setActiveInput(false, controllerID);

        switch (evt.gbutton.button)
        {
        default: break;
        case cro::GameController::DPadUp:
            m_uiLayout.prevItem();
            resetRepeatTimer(controllerID, RepeatTimeLong);
            break;
        case cro::GameController::DPadDown:
            m_uiLayout.nextItem();
            resetRepeatTimer(controllerID, RepeatTimeLong);
            break;
        case cro::GameController::DPadLeft:
            m_uiLayout.activateLeft();
            resetRepeatTimer(controllerID, RepeatTimeLong);
            break;
        case cro::GameController::DPadRight:
            m_uiLayout.activateRight();
            resetRepeatTimer(controllerID, RepeatTimeLong);
            break;
        }
    }
    else if (evt.type == SDL_EVENT_GAMEPAD_BUTTON_UP)
    {
        switch (evt.gbutton.button)
        {
        default: break;
        case cro::GameController::ButtonLeftShoulder:
            m_uiLayout.prevTab();
            break;
        case cro::GameController::ButtonRightShoulder:
            m_uiLayout.nextTab();
            break;
        case cro::GameController::ButtonX:
            LogI << "Implement me!" << std::endl;
            //TODO show options menu
            break;
        case cro::GameController::ButtonY:
            LogI << "Implement me!" << std::endl;
            //TODO chat window
            break;
        case cro::GameController::ButtonA:
            m_uiLayout.activate();
            break;
        case cro::GameController::ButtonB:
            LogI << "Implement me!" << std::endl;
            break;
        }
    }

    else if (evt.type == SDL_EVENT_MOUSE_BUTTON_UP)
    {
        if (evt.button.button == SDL_BUTTON_LEFT)
        {
            m_uiLayout.doMouseClick({ evt.motion.x, evt.motion.y }, m_menuState.m_uiScene.getActiveCamera().getComponent<cro::Camera>());
        }
        else if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            LogI << "Implement me!" << std::endl;
        }
    }

    else if (evt.type == SDL_EVENT_MOUSE_MOTION)
    {
        setActiveInput(true, 0);

        glm::vec2 pos(evt.motion.x, cro::App::getWindow().getSize().y - evt.motion.y);
        m_uiLayout.checkMouseOver(pos);
    }
    else if (evt.type == SDL_EVENT_GAMEPAD_AXIS_MOTION)
    {
        constexpr std::int16_t Threshold = std::numeric_limits<std::int16_t>::max() / 2;// cro::GameController::LeftThumbDeadZone * 2;// 15000;
        const auto controllerID = cro::GameController::controllerID(evt.gaxis.which);

        if (std::abs(evt.gaxis.value) > Threshold)
        {
            setActiveInput(false, controllerID);
        }


        if (controllerID != -1
            && controllerID < 4)
        {
            switch (evt.gaxis.axis)
            {
            default: break;
            case SDL_GAMEPAD_AXIS_LEFTX:
                if (evt.gaxis.value > Threshold)
                {
                    //right
                    m_controllerMasks[controllerID] |= InputFlag::Right;
                    m_controllerMasks[controllerID] &= ~InputFlag::Left;
                }
                else if (evt.gaxis.value < -Threshold)
                {
                    //left
                    m_controllerMasks[controllerID] |= InputFlag::Left;
                    m_controllerMasks[controllerID] &= ~InputFlag::Right;
                }
                else
                {
                    m_controllerMasks[controllerID] &= ~(InputFlag::Left | InputFlag::Right);
                }
                break;
            case SDL_GAMEPAD_AXIS_LEFTY:
                if (evt.gaxis.value > Threshold)
                {
                    //down
                    m_controllerMasks[controllerID] |= InputFlag::Down;
                    m_controllerMasks[controllerID] &= ~InputFlag::Up;
                }
                else if (evt.gaxis.value < -Threshold)
                {
                    //up
                    m_controllerMasks[controllerID] |= InputFlag::Up;
                    m_controllerMasks[controllerID] &= ~InputFlag::Down;
                }
                else
                {
                    m_controllerMasks[controllerID] &= ~(InputFlag::Up | InputFlag::Down);
                }
                break;
            }
        }

    }
    else if (evt.type == SDL_EVENT_MOUSE_WHEEL)
    {
        if (evt.wheel.y > 0)
        {
            m_uiLayout.prevItem();
        }
        else if (evt.wheel.y < 0)
        {
            m_uiLayout.nextItem();
        }
    }

}

void MenuState::LobbyMenu::resetRepeatTimer(std::int32_t i, cro::Time resetTime)
{
    m_inputRepeatClocks[i].restart();
    m_repeatTimes[i] = resetTime;
}

void MenuState::LobbyMenu::create(cro::Entity)
{
    m_uiLayout.loadAssets(*m_sharedData.sharedResources);

    //TODO replace this with passed in parameter
    auto rootNode = m_menuState.m_uiScene.createEntity();
    rootNode.addComponent<cro::Transform>();
    rootNode.addComponent<cro::Callback>().active = true;
    rootNode.getComponent<cro::Callback>().function =
        [](cro::Entity e, float)
        {
            const auto pos = glm::vec2(cro::App::getWindow().getSize()) / 2.f;
            e.getComponent<cro::Transform>().setPosition(glm::vec3(pos, 1.f));
        };

    //tab bar
    m_uiLayout.tabBar.background = m_menuState.m_uiScene.createEntity();
    m_uiLayout.tabBar.background.addComponent<cro::Transform>();
    m_uiLayout.tabBar.background.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    m_uiLayout.tabBar.background.getComponent<cro::Drawable2D>().setTexture(m_uiLayout.uiTexture);
    m_uiLayout.tabBar.background.addComponent<cro::UIElement>(cro::UIElement::Position, true);
    m_uiLayout.tabBar.background.getComponent<cro::UIElement>().relativePosition = { -0.5f, 0.5f };
    m_uiLayout.tabBar.background.getComponent<cro::UIElement>().absolutePosition = { 0.f, -(TabBarHeight * 2.f) };
    rootNode.getComponent<cro::Transform>().addChild(m_uiLayout.tabBar.background.getComponent<cro::Transform>());

    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    const float Spacing = 1.f / std::int32_t(TabID::Count/* + 1*/);
    for (auto i = 0; i < TabID::Count; ++i)
    {
        auto& item = m_uiLayout.tabBar.items[i];
        item.text = m_menuState.m_uiScene.createEntity();
        item.text.addComponent<cro::Transform>();
        item.text.addComponent<cro::Drawable2D>();
        item.text.addComponent<cro::Text>(smallFont).setString(ItemLabels[i]);
        item.text.getComponent<cro::Text>().setFillColour(TextNormalColour);
        item.text.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);

        auto& uiElement = item.text.addComponent<cro::UIElement>(cro::UIElement::Text, true);
        uiElement.characterSize = InfoTextSize;
        uiElement.depth = 0.1f;
        const float offset = (Spacing / 2.f) + (Spacing * i);
        uiElement.resizeCallback =
            [&, offset](cro::Entity e)
            {
                const auto x = std::floor((static_cast<float>(cro::App::getWindow().getSize().x) / cro::UIElementSystem::getViewScale()) * offset);// +2.f;
                const auto y = 12.f;
                e.getComponent<cro::UIElement>().absolutePosition = { x,y };
            };

        m_uiLayout.tabBar.background.getComponent<cro::Transform>().addChild(item.text.getComponent<cro::Transform>());
    }

    const auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    
    //title text
    auto entity = m_menuState.m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setString("Settings");
    entity.addComponent<cro::UIElement>(cro::UIElement::Text, true);
    entity.getComponent<cro::UIElement>().characterSize = UITextSize;
    entity.getComponent<cro::UIElement>().depth = 0.1f;
    entity.getComponent<cro::UIElement>().resizeCallback =
        [&, Spacing](cro::Entity e)
        {
            const auto x = std::floor((static_cast<float>(cro::App::getWindow().getSize().x) / cro::UIElementSystem::getViewScale()) / 2.f);
            constexpr auto y = 28.f;
            e.getComponent<cro::UIElement>().absolutePosition = { x,y };
        };
    m_uiLayout.tabBar.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_uiLayout.tabBar.titleText = entity;

    //text for scroll left
    entity = m_menuState.m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    entity.addComponent<cro::Text>(largeFont).setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<cro::UIElement>(cro::UIElement::Text, true);
    entity.getComponent<cro::UIElement>().characterSize = UITextSize;
    entity.getComponent<cro::UIElement>().depth = 0.1f;
    entity.getComponent<cro::UIElement>().resizeCallback =
        [&, Spacing](cro::Entity e)
        {
            const auto x = std::floor((static_cast<float>(cro::App::getWindow().getSize().x) / cro::UIElementSystem::getViewScale()) * (Spacing / 4.f));
            constexpr auto y = 26.f;
            e.getComponent<cro::UIElement>().absolutePosition = { x,y };
        };
    m_uiLayout.tabBar.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_uiLayout.tabBar.navLeft = entity;

    //text for scroll right
    entity = m_menuState.m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    entity.addComponent<cro::Text>(largeFont).setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<cro::UIElement>(cro::UIElement::Text, true);
    entity.getComponent<cro::UIElement>().characterSize = UITextSize;
    entity.getComponent<cro::UIElement>().depth = 0.1f;
    entity.getComponent<cro::UIElement>().resizeCallback =
        [&, Spacing](cro::Entity e)
        {
            const auto offset = (Spacing * (m_uiLayout.tabBar.items.size() - 1)) + (Spacing * 0.75f);
            const auto x = std::floor((static_cast<float>(cro::App::getWindow().getSize().x) / cro::UIElementSystem::getViewScale()) * offset);
            constexpr auto y = 26.f;
            e.getComponent<cro::UIElement>().absolutePosition = { x,y };
        };
    m_uiLayout.tabBar.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_uiLayout.tabBar.navRight = entity;


    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/options_buttons.spt", m_menuState.m_sharedData.sharedResources->textures);
    m_uiLayout.tabBar.navLeftRects[0] = spriteSheet.getSprite("l1").getTextureRect();
    m_uiLayout.tabBar.navLeftRects[1] = spriteSheet.getSprite("lb").getTextureRect();

    m_uiLayout.tabBar.navRightRects[0] = spriteSheet.getSprite("r1").getTextureRect();
    m_uiLayout.tabBar.navRightRects[1] = spriteSheet.getSprite("rb").getTextureRect();

    const auto bounds = spriteSheet.getSprite("l1").getTextureBounds();

    //sprite for scroll left
    entity = m_menuState.m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), bounds.height / 2.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("lb");
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, true);
    entity.getComponent<cro::UIElement>().depth = 0.1f;
    entity.getComponent<cro::UIElement>().resizeCallback =
        [&, Spacing](cro::Entity e)
        {
            const auto x = std::floor((static_cast<float>(cro::App::getWindow().getSize().x) / cro::UIElementSystem::getViewScale()) * (Spacing / 4.f));
            constexpr auto y = 23.f;
            e.getComponent<cro::UIElement>().absolutePosition = { x,y };
        };
    m_uiLayout.tabBar.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_uiLayout.tabBar.navLeftSprite = entity;

    //sprite for scroll right
    entity = m_menuState.m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), bounds.height / 2.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("rb");
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, true);
    entity.getComponent<cro::UIElement>().depth = 0.1f;
    entity.getComponent<cro::UIElement>().resizeCallback =
        [&, Spacing](cro::Entity e)
        {
            const auto offset = (Spacing * (m_uiLayout.tabBar.items.size() - 1)) + (Spacing * 0.75f);
            const auto x = std::floor((static_cast<float>(cro::App::getWindow().getSize().x) / cro::UIElementSystem::getViewScale()) * offset);
            constexpr auto y = 23.f;
            e.getComponent<cro::UIElement>().absolutePosition = { x,y };
        };
    m_uiLayout.tabBar.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_uiLayout.tabBar.navRightSprite = entity;


    //main sprite
    m_uiLayout.menuLayout.sprite = m_menuState.m_uiScene.createEntity();
    m_uiLayout.menuLayout.sprite.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.2f });
    m_uiLayout.menuLayout.sprite.addComponent<cro::Drawable2D>();
    m_uiLayout.menuLayout.sprite.addComponent<cro::Sprite>();
    rootNode.getComponent<cro::Transform>().addChild(m_uiLayout.menuLayout.sprite.getComponent<cro::Transform>());

    //details window on right side
    m_uiLayout.detailsPane.root = m_menuState.m_uiScene.createEntity();
    m_uiLayout.detailsPane.root.addComponent<cro::Transform>();
    m_uiLayout.detailsPane.root.addComponent<cro::UIElement>(cro::UIElement::Position, false);
    m_uiLayout.detailsPane.root.getComponent<cro::UIElement>().relativePosition = { 0.f, 0.f }; //this is set set when updating the active tab, might be right or left aligned
    rootNode.getComponent<cro::Transform>().addChild(m_uiLayout.detailsPane.root.getComponent<cro::Transform>());

    //text
    m_uiLayout.detailsPane.text = m_menuState.m_uiScene.createEntity();
    m_uiLayout.detailsPane.text.addComponent<cro::Transform>();
    m_uiLayout.detailsPane.text.addComponent<cro::Drawable2D>();
    m_uiLayout.detailsPane.text.addComponent<cro::Text>(largeFont);
    m_uiLayout.detailsPane.text.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    m_uiLayout.detailsPane.text.getComponent<cro::Text>().setFillColour(TextNormalColour);
    m_uiLayout.detailsPane.text.addComponent<cro::UIElement>(cro::UIElement::Text, true);
    m_uiLayout.detailsPane.text.getComponent<cro::UIElement>().absolutePosition = { DetailBackgroundOffset, -82.f }; //90
    m_uiLayout.detailsPane.text.getComponent<cro::UIElement>().characterSize = UITextSize;
    m_uiLayout.detailsPane.text.getComponent<cro::UIElement>().verticalSpacing = 3.f;
    m_uiLayout.detailsPane.text.getComponent<cro::UIElement>().depth = 0.2f;
    m_uiLayout.detailsPane.root.getComponent<cro::Transform>().addChild(m_uiLayout.detailsPane.text.getComponent<cro::Transform>());

    //image
    m_uiLayout.detailsPane.image = m_menuState.m_uiScene.createEntity();
    m_uiLayout.detailsPane.image.addComponent<cro::Transform>();
    m_uiLayout.detailsPane.image.addComponent<cro::Drawable2D>();
    m_uiLayout.detailsPane.image.addComponent<cro::Sprite>();
    m_uiLayout.detailsPane.image.addComponent<cro::UIElement>(cro::UIElement::Sprite, true);
    m_uiLayout.detailsPane.image.getComponent<cro::UIElement>().absolutePosition = { DetailBackgroundOffset, -10.f };
    m_uiLayout.detailsPane.image.getComponent<cro::UIElement>().depth = 0.2f;
    m_uiLayout.detailsPane.root.getComponent<cro::Transform>().addChild(m_uiLayout.detailsPane.image.getComponent<cro::Transform>());

    //background/9 patch
    m_uiLayout.detailsPane.background = m_menuState.m_uiScene.createEntity();
    m_uiLayout.detailsPane.background.addComponent<cro::Transform>().setOrigin({ 0.f, InfoBarHeight / 2.f });
    m_uiLayout.detailsPane.background.addComponent<cro::Drawable2D>().setTexture(m_uiLayout.uiTexture);
    m_uiLayout.detailsPane.background.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    m_uiLayout.detailsPane.background.addComponent<cro::UIElement>(cro::UIElement::Sprite, true);
    m_uiLayout.detailsPane.background.getComponent<cro::UIElement>().absolutePosition = { DetailBackgroundOffset, 8.f };
    m_uiLayout.detailsPane.background.getComponent<cro::UIElement>().resizeCallback =
        [&](cro::Entity e)
        {

        };
    m_uiLayout.detailsPane.background.getComponent<cro::UIElement>().depth = -0.3f;
    m_uiLayout.detailsPane.root.getComponent<cro::Transform>().addChild(m_uiLayout.detailsPane.background.getComponent<cro::Transform>());


    //displays a scroll icon so we can see how far down the items list we are
    //TODO this is identical to the Profile state so we can share this code
    struct ScrollData final
    {
        std::uint32_t lastIdx = 0;
        float amount = 0.f;
    };
    m_uiLayout.detailsPane.scrollIcon = m_menuState.m_uiScene.createEntity();
    m_uiLayout.detailsPane.scrollIcon.addComponent<cro::Transform>().setOrigin({ 2.f, 7.f });
    m_uiLayout.detailsPane.scrollIcon.addComponent<cro::Drawable2D>();
    m_uiLayout.detailsPane.scrollIcon.addComponent<cro::Sprite>() = spriteSheet.getSprite("scroll_handle");
    m_uiLayout.detailsPane.scrollIcon.addComponent<cro::UIElement>(cro::UIElement::Sprite, true);
    m_uiLayout.detailsPane.scrollIcon.getComponent<cro::UIElement>().depth = 0.2f;
    m_uiLayout.detailsPane.scrollIcon.addComponent<cro::Callback>().active = true;
    m_uiLayout.detailsPane.scrollIcon.getComponent<cro::Callback>().setUserData<ScrollData>();
    m_uiLayout.detailsPane.scrollIcon.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& [idx, ct] = e.getComponent<cro::Callback>().getUserData<ScrollData>();
            if (idx != m_uiLayout.menuLayout.itemIndex)
            {
                idx = m_uiLayout.menuLayout.itemIndex;
                ct = 1.f;
            }

            ct = std::max(0.f, ct - dt);
            auto c = cro::Colour::White;
            c.setAlpha(ct);
            e.getComponent<cro::Sprite>().setColour(c);

            const float ratio = static_cast<float>(std::max(1u, m_uiLayout.menuLayout.itemIndex)) / (m_uiLayout.menuLayout.items[m_uiLayout.tabBar.activeIndex].size() - 1);
            glm::vec2 pos = { -m_uiLayout.detailsPane.backgroundSize.x / 2.f, -m_uiLayout.detailsPane.backgroundSize.y * ratio };
            pos.y += m_uiLayout.detailsPane.backgroundSize.y / 2.f;

            e.getComponent<cro::Transform>().setPosition(pos * e.getComponent<cro::Transform>().getScale().x);
        };
    m_uiLayout.detailsPane.root.getComponent<cro::Transform>().addChild(m_uiLayout.detailsPane.scrollIcon.getComponent<cro::Transform>());


    //displays an Apply icon if an item requests it
    m_uiLayout.detailsPane.applyButton = m_menuState.m_uiScene.createEntity();
    m_uiLayout.detailsPane.applyButton.addComponent<cro::Transform>();
    m_uiLayout.detailsPane.applyButton.addComponent<cro::Callback>().active = true;
    m_uiLayout.detailsPane.applyButton.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            e.getComponent<cro::Transform>().setPosition(-(m_uiLayout.detailsPane.backgroundSize / 2.f) * cro::UIElementSystem::getViewScale());
        };
    m_uiLayout.detailsPane.root.getComponent<cro::Transform>().addChild(m_uiLayout.detailsPane.applyButton.getComponent<cro::Transform>());

    entity = m_menuState.m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("Enter - Apply");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIElement>(cro::UIElement::Text, true);
    entity.getComponent<cro::UIElement>().characterSize = UITextSize;
    entity.getComponent<cro::UIElement>().absolutePosition = { 12.f, 12.f };
    entity.getComponent<cro::UIElement>().depth = 0.2f;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            e.getComponent<cro::Drawable2D>().setFacing(
                m_sharedData.activeInput == SharedStateData::ActiveInput::Keyboard ?
                cro::Drawable2D::Facing::Front : cro::Drawable2D::Facing::Back);
        };
    m_uiLayout.detailsPane.applyButton.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    entity = m_menuState.m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("apply_xbox");
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, true);
    entity.getComponent<cro::UIElement>().absolutePosition = { 12.f, 4.f };
    entity.getComponent<cro::UIElement>().depth = 0.2f;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            e.getComponent<cro::Drawable2D>().setFacing(
                m_sharedData.activeInput == SharedStateData::ActiveInput::XBox ?
                cro::Drawable2D::Facing::Front : cro::Drawable2D::Facing::Back);
        };
    m_uiLayout.detailsPane.applyButton.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_menuState.m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("apply_ps");
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, true);
    entity.getComponent<cro::UIElement>().absolutePosition = { 12.f, 4.f };
    entity.getComponent<cro::UIElement>().depth = 0.2f;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            e.getComponent<cro::Drawable2D>().setFacing(
                m_sharedData.activeInput == SharedStateData::ActiveInput::PS ?
                cro::Drawable2D::Facing::Front : cro::Drawable2D::Facing::Back);
        };
    m_uiLayout.detailsPane.applyButton.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //menu layouts
    createPlayerTab();
    createCourseTab();
    createRulesTab();
    createScoresTab();

    m_uiLayout.updateTabBar(); //this also updates the menu items


    //info string at the bottom
    entity = m_menuState.m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    entity.addComponent<cro::Text>(largeFont).setString(KeyInfo);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIElement>(cro::UIElement::Text, true).characterSize = UITextSize;
    entity.getComponent<cro::UIElement>().depth = 0.1f;
    entity.getComponent<cro::UIElement>().absolutePosition = { 12.f, 16.f };
    entity.getComponent<cro::UIElement>().resizeCallback =
        [&](cro::Entity e)
        {
            e.getComponent<cro::Transform>().setOrigin(glm::vec2(cro::App::getWindow().getSize()) / 2.f);
        };
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_infoString = entity;

    m_infoRects[0] = spriteSheet.getSprite("info_ps").getTextureRect();
    m_infoRects[1] = spriteSheet.getSprite("info_xbox").getTextureRect();

    entity = m_menuState.m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("info_xbox");
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, true);
    entity.getComponent<cro::UIElement>().depth = 0.1f;
    entity.getComponent<cro::UIElement>().absolutePosition = { 12.f, 2.f };
    entity.getComponent<cro::UIElement>().resizeCallback =
        [&](cro::Entity e)
        {
            auto o = (glm::vec2(cro::App::getWindow().getSize()) / 2.f) / cro::UIElementSystem::getViewScale();
            o.x = std::round(o.x);
            o.y = std::round(o.y);
            e.getComponent<cro::Transform>().setOrigin(o);
        };
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_infoSprite = entity;

    entity = m_menuState.m_uiScene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [this](cro::Entity e, float)
        {
            m_uiLayout.activateTab(0);
            e.getComponent<cro::Callback>().active = false;
            m_menuState.m_uiScene.destroyEntity(e);
        };
}

void MenuState::LobbyMenu::createPlayerTab()
{
    auto* item = &m_uiLayout.menuLayout.items[TabID::Players].emplace_back();
    item->title = "Player Menu";
    item->displayType = Menu::Item::Heading;
    //item->description = "Customise in-game display settings";

    item = &m_uiLayout.menuLayout.items[TabID::Players].emplace_back();
    item->title = "Start Game";
    item->description = "Press and Hold to Start";
    item->selected =
        [&](const Menu::Item&)
        {
            /*m_uiLayout.detailsPane.image.getComponent<cro::Sprite>() = m_optionIcons[OptionIcon::BeaconColour];
            m_uiLayout.detailsPane.image.getComponent<cro::Transform>().setOrigin({ m_optionIcons[OptionIcon::BeaconColour].getTextureBounds().width / 2.f, 0.f });
            m_uiLayout.detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);*/
        };
    item->activated = [&](Menu::Item& i)
        {
            //TODO press / hold to start or ready up
        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = m_sharedData.showBeacon ? 1 : 0;
}

void MenuState::LobbyMenu::createCourseTab()
{
    auto* item = &m_uiLayout.menuLayout.items[TabID::Course].emplace_back();
    item->title = "Course Selection";
    item->displayType = Menu::Item::Heading;
    //item->description = "Customise in-game display settings";

    item = &m_uiLayout.menuLayout.items[TabID::Course].emplace_back();
    item->title = "Select Course";
    //item->description = "Draws a beacon at the pin position, visible from a distance";
    item->selected =
        [&](const Menu::Item&)
        {

        };
    item->activated = [&](Menu::Item& i)
        {

        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = 0;
}

void MenuState::LobbyMenu::createRulesTab()
{
    auto* item = &m_uiLayout.menuLayout.items[TabID::Rules].emplace_back();
    item->title = "Game Rules";
    item->displayType = Menu::Item::Heading;
    //item->description = "Customise in-game display settings";

    item = &m_uiLayout.menuLayout.items[TabID::Rules].emplace_back();
    item->title = "Scoring";
    //item->description = "Draws a beacon at the pin position, visible from a distance";
    item->selected =
        [&](const Menu::Item&)
        {

        };
    item->activated = [&](Menu::Item& i)
        {

        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = 0;
}

void MenuState::LobbyMenu::createScoresTab()
{
    //leaderboards
    auto* item = &m_uiLayout.menuLayout.items[TabID::Scores].emplace_back();
    item->title = "View Scores";
    item->displayType = Menu::Item::Heading;
    //item->description = "Customise in-game display settings";

    item = &m_uiLayout.menuLayout.items[TabID::Scores].emplace_back();
    item->title = "View Leaderboards";
    //item->description = "Draws a beacon at the pin position, visible from a distance";
    item->selected =
        [&](const Menu::Item&)
        {

        };
    item->activated = [&](Menu::Item& i)
        {
            //TODO push leaderboard state
        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = 0;


    //view leagues
    item = &m_uiLayout.menuLayout.items[TabID::Scores].emplace_back();
    item->title = "View Leagues";
    //item->description = "Draws a beacon at the pin position, visible from a distance";
    item->selected =
        [&](const Menu::Item&)
        {

        };
    item->activated = [&](Menu::Item& i)
        {
            //TODO push league state
        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = 0;


    //view previous rounds scores
    item = &m_uiLayout.menuLayout.items[TabID::Scores].emplace_back();
    item->title = "View Last Round's Scores";
    //item->description = "Draws a beacon at the pin position, visible from a distance";
    item->selected =
        [&](const Menu::Item&)
        {

        };
    item->activated = [&](Menu::Item& i)
        {
            //TODO push scores
        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = 0;
}