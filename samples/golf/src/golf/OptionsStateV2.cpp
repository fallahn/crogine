/*-----------------------------------------------------------------------

Matt Marchant 2025 - 2026
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

#include "OptionsStateV2.hpp"
#include "SharedStateData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "MessageIDs.hpp"
#include "../GolfGame.hpp"
#include "../WebsocketServer.hpp"

#include <Achievements.hpp>
#include <AchievementStrings.hpp>
#include <Social.hpp>

#include <crogine/audio/AudioDevice.hpp>
#include <crogine/core/Window.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/graphics/SimpleText.hpp>
#include <crogine/graphics/SimpleVertexArray.hpp>
#include <crogine/graphics/Shape2D.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/UIElement.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>

#include <crogine/ecs/systems/UIElementSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>

#include <crogine/util/Easings.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/OpenGL.hpp>

#include <filesystem>

namespace
{
    //TODO move this to an inline file or something
    //as it's also repeated in ScrubConsts.hpp
    //xbox
    static constexpr inline std::uint32_t ButtonLT = 0x2196;
    static constexpr inline std::uint32_t ButtonRT = 0x2197;
    static constexpr inline std::uint32_t ButtonLB = 0x2198;
    static constexpr inline std::uint32_t ButtonRB = 0x2199;
    static constexpr inline std::uint32_t ButtonX = 0x21D0;
    static constexpr inline std::uint32_t ButtonY = 0x21D1;
    static constexpr inline std::uint32_t ButtonB = 0x21D2;
    static constexpr inline std::uint32_t ButtonA = 0x21D3;
    static constexpr inline std::uint32_t ButtonStart = 0x21FB;


    //ps
    static constexpr inline std::uint32_t ButtonL1 = 0x21B0;
    static constexpr inline std::uint32_t ButtonR1 = 0x21B1;
    static constexpr inline std::uint32_t ButtonL2 = 0x21B2;
    static constexpr inline std::uint32_t ButtonR2 = 0x21B3;
    static constexpr inline std::uint32_t ButtonSquare = 0x21E0;
    static constexpr inline std::uint32_t ButtonTriangle = 0x21E1;
    static constexpr inline std::uint32_t ButtonCircle = 0x21E2;
    static constexpr inline std::uint32_t ButtonCross = 0x21E3;
    static constexpr inline std::uint32_t ButtonOption = 0x21E8;


    //static const cro::String XboxInfo = cro::String(ButtonX) + " Show Credits   " + cro::String(ButtonY) + " How To Play   " + cro::String(ButtonB) + " Close";
    //static const cro::String PSInfo = cro::String(ButtonSquare) + " Show Credits   " + cro::String(ButtonCross) + " How To Play   " + cro::String(ButtonCircle) + " Close";
    static const cro::String KeyInfo = "LCtrl - Show Credits   LAlt - How To Play   ESC - Close";

    constexpr std::array<std::size_t, 9u> AAIndexMap =
    {
        0,
        0,
        1,
        0,
        2,
        0,
        0,
        0,
        3
    };
    constexpr std::array<std::uint32_t, 4u> AASamples =
    {
        0,2,4,8
    };

    const std::array ItemLabels =
    {
        "Settings", "Keyboard", "Controller",
        "Graphics", "Audio", "Achievements",
        "Stats"
    };

    constexpr auto BackgroundDark = cro::Colour(0xc8b89faf);
    constexpr auto BackgroundYellow = cro::Colour(0xf2cf5caf);
    constexpr auto BackgroundRed = cro::Colour(0xb83530af);

    static constexpr cro::Time RepeatTimeLong = cro::seconds(0.5f);
    static constexpr cro::Time RepeatTimeShort = cro::seconds(0.05f);

    bool audioHackDone = false;

    glm::uvec2 lastWindowSize = { 0u,0u };
}

using namespace UI;

OptionsStateV2::OptionsStateV2(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State        (ss, ctx),
    m_scene             (ctx.appInstance.getMessageBus(), 192),
    m_sharedData        (sd),
    m_uiLayout          (TabID::Count, sd),
    m_keybindIndex      (-1),
    m_keybindItemIndex  (-1)
{
    //sigh, the window resized event might not necessarily mean
    //that the window resized (I know, I know,) so we track the
    //previous size and only update the layout when we need to
    lastWindowSize = cro::App::getWindow().getSize();
    
    ctx.mainWindow.setMouseCaptured(false);

    m_flagPreview.init(sd.flagPath);
    m_flagPreview.setText(m_sharedData.flagText);

    std::fill(m_controllerMasks.begin(), m_controllerMasks.end(), 0);
    std::fill(m_controllerPrevMasks.begin(), m_controllerPrevMasks.end(), 0);

    loadAssets();
    buildScene();
}

//public
bool OptionsStateV2::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse
        || m_rootNode.getComponent<cro::Callback>().active)
    {
        return false;
    }


    //we MUST be able to cancel keybinds with a controller!
    if (m_keybindIndex != -1)
    {
        if (evt.type == SDL_KEYUP)
        {
            updateKeybind(evt.key.keysym.sym);
        }
        else if (evt.type == SDL_CONTROLLERBUTTONUP)
        {
            if (evt.cbutton.button == cro::GameController::ButtonB)
            {
                cancelKeybind();
            }
        }

        return false;
    }


    //we need to refresh the audio device display when dis/re connect
    //WARNING we're indexing the item directly!
    if (evt.type == SDL_AUDIODEVICEADDED
        || evt.type == SDL_AUDIODEVICEREMOVED)
    {
        refreshAudioDevices(m_uiLayout.menuLayout.items[TabID::Audio][1]);
    }

    const auto setActiveInput =
        [&](bool mouse, std::int32_t controllerIndex)
        {
            if (mouse)
            {
                m_infoString.getComponent<cro::Text>().setString(KeyInfo); //garbled font bug strikes again!!
                m_infoString.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
                m_infoSprite.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
                m_sharedData.activeInput = SharedStateData::ActiveInput::Keyboard;

                m_uiLayout.tabBar.navLeft.getComponent<cro::Text>().setString("< " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::PrevClub]));
                m_uiLayout.tabBar.navRight.getComponent<cro::Text>().setString(cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::NextClub]) + " >");

                m_uiLayout.tabBar.navLeftSprite.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
                m_uiLayout.tabBar.navRightSprite.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);

                const auto viewScale = cro::UIElementSystem::getViewScale();
                const auto charSize = static_cast<std::uint32_t>((UITextSize) * viewScale);
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

                /*const auto viewScale = cro::UIElementSystem::getViewScale();
                const auto charSize = (LabelTextSize * 2) * viewScale;
                m_uiLayout.tabBar.navLeft.getComponent<cro::Text>().setCharacterSize(charSize);
                m_uiLayout.tabBar.navLeft.getComponent<cro::UIElement>().characterSize = LabelTextSize * 2;

                m_uiLayout.tabBar.navRight.getComponent<cro::Text>().setCharacterSize(charSize);
                m_uiLayout.tabBar.navRight.getComponent<cro::UIElement>().characterSize = LabelTextSize * 2;*/
            }
            cro::App::getWindow().setMouseCaptured(!mouse);
        };

    const auto showHelp = 
        [&]()
        {
            m_sharedData.showHelp = true;
            playSound(MenuSoundEvent::Activate);
        };

    const auto showCredits =
        [&]() 
        {
            requestStackPush(StateID::Credits);
            playSound(MenuSoundEvent::Activate);
        };

    if (evt.type == SDL_KEYUP)
    {
        setActiveInput(true, 0);

        if (evt.key.keysym.sym == SDLK_BACKSPACE
            || evt.key.keysym.sym == SDLK_ESCAPE)
        {
            quitState();
            return false;
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::NextClub])
        {
            m_uiLayout.nextTab();
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::PrevClub])
        {
            m_uiLayout.prevTab();
        }

        //done on key down evet for repeat when held
        /*else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Down]
            || evt.key.keysym.sym == SDLK_DOWN)
        {
            m_uiLayout.nextItem();
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Up]
            || evt.key.keysym.sym == SDLK_UP)
        {
            m_uiLayout.prevItem();
        }*/
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Action]
            || evt.key.keysym.sym == SDLK_RETURN)
        {
            m_uiLayout.activate();
        }

        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_LCTRL:
            showCredits();
            break;
        case SDLK_LALT:
            showHelp();
            break;
        }

    }
    else if (evt.type == SDL_KEYDOWN)
    {
        setActiveInput(true, 0);

        //do this here to take advantage of key repeat
        if (evt.key.keysym.sym == SDLK_DOWN)
        {
            m_uiLayout.nextItem();
        }
        else if (evt.key.keysym.sym == SDLK_UP)
        {
            m_uiLayout.prevItem();
        }
        else if (evt.key.keysym.sym == SDLK_LEFT)
        {
            m_uiLayout.activateLeft();
        }
        else if (evt.key.keysym.sym == SDLK_RIGHT)
        {
            m_uiLayout.activateRight();
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONDOWN)
    {
        const auto controllerID = cro::GameController::controllerID(evt.cbutton.which);
        setActiveInput(false, controllerID);

        switch (evt.cbutton.button)
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
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        switch (evt.cbutton.button)
        {
        default: break;
        //case cro::GameController::DPadLeft:
        //    m_uiLayout.activateLeft();
        //    break;
        //case cro::GameController::DPadRight:
        //    m_uiLayout.activateRight();
        //    break;
        case cro::GameController::ButtonLeftShoulder:
            m_uiLayout.prevTab();
            break;
        case cro::GameController::ButtonRightShoulder:
            m_uiLayout.nextTab();
            break;
        case cro::GameController::ButtonX:
            showCredits();
            break;
        case cro::GameController::ButtonY:
            showHelp();
            break;
        case cro::GameController::ButtonA:
            m_uiLayout.activate();
            break;
        case cro::GameController::ButtonB:
            quitState();
            return false;
        }
    }

    else if (evt.type == SDL_MOUSEBUTTONUP)
    {
        if (evt.button.button == SDL_BUTTON_LEFT)
        {
            m_uiLayout.doMouseClick({ evt.motion.x, evt.motion.y }, m_scene.getActiveCamera().getComponent<cro::Camera>());
        }
        else if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            quitState();
            return false;
        }
    }

    else if (evt.type == SDL_MOUSEMOTION)
    {
        setActiveInput(true, 0);

        glm::vec2 pos(evt.motion.x, cro::App::getWindow().getSize().y - evt.motion.y);
        m_uiLayout.checkMouseOver(pos);
    }
    else if (evt.type == SDL_CONTROLLERAXISMOTION)
    {
        constexpr std::int16_t Threshold = std::numeric_limits<std::int16_t>::max() / 2;// cro::GameController::LeftThumbDeadZone * 2;// 15000;
        const auto controllerID = cro::GameController::controllerID(evt.caxis.which);
        
        if (std::abs(evt.caxis.value) > Threshold)
        {
            setActiveInput(false, controllerID);
            if (controllerID < 4)
            {
                m_activityColours[controllerID] = CD32::Colours[CD32::Red];
            }
        }
        

        if (controllerID != -1
            && controllerID < 4)
        {
            switch (evt.caxis.axis)
            {
            default: break;
            case SDL_CONTROLLER_AXIS_LEFTX:
                if (evt.caxis.value > Threshold)
                {
                    //right
                    m_controllerMasks[controllerID] |= InputFlag::Right;
                    m_controllerMasks[controllerID] &= ~InputFlag::Left;
                }
                else if (evt.caxis.value < -Threshold)
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
            case SDL_CONTROLLER_AXIS_LEFTY:
                if (evt.caxis.value > Threshold)
                {
                    //down
                    m_controllerMasks[controllerID] |= InputFlag::Down;
                    m_controllerMasks[controllerID] &= ~InputFlag::Up;
                }
                else if (evt.caxis.value < -Threshold)
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
    else if (evt.type == SDL_MOUSEWHEEL)
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

    else if (evt.type == SDL_CONTROLLERDEVICEADDED
        || evt.type == SDL_CONTROLLERDEVICEREMOVED)
    {
        //refreshControllerDevices();
        //*sigh* the names aren't updated until AFTER the event
        //so we have to delay a frame or 2.
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<std::int32_t>(2);
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float)
            {
                auto& c = e.getComponent<cro::Callback>().getUserData<std::int32_t>();
                c--;

                if (c == 0)
                {
                    refreshControllerDevices();

                    e.getComponent<cro::Callback>().active = false;
                    m_scene.destroyEntity(e);
                }
            };

        //make sure to reset all timers etc
        std::fill(m_controllerMasks.begin(), m_controllerMasks.end(), 0);
        std::fill(m_controllerPrevMasks.begin(), m_controllerMasks.end(), 0);

        for (auto i = 0; i < 4; ++i)
        {
            resetRepeatTimer(i, RepeatTimeLong);
        }
    }


    m_scene.forwardEvent(evt);
    return false;
}

void OptionsStateV2::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            if (const auto newSize = glm::uvec2(data.data0, data.data1);
                newSize != lastWindowSize)
            {
                //hack to force the texture to resize properly
                m_uiLayout.menuLayout.texture.create(1, 1, false);
                m_uiLayout.updateTabBar();

                //realigns the current menu to the new screen size
                cro::Entity entity = m_scene.createEntity();
                entity.addComponent<cro::Callback>().active = true;
                entity.getComponent<cro::Callback>().function =
                    [&](cro::Entity e, float)
                    {
                        m_uiLayout.activateTab(m_uiLayout.tabBar.activeIndex);

                        e.getComponent<cro::Callback>().active = false;
                        m_scene.destroyEntity(e);
                    };

                lastWindowSize = newSize;
            }
        }
    }
    m_scene.forwardMessage(msg);
}

