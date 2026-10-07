/*-----------------------------------------------------------------------

Matt Marchant 2026
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
#include <crogine/gui/Codepoints.inl>

    const std::array ItemLabels =
    {
        "Players", "Course", "Rules", "Scores"
    };

    static const cro::String KeyInfo = u8"↓ LAlt- Options   F4 - Open Chat   ↓ ESC- Close";

    static constexpr cro::Time RepeatTimeLong = cro::seconds(0.5f);
    static constexpr cro::Time RepeatTimeShort = cro::seconds(0.05f);

    glm::uvec2 lastWindowSize = { 0u,0u };

    static const std::array WindStrings =
    {
        cro::String("Normal"), cro::String("Medium"), cro::String("High")
    };

    struct TickerData final
    {
        float width = 0.f;
        float offset = 8.f;
        float basePos = 0.f;
        float currentPos = 0.f; //this is actual position so we can round it to the nearest pixel
    };
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
            cro::App::getWindow().setCursorVisible(mouse);
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
    m_uiLayout.scrollToTarget(dt);

    //fast scroll when press / hold up or down
    const auto maskTest =
        [&](std::int32_t index, std::int32_t flag)
        {
            return ((m_controllerMasks[index] & flag) != 0) && ((m_controllerPrevMasks[index] & flag) == 0);
        };

    for (auto i = 0; i < cro::GameController::getControllerCount(); ++i)
    {
        //check stick input
        if (maskTest(i, InputFlag::Left))
        {
            m_uiLayout.activateLeft();
        }

        if (maskTest(i, InputFlag::Right))
        {
            m_uiLayout.activateRight();
        }

        if (maskTest(i, InputFlag::Up))
        {
            m_uiLayout.prevItem();
            resetRepeatTimer(i, RepeatTimeLong);
        }

        if (maskTest(i, InputFlag::Down))
        {
            m_uiLayout.nextItem();
            resetRepeatTimer(i, RepeatTimeLong);
        }

        m_controllerPrevMasks[i] = m_controllerMasks[i];

        //check for repeat inputs
        if (cro::GameController::isButtonPressed(i, cro::GameController::DPadDown)
            || (m_controllerMasks[i] & InputFlag::Down))
        {
            if (m_inputRepeatClocks[i].elapsed() > m_repeatTimes[i])
            {
                m_uiLayout.nextItem();
                resetRepeatTimer(i, RepeatTimeShort);
            }
        }

        if (cro::GameController::isButtonPressed(i, cro::GameController::DPadUp)
            || (m_controllerMasks[i] & InputFlag::Up))
        {
            if (m_inputRepeatClocks[i].elapsed() > m_repeatTimes[i])
            {
                m_uiLayout.prevItem();
                resetRepeatTimer(i, RepeatTimeShort);
            }
        }

        if (cro::GameController::isButtonPressed(i, cro::GameController::DPadLeft)
            /*|| (m_controllerMasks[i] & InputFlag::Left)*/)
        {
            if (m_inputRepeatClocks[i].elapsed() > m_repeatTimes[i])
            {
                m_uiLayout.activateLeft();
                resetRepeatTimer(i, RepeatTimeShort);
            }
        }

        if (cro::GameController::isButtonPressed(i, cro::GameController::DPadRight)
            /*|| (m_controllerMasks[i] & InputFlag::Right)*/)
        {
            if (m_inputRepeatClocks[i].elapsed() > m_repeatTimes[i])
            {
                m_uiLayout.activateRight();
                resetRepeatTimer(i, RepeatTimeShort);
            }
        }
    }



    //press/hold to exit or show options
    static constexpr float MaxHoldTime = 0.5f;
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
                playSound(MenuSoundEvent::Cancel);
                break;
            case ButtonFlags::Options:
                //unready this client so the host can't
                //launch the game while we're still looking
                //at the options window
                unready();
                m_menuState.requestStackPush(StateID::Options);
                break;
            case ButtonFlags::Action:
                if (m_timeoutCallback)
                {
                    m_timeoutCallback();
                    playSound(MenuSoundEvent::Activate);
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

void MenuState::LobbyMenu::refreshTabs()
{
    //this completely rebuilds tab
    //items based on hosting status
    createPlayerTab();
    createCourseTab();
    createRulesTab();
    createScoresTab();

    m_uiLayout.activateTab(m_uiLayout.tabBar.activeIndex);
}

void MenuState::LobbyMenu::clientStatusChanged()
{
    //whereas this just refreshes
    //the detail textures
    updatePlayersTab();
    updateCourseTab();
    updateRulesTab();
    updateScoresTab();

    if (m_uiLayout.tabBar.items[m_uiLayout.tabBar.activeIndex].selected)
    {
        //updates the visibility of items
        m_uiLayout.tabBar.items[m_uiLayout.tabBar.activeIndex].selected();
    }
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
            //m_menuState.m_lobbyWindowEntities[LobbyEntityID::MinPlayerCount].getComponent<cro::Callback>().active = true;
            m_menuState.m_audioEnts[AudioID::Nope].getComponent<cro::AudioEmitter>().play();
            m_menuState.m_audioEnts[AudioID::Nope].getComponent<cro::AudioEmitter>().setPlayingOffset(cro::seconds(0.f));
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
                m_sharedData.errorMessage = "start_game";
                m_menuState.requestStackPush(StateID::MessageOverlay);
                //moved to message handler for DialogueResultEvent
                //m_sharedData.clientConnection.netClient.sendPacket(PacketID::RequestGameStart, std::uint8_t(sv::StateID::Golf),
                //                                                    net::NetFlag::Reliable, ConstVal::NetChannelReliable);
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
        }
        else
        {
            LogI << "Shared Data Map Directory Is Empty" << std::endl;
        }
    }
}

void MenuState::LobbyMenu::pokePlayer()
{
    const auto client = m_menuState.m_displayOrder[m_menuState.m_selectedDisplayMember].client;
    const std::uint16_t data = std::uint16_t(ServerCommand::PokeClient) | ((client) << 8);
    m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, data, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
}

void MenuState::LobbyMenu::kickPlayer()
{
    const auto client = m_menuState.m_displayOrder[m_menuState.m_selectedDisplayMember].client;
    const std::uint16_t data = std::uint16_t(ServerCommand::KickClient) | ((client) << 8);
    m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, data, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
}

void MenuState::LobbyMenu::unready()
{
    //shortcut for unreadying if we're
    //opening the options or steam overlay
    if (!m_sharedData.hosting)
    {
        m_menuState.m_readyState[m_sharedData.clientConnection.connectionID] = 0;
        m_sharedData.clientConnection.netClient.sendPacket(PacketID::LobbyReady,
            std::uint16_t(m_sharedData.clientConnection.connectionID << 8 | 0),
            net::NetFlag::Reliable, ConstVal::NetChannelReliable);
    }
}

void MenuState::LobbyMenu::onShown()
{
#ifdef USE_GNS
    if (!m_sharedData.hosting)
    {
        auto str = m_introTicker.getComponent<cro::Text>().getString();
        str += " - Can't ready up? Try opening then closing the Steam Overlay.";
        m_introTicker.getComponent<cro::Text>().setString(str);
    }
#else
    cro::String str;
    if (m_sharedData.hosting)
    {
        str = "Hosting on: " + m_sharedData.clientConnection.netClient.getPeer().getAddress() + ":"
        + std::to_string(ConstVal::GamePort);
    }
    else
    {
        str = "Connected to: " + m_sharedData.targetIP + ":" + std::to_string(ConstVal::GamePort);
    }
    m_ipText.getComponent<cro::Text>().setString(str);
#endif

    refreshTabs(); //rebuilds the menu based on hosting state etc
    clientStatusChanged(); //refreshes the detail panes
}

void MenuState::LobbyMenu::resetRepeatTimer(std::int32_t i, cro::Time resetTime)
{
    m_inputRepeatClocks[i].restart();
    m_repeatTimes[i] = resetTime;
}

