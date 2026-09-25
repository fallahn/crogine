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
#include "PacketIDs.hpp"

#include <crogine/detail/OpenGL.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/UIElement.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/UIElementSystem.hpp>
#include <crogine/graphics/SpriteSheet.hpp>

using namespace UI;

namespace
{
#include "shaders/ProgressShader.inl"

    const std::array ItemLabels =
    {
        "Players", "Course", "Rules", "Scores"
    };

    //TODO this needs the down arrow icon for press/hold
    static const cro::String KeyInfo = "F4 - Open Chat   LAlt - Options   ESC - Close";

    static constexpr cro::Time RepeatTimeLong = cro::seconds(0.5f);
    static constexpr cro::Time RepeatTimeShort = cro::seconds(0.05f);

    glm::uvec2 lastWindowSize = { 0u,0u };
}

void MenuState::LobbyMenu::handleEvent(const cro::Event& evt)
{
    const auto setActiveInput =
        [this](bool mouse, std::int32_t controllerIndex)
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
            m_buttonFlags &= ~ButtonFlags::Options;
            break;
        case SDLK_ESCAPE:
        case SDLK_BACKSPACE:
            m_buttonFlags &= ~ButtonFlags::Quit;
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

        switch (evt.key.key)
        {
        default: break;
        case SDLK_LALT:
            m_buttonFlags |= ButtonFlags::Options;
            setProgressColour(CD32::Colours[CD32::BlueLight]);
            break;
        case SDLK_ESCAPE:
        case SDLK_BACKSPACE:
            m_buttonFlags |= ButtonFlags::Quit;
            setProgressColour(CD32::Colours[CD32::Red]);
            break;
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
        case cro::GameController::ButtonA:
            m_uiLayout.activate();
            break;
        case cro::GameController::ButtonB:
            m_buttonFlags |= ButtonFlags::Quit;
            setProgressColour(CD32::Colours[CD32::Red]);
            break;
        case cro::GameController::ButtonX:
            m_buttonFlags |= ButtonFlags::Options;
            setProgressColour(CD32::Colours[CD32::BlueLight]);
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
        case cro::GameController::ButtonY:
            //chat window is handled by MenuState event
            break;
        case cro::GameController::ButtonB:
            m_buttonFlags &= ~ButtonFlags::Quit;
            break;
        case cro::GameController::ButtonX:
            m_buttonFlags &= ~ButtonFlags::Options;
            break;
        }
    }

    else if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
    {
        if (evt.button.button == SDL_BUTTON_LEFT)
        {
            m_uiLayout.doMouseClick({ evt.motion.x, evt.motion.y }, m_menuState.m_uiScene.getActiveCamera().getComponent<cro::Camera>());
        }
        else if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            m_buttonFlags |= ButtonFlags::Quit;
            setProgressColour(CD32::Colours[CD32::Red]);
        }
    }
    else if (evt.type == SDL_EVENT_MOUSE_BUTTON_UP)
    {
        if (evt.button.button == SDL_BUTTON_LEFT)
        {
            //m_uiLayout.doMouseClick({ evt.motion.x, evt.motion.y }, m_menuState.m_uiScene.getActiveCamera().getComponent<cro::Camera>());
            m_buttonFlags &= ~ButtonFlags::Action;
        }
        else if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            m_buttonFlags &= ~ButtonFlags::Quit;
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

void MenuState::LobbyMenu::simulate(float dt)
{
    //press/hold to exit or show options
    static constexpr float MaxHoldTime = 0.35f;
    if (m_buttonFlags)
    {
        m_buttonHoldTimer = std::min(m_buttonHoldTimer + dt, MaxHoldTime);

        if (m_buttonHoldTimer >= MaxHoldTime)
        {
            switch (m_buttonFlags)
            {
            default:
                //mode than one button held, so invalid
                m_buttonHoldTimer = 0.f;
                break;
            case ButtonFlags::Quit:
                m_menuState.quitLobby();
                break;
            case ButtonFlags::Options:
                m_menuState.requestStackPush(StateID::Options);
                break;
            case ButtonFlags::Action:
                if (m_timeoutCallback)
                {
                    m_timeoutCallback();
                }
                break;
            }
            m_buttonFlags = 0;
        }
    }
    else
    {
        m_buttonHoldTimer = 0.f;
    }

    glUseProgram(m_progressShader.getGLHandle());
    glUniform1f(m_progressUniform, m_buttonHoldTimer / MaxHoldTime);
}

void MenuState::LobbyMenu::clientStatusChanged()
{
    updatePlayersTab();
    updateCourseTab();
    updateRulesTab();
    updateScoresTab();
}

void MenuState::LobbyMenu::readyStart()
{
    //make sure we've definitely sent the sever our selected clubset
    const std::uint16_t data = (m_sharedData.clientConnection.connectionID << 8) | std::uint8_t(Social::getClubLevel());
    m_sharedData.clientConnection.netClient.sendPacket(PacketID::ClubLevel, data, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

    if (m_sharedData.hosting)
    {
        //prevents starting the game if a game mode requires a certain number of players
        //or team play is not allowed
        if (m_menuState.m_connectedPlayerCount < ScoreType::MinPlayerCount[m_sharedData.scoreType]
            || m_menuState.m_connectedPlayerCount > ScoreType::MaxPlayerCount[m_sharedData.scoreType]
            || (m_menuState.m_sharedData.teamMode && !ScoreType::CanTeamPlay[m_sharedData.scoreType]))
        {
            LogI << FILE_LINE << " Implement correct drawing!" << std::endl;
            m_menuState.m_lobbyWindowEntities[LobbyEntityID::MinPlayerCount].getComponent<cro::Callback>().active = true;
            //m_audioEnts[AudioID::Nope].getComponent<cro::AudioEmitter>().play();
            //m_audioEnts[AudioID::Nope].getComponent<cro::AudioEmitter>().setPlayingOffset(cro::seconds(0.f));
        }
        else
        {
            //check all members ready
            bool ready = true;
            std::int32_t clientCount = 0;
            for (auto i = 0u; i < ConstVal::MaxClients; ++i)
            {
                if (m_sharedData.connectionData[i].playerCount != 0)
                {
                    clientCount++;
                    if (!m_menuState.m_readyState[i])
                    {
                        ready = false;
                        break;
                    }
                }
            }

            if (ready && m_sharedData.clientConnection.connected
                && m_sharedData.serverInstance.running()) //not running if we're not hosting :)
            {
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::RequestGameStart, std::uint8_t(sv::StateID::Golf), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                //m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        }
    }
    else
    {
        //toggle readyness but only if the selected course is locally available
        if (m_menuState.m_serverMapAvailable)
        {
            //this waits for the ready state to come back from the server
            //to set m_readyState to our request.
            const std::uint8_t ready = m_menuState.m_readyState[m_sharedData.clientConnection.connectionID] ? 0 : 1;
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::LobbyReady,
                std::uint16_t(m_sharedData.clientConnection.connectionID << 8 | ready),
                net::NetFlag::Reliable, ConstVal::NetChannelReliable);

            /*if (ready)
            {
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
            else
            {
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }*/
        }
        else
        {
            LogI << "Shared Data Map Directory Is Empty" << std::endl;

            //m_audioEnts[AudioID::Nope].getComponent<cro::AudioEmitter>().play();
        }
    }
}

void MenuState::LobbyMenu::resetRepeatTimer(std::int32_t i, cro::Time resetTime)
{
    m_inputRepeatClocks[i].restart();
    m_repeatTimes[i] = resetTime;
}

void MenuState::LobbyMenu::create(cro::Entity/* parent*/)
{
    if (m_progressShader.loadFromString(cro::RenderSystem2D::getDefaultVertexShader(), ProgressFrag))
    {
        m_progressUniform = m_progressShader.getUniformID("u_progress");
        m_progressColourUniform = m_progressShader.getUniformID("u_colour");
    }

    m_uiLayout.loadAssets(*m_sharedData.sharedResources);
    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    const auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);

    m_infoText.setFont(smallFont);
    m_infoText.setCharacterSize(InfoTextSize);
    m_infoText.setFillColour(TextNormalColour);
    m_infoText.setShadowColour(LeaderboardTextDark);
    m_infoText.setShadowOffset({ 1.f, -1.f });

    m_uiText.setFont(largeFont);
    m_uiText.setCharacterSize(UITextSize);
    m_uiText.setFillColour(TextNormalColour);
    m_uiText.setShadowColour(LeaderboardTextDark);
    m_uiText.setShadowOffset({ 1.f, -1.f });

    m_infoArray.setPrimitiveType(GL_TRIANGLES);

    auto rootNode = m_menuState.m_uiScene.createEntity();
    rootNode.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    rootNode.addComponent<cro::Callback>().setUserData<MenuData>();
    rootNode.getComponent<cro::Callback>().function = MenuCallback(MainMenuContext(&m_menuState));
    rootNode.addComponent<cro::UIElement>(cro::UIElement::Position, true).relativePosition = { 0.5f, 0.5f };
    m_menuState.m_menuEntities[MenuID::LobbyV2] = rootNode;
    
    //don't add to the parent else we get scaled up to the view scale on window resize...
    //parent.getComponent<cro::Transform>().addChild(rootNode.getComponent<cro::Transform>());

    //tab bar
    m_uiLayout.tabBar.background = m_menuState.m_uiScene.createEntity();
    m_uiLayout.tabBar.background.addComponent<cro::Transform>();
    m_uiLayout.tabBar.background.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    m_uiLayout.tabBar.background.getComponent<cro::Drawable2D>().setTexture(m_uiLayout.uiTexture);
    m_uiLayout.tabBar.background.addComponent<cro::UIElement>(cro::UIElement::Position, true);
    m_uiLayout.tabBar.background.getComponent<cro::UIElement>().relativePosition = { -0.5f, 0.5f };
    m_uiLayout.tabBar.background.getComponent<cro::UIElement>().absolutePosition = { 0.f, -(TabBarHeight * 2.f) };
    rootNode.getComponent<cro::Transform>().addChild(m_uiLayout.tabBar.background.getComponent<cro::Transform>());

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
    m_uiLayout.detailsPane.text.getComponent<cro::UIElement>().absolutePosition = { DetailBackgroundOffset, -104.f }; //90
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
    m_uiLayout.detailsPane.image.getComponent<cro::UIElement>().depth = 0.1f;
    m_uiLayout.detailsPane.root.getComponent<cro::Transform>().addChild(m_uiLayout.detailsPane.image.getComponent<cro::Transform>());

    //background/9 patch
    m_uiLayout.detailsPane.background = m_menuState.m_uiScene.createEntity();
    m_uiLayout.detailsPane.background.addComponent<cro::Transform>().setOrigin({ 0.f, InfoBarHeight / 2.f });
    m_uiLayout.detailsPane.background.addComponent<cro::Drawable2D>().setTexture(m_uiLayout.uiTexture);
    m_uiLayout.detailsPane.background.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    m_uiLayout.detailsPane.background.addComponent<cro::UIElement>(cro::UIElement::Sprite, true);
    m_uiLayout.detailsPane.background.getComponent<cro::UIElement>().absolutePosition = { DetailBackgroundOffset, 8.f };
    m_uiLayout.detailsPane.background.getComponent<cro::UIElement>().resizeCallback =
        [this](cro::Entity e)
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
        [this](cro::Entity e, float dt)
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
        [this](cro::Entity e, float)
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
        [this](cro::Entity e, float)
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
        [this](cro::Entity e, float)
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
        [this](cro::Entity e, float)
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
    static constexpr glm::vec2 InfoPos = glm::vec2(26.f, 21.f);
    entity = m_menuState.m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    entity.addComponent<cro::Text>(largeFont).setString(KeyInfo);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIElement>(cro::UIElement::Text, true).characterSize = UITextSize;
    entity.getComponent<cro::UIElement>().depth = 0.1f;
    entity.getComponent<cro::UIElement>().absolutePosition = InfoPos;
    entity.getComponent<cro::UIElement>().resizeCallback =
        [this](cro::Entity e)
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
    entity.getComponent<cro::UIElement>().absolutePosition = InfoPos;
    entity.getComponent<cro::UIElement>().resizeCallback =
        [this](cro::Entity e)
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




    //progress for hold-to-quit
    entity = m_menuState.m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setShader(&m_progressShader);
    //entity.getComponent<cro::Drawable2D>().setTexture(m_uiLayout.uiTexture);
    //hmm theres a bug here preventing the coords being forwarded to the
    //shader so we'll fudge coords in the colour channel
    entity.getComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, 16.f), cro::Colour(0.f, 1.f, 1.f, 1.f)),
            cro::Vertex2D(glm::vec2(0.f), cro::Colour(0.f, 0.f, 1.f, 1.f)),
            cro::Vertex2D(glm::vec2(16.f), cro::Colour(1.f, 1.f, 1.f, 1.f)),
            cro::Vertex2D(glm::vec2(16.f, 0.f), cro::Colour(1.f, 0.f, 1.f, 1.f)),
        });
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, true);
    entity.getComponent<cro::UIElement>().depth = 0.1f;
    entity.getComponent<cro::UIElement>().absolutePosition = { 2.f, 12.f };
    entity.getComponent<cro::UIElement>().resizeCallback =
        [this](cro::Entity e)
        {
            auto o = (glm::vec2(cro::App::getWindow().getSize()) / 2.f) / cro::UIElementSystem::getViewScale();
            o.x = std::round(o.x);
            o.y = std::round(o.y);
            e.getComponent<cro::Transform>().setOrigin(o);
        };
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //m_menuState.registerWindow([this]()
    //    {
    //        ImGui::Begin("sdfg");
    //        ImGui::Image(m_detailTextures[TabID::Scores].getTexture(), { 100.f, 100.f }, { 0.f, 1.f }, { 1.f, 0.f });
    //        ImGui::End();        
    //    });
}