bool OptionsStateV2::simulate(float dt)
{
    m_uiLayout.scrollToTarget(dt);

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
            /*|| (m_controllerMasks[i] & InputFlag::Down)*/)
        {
            if (m_inputRepeatClocks[i].elapsed() > m_repeatTimes[i])
            {
                m_uiLayout.activateLeft();
                resetRepeatTimer(i, RepeatTimeShort);
            }
        }

        if (cro::GameController::isButtonPressed(i, cro::GameController::DPadRight)
            /*|| (m_controllerMasks[i] & InputFlag::Up)*/)
        {
            if (m_inputRepeatClocks[i].elapsed() > m_repeatTimes[i])
            {
                m_uiLayout.activateRight();
                resetRepeatTimer(i, RepeatTimeShort);
            }
        }
    }

    m_scene.simulate(dt);
    return true;
}

void OptionsStateV2::render()
{
    m_scene.render();
}

//private
void OptionsStateV2::loadAssets()
{
    m_uiLayout.loadAssets(*m_sharedData.sharedResources);

    cro::Image img;
    img.create(1, 1, cro::Colour::White);
    m_colourPreview.loadFromImage(img);

    cro::SpriteSheet spriteSheet;
    if (spriteSheet.loadFromFile("assets/golf/sprites/options_buttons.spt", m_sharedData.sharedResources->textures))
    {
        m_uiLayout.tabBar.items[TabID::Settings].sprite = spriteSheet.getSprite("settings_icon");
        m_uiLayout.tabBar.items[TabID::Display].sprite = spriteSheet.getSprite("graphics_icon");
        m_uiLayout.tabBar.items[TabID::Keyboard].sprite = spriteSheet.getSprite("keyboard_icon");

        m_optionIcons[OptionIcon::Warning] = spriteSheet.getSprite("warning_icon");
        m_optionIcons[OptionIcon::SteamIcon] = spriteSheet.getSprite("workshop_button");
    }

    if (spriteSheet.loadFromFile("assets/golf/sprites/options_images.spt", m_sharedData.sharedResources->textures))
    {
        m_optionIcons[OptionIcon::GridDensity] = spriteSheet.getSprite("grid_density");
        m_optionIcons[OptionIcon::BeaconColour] = spriteSheet.getSprite("beacon_colour");
        m_optionIcons[OptionIcon::HighContrast] = spriteSheet.getSprite("high_contrast");
        m_optionIcons[OptionIcon::LargePower] = spriteSheet.getSprite("large_power");
        m_optionIcons[OptionIcon::DecimatePower] = spriteSheet.getSprite("decimate_power");
        m_optionIcons[OptionIcon::WidgetSpeed] = spriteSheet.getSprite("widget_speed");
        m_optionIcons[OptionIcon::PuttAssist] = spriteSheet.getSprite("putt_assist");
        m_optionIcons[OptionIcon::BallTrail] = spriteSheet.getSprite("ball_trail");
        m_optionIcons[OptionIcon::TeeMarker] = spriteSheet.getSprite("tee_marker");
        m_optionIcons[OptionIcon::ZoomFlight] = spriteSheet.getSprite("zoom_flight");
        m_optionIcons[OptionIcon::PuttFollow] = spriteSheet.getSprite("putt_follow");
        m_optionIcons[OptionIcon::RangeIndicator] = spriteSheet.getSprite("range_indicator");
    }
}