void MenuState::LobbyMenu::create(cro::Entity/* parent*/)
{
    //load assets first - should be own func?
    if (m_progressShader.loadFromString(cro::RenderSystem2D::getDefaultVertexShader(), ProgressFrag))
    {
        m_progressUniform = m_progressShader.getUniformID("u_progress");
        m_progressColourUniform = m_progressShader.getUniformID("u_colour");
    }

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/options_buttons.spt", m_menuState.m_sharedData.sharedResources->textures);
    m_detailSprites[DetailSprite::CourseThumb] = spriteSheet.getSprite("course_thumb");
    m_detailSprites[DetailSprite::WeatherIcon] = spriteSheet.getSprite("weather_icon");
    m_detailSprites[DetailSprite::TickerLeft] = spriteSheet.getSprite("ticker_left");
    m_detailSprites[DetailSprite::TickerCentre] = spriteSheet.getSprite("ticker_middle");
    m_detailSprites[DetailSprite::TickerRight] = spriteSheet.getSprite("ticker_right");
    m_detailSprites[DetailSprite::GameRules] = spriteSheet.getSprite("game_rules");



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
    m_detailArray.setPrimitiveType(GL_TRIANGLES);

    auto rootNode = m_menuState.m_uiScene.createEntity();
    rootNode.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    rootNode.addComponent<cro::Callback>().setUserData<MenuData>();
    rootNode.getComponent<cro::Callback>().function = MenuCallback(MainMenuContext(&m_menuState));
    rootNode.addComponent<cro::UIElement>(cro::UIElement::Position, true).relativePosition = { 0.5f, 0.5f };
    m_menuState.m_menuEntities[MenuID::LobbyV2] = rootNode;
    
    constexpr auto c = cro::Colour(0.f, 0.f, 0.f, BackgroundAlpha);
    auto bgNode = m_menuState.m_uiScene.createEntity();
    bgNode.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.5f });
    bgNode.addComponent<cro::Drawable2D>().setVertexData({
        cro::Vertex2D(glm::vec2(-0.5f, 0.5f), c),
        cro::Vertex2D(glm::vec2(-0.5f), c),
        cro::Vertex2D(glm::vec2(0.5f), c),
        cro::Vertex2D(glm::vec2(0.5f, -0.5f), c),
        });
    bgNode.addComponent<cro::Callback>().active = true;
    bgNode.getComponent<cro::Callback>().function =
        [](cro::Entity e, float)
        {
            e.getComponent<cro::Transform>().setScale(glm::vec2(cro::App::getWindow().getSize()));
        };
    rootNode.getComponent<cro::Transform>().addChild(bgNode.getComponent<cro::Transform>());


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
    entity.getComponent<cro::Text>().setString("Lobby");
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


    //detail entities contain the images
    //on the right hand pane. TODO we could
    //probably use a single texture and
    //update it based on its current page
    //rather than duplicating these...
    m_uiLayout.tabBar.items[TabID::Players].selected =
        [this]()
        {
            for (auto e : m_detailEntities)
            {
                e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            }

            for (auto e : m_courseDetailEntities)
            {
                //we can't modify the scale because they are customised
                //to the window size (ie not 1:1)
                e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            }

            for (auto e : m_playerDetailIcons)
            {
                e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
            }
            
            m_introTicker.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
            applyDetails(TabID::Players);
        };

    entity = m_menuState.m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    entity.addComponent<cro::Sprite>();
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, false);
    entity.getComponent<cro::UIElement>().absolutePosition = { 0.f, m_uiLayout.detailsPane.text.getComponent<cro::UIElement>().absolutePosition.y + 8.f };
    entity.getComponent<cro::UIElement>().depth = 0.1f;
    m_detailEntities[TabID::Players] = entity;
    m_uiLayout.detailsPane.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    
    
    const auto createTicker = [this](cro::Entity e)
        {
            e.addComponent<cro::Text>(m_sharedData.sharedResources->fonts.get(FontID::Label)).setCharacterSize(LabelTextSize);
            e.getComponent<cro::Text>().setFillColour(TextNormalColour);
            e.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
            e.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
            e.addComponent<cro::Callback>().active = true;
            e.getComponent<cro::Callback>().setUserData<TickerData>();
            e.getComponent<cro::Callback>().function =
                [this](cro::Entity e, float dt)
                {
                    if (e.getComponent<cro::Drawable2D>().getFacing() == cro::Drawable2D::Facing::Front)
                    {
                        static constexpr float LineHeight = 13.f;

                        const auto scrollBounds = cro::Text::getLocalBounds(e);
                        auto& [bgWidth, Offset, BasePosY, xPos] = e.getComponent<cro::Callback>().getUserData<TickerData>();
                        xPos -= 20.f * m_menuState.m_scrollSpeed * dt;

                        auto pos = e.getComponent<cro::Transform>().getPosition();
                        pos.x = /*std::round*/(xPos);
                        pos.y = BasePosY + std::floor(scrollBounds.height - LineHeight + scrollBounds.bottom);
                        pos.z = 0.3f;


                        if (xPos < -scrollBounds.width + Offset)
                        {
                            xPos = bgWidth;
                        }

                        e.getComponent<cro::Transform>().setPosition(pos);

                        const cro::FloatRect cropping = { -pos.x + Offset, -26.f + (BasePosY - pos.y), (bgWidth - (Offset * 2.f)), 18.f };
                        e.getComponent<cro::Drawable2D>().setCroppingArea(cropping);
                    }
                };

        };

    m_introTicker = m_menuState.m_uiScene.createEntity();
    m_introTicker.addComponent<cro::Transform>();
    m_introTicker.addComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    m_detailEntities[TabID::Players].getComponent<cro::Transform>().addChild(m_introTicker.getComponent<cro::Transform>());
    createTicker(m_introTicker);
    updateIntroTicker();
    updatePlayersTab();


    m_uiLayout.tabBar.items[TabID::Course].selected =
        [this]()
        {
            for (auto e : m_detailEntities)
            {
                e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            }
            
            for (auto e : m_courseDetailEntities)
            {
                e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
            }

            if (m_sharedData.scoreType != ScoreType::Stroke)
            {
                m_courseDetailEntities[CourseDetail::Ticker].getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            }

            for (auto e : m_playerDetailIcons)
            {
                e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            }
            
            m_introTicker.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            applyDetails(TabID::Course);

            //resets the scroll position to the beginning - TODO why is this callback activated when changing Item values?
            /*const float newPos = static_cast<float>(m_detailTextures[TabID::Course].getSize().x);
            auto pos = m_courseDetailEntities[CourseDetail::Ticker].getComponent<cro::Transform>().getPosition();
            pos.x = newPos;
            m_courseDetailEntities[CourseDetail::Ticker].getComponent<cro::Transform>().setPosition(pos);
            m_courseDetailEntities[CourseDetail::Ticker].getComponent<cro::Callback>().getUserData<TickerData>().currentPos = newPos;*/
        };


    entity = m_menuState.m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    entity.addComponent<cro::Sprite>();
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, false);
    entity.getComponent<cro::UIElement>().absolutePosition = { 0.f, m_uiLayout.detailsPane.text.getComponent<cro::UIElement>().absolutePosition.y + 8.f };
    entity.getComponent<cro::UIElement>().depth = 0.1f;
    m_detailEntities[TabID::Course] = entity;
    m_uiLayout.detailsPane.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //course thumbnails etc. Updated by updateCourseTab()
    for (auto& e : m_courseDetailEntities)
    {
        e = m_menuState.m_uiScene.createEntity();
        e.addComponent<cro::Transform>();
        e.addComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
        m_detailEntities[TabID::Course].getComponent<cro::Transform>().addChild(e.getComponent<cro::Transform>());
    }
    m_courseDetailEntities[CourseDetail::Thumbnail].addComponent<cro::Sprite>();


    //top five scores scroller (or personal best in non-Steam build)
    createTicker(m_courseDetailEntities[CourseDetail::Ticker]);
    
    updateCourseTab();




    m_uiLayout.tabBar.items[TabID::Rules].selected =
        [this]()
        {
            for (auto e : m_detailEntities)
            {
                e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            }
            
            for (auto e : m_courseDetailEntities)
            {
                e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            }

            for (auto e : m_playerDetailIcons)
            {
                e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            }
            m_introTicker.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            applyDetails(TabID::Rules);
        };

    entity = m_menuState.m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    entity.addComponent<cro::Sprite>();
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, false);
    entity.getComponent<cro::UIElement>().absolutePosition = { 0.f, m_uiLayout.detailsPane.text.getComponent<cro::UIElement>().absolutePosition.y + 8.f };
    entity.getComponent<cro::UIElement>().depth = 0.1f;
    m_detailEntities[TabID::Rules] = entity;
    m_uiLayout.detailsPane.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    updateRulesTab();

    m_uiLayout.tabBar.items[TabID::Scores].selected =
        [this]()
        {
            for (auto e : m_detailEntities)
            {
                e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            }

            for (auto e : m_courseDetailEntities)
            {
                e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            }

            for (auto e : m_playerDetailIcons)
            {
                e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            }
            m_introTicker.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            applyDetails(TabID::Scores);
        };

    entity = m_menuState.m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    entity.addComponent<cro::Sprite>();
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, false);
    entity.getComponent<cro::UIElement>().absolutePosition = { 0.f, m_uiLayout.detailsPane.text.getComponent<cro::UIElement>().absolutePosition.y + 8.f };
    entity.getComponent<cro::UIElement>().depth = 0.1f;
    m_detailEntities[TabID::Scores] = entity;
    m_uiLayout.detailsPane.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    updateScoresTab();



    m_uiLayout.updateTabBar(); //this also updates the menu items

    //info string at the bottom
    static constexpr glm::vec2 InfoPos = glm::vec2(20.f, 21.f);
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

    m_infoRects[0] = spriteSheet.getSprite("lobby_ps").getTextureRect();
    m_infoRects[1] = spriteSheet.getSprite("lobby_xbox").getTextureRect();

    entity = m_menuState.m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("lobby_xbox");
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, true);
    entity.getComponent<cro::UIElement>().depth = 0.1f;
    entity.getComponent<cro::UIElement>().absolutePosition = InfoPos - glm::vec2(0.f, 12.f);
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
    static constexpr float IconSize = 12.f;
    entity.getComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, IconSize), cro::Colour(0.f, 1.f, 1.f, 1.f)),
            cro::Vertex2D(glm::vec2(0.f), cro::Colour(0.f, 0.f, 1.f, 1.f)),
            cro::Vertex2D(glm::vec2(IconSize), cro::Colour(1.f, 1.f, 1.f, 1.f)),
            cro::Vertex2D(glm::vec2(IconSize, 0.f), cro::Colour(1.f, 0.f, 1.f, 1.f)),
        });
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, true);
    entity.getComponent<cro::UIElement>().depth = 0.1f;
    entity.getComponent<cro::UIElement>().absolutePosition = { 6.f, 12.f };
    entity.getComponent<cro::UIElement>().resizeCallback =
        [this](cro::Entity e)
        {
            auto o = (glm::vec2(cro::App::getWindow().getSize()) / 2.f) / cro::UIElementSystem::getViewScale();
            o.x = std::round(o.x);
            o.y = std::round(o.y);
            e.getComponent<cro::Transform>().setOrigin(o);
        };
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