void MenuState::LobbyMenu::createPlayerTab()
{
    auto* item = &m_uiLayout.menuLayout.items[TabID::Players].emplace_back();
    item->title = "Player Menu";
    item->displayType = Menu::Item::Heading;
    //item->description = "Customise in-game display settings";


    //ready-up / start game
    item = &m_uiLayout.menuLayout.items[TabID::Players].emplace_back();
    item->title = m_sharedData.hosting ?  "Start Game" : "Ready Up";
    item->description = m_sharedData.hosting ? "Press and Hold to Start" : "Press and Hold to Ready Up";
    item->selected =
        [this](const Menu::Item&)
        {

        };
    item->activated = [this](Menu::Item& i)
        {
            //press / hold to start or ready up
            m_buttonFlags |= ButtonFlags::Action;
            setProgressColour(CD32::Colours[CD32::GreenLight]);

            //set a callback to be activaed when timer expires
            m_timeoutCallback = std::bind(&LobbyMenu::readyStart, this);
        };
    item->labels = { "Let\'s Go!" };
    item->selectedIndex = 0;



    //select player
    item = &m_uiLayout.menuLayout.items[TabID::Players].emplace_back();
    item->title = "Select Player";
    item->description = "Add me";
    item->activated = [this](Menu::Item& i)
        {

        };
    item->labels = { "1", "2" };
    item->selectedIndex = 0;


    //teams mode
    item = &m_uiLayout.menuLayout.items[TabID::Players].emplace_back();
    item->title = "Teams";
    item->description = "Add me";
    item->activated = [this](Menu::Item& i)
        {

        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = 0;


    //move selected
    item = &m_uiLayout.menuLayout.items[TabID::Players].emplace_back();
    item->title = "Move Selected Player";
    item->description = "Add me";
    item->activated = [this](Menu::Item& i)
        {

        };
    item->labels = { "1", "2" };
    item->selectedIndex = 0;

    //we need to do this in a refresh after creating the lobby
    //as when the menu is first built the game is only just launched
    if (/*m_sharedData.hosting*/true)
    {
        //poke selected
        item = &m_uiLayout.menuLayout.items[TabID::Players].emplace_back();
        item->title = "Poke Player";
        item->description = "Add me";
        item->activated = [this](Menu::Item& i)
            {

            };
        item->labels = { "1" };
        item->selectedIndex = 0;


        //kick selected - TODO press/hold
        item = &m_uiLayout.menuLayout.items[TabID::Players].emplace_back();
        item->title = "Kick Player";
        item->description = "Add me";
        item->activated = [this](Menu::Item& i)
            {

            };
        item->labels = { "1" };
        item->selectedIndex = 0;
    }


    m_uiLayout.tabBar.items[TabID::Players].selected =
        [this]()
        {
            for (auto e : m_detailEntities)
            {
                e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            }
            applyDetails(TabID::Players);
        };


    auto entity = m_menuState.m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    entity.addComponent<cro::Sprite>();
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, false);
    entity.getComponent<cro::UIElement>().absolutePosition = { 0.f, m_uiLayout.detailsPane.text.getComponent<cro::UIElement>().absolutePosition.y + 8.f };
    entity.getComponent<cro::UIElement>().depth = 0.1f;
    m_detailEntities[TabID::Players] = entity;
    m_uiLayout.detailsPane.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    updatePlayersTab();
}