void OptionsStateV2::buildScene()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::UIElementSystem>(mb);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::SpriteSystem2D>(mb);
    m_scene.addSystem<cro::TextSystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::RenderSystem2D>(mb);
    m_scene.addSystem<cro::AudioPlayerSystem>(mb);



    struct RootCallbackData final
    {
        enum
        {
            FadeIn, FadeOut
        }state = FadeIn;
        float currTime = 0.f;
    };

    auto rootNode = m_scene.createEntity();
    rootNode.addComponent<cro::Transform>();
    rootNode.addComponent<cro::Callback>().active = true;
    rootNode.getComponent<cro::Callback>().setUserData<RootCallbackData>();
    rootNode.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& [state, currTime] = e.getComponent<cro::Callback>().getUserData<RootCallbackData>();

        switch (state)
        {
        default: break;
        case RootCallbackData::FadeIn:
            currTime = std::min(1.f, currTime + (dt * 2.f));
            e.getComponent<cro::Transform>().setScale(glm::vec2(cro::Util::Easing::easeOutQuint(currTime)));
            if (currTime == 1)
            {
                state = RootCallbackData::FadeOut;
                e.getComponent<cro::Callback>().active = false;
            }
            break;
        case RootCallbackData::FadeOut:
            currTime = std::max(0.f, currTime - (dt * 2.f));
            e.getComponent<cro::Transform>().setScale(glm::vec2(cro::Util::Easing::easeOutQuint(currTime)));
            if (currTime == 0)
            {
                state = RootCallbackData::FadeIn;
                e.getComponent<cro::Callback>().active = false;
                requestStackPop();            
            }
            break;
        }

    };

    m_rootNode = rootNode;


    //quad to darken the screen
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.4f });
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-0.5f, 0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(-0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.5f, -0.5f), cro::Colour::Black)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, rootNode](cro::Entity e, float)
    {
        auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
        e.getComponent<cro::Transform>().setScale(size);
        e.getComponent<cro::Transform>().setPosition(size / 2.f);

        auto scale = rootNode.getComponent<cro::Transform>().getScale().x;
        scale = std::min(1.f, scale);

        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        for (auto& v : verts)
        {
            v.colour.setAlpha(BackgroundAlpha * scale);
        }
    };

   

    //tab bar - we only create here, cachedPush() will update the drawable
    m_uiLayout.tabBar.background = m_scene.createEntity();
    m_uiLayout.tabBar.background.addComponent<cro::Transform>();
    m_uiLayout.tabBar.background.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    m_uiLayout.tabBar.background.getComponent<cro::Drawable2D>().setTexture(m_uiLayout.uiTexture);
    m_uiLayout.tabBar.background.addComponent<cro::UIElement>(cro::UIElement::Position, true);
    m_uiLayout.tabBar.background.getComponent<cro::UIElement>().relativePosition = { -0.5f, 0.5f };
    m_uiLayout.tabBar.background.getComponent<cro::UIElement>().absolutePosition = { 0.f, -(TabBarHeight * 2.f) };
    rootNode.getComponent<cro::Transform>().addChild(m_uiLayout.tabBar.background.getComponent<cro::Transform>());

    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info); 
    const float Spacing = 1.f / (TabID::Count + 1); //leave equivalent of half a tab either end
    for (auto i = 0; i < TabID::Count; ++i)
    {
        auto& item = m_uiLayout.tabBar.items[i];
        item.text = m_scene.createEntity();
        item.text.addComponent<cro::Transform>();
        item.text.addComponent<cro::Drawable2D>();
        item.text.addComponent<cro::Text>(smallFont).setString(ItemLabels[i]);
        item.text.getComponent<cro::Text>().setFillColour(TextNormalColour);
        item.text.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);

        auto& uiElement = item.text.addComponent<cro::UIElement>(cro::UIElement::Text, true);
        uiElement.characterSize = InfoTextSize;
        uiElement.depth = 0.1f;
        const float offset = (Spacing/* * 1.5f*/) + (Spacing * i);
        uiElement.resizeCallback = 
            [&, offset](cro::Entity e)
            {
                const auto x = std::ceil((static_cast<float>(cro::App::getWindow().getSize().x) / cro::UIElementSystem::getViewScale()) * offset) + 2.f;
                const auto y = 12.f;
                e.getComponent<cro::UIElement>().absolutePosition = { x,y };
            };

        m_uiLayout.tabBar.background.getComponent<cro::Transform>().addChild(item.text.getComponent<cro::Transform>());
    }

    const auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    entity = m_scene.createEntity();
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
            const auto y = 14.f;
            e.getComponent<cro::UIElement>().absolutePosition = { x,y };
        };
    m_uiLayout.tabBar.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_uiLayout.tabBar.navLeft = entity;

    entity = m_scene.createEntity();
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
            const auto offset = (Spacing * m_uiLayout.tabBar.items.size()) + (Spacing * 0.75f);
            const auto x = std::floor((static_cast<float>(cro::App::getWindow().getSize().x) / cro::UIElementSystem::getViewScale()) * offset);
            const auto y = 14.f;
            e.getComponent<cro::UIElement>().absolutePosition = { x,y };
        };
    m_uiLayout.tabBar.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_uiLayout.tabBar.navRight = entity;


    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/options_buttons.spt", m_sharedData.sharedResources->textures);
    m_uiLayout.tabBar.navLeftRects[0] = spriteSheet.getSprite("l1").getTextureRect();
    m_uiLayout.tabBar.navLeftRects[1] = spriteSheet.getSprite("lb").getTextureRect();

    m_uiLayout.tabBar.navRightRects[0] = spriteSheet.getSprite("r1").getTextureRect();
    m_uiLayout.tabBar.navRightRects[1] = spriteSheet.getSprite("rb").getTextureRect();

    const auto bounds = spriteSheet.getSprite("l1").getTextureBounds();

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), bounds.height / 2.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("lb");
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, true);
    entity.getComponent<cro::UIElement>().depth = 0.1f;
    entity.getComponent<cro::UIElement>().resizeCallback =
        [&, Spacing](cro::Entity e)
        {
            const auto x = std::floor((static_cast<float>(cro::App::getWindow().getSize().x) / cro::UIElementSystem::getViewScale()) * (Spacing / 4.f));
            const auto y = 10.f;
            e.getComponent<cro::UIElement>().absolutePosition = { x,y };
        };
    m_uiLayout.tabBar.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_uiLayout.tabBar.navLeftSprite = entity;


    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), bounds.height / 2.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("rb");
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, true);
    entity.getComponent<cro::UIElement>().depth = 0.1f;
    entity.getComponent<cro::UIElement>().resizeCallback =
        [&, Spacing](cro::Entity e)
        {
            const auto offset = (Spacing * m_uiLayout.tabBar.items.size()) + (Spacing * 0.75f);
            const auto x = std::floor((static_cast<float>(cro::App::getWindow().getSize().x) / cro::UIElementSystem::getViewScale()) * offset);
            const auto y = 10.f;
            e.getComponent<cro::UIElement>().absolutePosition = { x,y };
        };
    m_uiLayout.tabBar.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_uiLayout.tabBar.navRightSprite = entity;

    m_uiLayout.menuLayout.sprite = m_scene.createEntity();
    m_uiLayout.menuLayout.sprite.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.2f });
    m_uiLayout.menuLayout.sprite.addComponent<cro::Drawable2D>();
    m_uiLayout.menuLayout.sprite.addComponent<cro::Sprite>();
    rootNode.getComponent<cro::Transform>().addChild(m_uiLayout.menuLayout.sprite.getComponent<cro::Transform>());

    //details window on right side
    m_uiLayout.detailsPane.root = m_scene.createEntity();
    m_uiLayout.detailsPane.root.addComponent<cro::Transform>();
    m_uiLayout.detailsPane.root.addComponent<cro::UIElement>(cro::UIElement::Position, false);
    m_uiLayout.detailsPane.root.getComponent<cro::UIElement>().relativePosition = { 0.f, 0.f }; //this is set set when updating the active tab, might be right or left aligned
    rootNode.getComponent<cro::Transform>().addChild(m_uiLayout.detailsPane.root.getComponent<cro::Transform>());

    //text
    m_uiLayout.detailsPane.text = m_scene.createEntity();
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
    m_uiLayout.detailsPane.image = m_scene.createEntity();
    m_uiLayout.detailsPane.image.addComponent<cro::Transform>();
    m_uiLayout.detailsPane.image.addComponent<cro::Drawable2D>();
    m_uiLayout.detailsPane.image.addComponent<cro::Sprite>();
    m_uiLayout.detailsPane.image.addComponent<cro::UIElement>(cro::UIElement::Sprite, true);
    m_uiLayout.detailsPane.image.getComponent<cro::UIElement>().absolutePosition = { DetailBackgroundOffset, -10.f };
    m_uiLayout.detailsPane.image.getComponent<cro::UIElement>().depth = 0.2f;
    m_uiLayout.detailsPane.root.getComponent<cro::Transform>().addChild(m_uiLayout.detailsPane.image.getComponent<cro::Transform>());

    //background/9 patch
    m_uiLayout.detailsPane.background = m_scene.createEntity();
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
    m_uiLayout.detailsPane.scrollIcon = m_scene.createEntity();
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
    m_uiLayout.detailsPane.applyButton = m_scene.createEntity();
    m_uiLayout.detailsPane.applyButton.addComponent<cro::Transform>();
    m_uiLayout.detailsPane.applyButton.addComponent<cro::Callback>().active = true;
    m_uiLayout.detailsPane.applyButton.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            e.getComponent<cro::Transform>().setPosition(-(m_uiLayout.detailsPane.backgroundSize / 2.f) * cro::UIElementSystem::getViewScale());
        };
    m_uiLayout.detailsPane.root.getComponent<cro::Transform>().addChild(m_uiLayout.detailsPane.applyButton.getComponent<cro::Transform>());

    entity = m_scene.createEntity();
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


    entity = m_scene.createEntity();
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

    entity = m_scene.createEntity();
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




    //displays a cancel message when a keybind is in progress
    auto msgRoot = m_scene.createEntity();
    msgRoot.addComponent<cro::Transform>();
    msgRoot.addComponent<cro::Callback>().active = true;
    msgRoot.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            const float scale = m_keybindIndex == -1 ? 0.f : 1.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));

            e.getComponent<cro::Transform>().setPosition(-(m_uiLayout.detailsPane.backgroundSize / 2.f) * cro::UIElementSystem::getViewScale());
        };
    m_uiLayout.detailsPane.root.getComponent<cro::Transform>().addChild(msgRoot.getComponent<cro::Transform>());

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("Esc - Cancel");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIElement>(cro::UIElement::Text, true);
    entity.getComponent<cro::UIElement>().characterSize = UITextSize;
    entity.getComponent<cro::UIElement>().depth = 0.2f;
    entity.getComponent<cro::UIElement>().absolutePosition = { 12.f, 12.f };
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            e.getComponent<cro::Drawable2D>().setFacing(
                m_sharedData.activeInput == SharedStateData::ActiveInput::Keyboard ?
                cro::Drawable2D::Facing::Front : cro::Drawable2D::Facing::Back);
        };
    msgRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("cancel_xbox");
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, true);
    entity.getComponent<cro::UIElement>().absolutePosition = { 12.f, 4.f };
    //entity.getComponent<cro::UIElement>().absolutePosition = { 0.f, -8.f };
    entity.getComponent<cro::UIElement>().depth = 0.2f;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            e.getComponent<cro::Drawable2D>().setFacing(
                m_sharedData.activeInput == SharedStateData::ActiveInput::XBox ?
                cro::Drawable2D::Facing::Front : cro::Drawable2D::Facing::Back);
        };
    msgRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("cancel_ps");
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, true);
    //entity.getComponent<cro::UIElement>().absolutePosition = { 0.f, -8.f };
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
    msgRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //menu layouts
    createSettingsItems();
    createKeyboardItems();
    createControllerItems();
    createDisplayItems();
    createAudioItems();
    createAchievementItems();
    createStatItems();

    m_uiLayout.updateTabBar(); //this also updates the menu items


    //info string at the bottom
    entity = m_scene.createEntity();
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

    entity = m_scene.createEntity();
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


    //camera settings
    auto updateView = [&, rootNode](cro::Camera& cam) mutable
    {
        glm::vec2 size(GolfGame::getActiveTarget()->getSize());

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        rootNode.getComponent<cro::Transform>().setPosition(size / 2.f);
        /*auto& tx = m_uiLayout.menuLayout.sprite.getComponent<cro::Transform>();
        auto pos = tx.getPosition();
        pos.x = -(size.x / 2.f);
        pos.y = -(size.y / 2.f);
        tx.setPosition(pos);*/

        m_uiLayout.updateTabBar();
    };

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().resizeCallback = updateView;
    m_scene.setActiveCamera(entity);
    updateView(entity.getComponent<cro::Camera>());
}