#ifndef USE_GNS
    //hosting IP
    entity = m_menuState.m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Right);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<UIElement>().relativePosition = { 1.f, 0.f };
    entity.getComponent<UIElement>().absolutePosition = { -2.f, 10.f };
    entity.getComponent<UIElement>().depth = 0.05f;
    entity.getComponent<UIElement>().resizeCallback =
        [&](cro::Entity e)
        {
            const auto viewScale = cro::UIElementSystem::getViewScale();
            glm::vec2 p(e.getComponent<cro::Transform>().getPosition());
            e.getComponent<cro::Transform>().setPosition(p * viewScale);
            e.getComponent<cro::Transform>().setScale(glm::vec2(viewScale));
        };
    m_ipText = entity;
#endif


    //m_menuState.registerWindow([this]()
    //    {
    //        ImGui::Begin("sdfg");
    //        ImGui::Image(m_detailTextures[TabID::Scores].getTexture(), { 100.f, 100.f }, { 0.f, 1.f }, { 1.f, 0.f });
    //        ImGui::End();        
    //    });
}

void MenuState::LobbyMenu::createPlayerTab()
{
    m_uiLayout.menuLayout.items[TabID::Players].clear();

    auto* item = &m_uiLayout.menuLayout.items[TabID::Players].emplace_back();
    item->title = "Player Menu";
    item->displayType = Menu::Item::Heading;


    //ready-up / start game
    item = &m_uiLayout.menuLayout.items[TabID::Players].emplace_back();
    item->title = m_sharedData.hosting ?  u8"↓ Start Game" : u8"↓ Ready Up";
    item->description = m_sharedData.hosting ? "Press and Hold to Start" : "Press and Hold to Ready Up";
    item->selected = [this](Menu::Item& i)
        {
            i.description = m_sharedData.hosting ? 
                m_ruleViolation.empty() ? "Press and Hold to Start" : m_ruleViolation
                : "Press and Hold to Ready Up";
        };
    item->activated = [this](Menu::Item& i)
        {
            //press / hold to start or ready up
            m_buttonFlags |= ButtonFlags::Action;
            setProgressColour(CD32::Colours[CD32::GreenLight]);

            //set a callback to be activated when timer expires
            //(when we get the update from the server the detail
            //pane is redrawn for us)
            m_timeoutCallback = std::bind(&LobbyMenu::readyStart, this);
        };
    item->labels = { "Let\'s Go!" };
    item->selectedIndex = 0;

#ifdef USE_GNS
    //invite friends
    item = &m_uiLayout.menuLayout.items[TabID::Players].emplace_back();
    item->title = "Invite Friends";
    item->description = "Opens the Steam overlay to invite friends to this lobby.";
    cro::Util::String::wordWrap(item->description, WordWrapSmall);
    item->activated = [this](Menu::Item& i)
        {
            //unready ourself if not hosting so game can't be started
            //while the invite overlay is open
            unready();
            Social::inviteFriends(m_sharedData.lobbyID);
        };
    item->labels = { "Invite" };
    item->selectedIndex = 0;
#endif

    //we need to do this in a refresh after creating the lobby
    //as when the menu is first built the game is only just launched
    if (m_sharedData.hosting)
    {
#ifdef USE_GNS
        //friends only
        item = &m_uiLayout.menuLayout.items[TabID::Players].emplace_back();
        item->title = "Friends Only";
        item->description = "Only allow members of your Steam friends to join this lobby.";
        cro::Util::String::wordWrap(item->description, WordWrapSmall);
        item->activated = [this](Menu::Item& i)
            {
                m_menuState.m_matchMaking.setFriendsOnly(i.selectedIndex == 1);
            };
        item->labels = { "No", "Yes" };
        item->selectedIndex = m_menuState.m_matchMaking.getFriendsOnly() ? 1 : 0;
#endif

        //teams mode
        item = &m_uiLayout.menuLayout.items[TabID::Players].emplace_back();
        item->title = "Teams";
        item->description = "Players are paired up for the round.";
        cro::Util::String::wordWrap(item->description, WordWrapSmall); //TODO pretty sure word wrapping is done when text is rendered *anyway*
        item->activated = [this](Menu::Item& i)
            {
                m_sharedData.teamMode ? cro::Console::doCommand("sv_team_mode 0")
                    : cro::Console::doCommand("sv_team_mode 1");
            };
        item->labels = { "No", "Yes" };
        item->selectedIndex = m_sharedData.teamMode ? 1 : 0;


        //select player
        item = &m_uiLayout.menuLayout.items[TabID::Players].emplace_back();
        item->title = "Select Player";
        item->description = "Select the active player.";
        item->activated = [this](Menu::Item& i)
            {
                //TODO update index
                if (i.activationDirection == Menu::Item::Left)
                {
                    m_menuState.m_selectedDisplayMember = std::max(0, m_menuState.m_selectedDisplayMember - 1);
                }
                else if (i.activationDirection == Menu::Item::Right)
                {
                    m_menuState.m_selectedDisplayMember = std::min(static_cast<std::int32_t>(m_menuState.m_displayOrder.size() - 1),
                                                                    m_menuState.m_selectedDisplayMember + 1);
                }
                updatePlayersTab();
            };
        item->labels = { "Select", "Select" }; //we don't want the label to change, but to offer left/right buttons
        item->selectedIndex = 0;


        //move selected
        item = &m_uiLayout.menuLayout.items[TabID::Players].emplace_back();
        item->title = "Move Selected Player";
        item->description = "Moving the selected player decides which team they appear in.";
        cro::Util::String::wordWrap(item->description, WordWrapSmall);
        item->activated = [this](Menu::Item& i)
            {
                //tbh this probably doesn't matter if teams aren't active
                //if (m_sharedData.teamMode)
                {
                    if (i.activationDirection == Menu::Item::Left)
                    {
                        if (m_menuState.m_selectedDisplayMember > 0)
                        {
                            m_menuState.moveDisplayMemberUp();
                        }
                    }
                    else if (i.activationDirection == Menu::Item::Right)
                    {
                        if (m_menuState.m_selectedDisplayMember < static_cast<std::int32_t>(m_menuState.m_displayOrder.size()) - 1)
                        {
                            m_menuState.moveDisplayMemberDown();
                        }
                    }
                }
            };
        item->labels = { "Move", "Move" };
        item->selectedIndex = 0;

        //poke selected
        item = &m_uiLayout.menuLayout.items[TabID::Players].emplace_back();
        item->title = "Poke Player";
        item->description = "Give the selected player a little poke.";
        cro::Util::String::wordWrap(item->description, WordWrapSmall);
        item->activated = [this](Menu::Item& i)
            {
                m_buttonFlags |= ButtonFlags::Action;
                setProgressColour(CD32::Colours[CD32::Yellow]);

                m_timeoutCallback = std::bind(&LobbyMenu::pokePlayer, this);
            };
        item->labels = { u8"↓ Poke" };
        item->selectedIndex = 0;


        //kick selected
        item = &m_uiLayout.menuLayout.items[TabID::Players].emplace_back();
        item->title = "Kick Player";
        item->description = "Kick the selected player. This will also remove all players on the same client!";
        cro::Util::String::wordWrap(item->description, WordWrapSmall);
        item->activated = [this](Menu::Item& i)
            {
                m_buttonFlags |= ButtonFlags::Action;
                setProgressColour(CD32::Colours[CD32::Red]);

                m_timeoutCallback = std::bind(&LobbyMenu::kickPlayer, this);
            };
        item->labels = { u8"↓ Kick" };
        item->selectedIndex = 0;

    }
}