void MenuState::LobbyMenu::createCourseTab()
{
    auto* item = &m_uiLayout.menuLayout.items[TabID::Course].emplace_back();
    item->title = "Course Selection";
    item->displayType = Menu::Item::Heading;

    
    //course selection
    item = &m_uiLayout.menuLayout.items[TabID::Course].emplace_back();
    item->title = "Select Course";
    //item->description = "Draws a beacon at the pin position, visible from a distance";
    item->selected =
        [this](const Menu::Item&)
        {

        };
    item->activated = [this](Menu::Item& i)
        {

        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = 0;


    //hole count
    item = &m_uiLayout.menuLayout.items[TabID::Course].emplace_back();
    item->title = "Hole Count";
    item->description = "Add me";
    item->activated = [this](Menu::Item& i)
        {

        };
    item->labels = { "All 18", "Front 9", " Back 9" };
    item->selectedIndex = 0;


    //reverse course
    item = &m_uiLayout.menuLayout.items[TabID::Course].emplace_back();
    item->title = "Play in Reverse";
    item->description = "Add me";
    item->activated = [this](Menu::Item& i)
        {

        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = 0;


    //user courses
    item = &m_uiLayout.menuLayout.items[TabID::Course].emplace_back();
    item->title = "User Courses";
    item->description = "Add me";
    item->activated = [this](Menu::Item& i)
        {

        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = 0;


    //night mode
    item = &m_uiLayout.menuLayout.items[TabID::Course].emplace_back();
    item->title = "Night";
    item->description = "Add me";
    item->activated = [this](Menu::Item& i)
        {

        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = 0;


    //weather
    item = &m_uiLayout.menuLayout.items[TabID::Course].emplace_back();
    item->title = "Weather";
    item->description = "Add me";
    item->activated = [this](Menu::Item& i)
        {

        };
    item->labels = { "Clear", "Rain", "Showers", "Mist", "Random"};
    item->selectedIndex = 0;


    //random wind
    item = &m_uiLayout.menuLayout.items[TabID::Course].emplace_back();
    item->title = "Randomise Wind";
    item->description = "Add me";
    item->activated = [this](Menu::Item& i)
        {

        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = 0;


    //wind strength
    item = &m_uiLayout.menuLayout.items[TabID::Course].emplace_back();
    item->title = "Enable Snek";
    item->description = "Add me";
    item->activated = [this](Menu::Item& i)
        {

        };
    item->labels = { "Normal", "Medium", "High"};
    item->selectedIndex = 0;

    m_uiLayout.tabBar.items[TabID::Course].selected =
        [this]()
        {
            for (auto e : m_detailEntities)
            {
                e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            }
            applyDetails(TabID::Course);
        };

    auto entity = m_menuState.m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    entity.addComponent<cro::Sprite>();
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, false);
    entity.getComponent<cro::UIElement>().absolutePosition = { 0.f, m_uiLayout.detailsPane.text.getComponent<cro::UIElement>().absolutePosition.y + 8.f };
    entity.getComponent<cro::UIElement>().depth = 0.1f;
    m_detailEntities[TabID::Course] = entity;
    m_uiLayout.detailsPane.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    updateCourseTab();
}

void MenuState::LobbyMenu::createRulesTab()
{
    auto* item = &m_uiLayout.menuLayout.items[TabID::Rules].emplace_back();
    item->title = "Game Rules";
    item->displayType = Menu::Item::Heading;
    //item->description = "Customise in-game display settings";

    //choose rules / game mode
    item = &m_uiLayout.menuLayout.items[TabID::Rules].emplace_back();
    item->title = "Scoring";
    //item->description = "Draws a beacon at the pin position, visible from a distance";
    item->selected =
        [this](const Menu::Item&)
        {

        };
    item->activated = [this](Menu::Item& i)
        {

        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = 0;



    //set gimme radius
    item = &m_uiLayout.menuLayout.items[TabID::Rules].emplace_back();
    item->title = "Gimme Radius";
    item->description = "For brevity of play the ball is automatically holed when it is less than this distance from the pin.";
    cro::Util::String::wordWrap(item->description, WordWrapSmall);
    item->activated = [this](Menu::Item& i)
        {
            m_sharedData.gimmeRadius = i.selectedIndex;
        };
    item->labels = { "None", "Under the Leather", "Under the Putter" };
    item->selectedIndex = m_sharedData.gimmeRadius;


    //choose club set
    item = &m_uiLayout.menuLayout.items[TabID::Rules].emplace_back();
    item->title = "Clubs";
    item->description = "Choose a clubset with which to play";
    cro::Util::String::wordWrap(item->description, WordWrapSmall);
    item->activated = [this](Menu::Item& i)
        {
            m_sharedData.clubSet = m_sharedData.preferredClubSet = i.selectedIndex;
        };
    item->labels = { "Casual", "Regular", "Pro" };
    item->selectedIndex = m_sharedData.clubSet;

    //TODO this needs to be refreshed after the menu is launched
    if (/*m_sharedData.hosting*/true)
    {
        //enable snek
        item = &m_uiLayout.menuLayout.items[TabID::Rules].emplace_back();
        item->title = "Enable Snek";
        item->description = "The player who last misses a putt is left holding the snek";
        cro::Util::String::wordWrap(item->description, WordWrapSmall);
        item->activated = [this](Menu::Item& i)
            {
                if (m_sharedData.hosting
                    && m_sharedData.clientConnection.connected)
                {
                    const std::uint16_t d = (std::uint8_t(RuleMod::Snek) << 8) | std::uint8_t(i.selectedIndex);
                    m_sharedData.clientConnection.netClient.sendPacket(PacketID::RuleMod, d, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

                    cro::Console::print("snek enabled");
                }
            };
        item->labels = { "No", "Yes" };
        item->selectedIndex = 0;


        //enable big balls
        item = &m_uiLayout.menuLayout.items[TabID::Rules].emplace_back();
        item->title = "Enable Big Balls";
        item->description = "Player balls grow larger the further in the lead a player becomes";
        cro::Util::String::wordWrap(item->description, WordWrapSmall);
        item->activated = [this](Menu::Item& i)
            {
                if (m_sharedData.hosting
                    && m_sharedData.clientConnection.connected)
                {
                    const std::uint16_t d = (std::uint8_t(RuleMod::BigBalls) << 8) | std::uint8_t(i.selectedIndex);
                    m_sharedData.clientConnection.netClient.sendPacket(PacketID::RuleMod, d, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

                    cro::Console::print("Big Balls enabled");
                }
            };
        item->labels = { "No", "Yes" };
        item->selectedIndex = 0;
    }

    m_uiLayout.tabBar.items[TabID::Rules].selected =
        [this]()
        {
            for (auto e : m_detailEntities)
            {
                e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            }
            applyDetails(TabID::Rules);
        };

    auto entity = m_menuState.m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    entity.addComponent<cro::Sprite>();
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, false);
    entity.getComponent<cro::UIElement>().absolutePosition = { 0.f, m_uiLayout.detailsPane.text.getComponent<cro::UIElement>().absolutePosition.y + 8.f };
    entity.getComponent<cro::UIElement>().depth = 0.1f;
    m_detailEntities[TabID::Rules] = entity;
    m_uiLayout.detailsPane.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    updateRulesTab();
}

void MenuState::LobbyMenu::createScoresTab()
{
    //leaderboards
    auto* item = &m_uiLayout.menuLayout.items[TabID::Scores].emplace_back();
    item->title = "View Scores";
    item->displayType = Menu::Item::Heading;

    cro::String desc = "Browse the online leaderboards. Friends only filters can be enabled in the Options menu.";
#ifdef USE_GNS
    cro::Util::String::wordWrap(desc, WordWrapSmall);
    item = &m_uiLayout.menuLayout.items[TabID::Scores].emplace_back();
    item->title = "View Leaderboards";
    item->description = desc;
    item->activated = [this](Menu::Item& i)
        {
            m_menuState.requestStackPush(StateID::Leaderboard);
        };
    item->labels = { "OK" };
    item->selectedIndex = 0;
#endif



    //view leagues
    desc = "Browse the current League standings.";
    cro::Util::String::wordWrap(desc, WordWrapSmall);

    item = &m_uiLayout.menuLayout.items[TabID::Scores].emplace_back();
    item->title = "View Leagues";
    item->description = desc;
    item->activated = [this](Menu::Item& i)
        {
#ifdef USE_GNS
            //hmm I had this set to 7 for some reason - I think just trying to default
            //to the global league. This is why we need enums.
            m_sharedData.leagueTable = 9;//7
#else
            m_sharedData.leagueTable = 0;
#endif
            m_menuState.requestStackPush(StateID::League);
        };
    item->labels = { "OK" };
    item->selectedIndex = 0;


    //view previous rounds scores - this entity won't exist if the
    //lobby wasn't opened from a previous round.
    if (m_menuState.m_lobbyWindowEntities[LobbyEntityID::Scorecard].isValid())
    {
        item = &m_uiLayout.menuLayout.items[TabID::Scores].emplace_back();
        item->title = "View Last Round's Scores";
        item->selected =
        [this](const Menu::Item&)
        {

        };
        item->activated = [this](Menu::Item& i)
            {
                m_menuState.togglePreviousScoreCard();
            };
        item->labels = { "OK" };
        item->selectedIndex = 0;
    }

    //tab selection callback
    m_uiLayout.tabBar.items[TabID::Scores].selected = 
        [this]()
        {
            for (auto e : m_detailEntities)
            {
                e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            }
            applyDetails(TabID::Scores);
        };


    //create a specific entity to display the scores tab background
    auto entity = m_menuState.m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    entity.addComponent<cro::Sprite>();
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, false);
    entity.getComponent<cro::UIElement>().absolutePosition = { 0.f, m_uiLayout.detailsPane.text.getComponent<cro::UIElement>().absolutePosition.y + 8.f };
    entity.getComponent<cro::UIElement>().depth = 0.1f;
    m_detailEntities[TabID::Scores] = entity;
    m_uiLayout.detailsPane.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    updateScoresTab();
}

void MenuState::LobbyMenu::updatePlayersTab(bool resized)
{
    if (!m_detailTextures[TabID::Players].available()
        || resized)
    {
        const auto size = glm::uvec2(m_uiLayout.detailsPane.backgroundSize);
        if (size.x != 0 && size.y != 0)
        {
            static constexpr std::uint32_t BorderSize = 4;
            const std::uint32_t Offset = static_cast<std::uint32_t>(std::abs(m_detailEntities[TabID::Players].getComponent<cro::UIElement>().absolutePosition.y));

            m_detailTextures[TabID::Players].create(size.x - (BorderSize * 2), ((size.y / 2) - BorderSize) + Offset, false);
            m_detailEntities[TabID::Players].getComponent<cro::Sprite>().setTexture(m_detailTextures[TabID::Players].getTexture());
        }
    }

    m_detailTextures[TabID::Players].clear(cro::Colour::Yellow);
    m_detailTextures[TabID::Players].display();


    if (m_uiLayout.tabBar.activeIndex == TabID::Players)
    {
        //set this as the active background image
        applyDetails(TabID::Players);
    }
}

void MenuState::LobbyMenu::updateCourseTab(bool resized)
{
    if (!m_detailTextures[TabID::Course].available()
        || resized)
    {
        const auto size = glm::uvec2(m_uiLayout.detailsPane.backgroundSize);
        if (size.x != 0 && size.y != 0)
        {
            static constexpr std::uint32_t BorderSize = 4;
            const std::uint32_t Offset = static_cast<std::uint32_t>(std::abs(m_detailEntities[TabID::Course].getComponent<cro::UIElement>().absolutePosition.y));

            m_detailTextures[TabID::Course].create(size.x - (BorderSize * 2), ((size.y / 2) - BorderSize) + Offset, false);
            m_detailEntities[TabID::Course].getComponent<cro::Sprite>().setTexture(m_detailTextures[TabID::Course].getTexture());
        }
    }

    m_detailTextures[TabID::Course].clear(cro::Colour::Magenta);
    m_detailTextures[TabID::Course].display();


    if (m_uiLayout.tabBar.activeIndex == TabID::Course)
    {
        //set this as the active background image
        applyDetails(TabID::Course);
    }
}

void MenuState::LobbyMenu::updateRulesTab(bool resized)
{
    if (!m_detailTextures[TabID::Rules].available()
        || resized)
    {
        const auto size = glm::uvec2(m_uiLayout.detailsPane.backgroundSize);
        if (size.x != 0 && size.y != 0)
        {
            static constexpr std::uint32_t BorderSize = 4;
            const std::uint32_t Offset = static_cast<std::uint32_t>(std::abs(m_detailEntities[TabID::Rules].getComponent<cro::UIElement>().absolutePosition.y));

            m_detailTextures[TabID::Rules].create(size.x - (BorderSize * 2), ((size.y / 2) - BorderSize) + Offset, false);
            m_detailEntities[TabID::Rules].getComponent<cro::Sprite>().setTexture(m_detailTextures[TabID::Rules].getTexture());
        }
    }

    m_detailTextures[TabID::Rules].clear(cro::Colour::Cyan);
    m_detailTextures[TabID::Rules].display();


    if (m_uiLayout.tabBar.activeIndex == TabID::Rules)
    {
        //set this as the active background image
        applyDetails(TabID::Rules);
    }
}

void MenuState::LobbyMenu::updateScoresTab(bool resized)
{
    if (!m_detailTextures[TabID::Scores].available()
        || resized)
    {
        const auto size = glm::uvec2(m_uiLayout.detailsPane.backgroundSize);
        if (size.x != 0 && size.y != 0)
        {
            static constexpr std::uint32_t BorderSize = 4;
            const std::uint32_t Offset = static_cast<std::uint32_t>(std::abs(m_detailEntities[TabID::Scores].getComponent<cro::UIElement>().absolutePosition.y));

            m_detailTextures[TabID::Scores].create(size.x - (BorderSize * 2), ((size.y / 2) - BorderSize) + Offset, false);
            m_detailEntities[TabID::Scores].getComponent<cro::Sprite>().setTexture(m_detailTextures[TabID::Scores].getTexture());
        }
    }

    for (auto e : m_networkIcons)
    {
        m_menuState.m_uiScene.destroyEntity(e);
    }
    m_networkIcons.clear();



    //update the texture
    m_detailTextures[TabID::Scores].clear(CD32::Colours[CD32::GreyDark]);

    std::int32_t h = 0;
    std::int32_t clientCount = 0;
    const float TextureWidth = static_cast<float>(m_detailTextures[TabID::Scores].getSize().x);
    const float TextureHeight = static_cast<float>(m_detailTextures[TabID::Scores].getSize().y) - 22.f;
    static constexpr float RankSpacing = -14.f;

    m_uiText.setString("Connected Clients");
    m_uiText.setAlignment(cro::SimpleText::Alignment::Centre);
    m_uiText.setPosition({TextureWidth / 2.f, TextureHeight + 12.f});
    m_uiText.draw();

    for (const auto& c : m_sharedData.connectionData)
    {
        if (c.playerCount != 0)
        {
            //rank text - I've done this a weird-ass way by positioning this first then placing everything
            //else relative to it...
            std::string str = "Level " + std::to_string(c.level);
            str += "               " + std::to_string(c.playerCount) + " player(s)";
            m_infoText.setString(str);
            m_infoText.setPosition({ (TextureWidth / 2.f) - 56.f, (RankSpacing * clientCount) + TextureHeight });
            //m_infoText.draw(); //draw this last as it might need to render over the level bar

            //rank badge
            //this uses animation 0-5 based on level / 10
            const auto index = std::min(5, m_sharedData.connectionData[h].level / 10);
            m_infoQuad = m_menuState.m_sprites[SpriteID::LevelBadge];
            m_infoQuad.setTextureRect(m_menuState.m_sprites[SpriteID::LevelBadge].getAnimations()[index].frames[0].frame);
            m_infoQuad.setScale(glm::vec2(1.f));
            m_infoQuad.setPosition(m_infoText.getPosition() + glm::vec2(-18.f, -5.f));
            m_infoQuad.draw();

            //avatar icon
            const cro::FloatRect bounds = { 0.f, LabelTextureSize.y - (LabelIconSize.y * 4.f), LabelIconSize.x, LabelIconSize.y };
            m_infoQuad.setTexture(m_sharedData.nameTextures[h].getTexture());
            m_infoQuad.setTextureRect(bounds);
            m_infoQuad.setPosition(m_infoText.getPosition() + glm::vec2(-62.f, -4.f));
            m_infoQuad.setScale(glm::vec2(0.2f)); //hmm this mangles things even more when scaled up - but then 0.2 isn't a multiple of view scales anyway...
            m_infoQuad.draw();

            //network icon - actually an ent as it's dynamic
            auto entity = m_menuState.m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(glm::vec3(m_infoText.getPosition() - glm::vec2(46.f, 6.f), 0.1f));
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Sprite>() = m_menuState.m_sprites[SpriteID::NetStrength];
            entity.addComponent<cro::SpriteAnimation>();

            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [this, h](cro::Entity ent, float)
                {
                    ent.getComponent<cro::Drawable2D>().setFacing(m_detailEntities[TabID::Scores].getComponent<cro::Drawable2D>().getFacing());
                    if (m_sharedData.connectionData[h].playerCount == 0)
                    {
                        ent.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                    }
                    else
                    {
                        ent.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                        const auto index = std::min(4u, m_sharedData.connectionData[h].pingTime / 60);
                        ent.getComponent<cro::SpriteAnimation>().play(index);
                    }
                };
            m_detailEntities[TabID::Scores].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            m_networkIcons.push_back(entity);



            //if this is our local client then add the current xp level
            if (h == m_sharedData.clientConnection.connectionID)
            {
                //level progress
                constexpr float BarWidth = 80.f;
                constexpr float BarHeight = 10.f;

                m_infoArray.setPosition(m_infoText.getPosition()  + glm::vec2((BarWidth / 2.f) - 3.f, 3.f));

                constexpr auto CornerColour = cro::Colour(std::uint8_t(58), 57, 65); //grey
                //const auto CornerColour = cro::Colour(std::uint8_t(152), 122, 104); //beige

                const auto progress = Social::getLevelProgress();
                m_infoArray.setVertexData(
                    {
                        cro::Vertex2D(glm::vec2(-BarWidth / 2.f, BarHeight / 2.f), TextHighlightColour),
                        cro::Vertex2D(glm::vec2(-BarWidth / 2.f, -BarHeight / 2.f), TextHighlightColour),
                        cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + (BarWidth * progress.progress), BarHeight / 2.f), TextHighlightColour),

                        cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + (BarWidth * progress.progress), BarHeight / 2.f), TextHighlightColour),
                        cro::Vertex2D(glm::vec2(-BarWidth / 2.f, -BarHeight / 2.f), TextHighlightColour),
                        cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + (BarWidth * progress.progress), -BarHeight / 2.f), TextHighlightColour),

                        cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + (BarWidth * progress.progress), BarHeight / 2.f), LeaderboardTextDark),
                        cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + (BarWidth * progress.progress), -BarHeight / 2.f), LeaderboardTextDark),
                        cro::Vertex2D(glm::vec2(BarWidth / 2.f, BarHeight / 2.f), LeaderboardTextDark),

                        cro::Vertex2D(glm::vec2(BarWidth / 2.f, BarHeight / 2.f), LeaderboardTextDark),
                        cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + (BarWidth * progress.progress), -BarHeight / 2.f), LeaderboardTextDark),
                        cro::Vertex2D(glm::vec2(BarWidth / 2.f, -BarHeight / 2.f), LeaderboardTextDark),

                        //corners
                        cro::Vertex2D(glm::vec2(-BarWidth / 2.f, BarHeight / 2.f), CornerColour),
                        cro::Vertex2D(glm::vec2(-BarWidth / 2.f, (BarHeight / 2.f) - 1.f), CornerColour),
                        cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + 1.f, BarHeight / 2.f), CornerColour),

                        cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + 1.f, BarHeight / 2.f), CornerColour),
                        cro::Vertex2D(glm::vec2(-BarWidth / 2.f, (BarHeight / 2.f) - 1.f), CornerColour),
                        cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + 1.f, (BarHeight / 2.f) - 1.f), CornerColour),

                        cro::Vertex2D(glm::vec2(-BarWidth / 2.f, (-BarHeight / 2.f) + 1.f), CornerColour),
                        cro::Vertex2D(glm::vec2(-BarWidth / 2.f, -BarHeight / 2.f), CornerColour),
                        cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + 1.f, (-BarHeight / 2.f) + 1.f), CornerColour),

                        cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + 1.f, (-BarHeight / 2.f) + 1.f), CornerColour),
                        cro::Vertex2D(glm::vec2(-BarWidth / 2.f, -BarHeight / 2.f), CornerColour),
                        cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + 1.f, -BarHeight / 2.f), CornerColour),


                        cro::Vertex2D(glm::vec2((BarWidth / 2.f) - 1.f, BarHeight / 2.f), CornerColour),
                        cro::Vertex2D(glm::vec2((BarWidth / 2.f) - 1.f, (BarHeight / 2.f) - 1.f), CornerColour),
                        cro::Vertex2D(glm::vec2(BarWidth / 2.f, BarHeight / 2.f), CornerColour),

                        cro::Vertex2D(glm::vec2(BarWidth / 2.f, BarHeight / 2.f), CornerColour),
                        cro::Vertex2D(glm::vec2((BarWidth / 2.f) - 1.f, (BarHeight / 2.f) - 1.f), CornerColour),
                        cro::Vertex2D(glm::vec2(BarWidth / 2.f, (BarHeight / 2.f) - 1.f), CornerColour),

                        cro::Vertex2D(glm::vec2((BarWidth / 2.f) - 1.f, (-BarHeight / 2.f) + 1.f), CornerColour),
                        cro::Vertex2D(glm::vec2((BarWidth / 2.f) - 1.f, -BarHeight / 2.f), CornerColour),
                        cro::Vertex2D(glm::vec2(BarWidth / 2.f, (-BarHeight / 2.f) + 1.f), CornerColour),

                        cro::Vertex2D(glm::vec2(BarWidth / 2.f, (-BarHeight / 2.f) + 1.f), CornerColour),
                        cro::Vertex2D(glm::vec2((BarWidth / 2.f) - 1.f, -BarHeight / 2.f), CornerColour),
                        cro::Vertex2D(glm::vec2(BarWidth / 2.f, -BarHeight / 2.f), CornerColour),
                    });
                m_infoArray.draw();
            }

            m_infoText.draw();
            clientCount++;
        }
        /*else
        {
            m_infoText.setString("I am placeholder" + std::to_string(h));
            m_infoText.setPosition({ 64.f, (RankSpacing * h) + TextureHeight + 6.f });
            m_infoText.draw();
        }*/
        h++;
    }
    m_detailTextures[TabID::Scores].display();


    if (m_uiLayout.tabBar.activeIndex == TabID::Scores)
    {
        //set this as the active background image
        applyDetails(TabID::Scores);
    }
}