void OptionsStateV2::createSettingsItems()
{
    auto* item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Appearance";
    item->displayType = Menu::Item::Heading;
    item->description = "Customise in-game display settings";

    //use flag beacon
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Show Flag Beacon";
    item->description = "Draws a beacon at the pin position, visible from a distance";
    item->selected = 
        [&](const Menu::Item&)
        {
            m_uiLayout.detailsPane.image.getComponent<cro::Sprite>() = m_optionIcons[OptionIcon::BeaconColour];
            m_uiLayout.detailsPane.image.getComponent<cro::Transform>().setOrigin({ m_optionIcons[OptionIcon::BeaconColour].getTextureBounds().width / 2.f, 0.f });
            m_uiLayout.detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
        };
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.showBeacon = i.selectedIndex == 1;
        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = m_sharedData.showBeacon ? 1 : 0;

    //beacon colour
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Beacon Colour";
    item->description = "Choose the colour of the beacon";
    item->selected =
        [&](const Menu::Item&)
        {
            m_uiLayout.detailsPane.image.getComponent<cro::Sprite>() = m_optionIcons[OptionIcon::BeaconColour];
            m_uiLayout.detailsPane.image.getComponent<cro::Transform>().setOrigin({ m_optionIcons[OptionIcon::BeaconColour].getTextureBounds().width / 2.f, 0.f });
            m_uiLayout.detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
        };
    item->activated = 
        [&](Menu::Item& i)
        {
            const float amt = 0.1f * i.selectedIndex;
            m_sharedData.beaconColour = amt;

            //set the preview colour
            i.previewColour = getBeaconColour(m_sharedData.beaconColour);
        };
    item->labels = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" };
    item->selectedIndex = static_cast<std::int32_t>(std::floor(m_sharedData.beaconColour * 9.f));
    item->displayType = Menu::Item::Slider;
    item->previewColour = getBeaconColour(m_sharedData.beaconColour);
    item->texture = &m_colourPreview;
    item->uv = { 0.f, 0.f, 1.f, 1.f };


    //use ball trail
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Show Ball Trail";
    item->description = "Draw a trail behind player's ball when it's in flight";
    item->selected =
        [&](const Menu::Item&)
        {
            m_uiLayout.detailsPane.image.getComponent<cro::Sprite>() = m_optionIcons[OptionIcon::BallTrail];
            m_uiLayout.detailsPane.image.getComponent<cro::Transform>().setOrigin({ m_optionIcons[OptionIcon::BallTrail].getTextureBounds().width / 2.f, 0.f });
            m_uiLayout.detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
        };
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.showBallTrail = i.selectedIndex == 1;
        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = m_sharedData.showBallTrail ? 1 : 0;

    //ball trail colour
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Ball Trail Uses Beacon Colour";
    item->description = "Draws the ball trail with the beacon colour, else draws it white";
    item->selected =
        [&](const Menu::Item&)
        {
            m_uiLayout.detailsPane.image.getComponent<cro::Sprite>() = m_optionIcons[OptionIcon::BallTrail];
            m_uiLayout.detailsPane.image.getComponent<cro::Transform>().setOrigin({ m_optionIcons[OptionIcon::BallTrail].getTextureBounds().width / 2.f, 0.f });
            m_uiLayout.detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
        };
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.trailBeaconColour = i.selectedIndex == 1;
        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = m_sharedData.trailBeaconColour ? 1 : 0;

    //putting grid density
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Putting Grid Density";
    item->description = "Sets the transparency of the putting grid";
    item->selected =
        [&](const Menu::Item&)
        {
            m_uiLayout.detailsPane.image.getComponent<cro::Sprite>() = m_optionIcons[OptionIcon::GridDensity];
            m_uiLayout.detailsPane.image.getComponent<cro::Transform>().setOrigin({ m_optionIcons[OptionIcon::GridDensity].getTextureBounds().width / 2.f, 0.f });
            m_uiLayout.detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
        };
    item->activated = [&](Menu::Item& i)
        {
            const float amt = 0.1f * i.selectedIndex;
            m_sharedData.gridTransparency = amt;
        };
    item->labels = { "0.0", "0.1", "0.2", "0.3", "0.4", "0.5", "0.6", "0.7", "0.8", "0.9", "1.0" };
    item->selectedIndex = static_cast<std::int32_t>(std::floor(m_sharedData.gridTransparency * 10.f));
    item->displayType = Menu::Item::Slider;


    //tee ball colour
    static constexpr std::array ColourIndices =
    {
        CD32::Red, CD32::Yellow, CD32::BlueMid, CD32::BeigeLight,
        CD32::GreenLight, CD32::Orange, CD32::Black
    };
    std::int32_t activeColour = 0;
    if (const auto res = std::find(ColourIndices.cbegin(), ColourIndices.cend(), m_sharedData.teeColour);
        res != ColourIndices.cend())
    {
        activeColour = static_cast<std::int32_t>(std::distance(ColourIndices.cbegin(), res));
    }
    else
    {
        m_sharedData.teeColour = ColourIndices[0];
    }

    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Tee Marker Colour";
    item->description = "Choose the colour of the tee marker. Note that this doesn't affect the tee position, it's purely cosmetic";
    item->selected =
        [&](const Menu::Item&)
        {
            m_uiLayout.detailsPane.image.getComponent<cro::Sprite>() = m_optionIcons[OptionIcon::TeeMarker];
            m_uiLayout.detailsPane.image.getComponent<cro::Transform>().setOrigin({ m_optionIcons[OptionIcon::TeeMarker].getTextureBounds().width / 2.f, 0.f });
            m_uiLayout.detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
        };
    item->activated =
        [&](Menu::Item& i)
        {
            m_sharedData.teeColour = ColourIndices[i.selectedIndex];

            //set the preview colour
            i.previewColour = CD32::Colours[m_sharedData.teeColour];
        };
    item->labels = { "1", "2", "3", "4", "5", "6", "7" };
    item->selectedIndex = activeColour;
    item->displayType = Menu::Item::Slider;
    item->previewColour = CD32::Colours[m_sharedData.teeColour];
    item->texture = &m_colourPreview;
    item->uv = { 0.f, 0.f, 1.f, 1.f };




    //use imperial measurements
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Use Imperial Measurements";
    item->description = "Render distances in Yards, Feet and Inches instead of Metres and Centimetres";
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.imperialMeasurements = i.selectedIndex == 1;
        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = m_sharedData.imperialMeasurements ? 1 : 0;

    //large power bar
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Use Large Power Bar";
    item->description = "Draws a larger power bar at the bottom ofthe UI";
    item->selected =
        [&](const Menu::Item&)
        {
            m_uiLayout.detailsPane.image.getComponent<cro::Sprite>() = m_optionIcons[OptionIcon::LargePower];
            m_uiLayout.detailsPane.image.getComponent<cro::Transform>().setOrigin({ m_optionIcons[OptionIcon::LargePower].getTextureBounds().width / 2.f, 0.f });
            m_uiLayout.detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
        };
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.useLargePowerBar = i.selectedIndex == 1;
        };
    item->labels = { "No" , "Yes"  };
    item->selectedIndex = m_sharedData.useLargePowerBar ? 1 : 0;

    //high contrast power bar
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "High Contrast Power Bar";
    item->description = "Draws the power bar with inverted colours";
    item->selected =
        [&](const Menu::Item&)
        {
            m_uiLayout.detailsPane.image.getComponent<cro::Sprite>() = m_optionIcons[OptionIcon::HighContrast];
            m_uiLayout.detailsPane.image.getComponent<cro::Transform>().setOrigin({ m_optionIcons[OptionIcon::HighContrast].getTextureBounds().width / 2.f, 0.f });
            m_uiLayout.detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
        };
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.useContrastPowerBar = i.selectedIndex == 1;
        };
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.useContrastPowerBar ? 1 : 0;


    //decimated power bar
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Decimate Power Bar";
    item->description = "Draws a power bar with 10 segments instead of 8";
    item->selected =
        [&](const Menu::Item&)
        {
            m_uiLayout.detailsPane.image.getComponent<cro::Sprite>() = m_optionIcons[OptionIcon::DecimatePower];
            m_uiLayout.detailsPane.image.getComponent<cro::Transform>().setOrigin({ m_optionIcons[OptionIcon::DecimatePower].getTextureBounds().width / 2.f, 0.f });
            m_uiLayout.detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
        };
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.decimatePowerBar = i.selectedIndex == 1;
        };
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.decimatePowerBar ? 1 : 0;


    //decimalised distances
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Use Decimalised Distances";
    item->description = "Distances are drawn to the nearest 10th of a metre or yard";
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.decimateDistance = i.selectedIndex == 1;
        };
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.decimateDistance ? 1 : 0;


    //monthly rival
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Show Monthly Rival";
    item->description = "Shows the current monthly best on the scoreboard, if available";
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.showRival = i.selectedIndex == 1;
        };
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.showRival ? 1 : 0;


    //follow cam when putting
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Follow Cam When Putting";
    item->description = "The camera follows the ball when putting instead of displaying an overhead view";
    item->selected =
        [&](const Menu::Item&)
        {
            m_uiLayout.detailsPane.image.getComponent<cro::Sprite>() = m_optionIcons[OptionIcon::PuttFollow];
            m_uiLayout.detailsPane.image.getComponent<cro::Transform>().setOrigin({ m_optionIcons[OptionIcon::PuttFollow].getTextureBounds().width / 2.f, 0.f });
            m_uiLayout.detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
        };
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.puttFollowCam = i.selectedIndex == 1;
        };
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.puttFollowCam ? 1 : 0;


    //zoom follow cam
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Zoom Follow Cam";
    item->description = "Zoom the follow cam when the ball is in flight for a closer view";
    item->selected =
        [&](const Menu::Item&)
        {
            m_uiLayout.detailsPane.image.getComponent<cro::Sprite>() = m_optionIcons[OptionIcon::ZoomFlight];
            m_uiLayout.detailsPane.image.getComponent<cro::Transform>().setOrigin({ m_optionIcons[OptionIcon::ZoomFlight].getTextureBounds().width / 2.f, 0.f });
            m_uiLayout.detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
        };
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.zoomFollowCam = i.selectedIndex == 1;
        };
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.zoomFollowCam ? 1 : 0;


    //rotate when aiming
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Rotate When Aiming";
    item->description = "Automatically rotate the player camera when aiming";
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.rotateCamera = i.selectedIndex == 1;
        };
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.rotateCamera ? 1 : 0;

    
    //flag selection
    const auto selectionCallback =
        [&](const Menu::Item& i)
        {
            m_uiLayout.detailsPane.image.getComponent<cro::Sprite>().setTexture(m_flagPreview.getTexure());
            m_uiLayout.detailsPane.image.getComponent<cro::Sprite>().setTextureRect(m_flagPreview.getUV());
            m_uiLayout.detailsPane.image.getComponent<cro::Transform>().setOrigin({ m_flagPreview.getSize().x / 2.f, 0.f });
            m_uiLayout.detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
        };

    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Flag Selection";
#ifdef USE_GNS
    item->description = "More flags are available in the Steam Workshop";
#else
    item->description = "Select the flag's appearance";
#endif

    item->activated = [&](Menu::Item& i)
        {
            //cycle through flags
            m_flagPreview.setIndex(i.selectedIndex);
            m_uiLayout.detailsPane.image.getComponent<cro::Sprite>().setTextureRect(m_flagPreview.getUV());

            m_uiLayout.detailsPane.image.getComponent<cro::Transform>().setOrigin({ m_flagPreview.getSize().x / 2.f, 0.f });
            m_sharedData.flagPath = m_flagPreview.getPath();
        };

    for (auto i = 0; i < m_flagPreview.getCount(); ++i)
    {
        item->labels.push_back("Flag " + std::to_string(i));
    }
    item->selectedIndex = m_flagPreview.getIndex();
    item->selected = selectionCallback;

    //flag text type
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Flag Text";
    item->description = "Choose how text is displayed on the flag";
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.flagText = i.selectedIndex;
            m_flagPreview.setText(m_sharedData.flagText);
            m_uiLayout.detailsPane.image.getComponent<cro::Sprite>().setTexture(m_flagPreview.getTexure());
            m_uiLayout.detailsPane.image.getComponent<cro::Sprite>().setTextureRect(m_flagPreview.getUV());
        };
    item->labels = { "None" , "Black", "White"};
    item->selectedIndex = m_sharedData.flagText;
    item->selected = selectionCallback;


    //post FX selection (none as an option)
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Post Process";
    item->description = "Choose a visual effect";
    item->activated = [&](Menu::Item& i)
        {
            //cycle through effects
            switch (i.selectedIndex)
            {
            default: break;
            case 0:
            case 1:
            case 2:
            case 3:
            case 4:
                m_sharedData.usePostProcess = true;
                m_sharedData.postProcessIndex = i.selectedIndex;
                {
                    auto* msg = postMessage<SystemEvent>(cl::MessageID::SystemMessage);
                    msg->type = SystemEvent::PostProcessIndexChanged;
                }
                break;
            case ShaderNames.size():
            {
                //set to on then the message toggles to off...
                m_sharedData.usePostProcess = true;
                auto* msg = postMessage<SystemEvent>(cl::MessageID::SystemMessage);
                msg->type = SystemEvent::PostProcessToggled;
            }
                break;
            }
        };

    for (const auto& name : ShaderNames)
    {
        item->labels.push_back(name);
    }
    item->labels.push_back("None");
    item->selectedIndex = m_sharedData.usePostProcess ? m_sharedData.postProcessIndex : static_cast<std::int32_t>(ShaderNames.size());



    //lens flare
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Show Lens Flare";
    item->description = "Display a lens flare effect in sunny weather";
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.useLensFlare = i.selectedIndex == 1;
        };
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.useLensFlare ? 1 : 0;

    //reduced motion transition
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Reduced Motion Transition";
    item->description = "Hides the hole transition behind a loading screen to reduce motion sensitivity";
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.miniLoadingScreen = i.selectedIndex == 1;
        };
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.miniLoadingScreen ? 1 : 0;



    //----------control settings--------------//
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Control Settings";
    item->displayType = Menu::Item::Heading;
    item->description = "Configure input settings";

    //mouse button for action
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Use Left Mouse as Action Button";
    item->description = "Clicking left mouse button performs the same as the Action button";
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.useMouseAction = i.selectedIndex == 1;
        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = m_sharedData.useMouseAction ? 1 : 0;

    //hold for power
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Hold Action For Power";
    item->description = "Press and hold the Action button to choose swing power instead of the traditional 3-click system";
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.pressHold = i.selectedIndex == 1;
        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = m_sharedData.pressHold ? 1 : 0;


    //measure sensitivity
    static constexpr std::array<float, 6u> SpeedValues = { 0.5f, 1.f, 2.f, 3.f, 4.f, 5.f };
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Measure Sensitivity";
    item->description = "Sets the speed of the Measure Widget when putting";
    item->selected =
        [&](const Menu::Item&)
        {
            m_uiLayout.detailsPane.image.getComponent<cro::Sprite>() = m_optionIcons[OptionIcon::WidgetSpeed];
            m_uiLayout.detailsPane.image.getComponent<cro::Transform>().setOrigin({ m_optionIcons[OptionIcon::WidgetSpeed].getTextureBounds().width / 2.f, 0.f });
            m_uiLayout.detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
        };
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.measureSpeed = SpeedValues[i.selectedIndex];
        };
    item->labels = { "0.5", "1.0", "2.0", "3.0", "4.0", "5.0" };
    item->selectedIndex = (static_cast<std::int32_t>(std::floor(m_sharedData.measureSpeed) * 10.f) / 10);
    item->displayType = Menu::Item::Slider;


    //skip speed
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Shot Animation Skip Speed";
    item->description = "Set the amount of time required to hold the Action button to skip to the end of the current shot";
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.skipSpeed = i.selectedIndex == 1 ? 10.f : 60.f;
        };
    item->labels = { "Normal", "Fast" };
    item->selectedIndex = m_sharedData.skipSpeed == 10.f ? 1 : 0;



    //-------difficulty and behaviour-----//
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Gameplay Settings";
    item->displayType = Menu::Item::Heading;
    item->description = "Configure difficulty and accessibility settings";

    //putt assist
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Use Putting Assist";
    item->description = "Show a small flag above the power bar when putting to estimate the range";
    item->selected =
        [&](const Menu::Item&)
        {
            m_uiLayout.detailsPane.image.getComponent<cro::Sprite>() = m_optionIcons[OptionIcon::PuttAssist];
            m_uiLayout.detailsPane.image.getComponent<cro::Transform>().setOrigin({ m_optionIcons[OptionIcon::PuttAssist].getTextureBounds().width / 2.f, 0.f });
            m_uiLayout.detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
        };
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.showPuttingPower = i.selectedIndex == 1;
        };
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.showPuttingPower ? 1 : 0;
    
    
    //fixed range putter
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Fixed Range Putter";
    item->description = "Fixes the max range of the putter at 10m/33ft";
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.fixedPuttingRange = i.selectedIndex == 1;
        };
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.fixedPuttingRange ? 1 : 0;



    //precise range indicator
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Use Range Indicator Assist";
    item->description = "The Range Indicator for clubs longer than a Pitch Wedge will account for elevation in terrain and wind conditions";
    item->selected =
        [&](const Menu::Item&)
        {
            m_uiLayout.detailsPane.image.getComponent<cro::Sprite>() = m_optionIcons[OptionIcon::RangeIndicator];
            m_uiLayout.detailsPane.image.getComponent<cro::Transform>().setOrigin({ m_optionIcons[OptionIcon::RangeIndicator].getTextureBounds().width / 2.f, 0.f });
            m_uiLayout.detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
        };
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.calculateRange = i.selectedIndex == 0;
            Social::setLeaderboardFilter(Social::LeaderboardFilterValue::NoAssist, i.selectedIndex == 1);
        };
    item->labels = { "Yes" , "No" };
    item->selectedIndex = m_sharedData.calculateRange ? 0 : 1;



    //minimal UI
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Minimal UI";
    item->description = "Increases difficulty by removing most of the UI elements";
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.showMinimap = i.selectedIndex == 0;
        };
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.showMinimap ? 0 : 1;



    //in-game tips
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Display In-Game Tips";
    item->description = "Shows tips when playing on how to best take your shot";
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.showInGameTips = i.selectedIndex == 1;
        };
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.showInGameTips ? 1 : 0;