void MenuState::LobbyMenu::createCourseTab()
{
    m_uiLayout.menuLayout.items[TabID::Course].clear();

    auto* item = &m_uiLayout.menuLayout.items[TabID::Course].emplace_back();
    item->title = "Course Selection";
    item->displayType = Menu::Item::Heading;

    if (m_sharedData.hosting)
    {
        const bool hasUserCourses = m_menuState.m_sharedCourseData.courseData.size() > m_menuState.m_courseIndices[Range::Official].count;
        if (hasUserCourses)
        {
            //user courses
            item = &m_uiLayout.menuLayout.items[TabID::Course].emplace_back();
            item->title = "User Courses";
            item->description = "Select from courses created in the Course Remixer.";
            cro::Util::String::wordWrap(item->description, WordWrapSmall);
            item->activated = [this](Menu::Item& i)
                {
                    //hmm the game still has a space for workshop
                    //courses, but the count (for now) will always
                    //be zero
                    do
                    {
                        m_menuState.m_currentRange = (m_menuState.m_currentRange + (Range::Count - 1)) % Range::Count;
                        m_sharedData.courseIndex = m_menuState.m_courseIndices[m_menuState.m_currentRange].start;
                    } while (m_menuState.m_courseIndices[m_menuState.m_currentRange].count == 0);

                    i.selectedIndex = m_menuState.m_currentRange;

                    //silly hack which sends the server updaetd course info to refresh clients
                    m_menuState.prevCourse();
                    m_menuState.nextCourse();
                };
            item->labels = { "No", "Yes", /*"Workshop"*/ };
            item->selectedIndex = m_menuState.m_currentRange;
        }


        const auto resetScroll =
            [this]() 
            {
                const float newPos = static_cast<float>(m_detailTextures[TabID::Course].getSize().x);
                auto pos = m_courseDetailEntities[CourseDetail::Ticker].getComponent<cro::Transform>().getPosition();
                pos.x = newPos;
                m_courseDetailEntities[CourseDetail::Ticker].getComponent<cro::Transform>().setPosition(pos);
                m_courseDetailEntities[CourseDetail::Ticker].getComponent<cro::Callback>().getUserData<TickerData>().currentPos = newPos;
            };

        //course selection
        item = &m_uiLayout.menuLayout.items[TabID::Course].emplace_back();
        item->title = "Select Course";
        item->activated = 
            [this, resetScroll](Menu::Item& i)
            {
                resetScroll();
                if (i.activationDirection == Menu::Item::Left)
                {
                    m_menuState.prevCourse();
                }
                else
                {
                    m_menuState.nextCourse();
                }
            };
        item->labels = { "Select", "Select" };
        item->selectedIndex = 0;


        //hole count
        item = &m_uiLayout.menuLayout.items[TabID::Course].emplace_back();
        item->title = "Hole Count";
        item->activated = 
            [this, resetScroll](Menu::Item& i)
            {
                resetScroll();
                if (i.activationDirection == Menu::Item::Left)
                {
                    m_menuState.prevHoleCount();
                }
                else
                {
                    m_menuState.nextHoleCount();
                }

            };
        item->labels = { "All", "Front", " Back" };
        item->selectedIndex = m_sharedData.holeCount;


        //reverse course
        item = &m_uiLayout.menuLayout.items[TabID::Course].emplace_back();
        item->title = "Play Course In Reverse";
        item->activated = [this](Menu::Item&)
            {
                m_sharedData.reverseCourse = m_sharedData.reverseCourse == 0 ? 1 : 0;
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::ReverseCourse, m_sharedData.reverseCourse,
                                                                    net::NetFlag::Reliable, ConstVal::NetChannelReliable);
            };
        item->labels = { "No", "Yes" };
        item->selectedIndex = m_sharedData.reverseCourse;


        //night mode
        item = &m_uiLayout.menuLayout.items[TabID::Course].emplace_back();
        item->title = "Night Time";
        item->description = "Play the course at night.";
        item->activated = [this](Menu::Item& i)
            {
                m_sharedData.nightTime = static_cast<std::uint8_t>(i.selectedIndex);
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::NightTime, m_sharedData.nightTime, 
                                                                    net::NetFlag::Reliable, ConstVal::NetChannelReliable);
            };
        item->labels = { "No", "Yes" };
        item->selectedIndex = m_sharedData.nightTime;


        //weather
        item = &m_uiLayout.menuLayout.items[TabID::Course].emplace_back();
        item->title = "Weather";
        item->description = "Choose the weather conditions.";
        item->activated = [this](Menu::Item& i)
            {
                std::uint8_t weatherType = static_cast<std::uint8_t>(i.selectedIndex);
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::WeatherType, weatherType, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
            };
        item->labels = { WeatherStrings.begin(), WeatherStrings.end() };
        item->selectedIndex = m_sharedData.weatherType;


        //random wind
        item = &m_uiLayout.menuLayout.items[TabID::Course].emplace_back();
        item->title = "Randomise Wind";
        item->description = "When enabled wind speed and direction can change at any time.";
        cro::Util::String::wordWrap(item->description, WordWrapSmall);
        item->activated = [this](Menu::Item& i)
            {
                m_sharedData.randomWind = static_cast<std::uint8_t>(i.selectedIndex);
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::RandomWind, m_sharedData.randomWind, 
                                                                    net::NetFlag::Reliable, ConstVal::NetChannelReliable);
            };
        item->labels = { "No", "Yes" };
        item->selectedIndex = m_sharedData.randomWind;


        //wind strength
        item = &m_uiLayout.menuLayout.items[TabID::Course].emplace_back();
        item->title = "Wind Strength";
        item->description = "Select the maxmimum strength of the wind.";
        cro::Util::String::wordWrap(item->description, WordWrapSmall);
        item->activated = [this](Menu::Item& i)
            {
                m_sharedData.windStrength = static_cast<std::uint8_t>(i.selectedIndex);
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::MaxWind, std::uint8_t(m_sharedData.windStrength + 1), 
                                                                    net::NetFlag::Reliable, ConstVal::NetChannelReliable);
            };
        item->labels = { "Normal", "Medium", "High" };
        item->selectedIndex = m_sharedData.windStrength;
    }
}