void MenuState::LobbyMenu::applyDetails(std::int32_t idx)
{
    CRO_ASSERT(idx < TabID::Count, "");

    const glm::vec2 size = m_detailTextures[idx].getSize();
    m_detailEntities[idx].getComponent<cro::Transform>().setOrigin({ std::round(size.x / 2.f), 0.f});
    m_detailEntities[idx].getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
}

void MenuState::LobbyMenu::setProgressColour(cro::Colour c)
{
    glUseProgram(m_progressShader.getGLHandle());
    glUniform4f(m_progressColourUniform, c.getRed(), c.getGreen(), c.getBlue(), c.getAlpha());
}

void MenuState::LobbyMenu::resized(std::uint32_t x, std::uint32_t y)
{
    if (const auto newSize = glm::uvec2(x, y);
        newSize != lastWindowSize)
    {
        //hack to force the texture to resize properly
        m_uiLayout.menuLayout.texture.create(1, 1, false);
        m_uiLayout.updateTabBar();


        //realigns the current menu to the new screen size
        cro::Entity entity = m_menuState.m_uiScene.createEntity();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [this](cro::Entity e, float)
            {
                m_uiLayout.activateTab(m_uiLayout.tabBar.activeIndex);

                updatePlayersTab(true);
                updateCourseTab(true);
                updateRulesTab(true);
                updateScoresTab(true);

                e.getComponent<cro::Callback>().active = false;
                m_menuState.m_uiScene.destroyEntity(e);
            };

        lastWindowSize = newSize;
    }
}