#ifdef USE_GNS
    //---------leaderboard settings----------//
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Leaderboard Settings";
    item->displayType = Menu::Item::Heading;
    item->description = "Filter leaderboards results to display. Note these filters take effect when the Main Menu is next loaded.";

    //friends only
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Friends Only";
    item->description = "Leaderboards only display results from players on your Steam friends list. Takes effect next time the Main Menu is loaded.";
    item->activated = [&](Menu::Item& i)
        {
            Social::setLeaderboardFilter(Social::LeaderboardFilterValue::FriendsOnly, i.selectedIndex == 1);
        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = Social::getLeaderboardFilter(Social::LeaderboardFilterValue::FriendsOnly) ? 1 : 0;

    //assisted scores
    /*item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "View Assisted Scores";
    item->description = "Leaderboards only display results from players who used the Range Indicator assist, otherwise displays only unassisted scores.";
    item->activated = [&](Menu::Item& i)
        {
            Social::setLeaderboardFilter(Social::LeaderboardFilterValue::NoAssist, i.selectedIndex == 0);
        };
    item->count = 2;
    item->labels = { "No", "Yes" };
    item->selectedIndex = Social::getLeaderboardFilter(Social::LeaderboardFilterValue::NoAssist) ? 1 : 0;*/
#endif


    //----------config settings---------//
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Configuration";
    item->displayType = Menu::Item::Heading;


    //web socket
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Enable Web Socket";
    item->description = "See https://github.com/fallahn/svs for more info";
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.webSocket = i.selectedIndex == 1;

            if (m_sharedData.webSocket)
            {
                if (WebSock::getPort() && WebSock::getPort() != m_sharedData.webPort)
                {
                    WebSock::stop();
                }

                if (!WebSock::isRunning())
                {
                    WebSock::start(m_sharedData.webPort);
                }
            }
            else
            {
                if (WebSock::isRunning())
                {
                    WebSock::stop();
                }
            }
        };
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.webSocket ? 1 : 0;

    //TODO hmmmm we need to be able to set the port...


    //CSV logging
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Log Scores To CSV";
    item->description = "Files are saved to you user directory";
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.logCSV = i.selectedIndex == 1;
        };
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.logCSV ? 1 : 0;


    //disable chat
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Disable Chat";
    item->description = "Removes the in-game chat from multiplayer games";
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.blockChat = i.selectedIndex == 1;
        };
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.blockChat ? 1 : 0;


    //log chat
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Log Chat To File";
    item->description = "Logs in-game multiplayer chat to a text file in your user directory";
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.logChat = i.selectedIndex == 1;
        };
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.logChat ? 1 : 0;


#ifdef USE_GNS
    //enable remote content
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Download Remote Content";
    item->description = "Allow downloading remote content eg Workshop items in multiplayer";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.remoteContent = i.selectedIndex == 1;
        };
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.remoteContent ? 1 : 0;
#endif



    //reset hints
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Reset Hints";
    item->description = "Enable all in-game hints which were previously dismissed";
    item->selected =
        [&](const Menu::Item&)
        {
            m_uiLayout.detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
            m_uiLayout.detailsPane.image.getComponent<cro::Sprite>() = m_uiLayout.tabBar.items[m_uiLayout.tabBar.activeIndex].sprite;
            const auto bounds = m_uiLayout.detailsPane.image.getComponent<cro::Sprite>().getTextureBounds();
            m_uiLayout.detailsPane.image.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, 0.f });

            m_uiLayout.detailsPane.applyButton.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        };
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.showClubUpdate = true;
            m_sharedData.showRosterTip = true;
            m_sharedData.showTutorialTip = true;

            m_uiLayout.detailsPane.text.getComponent<cro::Text>().setString("Tutorials Reset!");
        };
    item->labels = { "OK" };
    item->selectedIndex = 0;


    //reset career
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Reset Career";
    item->description = "Resets all Career progress, preserving any unlocked items";
    item->selected =
        [&](const Menu::Item&)
        {
            m_uiLayout.detailsPane.image.getComponent<cro::Sprite>() = m_optionIcons[OptionIcon::Warning];
            m_uiLayout.detailsPane.image.getComponent<cro::Transform>().setOrigin({ m_optionIcons[OptionIcon::Warning].getTextureBounds().width / 2.f, 0.f });
            m_uiLayout.detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);

            m_uiLayout.detailsPane.applyButton.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        };
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.errorMessage = "reset_career";
            requestStackPush(StateID::MessageOverlay);
        };
    item->labels = { "OK" };
    item->selectedIndex = 0;


    //reset profile
    item = &m_uiLayout.menuLayout.items[TabID::Settings].emplace_back();
    item->title = "Reset Profile";
    item->description = "WARNING Resets all progress and unlocked items!!";
    item->selected =
        [&](const Menu::Item&)
        {
            m_uiLayout.detailsPane.image.getComponent<cro::Sprite>() = m_optionIcons[OptionIcon::Warning];
            m_uiLayout.detailsPane.image.getComponent<cro::Transform>().setOrigin({ m_optionIcons[OptionIcon::Warning].getTextureBounds().width / 2.f, 0.f });
            m_uiLayout.detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);

            m_uiLayout.detailsPane.applyButton.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        };
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.errorMessage = "reset_profile";
            requestStackPush(StateID::MessageOverlay);
        };
    item->labels = { "OK" };
    item->selectedIndex = 0;
}