void MenuState::LobbyMenu::createRulesTab()
{
    m_uiLayout.menuLayout.items[TabID::Rules].clear();

    auto* item = &m_uiLayout.menuLayout.items[TabID::Rules].emplace_back();
    item->title = "Game Rules";
    item->displayType = Menu::Item::Heading;
    item->description = "Configure the rules of play";

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


    if (m_sharedData.hosting)
    {
        //choose rules / game mode
        item = &m_uiLayout.menuLayout.items[TabID::Rules].emplace_back();
        item->title = "Scoring";
        //item->description = "";
        item->activated = [this](Menu::Item& i)
            {
                if (i.activationDirection == Menu::Item::Left)
                {
                    m_menuState.prevRules();
                }
                else
                {
                    m_menuState.nextRules();
                }
            };
        item->labels = { "Select", "Select" };
        item->selectedIndex = 0;

        //set gimme radius
        item = &m_uiLayout.menuLayout.items[TabID::Rules].emplace_back();
        item->title = "Gimme Radius";
        item->description = "For brevity of play the ball is automatically holed when it is less than this distance from the pin.";
        cro::Util::String::wordWrap(item->description, WordWrapSmall);
        item->activated = [this](Menu::Item& i)
            {
                m_sharedData.gimmeRadius = static_cast<std::uint8_t>(i.selectedIndex);
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::GimmeRadius, m_sharedData.gimmeRadius, 
                                                                    net::NetFlag::Reliable, ConstVal::NetChannelReliable);
            };
        //item->labels = { "None", "Under the Leather", "Under the Putter" };
        item->labels = { "Select", "Select", "Select" }; //rely on the details pain to explain this
        item->selectedIndex = m_sharedData.gimmeRadius;


        //fast CPU
        item = &m_uiLayout.menuLayout.items[TabID::Rules].emplace_back();
        item->title = "Fast CPU";
        item->description = "Automatically skip CPU player shots forward.";
        cro::Util::String::wordWrap(item->description, WordWrapSmall);
        item->activated = [this](Menu::Item& i)
            {
                m_sharedData.fastCPU = i.selectedIndex == 1;
                m_sharedData.clientConnection.netClient.sendPacket<std::uint8_t>(PacketID::FastCPU, std::uint8_t(i.selectedIndex),
                                                                                    net::NetFlag::Reliable, ConstVal::NetChannelReliable);
            };
        item->labels = { "No", "Yes" };
        item->selectedIndex = m_sharedData.fastCPU ? 1 : 0;


        //enable snek
        item = &m_uiLayout.menuLayout.items[TabID::Rules].emplace_back();
        item->title = "Enable Snek";
        item->description = "The player who last misses a putt is left holding the snek";
        item->description += EmSnake;
        cro::Util::String::wordWrap(item->description, WordWrapSmall);
        item->activated = [this](Menu::Item& i)
            {
                if (m_sharedData.clientConnection.connected)
                {
                    const std::uint16_t d = (std::uint8_t(RuleMod::Snek) << 8) | std::uint8_t(i.selectedIndex);
                    m_sharedData.clientConnection.netClient.sendPacket(PacketID::RuleMod, d, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

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
                if (m_sharedData.clientConnection.connected)
                {
                    const std::uint16_t d = (std::uint8_t(RuleMod::BigBalls) << 8) | std::uint8_t(i.selectedIndex);
                    m_sharedData.clientConnection.netClient.sendPacket(PacketID::RuleMod, d, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                }
            };
        item->labels = { "No", "Yes" };
        item->selectedIndex = 0;

        //allow assists
        item = &m_uiLayout.menuLayout.items[TabID::Rules].emplace_back();
        item->title = "Allow Assists";
        item->description = "Players are allowed to use Range Assist and Putt Assist";
        cro::Util::String::wordWrap(item->description, WordWrapSmall);
        item->activated = [this](Menu::Item& i)
            {
                if (m_sharedData.clientConnection.connected)
                {
                    const std::uint16_t d = (std::uint8_t(RuleMod::NoAssist) << 8) | std::uint8_t(i.selectedIndex);
                    m_sharedData.clientConnection.netClient.sendPacket(PacketID::RuleMod, d, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                }
            };
        item->labels = { "Yes", "No" };
        item->selectedIndex = 0;
    }
}

void MenuState::LobbyMenu::createScoresTab()
{
    m_uiLayout.menuLayout.items[TabID::Scores].clear();

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
        item->activated = [this](Menu::Item& i)
            {
                m_menuState.togglePreviousScoreCard();
            };
        item->labels = { "OK" };
        item->selectedIndex = 0;
    }
}

void MenuState::LobbyMenu::updateIntroTicker()
{
    struct ScoreInfo final
    {
        std::uint8_t clientID = 0;
        std::uint8_t playerID = 0;
        std::uint8_t lives = 0;
        std::int8_t score = 0;
    };

    std::vector<ScoreInfo> scoreInfo;
    const auto& courseData = m_menuState.m_sharedCourseData.courseData[m_sharedData.courseIndex];
    cro::String str = "Welcome to Super Video Golf!";

    //calculate the string for the intro ticker
    if (m_sharedData.gameMode == GameMode::FreePlay //at this point (when the menu is built) this will be set if we're returning from a tutorial or quit menu
        && m_sharedData.scoreType != ScoreType::NearestThePin) //don't bother scrolling these - we can still read them from the score card if we want to
    {
        for (auto i = 0u; i < m_sharedData.connectionData.size(); ++i)
        {
            for (auto j = 0u; j < m_sharedData.connectionData[i].playerCount; ++j)
            {
                if (!m_sharedData.connectionData[i].playerData[j].name.empty())
                {
                    auto& info = scoreInfo.emplace_back();
                    info.clientID = i;
                    info.playerID = j;
                    switch (m_sharedData.scoreType)
                    {
                    default:
                    case ScoreType::Elimination:
                        info.lives = m_sharedData.connectionData[i].playerData[j].skinScore;
                        [[fallthrough]];
                    case ScoreType::Stroke:
                    case ScoreType::ShortRound:
                    case ScoreType::MultiTarget:
                        info.score = m_sharedData.connectionData[i].playerData[j].parScore;
                        break;
                    case ScoreType::Match:
                    case ScoreType::NearestThePinPro:
                        info.score = m_sharedData.connectionData[i].playerData[j].matchScore;
                        break;
                    case ScoreType::Skins:
                        info.score = m_sharedData.connectionData[i].playerData[j].skinScore;
                        break;
                    case ScoreType::Stableford:
                    case ScoreType::StablefordPro:
                        for (auto k = 0u; k < m_sharedData.connectionData[i].playerData[j].holeScores.size(); ++k)
                        {
                            auto diff = static_cast<std::int32_t>(m_sharedData.connectionData[i].playerData[j].holeScores[k]) - courseData.parVals[k];
                            auto stableScore = 2 - diff;

                            if (m_sharedData.scoreType == ScoreType::Stableford)
                            {
                                stableScore = std::max(0, stableScore);
                            }
                            else if (stableScore < 2)
                            {
                                stableScore -= 2;
                            }
                            info.score += stableScore;
                        }
                        break;
                    case ScoreType::NearestThePin:

                        break;
                    }
                }
            }
        }

        std::sort(scoreInfo.begin(), scoreInfo.end(),
            [&](const ScoreInfo& a, const ScoreInfo& b)
            {
                switch (m_sharedData.scoreType)
                {
                default:
                case ScoreType::Elimination:
                    if (a.lives == b.lives)
                    {
                        return a.score < b.score;
                    }
                    return a.lives > b.lives;
                    //[[fallthrough]];
                case ScoreType::Stroke:
                case ScoreType::ShortRound:
                case ScoreType::MultiTarget:
                    return a.score < b.score;
                case ScoreType::Stableford:
                case ScoreType::StablefordPro:
                case ScoreType::NearestThePinPro:
                case ScoreType::Skins:
                case ScoreType::Match:
                    return a.score > b.score;
                }
            });


        std::vector<cro::String> names;
        for (const auto& score : scoreInfo)
        {
            names.push_back(m_sharedData.connectionData[score.clientID].playerData[score.playerID].name);
            names.back() += ": (" + std::to_string(score.score) + ")";
            switch (m_sharedData.scoreType)
            {
            default:
            case ScoreType::Elimination:
            case ScoreType::MultiTarget:
            case ScoreType::ShortRound:
            case ScoreType::Stroke:
                if (score.score < 0)
                {
                    names.back() += " Under Par";
                }
                else if (score.score > 0)
                {
                    names.back() += " Over Par";
                }
                break;
            case ScoreType::Stableford:
            case ScoreType::StablefordPro:
            case ScoreType::NearestThePinPro:
                names.back() += " Points";
                break;
            case ScoreType::Skins:
                names.back() += " Skins";
                break;
            case ScoreType::Match:
                names.back() += " Match Points";
                break;
            }
        }

        if (!names.empty())
        {
            str = "Last Round's Top Scorers: < " + names[0];
            for (auto i = 1u; i < names.size() && i < 4u; ++i)
            {
                str += " >< " + names[i];
            }
            str += " >";
        }
//#ifdef USE_GNS
//        else if (!m_sharedData.hosting)
//        {
//            str = "Can't ready up? Try opening then closing the Steam Overlay.";
//        }
//#endif
    }

    m_introTicker.getComponent<cro::Text>().setString(str);
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

        //this might actually get called before we have background size
        //so quit here and try again next time
        else
        {
            return;
        }
    }


    const float Width = static_cast<float>(m_detailTextures[TabID::Players].getSize().x);
    const float Top = static_cast<float>(m_detailTextures[TabID::Players].getSize().y);
    constexpr float Height = 14.f;

    std::vector<cro::Vertex2D> verts =
    {
        cro::Vertex2D(glm::vec2(0.f, Height), CD32::Colours[CD32::BeigeMid]),
        cro::Vertex2D(glm::vec2(0.f), CD32::Colours[CD32::BeigeMid]),
        cro::Vertex2D(glm::vec2(Width, Height), CD32::Colours[CD32::BeigeMid]),

        cro::Vertex2D(glm::vec2(Width, Height), CD32::Colours[CD32::BeigeMid]),
        cro::Vertex2D(glm::vec2(0.f), CD32::Colours[CD32::BeigeMid]),
        cro::Vertex2D(glm::vec2(Width, 0.f), CD32::Colours[CD32::BeigeMid])
    };

    m_detailArray.setPosition({ 0.f, Top - (Height * 2.f) });
    m_detailArray.setVertexData(verts);


    //render everything to texture
    m_detailTextures[TabID::Players].clear(CD32::Colours[CD32::BeigeLight]);
    for (auto i = 0; i < 8; ++i)
    {
        m_detailArray.draw();
        m_detailArray.move({ 0.f, -(Height * 2.f) });
    }

    //set verts to yellow and render over selected player position
    m_detailArray.setPosition({ 0.f, Top - ((m_menuState.m_selectedDisplayMember + 1) * Height) });
    for (auto& v : verts)
    {
        v.colour = CD32::Colours[CD32::Yellow];
    }
    m_detailArray.setVertexData(verts);
    m_detailArray.draw();

    //draws the edge border
    m_detailArray.setVertexData({
        cro::Vertex2D(glm::vec2(0.f, Top), CD32::Colours[CD32::BeigeLight]),
        cro::Vertex2D(glm::vec2(0.f), CD32::Colours[CD32::BeigeLight]),
        cro::Vertex2D(glm::vec2(13.f, Top), CD32::Colours[CD32::BeigeLight]),
        cro::Vertex2D(glm::vec2(13.f, Top), CD32::Colours[CD32::BeigeLight]),
        cro::Vertex2D(glm::vec2(0.f), CD32::Colours[CD32::BeigeLight]),
        cro::Vertex2D(glm::vec2(13.f, 0.f), CD32::Colours[CD32::BeigeLight]),

        cro::Vertex2D(glm::vec2(0.f, Top), CD32::Colours[CD32::Brown]),
        cro::Vertex2D(glm::vec2(0.f), CD32::Colours[CD32::Brown]),
        cro::Vertex2D(glm::vec2(12.f, Top), CD32::Colours[CD32::Brown]),
        cro::Vertex2D(glm::vec2(12.f, Top), CD32::Colours[CD32::Brown]),
        cro::Vertex2D(glm::vec2(0.f), CD32::Colours[CD32::Brown]),
        cro::Vertex2D(glm::vec2(12.f, 0.f), CD32::Colours[CD32::Brown]),

        cro::Vertex2D(glm::vec2(0.f, Top), CD32::Colours[CD32::BeigeDark]),
        cro::Vertex2D(glm::vec2(0.f), CD32::Colours[CD32::BeigeDark]),
        cro::Vertex2D(glm::vec2(11.f, Top), CD32::Colours[CD32::BeigeDark]),
        cro::Vertex2D(glm::vec2(11.f, Top), CD32::Colours[CD32::BeigeDark]),
        cro::Vertex2D(glm::vec2(0.f), CD32::Colours[CD32::BeigeDark]),
        cro::Vertex2D(glm::vec2(11.f, 0.f), CD32::Colours[CD32::BeigeDark])
        });
    m_detailArray.setPosition({ 0.f, 0.f });
    m_detailArray.draw();

    const float Bottom = Top - (Height * 16.f);
    m_detailArray.setVertexData({
        cro::Vertex2D(glm::vec2(0.f, Bottom), CD32::Colours[CD32::Olive]),
        cro::Vertex2D(glm::vec2(0.f), CD32::Colours[CD32::Olive]),
        cro::Vertex2D(glm::vec2(Width, Bottom), CD32::Colours[CD32::Olive]),
        cro::Vertex2D(glm::vec2(Width, Bottom), CD32::Colours[CD32::Olive]),
        cro::Vertex2D(glm::vec2(0.f), CD32::Colours[CD32::Olive]),
        cro::Vertex2D(glm::vec2(Width, 0.f), CD32::Colours[CD32::Olive]),

        cro::Vertex2D(glm::vec2(0.f, Bottom - 1.f), CD32::Colours[CD32::Brown]),
        cro::Vertex2D(glm::vec2(0.f), CD32::Colours[CD32::Brown]),
        cro::Vertex2D(glm::vec2(Width, Bottom - 1.f), CD32::Colours[CD32::Brown]),
        cro::Vertex2D(glm::vec2(Width, Bottom - 1.f), CD32::Colours[CD32::Brown]),
        cro::Vertex2D(glm::vec2(0.f), CD32::Colours[CD32::Brown]),
        cro::Vertex2D(glm::vec2(Width, 0.f), CD32::Colours[CD32::Brown]),

        });
    m_detailArray.draw();


    //background for ticker text - nice idea, but it doesn't fit :(
    /*const auto tickerBounds = m_detailSprites[DetailSprite::TickerLeft].getTextureBounds();
    m_detailQuad.setOrigin({ 0.f, 0.f });
    m_detailQuad.setScale(glm::vec2(1.f));
    m_detailQuad = m_detailSprites[DetailSprite::TickerLeft];
    m_detailQuad.setPosition({ 0.f, Bottom - tickerBounds.height });
    m_detailQuad.draw();

    const float centreWidth = Width - (tickerBounds.width * 2.f);
    m_detailQuad = m_detailSprites[DetailSprite::TickerCentre];
    m_detailQuad.move({ tickerBounds.width, 0.f });
    m_detailQuad.setScale({ centreWidth, 1.f });
    m_detailQuad.draw();

    m_detailQuad = m_detailSprites[DetailSprite::TickerRight];
    m_detailQuad.move({ centreWidth, 0.f });
    m_detailQuad.setScale(glm::vec2(1.f));
    m_detailQuad.draw();*/


    m_detailTextures[TabID::Players].display();



    //create child ents to display all the info (eg so text scales
    //properly, rather than pre-rendering on the texture...)
    for (auto e : m_playerDetailIcons)
    {
        m_menuState.m_uiScene.destroyEntity(e);
    }
    m_playerDetailIcons.clear();

    const auto& displayMembers = m_menuState.m_displayOrder;
    static constexpr float RowSpacing = 14.f;
    cro::String nameString;
    std::int32_t row = 0;
    for (const auto [cID, pID] : displayMembers)
    {
        const auto& c = m_sharedData.connectionData[cID];

        std::size_t charOffset = 0;
        if (m_sharedData.teamMode)
        {
            nameString += cro::String(std::uint32_t(pc::TeamEmoji[c.playerData[pID].teamIndex]));
            charOffset = 1;
        }

        nameString += c.playerData[pID].name.substr(0, ConstVal::MaxStringChars - charOffset) + "\n";


        const glm::vec3 iconPos(2.f, (Top - (row * RowSpacing)) - 10.f, 0.1f);
        row++;

        //add a ready status for that client
        auto entity = m_menuState.m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(iconPos);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = m_menuState.m_sprites[SpriteID::ReadyStatus];
        entity.addComponent<cro::SpriteAnimation>();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&, cID](cro::Entity e2, float) //apparently captured structured bindings actually needs c++ 20
            {
                auto index = m_menuState.m_readyState[cID] ? 1 : 0;
                e2.getComponent<cro::SpriteAnimation>().play(index);
            };
        m_playerDetailIcons.push_back(entity);
        m_detailEntities[TabID::Players].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }

    auto entity = m_menuState.m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 16.f, Top - 3.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_sharedData.sharedResources->fonts.get(FontID::UI)).setString(nameString);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setVerticalSpacing(6.f);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    m_playerDetailIcons.push_back(entity);
    m_detailEntities[TabID::Players].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //update the ticker output
    TickerData td =
    {
        //this is the cropping width
        .width = Width,
        //offset from the edge
        .offset = 8.f,
        .basePos = Bottom + 10.f,
        .currentPos = Width
    };
    m_introTicker.getComponent<cro::Callback>().setUserData<TickerData>(td);

    if (m_uiLayout.tabBar.activeIndex == TabID::Players)
    {
        //set this as the active background image
        applyDetails(TabID::Players);
    }
}

#ifdef USE_GNS
void MenuState::LobbyMenu::getMonthlyProgress(cro::String& dst)
{
    const auto count = Social::getMonthlyCompletionCount(m_sharedData.mapDirectory, m_sharedData.holeCount);
    if (count != 0)
    {
        const cro::String completed = "\n\n\nCompleted " + std::to_string(count) + "x this month!";
        cro::String monthlyBest;

        auto best = Social::getMonthlyBest(m_sharedData.mapDirectory, m_sharedData.holeCount);
        if (best)
        {
            monthlyBest = "\nMonthly Best: " + std::to_string(best);
        }
        else
        {
            best = Social::getPersonalBest(m_sharedData.mapDirectory, m_sharedData.holeCount);
            if (best)
            {
                monthlyBest = "\nPersonal Best: " + std::to_string(best);
            }
            else
            {
                monthlyBest = "\nFetching Score...";
            }
        }

        dst += completed;
        dst += monthlyBest;
    }
}
#endif

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
        else
        {
            return;
        }
    }


    m_uiText.setAlignment(cro::SimpleText::Alignment::Centre);
    m_infoText.setAlignment(cro::SimpleText::Alignment::Centre);
    const auto texSize = glm::vec2(m_detailTextures[TabID::Course].getSize());

    m_detailTextures[TabID::Course].clear(CD32::Colours[CD32::GreyDark]);
    
    //render course title
    m_uiText.setString(m_courseDetails.title);
    m_uiText.setPosition({ texSize.x / 2.f, texSize.y - 12.f });
    m_uiText.draw();

    //render description
    m_infoText.setString(m_courseDetails.desc);
    m_infoText.setFillColour(TextNormalColour);
    if (const auto p = m_courseDetails.desc.find("(DLC)"); p != cro::String::InvalidPos)
    {
        m_infoText.setFillColour(TextGoldColour, p);
    }
    m_infoText.setPosition({ texSize.x / 2.f, texSize.y - 24.f });
    m_infoText.draw();


    //render thumbnail background
    m_detailQuad = m_detailSprites[DetailSprite::CourseThumb];
    m_detailQuad.setOrigin(m_detailQuad.getSize() / 2.f);
    m_detailQuad.setPosition({ std::round(texSize.x / 4.f) + 12.f, texSize.y - 102.f });
    m_detailQuad.setScale({ 1.f, 1.f });
    m_detailQuad.draw();

    //display thumbnail if available
    const cro::Texture* t = nullptr;

    if (m_menuState.m_currentRange == Range::Official)
    {
        if (m_menuState.m_sharedCourseData.videoPaths.count(m_sharedData.mapDirectory) != 0
            && m_menuState.m_sharedCourseData.videoPlayer.loadFromFile(m_menuState.m_sharedCourseData.videoPaths.at(m_sharedData.mapDirectory)))
        {
            m_menuState.m_sharedCourseData.videoPlayer.setLooped(true);
            m_menuState.m_sharedCourseData.videoPlayer.play();
            m_menuState.m_sharedCourseData.videoPlayer.update(1.f / 30.f);

            t = &m_menuState.m_sharedCourseData.videoPlayer.getTexture();
        }

        else if (m_menuState.m_sharedCourseData.courseThumbs.count(m_sharedData.mapDirectory) != 0)
        {
            t = m_menuState.m_sharedCourseData.courseThumbs.at(m_sharedData.mapDirectory).get();
        }
    }

    if (t)
    {
        auto entity = m_courseDetailEntities[CourseDetail::Thumbnail];
        entity.getComponent<cro::Transform>().setPosition(glm::vec3(m_detailQuad.getPosition(), 0.1f));
        entity.getComponent<cro::Sprite>().setTexture(*t);
        
        const auto thumbSize = glm::vec2(t->getSize());
        const auto scale = CourseThumbnailSize / thumbSize;
        entity.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        entity.getComponent<cro::Transform>().setOrigin(thumbSize / 2.f);
        entity.getComponent<cro::Transform>().move({ 0.f, 9.f });
    }
    else
    {
        m_courseDetailEntities[CourseDetail::Thumbnail].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    }

    //render hole count
    m_infoText.setString(m_courseDetails.holeCount);
    m_infoText.setFillColour(TextNormalColour);
    m_infoText.setPosition(m_detailQuad.getPosition() - glm::vec2(0.f, 58.f));
    m_infoText.draw();


    //background for ticker text
    const auto tickerBounds = m_detailSprites[DetailSprite::TickerLeft].getTextureBounds();
    const auto oldPos = m_detailQuad.getPosition() - m_detailQuad.getOrigin();
    m_detailQuad.setOrigin({ 0.f, 0.f });
    m_detailQuad = m_detailSprites[DetailSprite::TickerLeft];
    m_detailQuad.setPosition({ 0.f, oldPos.y - tickerBounds.height - 16.f });
    m_detailQuad.draw();

    const float centreWidth = texSize.x - (tickerBounds.width * 2.f);
    m_detailQuad = m_detailSprites[DetailSprite::TickerCentre];
    m_detailQuad.move({ tickerBounds.width, 0.f });
    m_detailQuad.setScale({ centreWidth, 1.f });
    m_detailQuad.draw();

    m_detailQuad = m_detailSprites[DetailSprite::TickerRight];
    m_detailQuad.move({ centreWidth, 0.f });
    m_detailQuad.setScale(glm::vec2(1.f));
    m_detailQuad.draw();

    const float vertsHeight = m_detailQuad.getPosition().y;

    m_detailArray.setVertexData({
        cro::Vertex2D(glm::vec2(0.f, vertsHeight), CD32::Colours[CD32::Brown]),
        cro::Vertex2D(glm::vec2(texSize.x, vertsHeight), CD32::Colours[CD32::Brown]),
        cro::Vertex2D(glm::vec2(0.f), CD32::Colours[CD32::Brown]),
        cro::Vertex2D(glm::vec2(0.f), CD32::Colours[CD32::Brown]),
        cro::Vertex2D(glm::vec2(texSize.x, vertsHeight), CD32::Colours[CD32::Brown]),
        cro::Vertex2D(glm::vec2(texSize.x, 0.f), CD32::Colours[CD32::Brown]),

        });
    m_detailArray.setPosition(glm::vec2(0.f));
    m_detailArray.draw();

    if (m_sharedData.scoreType == ScoreType::Stroke)
    {
        //render ticker for leaderboards / personal best
        const auto posX = m_courseDetailEntities[CourseDetail::Ticker].getComponent<cro::Transform>().getPosition().x;
        m_courseDetailEntities[CourseDetail::Ticker].getComponent<cro::Transform>().setPosition({ posX, vertsHeight + 33.f, 0.2f });
        TickerData td =
        {
            //this is the cropping width
            .width = texSize.x,
            //offset from the edge
            .offset = 8.f,
            .basePos = vertsHeight + 33.f,
            .currentPos = posX
        };
        m_courseDetailEntities[CourseDetail::Ticker].getComponent<cro::Callback>().setUserData<TickerData>(td);

        const auto scoreStr = Social::getTopFive(m_sharedData.mapDirectory, m_sharedData.holeCount);
        m_courseDetailEntities[CourseDetail::Ticker].getComponent<cro::Text>().setString(scoreStr);
        m_courseDetailEntities[CourseDetail::Ticker].getComponent<cro::Transform>().setScale(glm::vec2(1.f));
    }
    else
    {
        m_courseDetailEntities[CourseDetail::Ticker].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    }

    //render reverse state, night time, weather, wind speed, wind random
    cro::String str = "Reverse Order:   ";
    str += m_sharedData.reverseCourse ? "Yes" : "No";
    str += "\nNight Time:          ";
    str += m_sharedData.nightTime ? "Yes" : "No";
    str += "\nWeather:            " + WeatherStrings[m_sharedData.weatherType];
    str += "\nRandom Wind:       ";
    str += m_sharedData.randomWind ? "Yes" : "No";
    str += "\nWind Strength:     " + WindStrings[m_sharedData.windStrength];