void OptionsStateV2::createKeyboardItems()
{
    //config
    /*auto* item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Configuration";
    item->displayType = Menu::Item::Heading;*/

    
    ////mouse button for action
    //item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    //item->title = "Use Left Mouse as Action Button";
    //item->description = "Clicking left mouse button performs the same as the Action key";
    //cro::Util::String::wordWrap(item->description, 36);
    //item->activated = [&](Menu::Item& i)
    //    {
    //        m_sharedData.useMouseAction = i.selectedIndex == 0 ? false : true;
    //    };
    //item->count = 2;
    //item->labels = { "No", "Yes" };
    //item->selectedIndex = m_sharedData.useMouseAction ? 1 : 0;

    ////hold for power
    //item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    //item->title = "Hold Action For Power";
    //item->description = "Press and hold the Action key to choose swing power instead of the traditional 3-click system";
    //cro::Util::String::wordWrap(item->description, 36);
    //item->activated = [&](Menu::Item& i)
    //    {
    //        m_sharedData.pressHold = i.selectedIndex == 0 ? false : true;
    //    };
    //item->count = 2;
    //item->labels = { "No", "Yes" };
    //item->selectedIndex = m_sharedData.pressHold ? 1 : 0;


    
    
    //keybinds
    auto* item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Key Bindings";
    item->displayType = Menu::Item::Heading;

    cro::String keybindDesc = "Press Enter to select a new key";


    //prev club
    auto itemIndex = static_cast<std::int32_t>(m_uiLayout.menuLayout.items[TabID::Keyboard].size());
    item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Previous Club";
    item->description = keybindDesc;
    item->activated = [&, itemIndex](Menu::Item& i)
        {
            m_uiLayout.detailsPane.text.getComponent<cro::Text>().setString("Press a key");
            m_keybindIndex = InputBinding::PrevClub;
            m_keybindItemIndex = itemIndex;
        };
    item->labels = { "Key: " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::PrevClub])};
    item->selectedIndex = 0;

    //next club
    itemIndex = static_cast<std::int32_t>(m_uiLayout.menuLayout.items[TabID::Keyboard].size());
    item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Next Club";
    item->description = keybindDesc;
    item->activated = [&, itemIndex](Menu::Item& i)
        {
            m_uiLayout.detailsPane.text.getComponent<cro::Text>().setString("Press a key");
            m_keybindIndex = InputBinding::NextClub;
            m_keybindItemIndex = itemIndex;
        };
    item->labels = { "Key: " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::NextClub]) };
    item->selectedIndex = 0;

    //aim left
    itemIndex = static_cast<std::int32_t>(m_uiLayout.menuLayout.items[TabID::Keyboard].size());
    item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Aim Left";
    item->description = keybindDesc;
    item->activated = [&, itemIndex](Menu::Item& i)
        {
            m_uiLayout.detailsPane.text.getComponent<cro::Text>().setString("Press a key");
            m_keybindIndex = InputBinding::Left;
            m_keybindItemIndex = itemIndex;
        };
    item->labels = { "Key: " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Left]) };
    item->selectedIndex = 0;

    //aim right
    itemIndex = static_cast<std::int32_t>(m_uiLayout.menuLayout.items[TabID::Keyboard].size());
    item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Aim Right";
    item->description = keybindDesc;
    item->activated = [&, itemIndex](Menu::Item& i)
        {
            m_uiLayout.detailsPane.text.getComponent<cro::Text>().setString("Press a key");
            m_keybindIndex = InputBinding::Right;
            m_keybindItemIndex = itemIndex;
        };
    item->labels = { "Key: " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Right]) };
    item->selectedIndex = 0;

    //camera up
    itemIndex = static_cast<std::int32_t>(m_uiLayout.menuLayout.items[TabID::Keyboard].size());
    item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Camera Up";
    item->description = keybindDesc;
    item->activated = [&, itemIndex](Menu::Item& i)
        {
            m_uiLayout.detailsPane.text.getComponent<cro::Text>().setString("Press a key");
            m_keybindIndex = InputBinding::Up;
            m_keybindItemIndex = itemIndex;
        };
    item->labels = { "Key: " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Up]) };
    item->selectedIndex = 0;

    //camera down
    itemIndex = static_cast<std::int32_t>(m_uiLayout.menuLayout.items[TabID::Keyboard].size());
    item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Camera Down";
    item->description = keybindDesc;
    item->activated = [&, itemIndex](Menu::Item& i)
        {
            m_uiLayout.detailsPane.text.getComponent<cro::Text>().setString("Press a key");
            m_keybindIndex = InputBinding::Down;
            m_keybindItemIndex = itemIndex;
        };
    item->labels = { "Key: " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Down]) };
    item->selectedIndex = 0;

    //action
    itemIndex = static_cast<std::int32_t>(m_uiLayout.menuLayout.items[TabID::Keyboard].size());
    item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Action (Take Shot)";
    item->description = keybindDesc;
    item->activated = [&, itemIndex](Menu::Item& i)
        {
            m_uiLayout.detailsPane.text.getComponent<cro::Text>().setString("Press a key");
            m_keybindIndex = InputBinding::Action;
            m_keybindItemIndex = itemIndex;
        };
    item->labels = { "Key: " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Action]) };
    item->selectedIndex = 0;

    //spin menu
    itemIndex = static_cast<std::int32_t>(m_uiLayout.menuLayout.items[TabID::Keyboard].size());
    item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Show Spin Menu";
    item->description = keybindDesc;
    item->activated = [&, itemIndex](Menu::Item& i)
        {
            m_uiLayout.detailsPane.text.getComponent<cro::Text>().setString("Press a key");
            m_keybindIndex = InputBinding::SpinMenu;
            m_keybindItemIndex = itemIndex;
        };
    item->labels = { "Key: " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::SpinMenu]) };
    item->selectedIndex = 0;

    //emote wheel
    itemIndex = static_cast<std::int32_t>(m_uiLayout.menuLayout.items[TabID::Keyboard].size());
    item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Show Emote Wheel";
    item->description = keybindDesc;
    item->activated = [&, itemIndex](Menu::Item& i)
        {
            m_uiLayout.detailsPane.text.getComponent<cro::Text>().setString("Press a key");
            m_keybindIndex = InputBinding::EmoteMenu;
            m_keybindItemIndex = itemIndex;
        };
    item->labels = { "Key: " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::EmoteMenu]) };
    item->selectedIndex = 0;

    //cancel shot
    itemIndex = static_cast<std::int32_t>(m_uiLayout.menuLayout.items[TabID::Keyboard].size());
    item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Cancel Shot In Progress";
    item->description = keybindDesc;
    item->activated = [&, itemIndex](Menu::Item& i)
        {
            m_uiLayout.detailsPane.text.getComponent<cro::Text>().setString("Press a key");
            m_keybindIndex = InputBinding::CancelShot;
            m_keybindItemIndex = itemIndex;
        };
    item->labels = { "Key: " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::CancelShot]) };
    item->selectedIndex = 0;





    //fixed keys
    item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Fixed Keys";
    item->displayType = Menu::Item::Heading;

    item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Measure Putt";
    item->subTitle = "Displays a distance widget to meaure the green when putting";
    item->description = "Key: Number 1 (Top Row)";
    item->displayType = Menu::Item::TextOnly;

    item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Freecam";
    item->subTitle = "Enter freecam / photo mode";
    item->description = "Key: Number 2 (Top Row)";
    item->displayType = Menu::Item::TextOnly;

    item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Rotate Camera Left";
    item->description = "Key: Number 3 (Top Row)";
    item->displayType = Menu::Item::TextOnly;

    item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Rotate Camera Right";
    item->description = "Key: Number 4 (Top Row)";
    item->displayType = Menu::Item::TextOnly;

    item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Zoom Minimap";
    item->description = "Key: Number 5 (Top Row)";
    item->displayType = Menu::Item::TextOnly;

    item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Rotate Camera To Target";
    item->description = "Key: Number 6 (Top Row)";
    item->displayType = Menu::Item::TextOnly;

    item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Show Scores";
    item->description = "Key: Tab";
    item->displayType = Menu::Item::TextOnly;

    item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Open Menu";
    item->description = "Key: Escape";
    item->displayType = Menu::Item::TextOnly;

    item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Toggle Ball Labels";
    item->subTitle = "Show or hide player name labels when putting";
    item->description = "Key: F2";
    item->displayType = Menu::Item::TextOnly;

    item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Toggle UI";
    item->description = "Key: F3";
    item->displayType = Menu::Item::TextOnly;

    item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Toggle Chat";
    item->subTitle = "Show the in-game chat window";
    item->description = "Key: F4";
    item->displayType = Menu::Item::TextOnly;

    item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Take Screenshot";
    item->description = "Key: F5";
    item->displayType = Menu::Item::TextOnly;

    item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Toggle Putting Grid";
    item->description = "Key: F7";
    item->displayType = Menu::Item::TextOnly;

    item = &m_uiLayout.menuLayout.items[TabID::Keyboard].emplace_back();
    item->title = "Toggle Full Screen";
    item->description = "Key: F11";
    item->displayType = Menu::Item::TextOnly;
}

void OptionsStateV2::createControllerItems()
{
    auto ent = m_scene.createEntity();
    ent.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    m_uiLayout.detailsPane.root.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
    m_uiLayout.detailsPane.tabDetails[TabID::Controller] = ent;

    //set detail image based on input activity
    cro::SpriteSheet controllerSprites;
    controllerSprites.loadFromFile("assets/golf/sprites/control_layout.spt", m_sharedData.sharedResources->textures);

    struct SpriteData final
    {
        enum
        {
            Deck, Xbox, PS,
            Count
        };
        std::array<cro::FloatRect, SpriteData::Count> bounds = {};
    }spriteData;
    spriteData.bounds[SpriteData::Deck] = controllerSprites.getSprite("deck").getTextureRect();
    spriteData.bounds[SpriteData::Xbox] = controllerSprites.getSprite("xbox").getTextureRect();
    spriteData.bounds[SpriteData::PS] = controllerSprites.getSprite("ps").getTextureRect();

    ent = m_scene.createEntity();
    ent.addComponent<cro::Transform>().setOrigin({ std::floor(spriteData.bounds[0].width / 2.f) - DetailBackgroundOffset, std::floor(spriteData.bounds[0].height / 5.f) - 2.f });
    ent.addComponent<cro::Drawable2D>();
    ent.addComponent<cro::Sprite>() = controllerSprites.getSprite("deck");
    ent.addComponent<cro::UIElement>(cro::UIElement::Sprite, true);
    ent.getComponent<cro::UIElement>().depth = 0.1f;
    ent.addComponent<cro::Callback>().active = true;
    ent.getComponent<cro::Callback>().setUserData<SpriteData>(spriteData);
    ent.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            const auto& data = e.getComponent<cro::Callback>().getUserData<SpriteData>();
            if (Social::isSteamdeck())
            {
                e.getComponent<cro::Sprite>().setTextureRect(data.bounds[SpriteData::Deck]);
            }
            else
            {
                if (m_sharedData.activeInput == SharedStateData::ActiveInput::PS)
                {
                    e.getComponent<cro::Sprite>().setTextureRect(data.bounds[SpriteData::PS]);
                }
                else if (m_sharedData.activeInput == SharedStateData::ActiveInput::Keyboard)
                {
                    if (cro::GameController::getControllerCount())
                    {
                        const auto idx = cro::GameController::hasPSLayout(0) ? SpriteData::PS : SpriteData::Xbox;
                        e.getComponent<cro::Sprite>().setTextureRect(data.bounds[idx]);
                    }
                    else
                    {
                        e.getComponent<cro::Sprite>().setTextureRect(data.bounds[SpriteData::Xbox]);
                    }
                }
                else
                {
                    e.getComponent<cro::Sprite>().setTextureRect(data.bounds[SpriteData::Xbox]);
                }
            }
        };
    m_uiLayout.detailsPane.tabDetails[TabID::Controller].getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());


    //set detail text to controller list
    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    ent = m_scene.createEntity();
    ent.addComponent<cro::Transform>();
    ent.addComponent<cro::Drawable2D>();
    ent.addComponent<cro::Text>(smallFont).setFillColour(TextNormalColour);
    ent.getComponent<cro::Text>();// .setString("1 Buns Flaps\n2 Game Controller\n3 Super awesome arcade stick\n4 this is made up y'know");
    ent.addComponent<cro::UIElement>(cro::UIElement::Text, true);
    ent.getComponent<cro::UIElement>().depth = 0.2f;
    ent.getComponent<cro::UIElement>().characterSize = InfoTextSize;
    ent.getComponent<cro::UIElement>().absolutePosition = { DetailBackgroundPadding / 2.f, -std::floor((spriteData.bounds[0].height / 5.f) + 4.f) };
    ent.addComponent<cro::Callback>().active = true;
    ent.getComponent<cro::Callback>().function = 
        [&](cro::Entity e, float)
        {
            if (cro::GameController::getControllerCount())
            {
                e.getComponent<cro::Text>().setString(m_controllerString);
            }
            else
            {
                e.getComponent<cro::Text>().setString("No Controllers Connected");
            }
            auto bounds = cro::Text::getLocalBounds(e);
            auto posX = std::round(bounds.width / 2.f) + 4.f;
            e.getComponent<cro::Transform>().setOrigin({ posX, 0.f });            
        };
    m_uiLayout.detailsPane.tabDetails[TabID::Controller].getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
    auto textEnt = ent;

    //activity icons next to description
    ent = m_scene.createEntity();
    ent.addComponent<cro::Transform>();
    ent.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    ent.addComponent<cro::UIElement>(cro::UIElement::Sprite, true);
    ent.getComponent<cro::UIElement>().absolutePosition = { -12.f, -8.f };
    ent.addComponent<cro::Callback>().active = true;
    ent.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            static constexpr float Height = 8.f;
            const auto addQuad = 
                [&](glm::vec2 pos, cro::Colour c, std::vector<cro::Vertex2D>& dst)
                {
                    dst.emplace_back(glm::vec2(pos.x, pos.y + Height), c);
                    dst.emplace_back(pos, c);
                    dst.emplace_back(glm::vec2(pos.x + Height, pos.y + Height), c);
                    dst.emplace_back(glm::vec2(pos.x + Height, pos.y + Height), c);
                    dst.emplace_back(pos, c);
                    dst.emplace_back(glm::vec2(pos.x + Height, pos.y), c);
                };

            if (cro::GameController::getControllerCount())
            {
                std::vector<cro::Vertex2D> verts;
                glm::vec2 pos(0.f);
                for (auto i = 0; i < cro::GameController::getControllerCount(); ++i)
                {
                    addQuad(pos, m_activityColours[i], verts);
                    pos.y -= Height + 4.f;
                    m_activityColours[i] = CD32::Colours[CD32::BlueDarkest];
                }
                e.getComponent<cro::Drawable2D>().setVertexData(verts);
                e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
            }
            else
            {
                e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            }
        };
    textEnt.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());



    //menu items

    auto* item = &m_uiLayout.menuLayout.items[TabID::Controller].emplace_back();
    item->title = "Controller Settings";
    item->displayType = Menu::Item::Heading;

    //input sensitivity
    item = &m_uiLayout.menuLayout.items[TabID::Controller].emplace_back();
    item->title = "Look Sensitivity";
    item->activated = [&](Menu::Item& i)
        {
            const float amt = 0.1f * i.selectedIndex;
            m_sharedData.mouseSpeed = std::clamp(ConstVal::MinMouseSpeed + amt, ConstVal::MinMouseSpeed, ConstVal::MaxMouseSpeed);
        };
    //TODO this should really be reading the min/max constvals
    item->labels = { "0.5", "0.6", "0.7", "0.8", "0.9", "1.0", "1.1", "1.2", "1.3", "1.4", "1.5", "1.6", "1.7", "1.8", "1.9", "2.0"};
    item->selectedIndex = static_cast<std::int32_t>(std::floor((m_sharedData.mouseSpeed - ConstVal::MinMouseSpeed) * 10.f));
    item->displayType = Menu::Item::Slider;
    item->wrapValue = false;



    //thumbstick deadzone
    static constexpr auto MinDeadZone = -3000;
    static constexpr auto MaxDeadzone = 24000;
    item = &m_uiLayout.menuLayout.items[TabID::Controller].emplace_back();
    item->title = "Thumbstick Deadzone";
    item->description = "Adjusts the minimum movement of the thumbstick before input is accepted by the game: larger values require more movement.";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            const float amt = 0.1f * (i.selectedIndex + 1);
            cro::GameController::LeftThumbDeadZone.setOffset(MinDeadZone + std::int16_t(static_cast<float>(MaxDeadzone - MinDeadZone) * amt));            
        };
    item->labels = { "0.1", "0.2", "0.3", "0.4", "0.5", "0.6", "0.7", "0.8", "0.9", "1.0" };
    item->selectedIndex = static_cast<std::int32_t>(static_cast<float>(cro::GameController::LeftThumbDeadZone.getOffset() - MinDeadZone) / (MaxDeadzone - MinDeadZone) * 10.f);
    item->displayType = Menu::Item::Slider;
    item->wrapValue = false;
    

    //invert X axis
    item = &m_uiLayout.menuLayout.items[TabID::Controller].emplace_back();
    item->title = "Invert X axis";
    item->description = "Invert the controller X axis when in camera mode";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.invertX = i.selectedIndex == 1;
        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = m_sharedData.invertX ? 1 : 0;

    //invert Y axis
    item = &m_uiLayout.menuLayout.items[TabID::Controller].emplace_back();
    item->title = "Invert Y axis";
    item->description = "Invert the controller Y axis when in camera mode";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.invertY = i.selectedIndex == 1;
        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = m_sharedData.invertY ? 1 : 0;

    //enable swingput
    item = &m_uiLayout.menuLayout.items[TabID::Controller].emplace_back();
    item->title = "Enable Swingput";
    item->description = "With either trigger held, pull back on a thumbstick to charge the power. Push forward on the stick to take your shot.";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.useSwingput = i.selectedIndex == 1;
        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = m_sharedData.useSwingput ? 1 : 0;

    //vibration
    item = &m_uiLayout.menuLayout.items[TabID::Controller].emplace_back();
    item->title = "Use Vibration";
    item->description = "Enable vibration effects on supported controllers";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.enableRumble = i.selectedIndex;
        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = m_sharedData.enableRumble;

#ifdef USE_GNS
    item = &m_uiLayout.menuLayout.items[TabID::Controller].emplace_back();
    item->title = "Rebind Buttons";
    item->description = "Review a Steam guide on rebinding the controller buttons with Steam Input";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            Social::showControllerBinding();
        };
    item->labels = { "View Steam Guide" };
    item->texture = m_optionIcons[OptionIcon::SteamIcon].getTexture();
    item->uv = m_optionIcons[OptionIcon::SteamIcon].getTextureRect();
#endif
}

void OptionsStateV2::createDisplayItems()
{
    m_uiLayout.menuLayout.items[TabID::Display].clear();

    auto* item = &m_uiLayout.menuLayout.items[TabID::Display].emplace_back();
    item->title = "Configuration";
    item->displayType = Menu::Item::Heading;
    if (cro::App::getWindow().getGPUVendor() == cro::GPUVendor::NVidia)
    {
        item->description = "Tip: disable Threaded Optimization in the nvidia control panel and restart the game if you experience stuttering.";
    }

    //TODO Presets


    //anti-aliasing
    item = &m_uiLayout.menuLayout.items[TabID::Display].emplace_back();
    item->title = "Antialiasing";
    //item->description = "Switch between billboard and 3D trees. Classic trees are applied when the game is loaded";
    //cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            //sets shared data value
            toggleAntialiasing(m_sharedData, AASamples[i.selectedIndex] != 0, AASamples[i.selectedIndex]);
        };
    item->labels = { "None", "2x MSAA", "4x MSAA", "8x MSAA" };
    item->selectedIndex = AAIndexMap[m_sharedData.multisamples];


    //Resolution
    item = &m_uiLayout.menuLayout.items[TabID::Display].emplace_back();
    item->title = "Resolution";
    item->selected = 
        [&](const Menu::Item&)
        {
            //TODO display a graphic

            m_uiLayout.detailsPane.applyButton.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        };
    item->activated =
        [&](Menu::Item& i)
        {
            if (!i.valueChangedOnActivate)
            {
                cro::App::getWindow().setWindowedSize(m_sharedData.resolutions[i.selectedIndex]);
                cro::App::getWindow().setFullscreenSize(m_sharedData.resolutions[i.selectedIndex]);
            }
        };
    item->alwaysActivate = true;
    
    for (const auto& s : m_sharedData.resolutionStrings)
    {
        item->labels.push_back(s);
    }
    item->wrapValue = false;

    const auto size = cro::App::getWindow().isFullscreen() ? cro::App::getWindow().getFullscreenSize() : cro::App::getWindow().getWindowedSize();
    for (auto i = 0u; i < m_sharedData.resolutions.size(); ++i)
    {
        if (m_sharedData.resolutions[i].x == size.x 
            && m_sharedData.resolutions[i].y == size.y)
        {
            item->selectedIndex = i;
            break;
        }
    }



    //FOV
    item = &m_uiLayout.menuLayout.items[TabID::Display].emplace_back();
    item->title = "FOV";
    item->activated = 
        [&](Menu::Item& i) 
        {
            m_sharedData.fov = static_cast<float>(std::atoi(i.labels[i.selectedIndex].toAnsiString().c_str()));

            //raise a window resize message to trigger callbacks
            auto size = cro::App::getWindow().getSize();
            auto* msg = postMessage<cro::Message::WindowEvent>(cro::Message::WindowMessage);
            msg->data0 = size.x;
            msg->data1 = size.y;
            msg->event = SDL_WINDOWEVENT_SIZE_CHANGED;
        };
    for (std::int32_t i = MinFOV; i < MaxFOV + 1; i += 5)
    {
        item->labels.push_back(std::to_string(i));
    }
    item->selectedIndex = static_cast<std::int32_t>((m_sharedData.fov - MinFOV) / 5.f);
    item->wrapValue = false;


    //pixel scaling
    item = &m_uiLayout.menuLayout.items[TabID::Display].emplace_back();
    item->title = "Use Pixel Scaling (Default: OFF)";
    item->description = "Renders the game at a low resolution and then scales the output for a pixelated, retro look. Shortcut +/- on numpad";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            //this func toggles the actual property...
            togglePixelScale(m_sharedData, i.selectedIndex != 0);
        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = m_sharedData.pixelScale ? 1 : 0;

    //vertex snap
    item = &m_uiLayout.menuLayout.items[TabID::Display].emplace_back();
    item->title = "Use Vertex Snapping (Default: OFF)";
    item->description = "Usually used in conjunction with Pixel Scaling. May cause z-fighting. Requires restart.";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.vertexSnap = i.selectedIndex == 1;
        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = m_sharedData.vertexSnap ? 1 : 0;


    //full screen
    item = &m_uiLayout.menuLayout.items[TabID::Display].emplace_back();
    item->title = "Enable Full Screen";
    item->description = "Shortcut: F11 or Alt+Enter";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            cro::App::getWindow().setFullScreen(i.selectedIndex == 1);
        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = cro::App::getWindow().isFullscreen() ? 1 : 0;


    //full screen borderless/exclusive
    item = &m_uiLayout.menuLayout.items[TabID::Display].emplace_back();
    item->title = "Full Screen Mode";
    item->description = "When in full screen run the game in a borderless window at the desktop resolution, or exclusive full screen at the window resolution.";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            cro::App::getWindow().setExclusiveFullscreen(i.selectedIndex == 1);

            if (cro::App::getWindow().isFullscreen())
            {
                //apply the setting
                //cro::App::getWindow().setFullScreen(false);
                cro::App::getWindow().setFullScreen(true);
            }
        };
    item->labels = { "Borderless Window", "Exclusive Mode" };
    item->selectedIndex = cro::App::getWindow().getExclusiveFullscreen() ? 1 : 0;


    item = &m_uiLayout.menuLayout.items[TabID::Display].emplace_back();
    item->title = "Show Window Border";
    item->description = "Display a border around the game window when not in full screen.";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            cro::App::getWindow().setBorderVisible(i.selectedIndex == 1);
        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = cro::App::getWindow().getBorderVisible() ? 1 : 0;


    //vsync
    item = &m_uiLayout.menuLayout.items[TabID::Display].emplace_back();
    item->title = "Enable VSync";
    item->description = "Synchronises the game's refresh rate with your monitor";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            cro::App::getWindow().setVsyncEnabled(i.selectedIndex == 1);
        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = cro::App::getWindow().getVsyncEnabled() ? 1 : 0;

    //tree quality
    item = &m_uiLayout.menuLayout.items[TabID::Display].emplace_back();
    item->title = "Tree Quality";
    item->description = "Switch between billboard and 3D trees. Classic trees are applied when the game is loaded";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.treeQuality = i.selectedIndex;
            auto* msg = postMessage<SystemEvent>(cl::MessageID::SystemMessage);
            msg->type = SystemEvent::TreeQualityChanged;
        };
    item->labels = { "Classic", "Low", "High" };
    item->selectedIndex = m_sharedData.treeQuality;


    //shadow quality
    item = &m_uiLayout.menuLayout.items[TabID::Display].emplace_back();
    item->title = "Shadow Quality";
    item->description = "NOTE Toggling Classic shadows requires a restart and may cause visual artifacts until done so";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.shadowQuality = i.selectedIndex;
            auto* msg = postMessage<SystemEvent>(cl::MessageID::SystemMessage);
            msg->type = SystemEvent::ShadowQualityChanged;
        };
    item->labels = { "Very Low", "Low", "High", "Very High", "Classic" };
    item->selectedIndex = m_sharedData.shadowQuality;


    //crowd density
    item = &m_uiLayout.menuLayout.items[TabID::Display].emplace_back();
    item->title = "Crowd Density";
    item->description = "NOTE Very high density crowds may cause a drop in performance";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.crowdDensity = i.selectedIndex;
            auto* msg = postMessage<SystemEvent>(cl::MessageID::SystemMessage);
            msg->type = SystemEvent::CrowdDensityChanged;
        };
    item->labels = { "Low", "Normal", "High", "Extreme", "None" };
    item->selectedIndex = m_sharedData.crowdDensity;

    //if (!Social::isSteamdeck())
    //{
        //grass density
        item = &m_uiLayout.menuLayout.items[TabID::Display].emplace_back();
        item->title = "Grass Density";
        //item->description = "Changes the appearance of the grass in the rough. Setting this to Low can improve performance";
        item->description = "Changes the density of the grass in the rough. Setting this to Low can improve performance. Takes affect when the next hole loads.";
        cro::Util::String::wordWrap(item->description, 36);
        item->activated = [&](Menu::Item& i)
            {
                m_sharedData.grassDensity = i.selectedIndex;
                auto* msg = postMessage<SystemEvent>(cl::MessageID::SystemMessage);
                msg->type = SystemEvent::GrassDensityChanged;
            };
        item->labels = { "Low", "High" };
        item->selectedIndex = m_sharedData.grassDensity;
    //}
}