#ifdef USE_GNS
    getMonthlyProgress(str);
#endif

    m_infoText.setAlignment(cro::SimpleText::Alignment::Left);
    m_infoText.setString(str);
    m_infoText.setPosition({ (texSize.x / 2.f) + 24.f, texSize.y - 48.f });
    m_infoText.draw();

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
        else
        {
            return;
        }
    }

    const auto texSize = glm::vec2(m_detailTextures[TabID::Rules].getSize());

    m_detailTextures[TabID::Rules].clear(CD32::Colours[CD32::GreyDark]);
    
    m_detailQuad = m_detailSprites[DetailSprite::GameRules];
    const auto bounds = m_detailSprites[DetailSprite::GameRules].getTextureBounds();
    m_detailQuad.setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
    m_detailQuad.setPosition({ texSize.x / 2.f, texSize.y - (bounds.height - 101.f) });;
    m_detailQuad.draw();
    
    //list the different rule types / description
    m_uiText.setPosition({ texSize.x / 2.f, texSize.y - 16.f });
    m_uiText.setString(ScoreTypes[m_sharedData.scoreType]);
    m_uiText.draw();

    m_infoText.setString(RuleDescriptions[m_sharedData.scoreType]);
    auto tWidth = m_infoText.getLocalBounds().width;
    m_infoText.setPosition({ std::round((texSize.x - tWidth) / 2.f), texSize.y - 39.f});
    m_infoText.draw();

    //list the current gimme selection
    m_uiText.setPosition({ texSize.x / 2.f, texSize.y - 120.f });
    m_uiText.setString("Gimme Type");
    m_uiText.draw();

    m_infoText.setPosition({ texSize.x / 2.f, texSize.y - 138.f });
    m_infoText.setString(GimmeString[m_sharedData.gimmeRadius]);
    m_infoText.setAlignment(cro::SimpleText::Alignment::Centre);
    m_infoText.draw();


    //list other items players won't see when not hosting
    cro::String str = m_sharedData.fastCPU ? EmCheck : EmCross;
    str += " Skip CPU\n";
    str += m_ruleMods[RuleMod::Snek] ? EmCheck : EmCross;
    str += " Snek\n";
    str += m_ruleMods[RuleMod::BigBalls] ? EmCheck : EmCross;
    str += " Big Balls\n";
    str += m_ruleMods[RuleMod::NoAssist] ? EmCross : EmCheck;
    str += " Allow Assists";

    m_infoText.setString(str);
    tWidth = m_infoText.getLocalBounds().width;
    m_infoText.setPosition({ std::round((texSize.x - tWidth) / 2.f), texSize.y - 165.f });
    m_infoText.setAlignment(cro::SimpleText::Alignment::Left);
    m_infoText.draw();

    //show a message if the player count doesn't match
    //the selected game mode
    m_ruleViolation.clear();
    m_uiText.setPosition({ texSize.x / 2.f, 15.f });
    m_uiText.setFillColour(CD32::Colours[CD32::Red]);
    if (m_menuState.m_connectedPlayerCount < ScoreType::MinPlayerCount[m_sharedData.scoreType]
        || m_menuState.m_connectedPlayerCount > ScoreType::MaxPlayerCount[m_sharedData.scoreType])
    {
        if (m_menuState.m_connectedPlayerCount < ScoreType::MinPlayerCount[m_sharedData.scoreType])
        {
            m_uiText.setString(MinPlayerWarning);
            m_ruleViolation = MinPlayerWarning;
        }
        else
        {
            m_uiText.setString(MaxPlayerWarning);
            m_ruleViolation = MaxPlayerWarning;
        }
        m_uiText.draw();
    }
    else if (m_sharedData.teamMode && !ScoreType::CanTeamPlay[m_sharedData.scoreType])
    {
        m_ruleViolation = NoTeamplayWarning;
        m_uiText.setString(NoTeamplayWarning);
        m_uiText.draw();
    }
    m_uiText.setFillColour(TextNormalColour);


    //bottom border
    const float Width = texSize.x;
    const float Bottom = 12.f;
    m_detailArray.setVertexData({
        cro::Vertex2D(glm::vec2(0.f, Bottom), CD32::Colours[CD32::Olive]),
        cro::Vertex2D(glm::vec2(0.f), CD32::Colours[CD32::Olive]),
        cro::Vertex2D(glm::vec2(Width, Bottom), CD32::Colours[CD32::Olive]),
        cro::Vertex2D(glm::vec2(Width, Bottom), CD32::Colours[CD32::Olive]),
        cro::Vertex2D(glm::vec2(0.f), CD32::Colours[CD32::Olive]),
        cro::Vertex2D(glm::vec2(Width, 0.f), CD32::Colours[CD32::Olive]),

        cro::Vertex2D(glm::vec2(0.f, Bottom - 1.f), CD32::Colours[CD32::Brown]),
        cro::Vertex2D(glm::vec2(0.f), CD32::Colours[CD32::Brown]),
        cro::Vertex2D(glm::vec2(Width, Bottom - 1.f), CD32::Colours[CD32::Brown]),
        cro::Vertex2D(glm::vec2(Width, Bottom - 1.f), CD32::Colours[CD32::Brown]),
        cro::Vertex2D(glm::vec2(0.f), CD32::Colours[CD32::Brown]),
        cro::Vertex2D(glm::vec2(Width, 0.f), CD32::Colours[CD32::Brown]),

        });
    m_detailArray.setPosition({ 0.f, 0.f });
    m_detailArray.draw();


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
        else
        {
            return;
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
    const float TextureHeight = static_cast<float>(m_detailTextures[TabID::Scores].getSize().y) - 24.f;
    static constexpr float RankSpacing = -14.f;

    m_uiText.setString("Connected Clients");
    m_uiText.setAlignment(cro::SimpleText::Alignment::Centre);
    m_uiText.setPosition({TextureWidth / 2.f, TextureHeight + 14.f});
    m_uiText.draw();

    m_detailQuad.setOrigin({ 0.f, 0.f });

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
            m_detailQuad = m_menuState.m_sprites[SpriteID::LevelBadge];
            m_detailQuad.setTextureRect(m_menuState.m_sprites[SpriteID::LevelBadge].getAnimations()[index].frames[0].frame);
            m_detailQuad.setScale(glm::vec2(1.f));
            m_detailQuad.setPosition(m_infoText.getPosition() + glm::vec2(-18.f, -5.f));
            m_detailQuad.draw();

            //avatar icon
            const cro::FloatRect bounds = { 0.f, LabelTextureSize.y - (LabelIconSize.y * 4.f), LabelIconSize.x, LabelIconSize.y };
            m_detailQuad.setTexture(m_sharedData.nameTextures[h].getTexture());
            m_detailQuad.setTextureRect(bounds);
            m_detailQuad.setPosition(m_infoText.getPosition() + glm::vec2(-62.f, -4.f));
            m_detailQuad.setScale(glm::vec2(0.2f)); //hmm this mangles things even more when scaled up - but then 0.2 isn't a multiple of view scales anyway...
            m_detailQuad.draw();

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



    //bottom border - TODO this needs to make sure a full server doesn't get clipped by this
    const float Width = TextureWidth;
    const float Bottom = 10.f;
    m_detailArray.setVertexData({
        cro::Vertex2D(glm::vec2(0.f, Bottom), CD32::Colours[CD32::Olive]),
        cro::Vertex2D(glm::vec2(0.f), CD32::Colours[CD32::Olive]),
        cro::Vertex2D(glm::vec2(Width, Bottom), CD32::Colours[CD32::Olive]),
        cro::Vertex2D(glm::vec2(Width, Bottom), CD32::Colours[CD32::Olive]),
        cro::Vertex2D(glm::vec2(0.f), CD32::Colours[CD32::Olive]),
        cro::Vertex2D(glm::vec2(Width, 0.f), CD32::Colours[CD32::Olive]),

        cro::Vertex2D(glm::vec2(0.f, Bottom - 1.f), CD32::Colours[CD32::Brown]),
        cro::Vertex2D(glm::vec2(0.f), CD32::Colours[CD32::Brown]),
        cro::Vertex2D(glm::vec2(Width, Bottom - 1.f), CD32::Colours[CD32::Brown]),
        cro::Vertex2D(glm::vec2(Width, Bottom - 1.f), CD32::Colours[CD32::Brown]),
        cro::Vertex2D(glm::vec2(0.f), CD32::Colours[CD32::Brown]),
        cro::Vertex2D(glm::vec2(Width, 0.f), CD32::Colours[CD32::Brown]),

        });
    m_detailArray.setPosition({ 0.f, 0.f });
    m_detailArray.draw();

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

                //we have to call this a second time to make sure the
                //callbacks are run and approriate tab details are hidden
                m_uiLayout.activateTab(m_uiLayout.tabBar.activeIndex);

                e.getComponent<cro::Callback>().active = false;
                m_menuState.m_uiScene.destroyEntity(e);
            };

        lastWindowSize = newSize;
    }
}