void OptionsStateV2::createAudioItems()
{
    m_uiLayout.tabBar.items[TabID::Audio].displayWidth = 0.7f;
    m_uiLayout.tabBar.items[TabID::Audio].alignment = TabBar::Item::Centre;

    auto* item = &m_uiLayout.menuLayout.items[TabID::Audio].emplace_back();
    item->title = "Configuration";
    item->displayType = Menu::Item::Heading;

    //device selection - WARNING we have a hack when handling device
    //disconnect events which indexes this directly!! Don't chage the index!
    item = &m_uiLayout.menuLayout.items[TabID::Audio].emplace_back();
    item->title = "Audio Device";
    //item->description = "Enable text to speech playback for in-game chat";
    //cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& it)
        {
            const auto& devices = cro::AudioDevice::getDeviceList();
            if (devices.empty())
            {
                return;
            }

            it.selectedIndex %= devices.size();

            //we have to copy the device name else this function
            //will *rearrange* the devices list while we're referencing it!!!
            const std::string deviceName = devices[it.selectedIndex];
            cro::AudioDevice::setActiveDevice(deviceName);
        };
    refreshAudioDevices(*item);

    //currently the deck needs to re-apply the audio device for some reason
    if (!audioHackDone)
    {
        item->activated(*item);
        audioHackDone = true;
    }



    //text to speech
    item = &m_uiLayout.menuLayout.items[TabID::Audio].emplace_back();
    item->title = "Use Text To Speech for Chat";
    item->description = "Enable text to speech playback for in-game chat";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.useTTS = i.selectedIndex == 0 ? false : true;
        };
    item->labels = { "No", "Yes" };
    item->selectedIndex = m_sharedData.useTTS ? 1 : 0;


    //mixer
    item = &m_uiLayout.menuLayout.items[TabID::Audio].emplace_back();
    item->title = "Volume Levels";
    item->displayType = Menu::Item::Heading;


    //main vol
    item = &m_uiLayout.menuLayout.items[TabID::Audio].emplace_back();
    item->title = "Master Volume";
    item->activated =
        [](Menu::Item& it)
        {
            cro::AudioMixer::setMasterVolume(static_cast<float>(it.selectedIndex) / 10.f);
        };

    for (auto j = 0; j < 11; ++j)
    {
        item->labels.push_back("Vol: " + std::to_string(j));
    }
    item->selectedIndex = std::clamp(static_cast<std::int32_t>(cro::AudioMixer::getMasterVolume() * 10.f), 0, 10);
    item->displayType = Menu::Item::Slider;
    item->wrapValue = false;


    for (auto i = 0; i < MixerChannel::Count; ++i)
    {
        auto& item = m_uiLayout.menuLayout.items[TabID::Audio].emplace_back();
        item.title = MixerLabels[i];
        //item.description = "This is the item description for " + std::to_string(i + 1);
        item.activated = 
            [i](Menu::Item& it)
            {
                cro::AudioMixer::setVolume(static_cast<float>(it.selectedIndex) / 10.f, i);
            };

        for (auto j = 0; j < 11; ++j) //TODO how do we determine this arbitrary number of channels?
        {
            item.labels.push_back("Vol: " + std::to_string(j));
        }
        item.selectedIndex = std::clamp(static_cast<std::int32_t>(cro::AudioMixer::getVolume(i) * 10.f), 0, 10);
        item.displayType = Menu::Item::Slider;
        item.wrapValue = false;
    }
}

void OptionsStateV2::createAchievementItems()
{
    m_uiLayout.tabBar.items[TabID::Achievements].displayWidth = 0.9f;
    m_uiLayout.tabBar.items[TabID::Achievements].alignment = TabBar::Item::Centre;
    m_uiLayout.menuLayout.items[TabID::Achievements].clear();

    //TODO display progress of achievements
    //which are based on stats.

    for(const auto& s : AchievementStrings)
    {
        const auto icon = Achievements::getIcon(s);
        const auto* ach = Achievements::getAchievement(s);

        if (ach)
        {
            auto& item = m_uiLayout.menuLayout.items[TabID::Achievements].emplace_back();
            
            if (!ach->achieved && AchievementDesc[ach->id].second)
            {
                item.title = "Hidden Achievement";
            }
            else
            {
                item.title = ach->name;
#ifdef USE_GNS
                std::stringstream ss;
                ss.precision(2);
                //ss << std::setw(2) << std::setfill('0');
                ss << ach->percent;
                item.title += " - (Achieved by " + ss.str() + "% of Players)";
#endif
                item.subTitle = AchievementDesc[ach->id].first;

                if (ach->achieved)
                {
                    item.subTitle += "\nUnlocked: " + cro::SysTime::dateString(ach->timestamp);
                }
            }

            if (icon.texture)
            {
                item.texture = icon.texture;
                item.uv = icon.textureRect;

                const auto texSize = glm::vec2(icon.texture->getSize());
                item.uv.left *= texSize.x;
                item.uv.width *= texSize.x;
                item.uv.bottom *= texSize.y;
                item.uv.height *= texSize.y;
            }
            item.displayType = Menu::Item::TextOnly;
        }
    }
}

void OptionsStateV2::createStatItems()
{
    m_uiLayout.tabBar.items[TabID::Stats].displayWidth = 0.7f;
    m_uiLayout.tabBar.items[TabID::Stats].alignment = TabBar::Item::Centre;
    m_uiLayout.menuLayout.items[TabID::Stats].clear();

    const auto formatValue =
        [](std::int32_t type, float statValue)
        {
            std::string value;
            switch (type)
            {
            default:
            case StatType::Float:
            {
                std::stringstream ss;
                ss.precision(2);
                ss << std::fixed << statValue;
                value = ss.str();
            }
            break;
            case StatType::Integer:
                value = std::to_string(static_cast<std::int32_t>(statValue));
                break;
            case StatType::Percent:
            {
                const float v = statValue * 100.f;
                std::stringstream ss;
                ss.precision(2);
                ss << std::fixed << v << "%";
                value = ss.str();
            }
            break;
            case StatType::Time:
            {
                std::int32_t v = static_cast<std::int32_t>(statValue);
                const auto seconds = v % 60;
                auto minutes = v / 60;
                const auto hours = minutes / 60;
                minutes %= 60;

                std::stringstream ss;
                ss << hours << "h " << minutes << "m " << seconds << "s";
                value = ss.str();
            }
            break;
            }

            return value;
        };


    for(const auto& s : StatStrings)
    {
        const auto* stat = Achievements::getStat(s);

        if (stat)
        {
            auto& item = m_uiLayout.menuLayout.items[TabID::Stats].emplace_back();
            item.title = StatLabels[stat->id];
            item.subTitle = formatValue(StatTypes[stat->id], stat->value);
            item.displayType = Menu::Item::TextOnly;
        }
    }
}

void OptionsStateV2::onCachedPush()
{
    createDisplayItems();

    //refreshes stats/achievements when opening the window
    createAchievementItems();
    createStatItems();

    refreshControllerDevices();
    m_uiLayout.activateTab(0);

    m_rootNode.getComponent<cro::Callback>().active = true;
}

void OptionsStateV2::onCachedPop()
{

}

void OptionsStateV2::resetRepeatTimer(std::int32_t i, cro::Time resetTime)
{
    m_inputRepeatClocks[i].restart();
    m_repeatTimes[i] = resetTime;
}

void OptionsStateV2::updateKeybind(SDL_Keycode key)
{
    if (key == SDLK_ESCAPE
        || key == SDLK_BACKSPACE)
    {
        cancelKeybind();
        return;
    }


    //prevent binding top row and function keys
    const std::array LockedKeys =
    {
        SDLK_1,
        SDLK_2,
        SDLK_3,
        SDLK_4,
        SDLK_5,
        SDLK_6,
        SDLK_7,
        SDLK_8,
        SDLK_9,
        SDLK_0,

        SDLK_F1,
        SDLK_F2,
        SDLK_F3,
        SDLK_F4,
        SDLK_F5,
        SDLK_F6,
        SDLK_F7,
        SDLK_F8,
        SDLK_F9,
        SDLK_F10,
        SDLK_F11,
        SDLK_F12,
    };

    if (auto result = std::find(std::begin(LockedKeys), std::end(LockedKeys), key); result != std::end(LockedKeys))
    {
        cro::String msg("This key cannot be assigned. Press a key.");
        cro::Util::String::wordWrap(msg, 36);
        m_uiLayout.detailsPane.text.getComponent<cro::Text>().setString(msg);

        return;
    }


    auto& keys = m_sharedData.inputBinding.keys;
    if (auto result = std::find(keys.begin(), keys.end(), key); result != keys.end())
    {
        cro::String msg = cro::Keyboard::keyString(key);
        msg += " is already bound. Press a key";
        cro::Util::String::wordWrap(msg, 36);
        m_uiLayout.detailsPane.text.getComponent<cro::Text>().setString(msg);

        return;
    }


    keys[m_keybindIndex] = key;

    //m_uiLayout.detailsPane.text.getComponent<cro::Text>().setString(
    //    "Key: " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[m_keybindIndex]));
    
    m_uiLayout.menuLayout.items[TabID::Keyboard][m_keybindItemIndex].labels[0] = 
        "Key: " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[m_keybindIndex]);

    playSound(MenuSoundEvent::Activate);
    m_uiLayout.updateMenuItems();

    m_keybindIndex = -1;
    m_keybindItemIndex = -1;
}

void OptionsStateV2::cancelKeybind()
{
    playSound(MenuSoundEvent::Cancel);

    m_uiLayout.detailsPane.text.getComponent<cro::Text>().setString("Press Enter to select a new key");

    m_keybindIndex = -1;
    m_keybindItemIndex = -1;
}

void OptionsStateV2::refreshControllerDevices()
{
    cro::String str;
    for (auto i = 0; i < std::min(4, cro::GameController::getControllerCount()); ++i)
    {
        str += std::to_string(i + 1) + ". ";
        str += cro::GameController::getPrintableName(i);
        str += "\n";
    }
    m_controllerString = str;
}

void OptionsStateV2::refreshAudioDevices(Menu::Item& item)
{
    item.labels.clear();

    std::string str;
    auto deviceList = cro::AudioDevice::getDeviceList();
    if (deviceList.empty())
    {
        //hmmm how do we have both the count set to zero but labels set to one?
        item.labels.push_back("No Device Available");
        item.selectedIndex = 0;
        return;
    }
    else
    {
        /*if (Social::isSteamdeck())
        {
            str = "Default";
        }
        else*/
        {
            str = cro::AudioDevice::getActiveDevice();
        }
    }

    static const std::string RemoveMe("OpenAL Soft on ");
    if (str.find(RemoveMe) != std::string::npos)
    {
        str = str.substr(RemoveMe.size());
    }
    for (auto& d : deviceList)
    {
        if (d.find(RemoveMe) != std::string::npos)
        {
            d = d.substr(RemoveMe.size());
        }
        item.labels.push_back(d);
    }

    //item.count = static_cast<std::int32_t>(deviceList.size());
    if (const auto res = std::find(deviceList.cbegin(), deviceList.cend(), str);
        res != deviceList.cend())
    {
        item.selectedIndex = static_cast<std::int32_t>(std::distance(deviceList.cbegin(), res));
    }
    else
    {
        item.selectedIndex = 0;
        item.activated(item);
    }
}

void OptionsStateV2::quitState()
{
    m_rootNode.getComponent<cro::Callback>().active = true;
    playSound(MenuSoundEvent::Cancel);
}