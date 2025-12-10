/*-----------------------------------------------------------------------

Matt Marchant 2025
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

#include <Achievements.hpp>
#include <AchievementStrings.hpp>

#include <crogine/audio/AudioDevice.hpp>
#include <crogine/core/Window.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/graphics/SimpleText.hpp>
#include <crogine/graphics/SimpleVertexArray.hpp>

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


    const std::array ItemLabels =
    {
        "Settings", "Keyboard", "Controller",
        "Display", "Audio", "Achievements",
        "Stats"
    };

    constexpr float TabBarHeight = 16.f;

    constexpr float ItemHeight = TabBarHeight * 2.5f;
    constexpr float ItemSpacing = 4.f;
    constexpr glm::vec2 ItemImage = glm::vec2(ItemHeight - (ItemSpacing * 2.f), ItemHeight - (ItemSpacing * 2.f));

    constexpr float InfoBarHeight = 24.f; //space at the bottom

    constexpr auto BackgroundDark = cro::Colour(0xc8b89faf);
    constexpr auto BackgroundYellow = cro::Colour(0xf2cf5caf);
    constexpr auto BackgroundRed = cro::Colour(0xb83530af);


    static constexpr cro::Time RepeatTimeLong = cro::seconds(0.5f);
    static constexpr cro::Time RepeatTimeShort = cro::seconds(0.05f);

    void playSound(std::int32_t id)
    {
        cro::App::postMessage<MenuSoundEvent>(cl::MessageID::MenuSoundMessage)->type = id;
    }
}

OptionsStateV2::OptionsStateV2(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State        (ss, ctx),
    m_scene             (ctx.appInstance.getMessageBus(), 192),
    m_sharedData        (sd),
    m_uiTexture         (nullptr),
    m_keybindIndex      (-1),
    m_keybindItemIndex  (-1)
{
    ctx.mainWindow.setMouseCaptured(false);

    m_flagPreview.init(sd.flagPath);
    m_flagPreview.setText(m_sharedData.flagText);

    std::fill(m_controllerMasks.begin(), m_controllerMasks.end(), 0);
    std::fill(m_controllerPrevMasks.begin(), m_controllerMasks.end(), 0);

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
        refreshAudioDevices(m_menuLayout.items[TabBar::Item::Audio][1]);
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

                m_tabBar.navLeft.getComponent<cro::Text>().setString("<  " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::PrevClub]));
                m_tabBar.navRight.getComponent<cro::Text>().setString(cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::NextClub]) + "  >");

                m_tabBar.navLeftSprite.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
                m_tabBar.navRightSprite.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);

                const auto viewScale = cro::UIElementSystem::getViewScale();
                const auto charSize = static_cast<std::uint32_t>((UITextSize) * viewScale);
                m_tabBar.navLeft.getComponent<cro::Text>().setCharacterSize(charSize);
                m_tabBar.navLeft.getComponent<cro::UIElement>().characterSize = UITextSize;
                m_tabBar.navLeft.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);

                m_tabBar.navRight.getComponent<cro::Text>().setCharacterSize(charSize);
                m_tabBar.navRight.getComponent<cro::UIElement>().characterSize = UITextSize;
                m_tabBar.navRight.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
            }
            else
            {
                m_infoString.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
                m_infoSprite.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);

                m_tabBar.navLeft.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
                m_tabBar.navRight.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);

                m_tabBar.navLeftSprite.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
                m_tabBar.navRightSprite.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);

                if (cro::GameController::hasPSLayout(controllerIndex))
                {
                    m_sharedData.activeInput = SharedStateData::ActiveInput::PS;
                    m_infoSprite.getComponent<cro::Sprite>().setTextureRect(m_infoRects[0]);

                    m_tabBar.navLeftSprite.getComponent<cro::Sprite>().setTextureRect(m_tabBar.navLeftRects[0]);
                    m_tabBar.navRightSprite.getComponent<cro::Sprite>().setTextureRect(m_tabBar.navRightRects[0]);
                }
                else
                {
                    m_sharedData.activeInput = SharedStateData::ActiveInput::XBox;
                    m_infoSprite.getComponent<cro::Sprite>().setTextureRect(m_infoRects[1]);

                    m_tabBar.navLeftSprite.getComponent<cro::Sprite>().setTextureRect(m_tabBar.navLeftRects[1]);
                    m_tabBar.navRightSprite.getComponent<cro::Sprite>().setTextureRect(m_tabBar.navRightRects[1]);
                }

                /*const auto viewScale = cro::UIElementSystem::getViewScale();
                const auto charSize = (LabelTextSize * 2) * viewScale;
                m_tabBar.navLeft.getComponent<cro::Text>().setCharacterSize(charSize);
                m_tabBar.navLeft.getComponent<cro::UIElement>().characterSize = LabelTextSize * 2;

                m_tabBar.navRight.getComponent<cro::Text>().setCharacterSize(charSize);
                m_tabBar.navRight.getComponent<cro::UIElement>().characterSize = LabelTextSize * 2;*/
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
            nextTab();
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::PrevClub])
        {
            prevTab();
        }

        //done on key down evet for repeat when held
        /*else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Down]
            || evt.key.keysym.sym == SDLK_DOWN)
        {
            nextItem();
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Up]
            || evt.key.keysym.sym == SDLK_UP)
        {
            prevItem();
        }*/

        else if (/*evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Left]
            || */evt.key.keysym.sym == SDLK_LEFT)
        {
            activateLeft();
        }
        else if (/*evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Right]
            || */evt.key.keysym.sym == SDLK_RIGHT)
        {
            activateRight();
        }

        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Action]
            || evt.key.keysym.sym == SDLK_RETURN)
        {
            activate();
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

        //do this here to take advantageof key repeat
        if (/*evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Down]
            || */evt.key.keysym.sym == SDLK_DOWN)
        {
            nextItem();
        }
        else if (/*evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Up]
            ||*/ evt.key.keysym.sym == SDLK_UP)
        {
            prevItem();
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONDOWN)
    {
        setActiveInput(false, cro::GameController::controllerID(evt.cbutton.which));

        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::DPadUp:
            prevItem();
            resetRepeatTimer(RepeatTimeLong);
            break;
        case cro::GameController::DPadDown:
            nextItem();
            resetRepeatTimer(RepeatTimeLong);
            break;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::DPadLeft:
            activateLeft();
            break;
        case cro::GameController::DPadRight:
            activateRight();
            break;
        case cro::GameController::ButtonLeftShoulder:
            prevTab();
            break;
        case cro::GameController::ButtonRightShoulder:
            nextTab();
            break;
        case cro::GameController::ButtonX:
            showCredits();
            break;
        case cro::GameController::ButtonY:
            showHelp();
            break;
        case cro::GameController::ButtonA:
            activate();
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
            doMouseClick({ evt.motion.x, evt.motion.y });
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
        checkMouseOver(pos);
    }
    else if (evt.type == SDL_CONTROLLERAXISMOTION)
    {
        const std::int16_t Threshold = cro::GameController::LeftThumbDeadZone * 2;// 15000;
        
        if (std::abs(evt.caxis.value) > Threshold)
        {
            setActiveInput(false, cro::GameController::controllerID(evt.caxis.which));
        }
        
        const auto controllerID = cro::GameController::controllerID(evt.caxis.which);
            
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
            prevItem();
        }
        else if (evt.wheel.y < 0)
        {
            nextItem();
        }
    }

    //m_scene.getSystem<cro::UISystem>()->handleEvent(evt);
    m_scene.forwardEvent(evt);
    return false;
}

void OptionsStateV2::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool OptionsStateV2::simulate(float dt)
{
    //TODO this doesn't actually do what I wanted, but it's servicable
    const float texHeight = static_cast<float>(m_menuLayout.texture.getSize().y);
    static constexpr float Stride = ItemHeight + ItemSpacing;
    const float Extents = m_tabBar.background.getComponent<cro::Transform>().getPosition().y / cro::UIElementSystem::getViewScale();
    const float target = std::clamp((texHeight - (Stride * m_menuLayout.itemIndex)) - Extents, -ItemHeight, texHeight - (Extents * 2.f));

    auto origin = m_menuLayout.sprite.getComponent<cro::Transform>().getOrigin();
    const float diff = target - origin.y;
    origin.y += diff * (dt * 10.f);
    m_menuLayout.sprite.getComponent<cro::Transform>().setOrigin(origin);

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
            activateLeft();
        }

        if (maskTest(i, InputFlag::Right))
        {
            activateRight();
        }

        if (maskTest(i, InputFlag::Up))
        {
            prevItem();
            resetRepeatTimer(RepeatTimeLong);
        }

        if (maskTest(i, InputFlag::Down))
        {
            nextItem();
            resetRepeatTimer(RepeatTimeLong);
        }

        m_controllerPrevMasks[i] = m_controllerMasks[i];
        
        //check for repeat inputs
        if (cro::GameController::isButtonPressed(i, cro::GameController::DPadDown)
            || (m_controllerMasks[i] & InputFlag::Down))
        {
            if (m_inputRepeatClock.elapsed() > m_repeatTime)
            {
                nextItem();
                resetRepeatTimer(RepeatTimeShort);
            }
        }

        if (cro::GameController::isButtonPressed(i, cro::GameController::DPadUp)
            || (m_controllerMasks[i] & InputFlag::Up))
        {
            if (m_inputRepeatClock.elapsed() > m_repeatTime)
            {
                prevItem();
                resetRepeatTimer(RepeatTimeShort);
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
    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);
    m_menuText.setFont(font);
    m_menuText.setCharacterSize(InfoTextSize);

    const auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    m_menuTextLarge.setFont(largeFont);
    m_menuTextLarge.setCharacterSize(UITextSize);
    m_menuTextLarge.setAlignment(cro::SimpleText::Alignment::Centre);

    m_itemSlider.setPrimitiveType(GL_TRIANGLES);

    cro::SpriteSheet spriteSheet;
    if (spriteSheet.loadFromFile("assets/golf/sprites/options_buttons.spt", m_sharedData.sharedResources->textures))
    {
        m_uiTexture = spriteSheet.getTexture();

        const auto parseSprite = [&](const std::string& spr, SpriteSection& dst)
            {
                auto bounds = spriteSheet.getSprite(spr).getTextureBounds();
                auto uv = spriteSheet.getSprite(spr).getTextureRectNormalised();
                dst.size = { bounds.width, bounds.height };
                dst.uv = { uv.left, uv.bottom, uv.left + uv.width, uv.bottom + uv.height };
            };

        //active tab
        parseSprite("tab_active_left", m_tabActive[0]);
        parseSprite("tab_active_right", m_tabActive[1]);

        //inactive tab
        parseSprite("tab_inactive_left", m_tabInactive[0]);
        parseSprite("tab_inactive_right", m_tabInactive[1]);

        //highlight tab
        parseSprite("tab_highlight_left", m_tabHighlight[0]);
        parseSprite("tab_highlight_right", m_tabHighlight[1]);



        //background 9-patch
        parseSprite("background_centre", m_backgroundSections[BackgroundSection::Centre]);
        parseSprite("background_top", m_backgroundSections[BackgroundSection::Top]);
        parseSprite("background_left", m_backgroundSections[BackgroundSection::Left]);
        parseSprite("background_right", m_backgroundSections[BackgroundSection::Right]);
        parseSprite("background_bottom", m_backgroundSections[BackgroundSection::Bottom]);
        parseSprite("background_tl", m_backgroundSections[BackgroundSection::TL]);
        parseSprite("background_tr", m_backgroundSections[BackgroundSection::TR]);
        parseSprite("background_bl", m_backgroundSections[BackgroundSection::BL]);
        parseSprite("background_br", m_backgroundSections[BackgroundSection::BR]);



        //item backgrounds
        parseSprite("item_background_left", m_itemSection[0]);
        parseSprite("item_background_right", m_itemSection[1]);

        //item active
        parseSprite("item_active_left", m_itemActiveSection[0]);
        parseSprite("item_active_right", m_itemActiveSection[1]);

        //item active highlight
        parseSprite("item_highlight_active_left", m_itemActiveHighlightSection[0]);
        parseSprite("item_highlight_active_right", m_itemActiveHighlightSection[1]);

        //item highlight
        parseSprite("item_highlight_left", m_itemHighlightSection[0]);
        parseSprite("item_highlight_right", m_itemHighlightSection[1]);

        //item title
        parseSprite("item_title_left", m_itemTitleSection[0]);
        parseSprite("item_title_right", m_itemTitleSection[1]);


        m_itemBackground.setTexture(*m_uiTexture);
        m_itemBackground.setPrimitiveType(GL_TRIANGLES);

        m_itemBackgroundActive.setTexture(*m_uiTexture);
        m_itemBackgroundActive.setPrimitiveType(GL_TRIANGLES);
        
        m_itemBackgroundActiveHighlight.setTexture(*m_uiTexture);
        m_itemBackgroundActiveHighlight.setPrimitiveType(GL_TRIANGLES);

        m_itemBackgroundHighlight.setTexture(*m_uiTexture);
        m_itemBackgroundHighlight.setPrimitiveType(GL_TRIANGLES);

        m_itemBackgroundTitle.setTexture(*m_uiTexture);
        m_itemBackgroundTitle.setPrimitiveType(GL_TRIANGLES);
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
    m_tabBar.background = m_scene.createEntity();
    m_tabBar.background.addComponent<cro::Transform>();
    m_tabBar.background.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    m_tabBar.background.getComponent<cro::Drawable2D>().setTexture(m_uiTexture);
    m_tabBar.background.addComponent<cro::UIElement>(cro::UIElement::Position, true);
    m_tabBar.background.getComponent<cro::UIElement>().relativePosition = { -0.5f, 0.5f };
    m_tabBar.background.getComponent<cro::UIElement>().absolutePosition = { 0.f, -(TabBarHeight * 2.f) };
    rootNode.getComponent<cro::Transform>().addChild(m_tabBar.background.getComponent<cro::Transform>());

    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info); 
    const float Spacing = 1.f / (TabBar::Item::Count + 2); //leave equivalent of a tab either end
    for (auto i = 0; i < TabBar::Item::Count; ++i)
    {
        auto& item = m_tabBar.items[i];
        item.text = m_scene.createEntity();
        item.text.addComponent<cro::Transform>();
        item.text.addComponent<cro::Drawable2D>();
        item.text.addComponent<cro::Text>(smallFont).setString(ItemLabels[i]);
        item.text.getComponent<cro::Text>().setFillColour(TextNormalColour);
        item.text.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);

        auto& uiElement = item.text.addComponent<cro::UIElement>(cro::UIElement::Text, true);
        uiElement.characterSize = InfoTextSize;
        uiElement.depth = 0.1f;
        const float offset = (Spacing * 1.5f) + (Spacing * i);
        uiElement.resizeCallback = 
            [&, offset](cro::Entity e)
            {
                const auto x = std::ceil((static_cast<float>(cro::App::getWindow().getSize().x) / cro::UIElementSystem::getViewScale()) * offset) + 1.f;
                const auto y = 12.f;
                e.getComponent<cro::UIElement>().absolutePosition = { x,y };
            };

        m_tabBar.background.getComponent<cro::Transform>().addChild(item.text.getComponent<cro::Transform>());
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
            const auto x = std::floor((static_cast<float>(cro::App::getWindow().getSize().x) / cro::UIElementSystem::getViewScale()) * (Spacing / 2.f));
            const auto y = 14.f;
            e.getComponent<cro::UIElement>().absolutePosition = { x,y };
        };
    m_tabBar.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_tabBar.navLeft = entity;

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
            const auto offset = (Spacing * (m_tabBar.items.size() + 1)) + (Spacing / 2.f);
            const auto x = std::floor((static_cast<float>(cro::App::getWindow().getSize().x) / cro::UIElementSystem::getViewScale()) * offset);
            const auto y = 14.f;
            e.getComponent<cro::UIElement>().absolutePosition = { x,y };
        };
    m_tabBar.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_tabBar.navRight = entity;


    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/options_buttons.spt", m_sharedData.sharedResources->textures);
    m_tabBar.navLeftRects[0] = spriteSheet.getSprite("l1").getTextureRect();
    m_tabBar.navLeftRects[1] = spriteSheet.getSprite("lb").getTextureRect();

    m_tabBar.navRightRects[0] = spriteSheet.getSprite("r1").getTextureRect();
    m_tabBar.navRightRects[1] = spriteSheet.getSprite("rb").getTextureRect();

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
            const auto x = std::floor((static_cast<float>(cro::App::getWindow().getSize().x) / cro::UIElementSystem::getViewScale()) * (Spacing / 2.f));
            const auto y = 10.f;
            e.getComponent<cro::UIElement>().absolutePosition = { x,y };
        };
    m_tabBar.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_tabBar.navLeftSprite = entity;


    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), bounds.height / 2.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("rb");
    entity.addComponent<cro::UIElement>(cro::UIElement::Sprite, true);
    entity.getComponent<cro::UIElement>().depth = 0.1f;
    entity.getComponent<cro::UIElement>().resizeCallback =
        [&, Spacing](cro::Entity e)
        {
            const auto offset = (Spacing * (m_tabBar.items.size() + 1)) + (Spacing / 2.f);
            const auto x = std::floor((static_cast<float>(cro::App::getWindow().getSize().x) / cro::UIElementSystem::getViewScale()) * offset);
            const auto y = 10.f;
            e.getComponent<cro::UIElement>().absolutePosition = { x,y };
        };
    m_tabBar.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_tabBar.navRightSprite = entity;



    //menu layout
    createSettingsItems();
    createKeyboardItems();
    createControllerItems();
    createDisplayItems();
    createAudioItems();
    createAchievementItems();
    createStatItems();

    m_menuLayout.sprite = m_scene.createEntity();
    m_menuLayout.sprite.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.2f });
    m_menuLayout.sprite.addComponent<cro::Drawable2D>();
    m_menuLayout.sprite.addComponent<cro::Sprite>();
    rootNode.getComponent<cro::Transform>().addChild(m_menuLayout.sprite.getComponent<cro::Transform>());

    //details window on right side
    m_detailsPane.root = m_scene.createEntity();
    m_detailsPane.root.addComponent<cro::Transform>();
    m_detailsPane.root.addComponent<cro::UIElement>(cro::UIElement::Position, false);
    m_detailsPane.root.getComponent<cro::UIElement>().relativePosition = { 0.25f, 0.f };
    rootNode.getComponent<cro::Transform>().addChild(m_detailsPane.root.getComponent<cro::Transform>());

    //text
    m_detailsPane.text = m_scene.createEntity();
    m_detailsPane.text.addComponent<cro::Transform>();
    m_detailsPane.text.addComponent<cro::Drawable2D>();
    m_detailsPane.text.addComponent<cro::Text>(largeFont);
    m_detailsPane.text.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    m_detailsPane.text.getComponent<cro::Text>().setFillColour(TextNormalColour);
    m_detailsPane.text.addComponent<cro::UIElement>(cro::UIElement::Text, true);
    m_detailsPane.text.getComponent<cro::UIElement>().absolutePosition = { 0.f, -10.f };
    m_detailsPane.text.getComponent<cro::UIElement>().characterSize = UITextSize;
    m_detailsPane.text.getComponent<cro::UIElement>().verticalSpacing = 4.f;
    m_detailsPane.text.getComponent<cro::UIElement>().depth = 0.2f;
    m_detailsPane.root.getComponent<cro::Transform>().addChild(m_detailsPane.text.getComponent<cro::Transform>());

    //image
    m_detailsPane.image = m_scene.createEntity();
    m_detailsPane.image.addComponent<cro::Transform>();
    m_detailsPane.image.addComponent<cro::Drawable2D>();
    m_detailsPane.image.addComponent<cro::Sprite>();
    m_detailsPane.image.addComponent<cro::UIElement>(cro::UIElement::Sprite, true);
    m_detailsPane.image.getComponent<cro::UIElement>().absolutePosition = { 0.f, 10.f };
    m_detailsPane.image.getComponent<cro::UIElement>().depth = 0.2f;
    m_detailsPane.root.getComponent<cro::Transform>().addChild(m_detailsPane.image.getComponent<cro::Transform>());

    //background/9 patch
    m_detailsPane.background = m_scene.createEntity();
    m_detailsPane.background.addComponent<cro::Transform>().setOrigin({ 0.f, InfoBarHeight / 2.f });
    m_detailsPane.background.addComponent<cro::Drawable2D>().setTexture(m_uiTexture);
    m_detailsPane.background.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    m_detailsPane.background.addComponent<cro::UIElement>(cro::UIElement::Sprite, true);
    m_detailsPane.background.getComponent<cro::UIElement>().absolutePosition = { 0.f, 10.f };
    m_detailsPane.background.getComponent<cro::UIElement>().depth = -0.3f;
    m_detailsPane.root.getComponent<cro::Transform>().addChild(m_detailsPane.background.getComponent<cro::Transform>());


    //displays a cancel message when a keybind is in progress
    auto msgRoot = m_scene.createEntity();
    msgRoot.addComponent<cro::Transform>();
    msgRoot.addComponent<cro::UIElement>(cro::UIElement::Position, false);
    msgRoot.getComponent<cro::UIElement>().relativePosition = { 0.03f, -0.37f };
    msgRoot.addComponent<cro::Callback>().active = true;
    msgRoot.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            const float scale = m_keybindIndex == -1 ? 0.f : 1.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };
    rootNode.getComponent<cro::Transform>().addChild(msgRoot.getComponent<cro::Transform>());

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("Esc - Cancel");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIElement>(cro::UIElement::Text, true);
    entity.getComponent<cro::UIElement>().characterSize = UITextSize;
    entity.getComponent<cro::UIElement>().depth = 0.2f;
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
    entity.getComponent<cro::UIElement>().absolutePosition = { 0.f, -8.f };
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
    entity.getComponent<cro::UIElement>().absolutePosition = { 0.f, -8.f };
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

    updateTabBar(); //this also updates the menu items


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
        /*auto& tx = m_menuLayout.sprite.getComponent<cro::Transform>();
        auto pos = tx.getPosition();
        pos.x = -(size.x / 2.f);
        pos.y = -(size.y / 2.f);
        tx.setPosition(pos);*/

        refreshView();
    };

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().resizeCallback = updateView;
    m_scene.setActiveCamera(entity);
    updateView(entity.getComponent<cro::Camera>());
}

void OptionsStateV2::createSettingsItems()
{
    auto* item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Appearance";
    item->displayType = Menu::Item::Heading;

    //use flag beacon
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Show Flag Beacon";
    item->description = "Draws a beacon at the pin position, visible from a distance";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.showBeacon = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No", "Yes" };
    item->selectedIndex = m_sharedData.showBeacon ? 1 : 0;

    //beacon colour
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Beacon Colour";
    item->description = "Choose the colour of the beacon";
    item->activated = [&](Menu::Item& i)
        {
            const float amt = 0.1f * i.selectedIndex;
            m_sharedData.beaconColour = amt;

            //set the preview colour
            const cro::Detail::ColourLowP c = getBeaconColour(m_sharedData.beaconColour);
            m_beaconPreview.update(&c);
        };
    item->count = 10; //hmmm why don't I infer this from the size of the label vector?
    item->labels = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" };
    item->selectedIndex = static_cast<std::int32_t>(std::floor(m_sharedData.beaconColour * 9.f));
    item->displayType = Menu::Item::Slider;

    //TODO set this on a sub-tex of some other texture
    const auto c = getBeaconColour(m_sharedData.beaconColour);
    cro::Image img;
    img.create(1, 1, c);
    m_beaconPreview.loadFromImage(img);
    item->texture = &m_beaconPreview;
    item->uv = { 0.f, 0.f, 1.f, 1.f };


    //use ball trail
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Show Ball Trail";
    item->description = "Draw a trail behind player's ball when it's in flight";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.showBallTrail = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No", "Yes" };
    item->selectedIndex = m_sharedData.showBallTrail ? 1 : 0;

    //ball trail colour
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Ball Trail Uses Beacon Colour";
    item->description = "Draws the ball trail with the beacon colour, else draws it white";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.trailBeaconColour = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No", "Yes" };
    item->selectedIndex = m_sharedData.trailBeaconColour ? 1 : 0;

    //putting grid density
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Putting Grid Density";
    item->description = "Sets the transparency of the putting grid";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            const float amt = 0.1f * i.selectedIndex;
            m_sharedData.gridTransparency = amt;
        };
    item->count = 11;
    item->labels = { "0.0", "0.1", "0.2", "0.3", "0.4", "0.5", "0.6", "0.7", "0.8", "0.9", "1.0" };
    item->selectedIndex = static_cast<std::int32_t>(std::floor(m_sharedData.gridTransparency * 10.f));
    item->displayType = Menu::Item::Slider;


    //use imperial measurements
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Use Imperial Measurements";
    item->description = "Render distances in Yards, Feet and Inches instead of Metres and Centimetres";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.imperialMeasurements = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No", "Yes" };
    item->selectedIndex = m_sharedData.imperialMeasurements ? 1 : 0;

    //large power bar
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Use Large Power Bar";
    item->description = "Draws a larger power bar at the bottom ofthe UI";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.useLargePowerBar = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No" , "Yes"  };
    item->selectedIndex = m_sharedData.useLargePowerBar ? 1 : 0;

    //high contrast power bar
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "High Contrast Power Bar";
    item->description = "Draws the power bar with inverted colours";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.useContrastPowerBar = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.useContrastPowerBar ? 1 : 0;


    //decimated power bar
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Decimate Power Bar";
    item->description = "Draws a power bar with 10 segements instead of 8";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.decimatePowerBar = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.decimatePowerBar ? 1 : 0;


    //decimalised distances
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Use Decimalised Distances";
    item->description = "Distances are drawn to the nearest 10th of a metre or yard";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.decimateDistance = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.decimateDistance ? 1 : 0;


    //monthly rival
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Show Monthly Rival";
    item->description = "Shows the current monthly best on the scoreboard, if available";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.showRival = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.showRival ? 1 : 0;


    //follow cam when putting
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Follow Cam When Putting";
    item->description = "The camera follows the ball when putting instead of displaying an overhead view";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.puttFollowCam = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.puttFollowCam ? 1 : 0;


    //zoom follow cam
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Zoom Follow Cam";
    item->description = "Zoom the follow cam when the ball is in flight for a closer view";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.zoomFollowCam = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.zoomFollowCam ? 1 : 0;


    //rotate when aiming
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Rotate When Aiming";
    item->description = "Automatically rotate the player camera when aiming";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.rotateCamera = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.rotateCamera ? 1 : 0;

    
    //flag selection
    const auto selectionCallback =
        [&](const Menu::Item& i)
        {
            m_detailsPane.image.getComponent<cro::Sprite>().setTexture(m_flagPreview.getTexure());
            m_detailsPane.image.getComponent<cro::Sprite>().setTextureRect(m_flagPreview.getUV());
            m_detailsPane.image.getComponent<cro::Transform>().setOrigin({ m_flagPreview.getSize().x / 2.f, 0.f });
            m_detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
        };

    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Flag Selection";
#ifdef USE_GNS
    item->description = "More flags are available in the Steam Workshop";
#else
    item->description = "Select the flag's appearance";
#endif
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            //cycle through flags
            m_flagPreview.setIndex(i.selectedIndex);
            m_detailsPane.image.getComponent<cro::Sprite>().setTextureRect(m_flagPreview.getUV());

            m_detailsPane.image.getComponent<cro::Transform>().setOrigin({ m_flagPreview.getSize().x / 2.f, 0.f });
            m_sharedData.flagPath = m_flagPreview.getPath();
        };
    item->count = m_flagPreview.getCount();
    for (auto i = 0; i < item->count; ++i)
    {
        item->labels.push_back("Flag " + std::to_string(i));
    }
    item->selectedIndex = m_flagPreview.getIndex();
    item->selected = selectionCallback;

    //flag text type
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Flag Text";
    item->description = "Choose how text is displayed on the flag";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.flagText = i.selectedIndex;
            m_flagPreview.setText(m_sharedData.flagText);
            m_detailsPane.image.getComponent<cro::Sprite>().setTexture(m_flagPreview.getTexure());
            m_detailsPane.image.getComponent<cro::Sprite>().setTextureRect(m_flagPreview.getUV());
        };
    item->count = 3;
    item->labels = { "None" , "Black", "White"};
    item->selectedIndex = m_sharedData.flagText;
    item->selected = selectionCallback;


    //post FX selection (none as an option)
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
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
    item->count = static_cast<std::int32_t>(ShaderNames.size() + 1);
    for (const auto& name : ShaderNames)
    {
        item->labels.push_back(name);
    }
    item->labels.push_back("None");
    item->selectedIndex = m_sharedData.usePostProcess ? m_sharedData.postProcessIndex : static_cast<std::int32_t>(ShaderNames.size());


    //TODO tee ball colour


    //lens flare
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Show Lens Flare";
    item->description = "Display a lens flare effect in sunny weather";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.useLensFlare = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.useLensFlare ? 1 : 0;

    //reduced motion transition
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Reduced Motion Transition";
    item->description = "Hides the hole transition behind a loading screen to reduce motion sensitivity";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.miniLoadingScreen = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.miniLoadingScreen ? 1 : 0;



    //-------difficulty and behaviour-----//
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Gameplay Settings";
    item->displayType = Menu::Item::Heading;


    //putt assist
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Use Putt Assist";
    item->description = "Show a small flag above the power bar when putting to estimate the range";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.showPuttingPower = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.showPuttingPower ? 1 : 0;
    
    
    //fixed range putter
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Fixed Range Putter";
    item->description = "Fixes the max range of the putter at 10m/33ft";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.fixedPuttingRange = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.fixedPuttingRange ? 1 : 0;



    //precise range indicator
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Estimated Range Indicator";
    item->description = "Increases difficulty by omitting elevation and wind from the range indicator prediction";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.calculateRange = i.selectedIndex == 0 ? true : false;
        };
    item->count = 2;
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.calculateRange ? 0 : 1;



    //minimal UI
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Minimal UI";
    item->description = "Increases difficulty by removing most of the UI elements";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.showMinimap = i.selectedIndex == 0 ? true : false;
        };
    item->count = 2;
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.showMinimap ? 0 : 1;



    //in-game tips
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Display In-Game Tips";
    item->description = "Shows tips when playing on how to best take your shot";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.showInGameTips = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.showInGameTips ? 1 : 0;

  


    //----------config settings---------//
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Configuration";
    item->displayType = Menu::Item::Heading;


    //web socket
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Enable Web Socket";
    item->description = "See https://github.com/fallahn/svs for more info";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.webSocket = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.webSocket ? 1 : 0;

    //TODO hmmmm we need to be able to set the port...


    //CSV logging
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Log Scores To CSV";
    item->description = "Files are saved to you user directory";
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.logCSV = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.logCSV ? 1 : 0;


    //disable chat
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Disable Chat";
    item->description = "Removes the in-game chat from multiplayer games";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.blockChat = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.blockChat ? 1 : 0;


    //log chat
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Log Chat To File";
    item->description = "Logs in-game multiplayer chat to a text file in your user directory";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.logChat = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.logChat ? 1 : 0;



    //enable remote content
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Download Remote Content";
    item->description = "Allow downloading remote content eg Workshop items in multiplayer";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.remoteContent = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No" , "Yes" };
    item->selectedIndex = m_sharedData.remoteContent ? 1 : 0;




    //--------set background colour for these EG yellow/red for warning!!-------//
    //reset hints
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Reset Hints";
    item->description = "Enable all in-game hints which were previously dismissed";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.showClubUpdate = true;
            m_sharedData.showRosterTip = true;
            m_sharedData.showTutorialTip = true;

            m_detailsPane.text.getComponent<cro::Text>().setString("Tutorials Reset!");
        };
    item->count = 1;
    item->labels = { "OK" };
    item->selectedIndex = 0;


    //reset career
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Reset Career";
    item->description = "Resets all Career progress, preserving any unlocked items";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.errorMessage = "reset_career";
            requestStackPush(StateID::MessageOverlay);
        };
    item->count = 1;
    item->labels = { "OK" };
    item->selectedIndex = 0;


    //reset profile
    item = &m_menuLayout.items[TabBar::Item::Settings].emplace_back();
    item->title = "Reset Profile";
    item->description = "WARNING Resets all progress and unlocked items!!";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.errorMessage = "reset_profile";
            requestStackPush(StateID::MessageOverlay);
        };
    item->count = 1;
    item->labels = { "OK" };
    item->selectedIndex = 0;
}

void OptionsStateV2::createKeyboardItems()
{
    //config
    auto* item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Configuration";
    item->displayType = Menu::Item::Heading;

    
    //mouse button for action
    item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Use Left Mouse as Action Button";
    item->description = "Clicking left mouse button performs the same as the Action key";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.useMouseAction = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No", "Yes" };
    item->selectedIndex = m_sharedData.useMouseAction ? 1 : 0;

    //hold for power
    item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Hold Action For Power";
    item->description = "Press and hold the Action key to choose swing power instead of the traditional 3-click system";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.pressHold = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No", "Yes" };
    item->selectedIndex = m_sharedData.pressHold ? 1 : 0;


    
    
    //keybinds
    item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Key Bindings";
    item->displayType = Menu::Item::Heading;

    cro::String keybindDesc = "Press Enter to select a new key";
    cro::Util::String::wordWrap(keybindDesc, 36);


    //prev club
    auto itemIndex = static_cast<std::int32_t>(m_menuLayout.items[TabBar::Item::Keyboard].size());
    item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Previous Club";
    item->description = keybindDesc;
    item->activated = [&, itemIndex](Menu::Item& i)
        {
            m_detailsPane.text.getComponent<cro::Text>().setString("Press a key");
            m_keybindIndex = InputBinding::PrevClub;
            m_keybindItemIndex = itemIndex;
        };
    item->count = 1;
    item->labels = { "Key: " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::PrevClub])};
    item->selectedIndex = 0;

    //next club
    itemIndex = static_cast<std::int32_t>(m_menuLayout.items[TabBar::Item::Keyboard].size());
    item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Next Club";
    item->description = keybindDesc;
    item->activated = [&, itemIndex](Menu::Item& i)
        {
            m_detailsPane.text.getComponent<cro::Text>().setString("Press a key");
            m_keybindIndex = InputBinding::NextClub;
            m_keybindItemIndex = itemIndex;
        };
    item->count = 1;
    item->labels = { "Key: " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::NextClub]) };
    item->selectedIndex = 0;

    //aim left
    itemIndex = static_cast<std::int32_t>(m_menuLayout.items[TabBar::Item::Keyboard].size());
    item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Aim Left";
    item->description = keybindDesc;
    item->activated = [&, itemIndex](Menu::Item& i)
        {
            m_detailsPane.text.getComponent<cro::Text>().setString("Press a key");
            m_keybindIndex = InputBinding::Left;
            m_keybindItemIndex = itemIndex;
        };
    item->count = 1;
    item->labels = { "Key: " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Left]) };
    item->selectedIndex = 0;

    //aim right
    itemIndex = static_cast<std::int32_t>(m_menuLayout.items[TabBar::Item::Keyboard].size());
    item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Aim Right";
    item->description = keybindDesc;
    item->activated = [&, itemIndex](Menu::Item& i)
        {
            m_detailsPane.text.getComponent<cro::Text>().setString("Press a key");
            m_keybindIndex = InputBinding::Right;
            m_keybindItemIndex = itemIndex;
        };
    item->count = 1;
    item->labels = { "Key: " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Right]) };
    item->selectedIndex = 0;

    //camera up
    itemIndex = static_cast<std::int32_t>(m_menuLayout.items[TabBar::Item::Keyboard].size());
    item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Camera Up";
    item->description = keybindDesc;
    item->activated = [&, itemIndex](Menu::Item& i)
        {
            m_detailsPane.text.getComponent<cro::Text>().setString("Press a key");
            m_keybindIndex = InputBinding::Up;
            m_keybindItemIndex = itemIndex;
        };
    item->count = 1;
    item->labels = { "Key: " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Up]) };
    item->selectedIndex = 0;

    //camera down
    itemIndex = static_cast<std::int32_t>(m_menuLayout.items[TabBar::Item::Keyboard].size());
    item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Camera Down";
    item->description = keybindDesc;
    item->activated = [&, itemIndex](Menu::Item& i)
        {
            m_detailsPane.text.getComponent<cro::Text>().setString("Press a key");
            m_keybindIndex = InputBinding::Down;
            m_keybindItemIndex = itemIndex;
        };
    item->count = 1;
    item->labels = { "Key: " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Down]) };
    item->selectedIndex = 0;

    //action
    itemIndex = static_cast<std::int32_t>(m_menuLayout.items[TabBar::Item::Keyboard].size());
    item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Action (Take Shot)";
    item->description = keybindDesc;
    item->activated = [&, itemIndex](Menu::Item& i)
        {
            m_detailsPane.text.getComponent<cro::Text>().setString("Press a key");
            m_keybindIndex = InputBinding::Action;
            m_keybindItemIndex = itemIndex;
        };
    item->count = 1;
    item->labels = { "Key: " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Action]) };
    item->selectedIndex = 0;

    //spin menu
    itemIndex = static_cast<std::int32_t>(m_menuLayout.items[TabBar::Item::Keyboard].size());
    item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Show Spin Menu";
    item->description = keybindDesc;
    item->activated = [&, itemIndex](Menu::Item& i)
        {
            m_detailsPane.text.getComponent<cro::Text>().setString("Press a key");
            m_keybindIndex = InputBinding::SpinMenu;
            m_keybindItemIndex = itemIndex;
        };
    item->count = 1;
    item->labels = { "Key: " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::SpinMenu]) };
    item->selectedIndex = 0;

    //emote wheel
    itemIndex = static_cast<std::int32_t>(m_menuLayout.items[TabBar::Item::Keyboard].size());
    item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Show Emote Wheel";
    item->description = keybindDesc;
    item->activated = [&, itemIndex](Menu::Item& i)
        {
            m_detailsPane.text.getComponent<cro::Text>().setString("Press a key");
            m_keybindIndex = InputBinding::EmoteMenu;
            m_keybindItemIndex = itemIndex;
        };
    item->count = 1;
    item->labels = { "Key: " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::EmoteMenu]) };
    item->selectedIndex = 0;

    //cancel shot
    itemIndex = static_cast<std::int32_t>(m_menuLayout.items[TabBar::Item::Keyboard].size());
    item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Cancel Shot In Progress";
    item->description = keybindDesc;
    item->activated = [&, itemIndex](Menu::Item& i)
        {
            m_detailsPane.text.getComponent<cro::Text>().setString("Press a key");
            m_keybindIndex = InputBinding::CancelShot;
            m_keybindItemIndex = itemIndex;
        };
    item->count = 1;
    item->labels = { "Key: " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::CancelShot]) };
    item->selectedIndex = 0;





    //fixed keys
    item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Fixed Keys";
    item->displayType = Menu::Item::Heading;

    item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Measure Putt";
    item->subTitle = "Displays a distance widget to meaure the green when putting";
    item->description = "Key: Number 1 (Top Row)";
    item->displayType = Menu::Item::TextOnly;

    item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Freecam";
    item->subTitle = "Enter freecam / photo mode";
    item->description = "Key: Number 2 (Top Row)";
    item->displayType = Menu::Item::TextOnly;

    item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Rotate Camera Left";
    item->description = "Key: Number 3 (Top Row)";
    item->displayType = Menu::Item::TextOnly;

    item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Rotate Camera Right";
    item->description = "Key: Number 4 (Top Row)";
    item->displayType = Menu::Item::TextOnly;

    item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Zoom Minimap";
    item->description = "Key: Number 5 (Top Row)";
    item->displayType = Menu::Item::TextOnly;

    item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Show Scores";
    item->description = "Key: Tab";
    item->displayType = Menu::Item::TextOnly;

    item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Open Menu";
    item->description = "Key: Escape";
    item->displayType = Menu::Item::TextOnly;

    item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Toggle Ball Labels";
    item->subTitle = "Show or hide player name labels when putting";
    item->description = "Key: F2";
    item->displayType = Menu::Item::TextOnly;

    item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Toggle UI";
    item->description = "Key: F3";
    item->displayType = Menu::Item::TextOnly;

    item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Toggle Chat";
    item->subTitle = "Show the in-game chat window";
    item->description = "Key: F4";
    item->displayType = Menu::Item::TextOnly;

    item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Take Screenshot";
    item->description = "Key: F5";
    item->displayType = Menu::Item::TextOnly;

    item = &m_menuLayout.items[TabBar::Item::Keyboard].emplace_back();
    item->title = "Toggle Putting Grid";
    item->description = "Key: F7";
    item->displayType = Menu::Item::TextOnly;
}

void OptionsStateV2::createControllerItems()
{
    //TODO input sensitivity
    //TODO thumbstick deadzone
    //TODO set detail image based on input activity
    //TODO set detail text to controller list (how to show activity?)

    //invert X axis
    auto* item = &m_menuLayout.items[TabBar::Item::Controller].emplace_back();
    item->title = "Invert X axis";
    item->description = "Invert the controller X axis when in camera mode";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.invertX = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No", "Yes" };
    item->selectedIndex = m_sharedData.invertX ? 1 : 0;

    //invert Y axis
    item = &m_menuLayout.items[TabBar::Item::Controller].emplace_back();
    item->title = "Invert Y axis";
    item->description = "Invert the controller Y axis when in camera mode";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.invertY = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No", "Yes" };
    item->selectedIndex = m_sharedData.invertY ? 1 : 0;

    //enable swingput
    item = &m_menuLayout.items[TabBar::Item::Controller].emplace_back();
    item->title = "Enable Swingput";
    item->description = "With either trigger held, pull back on a thumbstick to charge the power. Push forward on the stick to take your shot. Timing is important!";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.useSwingput = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No", "Yes" };
    item->selectedIndex = m_sharedData.useSwingput ? 1 : 0;

    //vibration
    item = &m_menuLayout.items[TabBar::Item::Controller].emplace_back();
    item->title = "Use Vibration";
    item->description = "Enable vibration effects on supported controllers";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.enableRumble = i.selectedIndex;
        };
    item->count = 2;
    item->labels = { "No", "Yes" };
    item->selectedIndex = m_sharedData.enableRumble;

    item = &m_menuLayout.items[TabBar::Item::Controller].emplace_back();
    item->title = "Hold Action For Power";
    item->description = "Press and hold the Action key to choose swing power instead of the traditional 3-click system";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.pressHold = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No", "Yes" };
    item->selectedIndex = m_sharedData.pressHold ? 1 : 0;

#ifdef USE_GNS

    item = &m_menuLayout.items[TabBar::Item::Controller].emplace_back();
    item->title = "Rebind Buttons";
    item->description = "Review a Steam guide on rebinding the controller buttons with Steam Input";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            Social::showControllerBinding();
        };
    item->count = 1;
    item->labels = { "View Steam Guide" };

#endif
}

void OptionsStateV2::createDisplayItems()
{
    for (auto i = 0; i < 5; ++i)
    {
        auto& item = m_menuLayout.items[3].emplace_back();
        item.title = "Dummy Item";
        item.description = "This is the item description for " + std::to_string(i + 1);
        item.activated = [](Menu::Item& i) {LogI << "Callback!" << std::endl; };
        item.count = cro::Util::Random::value(2, 4);
        for (auto j = 0; j < item.count; ++j)
        {
            item.labels.push_back("Option " + std::to_string(j + 1));
        }
    }
}

void OptionsStateV2::createAudioItems()
{
    m_tabBar.items[TabBar::Item::Audio].displayWidth = 0.8f;
    m_tabBar.items[TabBar::Item::Audio].alignment = TabBar::Item::Centre;

    auto* item = &m_menuLayout.items[TabBar::Item::Audio].emplace_back();
    item->title = "Configuration";
    item->displayType = Menu::Item::Heading;

    //device selection - WARNING we have a hack when handling device
    //disconnect events which indexes this directly!! Don't chage the index!
    item = &m_menuLayout.items[TabBar::Item::Audio].emplace_back();
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
            cro::AudioDevice::setActiveDevice(devices[it.selectedIndex]);
        };
    refreshAudioDevices(*item);


    //text to speech
    item = &m_menuLayout.items[TabBar::Item::Audio].emplace_back();
    item->title = "Use Text To Speech for Chat";
    item->description = "Enable text to speech playback for in-game chat";
    cro::Util::String::wordWrap(item->description, 36);
    item->activated = [&](Menu::Item& i)
        {
            m_sharedData.useTTS = i.selectedIndex == 0 ? false : true;
        };
    item->count = 2;
    item->labels = { "No", "Yes" };
    item->selectedIndex = m_sharedData.useTTS ? 1 : 0;


    //mixer
    item = &m_menuLayout.items[TabBar::Item::Audio].emplace_back();
    item->title = "Volume Levels";
    item->displayType = Menu::Item::Heading;

    for (auto i = 0; i < MixerChannel::Count; ++i)
    {
        auto& item = m_menuLayout.items[TabBar::Item::Audio].emplace_back();
        item.title = MixerLabels[i];
        //item.description = "This is the item description for " + std::to_string(i + 1);
        item.activated = 
            [i](Menu::Item& it)
            {
                cro::AudioMixer::setVolume(static_cast<float>(it.selectedIndex) / 10.f, i);
            };
        item.count = 11;
        for (auto j = 0; j < item.count; ++j)
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
    m_tabBar.items[TabBar::Item::Achievements].displayWidth = 0.8f;
    m_tabBar.items[TabBar::Item::Achievements].alignment = TabBar::Item::Centre;

    //TODO display progress of achievements
    //which are based on stats.

    for(const auto& s : AchievementStrings)
    {
        auto icon = Achievements::getIcon(s);
        const auto* ach = Achievements::getAchievement(s);

        if (ach)
        {
            auto& item = m_menuLayout.items[TabBar::Item::Achievements].emplace_back();
            
            if (!ach->achieved && AchievementDesc[ach->id].second)
            {
                item.title = "Hidden Achievement";
            }
            else
            {
                item.title = ach->name;
                item.subTitle = AchievementDesc[ach->id].first;

                if (ach->achieved)
                {
                    item.subTitle += "\nUnlocked: " + cro::SysTime::dateString(ach->timestamp);
                }
            }
            item.texture = icon.texture;
            item.uv = icon.textureRect;

            const auto texSize = glm::vec2(icon.texture->getSize());
            item.uv.left *= texSize.x;
            item.uv.width *= texSize.x;
            item.uv.bottom *= texSize.y;
            item.uv.height *= texSize.y;

            item.displayType = Menu::Item::TextOnly;
            item.count = 0;
        }
    }
}

void OptionsStateV2::createStatItems()
{
    m_tabBar.items[TabBar::Item::Stats].displayWidth = 0.8f;
    m_tabBar.items[TabBar::Item::Stats].alignment = TabBar::Item::Centre;

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
            auto& item = m_menuLayout.items[TabBar::Item::Stats].emplace_back();
            item.title = StatLabels[stat->id];
            item.subTitle = formatValue(StatTypes[stat->id], stat->value);
            item.count = 0;
            item.displayType = Menu::Item::TextOnly;
        }
    }
}

void OptionsStateV2::onCachedPush()
{
    refreshView();

    m_rootNode.getComponent<cro::Callback>().active = true;
}

void OptionsStateV2::onCachedPop()
{

}

void OptionsStateV2::resetRepeatTimer(cro::Time resetTime)
{
    m_inputRepeatClock.restart();
    m_repeatTime = resetTime;
}

void OptionsStateV2::updateTabBar()
{
    const glm::vec2 WindowSize = cro::App::getWindow().getSize();

    const float Spacing = 1.f / (TabBar::Item::Count + 2); //leave equivalent of a tab either end
    const float TabWidth = std::round(Spacing * WindowSize.x);

    std::vector<cro::Vertex2D> verts;
    const auto viewScale = cro::UIElementSystem::getViewScale();
    
    if (m_uiTexture)
    {
        const auto width = TabWidth - viewScale;
        const auto height = TabBarHeight * viewScale;

        const auto addQuad = 
            [&](glm::vec2 position, const SpriteSection& left, const SpriteSection& right)
            {
                const auto sectionWidth = left.size.x * viewScale;

                //left section
                verts.emplace_back(glm::vec2(position.x, position.y + height), glm::vec2(left.uv.left, left.uv.height));
                verts.emplace_back(position, glm::vec2(left.uv.left, left.uv.bottom));
                verts.emplace_back(glm::vec2(position.x + sectionWidth, position.y + height), glm::vec2(left.uv.width, left.uv.height));
                verts.emplace_back(glm::vec2(position.x + sectionWidth, position.y + height), glm::vec2(left.uv.width, left.uv.height));
                verts.emplace_back(position, glm::vec2(left.uv.left, left.uv.bottom));
                verts.emplace_back(glm::vec2(position.x + sectionWidth, position.y), glm::vec2(left.uv.width, left.uv.bottom));


                //middle section
                position.x += sectionWidth;
                const auto centreWidth = (TabWidth - (sectionWidth * 2.f));
                verts.emplace_back(glm::vec2(position.x, position.y + height), glm::vec2(left.uv.width, left.uv.height));
                verts.emplace_back(position, glm::vec2(left.uv.width, left.uv.bottom));
                verts.emplace_back(glm::vec2(position.x + centreWidth, position.y + height), glm::vec2(right.uv.left, right.uv.height));
                verts.emplace_back(glm::vec2(position.x + centreWidth, position.y + height), glm::vec2(right.uv.left, right.uv.height));
                verts.emplace_back(position, glm::vec2(left.uv.width, left.uv.bottom));
                verts.emplace_back(glm::vec2(position.x + centreWidth, position.y), glm::vec2(right.uv.left, right.uv.bottom));


                //right section
                position.x += centreWidth;
                verts.emplace_back(glm::vec2(position.x, position.y + height), glm::vec2(right.uv.left, right.uv.height));
                verts.emplace_back(position, glm::vec2(right.uv.left, right.uv.bottom));
                verts.emplace_back(glm::vec2(position.x + sectionWidth, position.y + height), glm::vec2(right.uv.width, right.uv.height));
                verts.emplace_back(glm::vec2(position.x + sectionWidth, position.y + height), glm::vec2(right.uv.width, right.uv.height));
                verts.emplace_back(position, glm::vec2(right.uv.left, right.uv.bottom));
                verts.emplace_back(glm::vec2(position.x + sectionWidth, position.y), glm::vec2(right.uv.width, right.uv.bottom));
            };

        for (auto i = 0u; i < m_tabBar.items.size(); ++i)
        {
            const auto active = i == m_tabBar.activeIndex;
            const auto hovered = (i == m_tabBar.hoveredIndex && m_sharedData.activeInput == SharedStateData::ActiveInput::Keyboard);

            const float kludgeOffset = (2.f * viewScale);
            glm::vec2 position = { (TabWidth + kludgeOffset) + ((i * TabWidth) + viewScale), 0.f };
            if (active)
            {
                addQuad(position, m_tabActive[0], m_tabActive[1]);
            }
            else if(hovered)
            {
                addQuad(position, m_tabHighlight[0], m_tabHighlight[1]);
            }
            else
            {
                addQuad(position, m_tabInactive[0], m_tabInactive[1]);
            }

            //set the text
            position += glm::vec2(m_tabBar.background.getComponent<cro::Transform>().getPosition());
            position += WindowSize / 2.f; //screen centre
            m_tabBar.items[i].hitbox = { position, glm::vec2(width, height)};
            m_tabBar.items[i].text.getComponent<cro::Text>().setFillColour(active ? TextNormalColour :
                hovered ? CD32::Colours[CD32::Yellow] : CD32::Colours[CD32::BeigeMid]);
        }

        //add a quad to the verts as an underline
        const auto backgroundCentre = m_backgroundSections[BackgroundSection::Centre].uv;
        const glm::vec2 uv0(backgroundCentre.left, backgroundCentre.bottom);
        const glm::vec2 uv1(backgroundCentre.width, backgroundCentre.height);
        verts.emplace_back(glm::vec2(0.f, 0.f), glm::vec2(uv0.x, uv1.y));
        verts.emplace_back(glm::vec2(0.f, -viewScale), uv0);
        verts.emplace_back(glm::vec2(WindowSize.x, 0.f), uv1);
        verts.emplace_back(glm::vec2(WindowSize.x, 0.f), uv1);
        verts.emplace_back(glm::vec2(0.f, -viewScale), uv0);
        verts.emplace_back(glm::vec2(WindowSize.x, -viewScale), glm::vec2(uv1.x, uv0.y));
    }
    else
    {
        const auto addQuad =
            [&](cro::Colour c, glm::vec2 position, glm::vec2 size)
            {
                verts.emplace_back(glm::vec2(position.x, position.y + size.y), c);
                verts.emplace_back(position, c);
                verts.emplace_back(position + size, c);

                verts.emplace_back(position + size, c);
                verts.emplace_back(position, c);
                verts.emplace_back(glm::vec2(position.x + size.x, position.y), c);
            };

        //update the verts for the tab bar.
        for (auto i = 0u; i < m_tabBar.items.size(); ++i)
        {
            const auto active = i == m_tabBar.activeIndex;
            const auto hovered = (i == m_tabBar.hoveredIndex && m_sharedData.activeInput == SharedStateData::ActiveInput::Keyboard);

            const auto colour = active ? CD32::Colours[CD32::Brown] :
                hovered ?
                CD32::Colours[CD32::Yellow] : CD32::Colours[CD32::TanDarkest];

            glm::vec2 position = { TabWidth + (i * TabWidth), 0.f };
            const glm::vec2 size = { TabWidth - viewScale, TabBarHeight * viewScale };
            addQuad(colour, position, size);

            position += glm::vec2(m_tabBar.background.getComponent<cro::Transform>().getPosition());
            position += WindowSize / 2.f; //screen centre
            m_tabBar.items[i].hitbox = { position, size };
            m_tabBar.items[i].text.getComponent<cro::Text>().setFillColour(active ? TextNormalColour :
                hovered ? CD32::Colours[CD32::Black] : CD32::Colours[CD32::BeigeMid]);
        }

        addQuad(CD32::Colours[CD32::Brown], { 0.f, -viewScale }, { WindowSize.x, viewScale });
    }

    m_tabBar.background.getComponent<cro::Drawable2D>().setVertexData(verts);

    switch (m_tabBar.items[m_tabBar.activeIndex].alignment)
    {
    default:
    case TabBar::Item::Left:
        m_menuLayout.sprite.getComponent<cro::Transform>().setPosition({ 0.f, 0.f });

        m_detailsPane.root.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        m_detailsPane.root.getComponent<cro::UIElement>().relativePosition.x = 0.25f;
        break;
    case TabBar::Item::Centre:
    {
        const float x = std::round((WindowSize.x - (static_cast<float>(m_menuLayout.texture.getSize().x * cro::UIElementSystem::getViewScale()) * m_tabBar.items[m_tabBar.activeIndex].displayWidth)) / 2.f);
        m_menuLayout.sprite.getComponent<cro::Transform>().setPosition({ x, 0.f });

        m_detailsPane.root.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    }
        break;
    case TabBar::Item::Right:
    {
        const float x = std::round(WindowSize.x - (static_cast<float>(m_menuLayout.texture.getSize().x * cro::UIElementSystem::getViewScale()) * m_tabBar.items[m_tabBar.activeIndex].displayWidth));
        m_menuLayout.sprite.getComponent<cro::Transform>().setPosition({ x, 0.f });

        m_detailsPane.root.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        m_detailsPane.root.getComponent<cro::UIElement>().relativePosition.x = -0.25f;
    }
        break;
    }
    m_menuLayout.sprite.getComponent<cro::Transform>().move(-WindowSize / 2.f);
    
    resizeItemGraphics();
    updateMenuItems();
}

void OptionsStateV2::nextTab()
{
    m_tabBar.activeIndex = (m_tabBar.activeIndex + 1) % TabBar::Item::Count;
    m_menuLayout.itemIndex = 0;
    refreshView();
    
    playSound(MenuSoundEvent::Activate);
}

void OptionsStateV2::prevTab()
{
    m_tabBar.activeIndex = (m_tabBar.activeIndex + (TabBar::Item::Count - 1)) % TabBar::Item::Count;
    refreshView();
    m_menuLayout.itemIndex = 0;
    
    playSound(MenuSoundEvent::Cancel);
}

void OptionsStateV2::resizeItemGraphics()
{
    //this is all done 1:1 as the ui element/nodes scale this for us

    const auto& items = m_menuLayout.items[m_tabBar.activeIndex];
    const auto viewScale = cro::UIElementSystem::getViewScale();

    //calc max texture size and resize first if necessary
    const auto texHeight = static_cast<std::uint32_t>(((ItemHeight + ItemSpacing) * items.size() + ItemSpacing));
    const auto texWidth = static_cast<std::uint32_t>(static_cast<float>(cro::App::getWindow().getSize().x) / viewScale);

    if (!m_menuLayout.texture.available()
        || texWidth > m_menuLayout.texture.getSize().x
        || texHeight > m_menuLayout.texture.getSize().y)
    {
        m_menuLayout.texture.create(texWidth, texHeight, false);
    }


    //update all the item backgrounds based on current window size and selected tab
    //these aren't scaled by view size here - the target they're rendered to is
    glm::vec2 renderSize = glm::vec2(m_menuLayout.texture.getSize());
    renderSize.x = std::round(renderSize.x * m_tabBar.items[m_tabBar.activeIndex].displayWidth);

    std::vector<cro::Vertex2D> verts;
    const auto calcVerts =
        [&](const SpriteSection& left, const SpriteSection& right)
        {
            glm::vec2 position(0.f);
            verts.emplace_back(glm::vec2(position.x, left.size.y), glm::vec2(left.uv.left, left.uv.height));
            verts.emplace_back(position, glm::vec2(left.uv.left, left.uv.bottom));
            verts.emplace_back(glm::vec2(position.x + left.size.x, left.size.y), glm::vec2(left.uv.width, left.uv.height));
            verts.emplace_back(glm::vec2(position.x + left.size.x, left.size.y), glm::vec2(left.uv.width, left.uv.height));
            verts.emplace_back(position, glm::vec2(left.uv.left, left.uv.bottom));
            verts.emplace_back(glm::vec2(position.x + left.size.x, position.y), glm::vec2(left.uv.width, left.uv.bottom));

            
            position.x += left.size.x;
            const auto centreWidth = renderSize.x - (left.size.x * 2.f);
            verts.emplace_back(glm::vec2(position.x, left.size.y), glm::vec2(left.uv.width, left.uv.height));
            verts.emplace_back(position, glm::vec2(left.uv.width, left.uv.bottom));
            verts.emplace_back(glm::vec2(position.x + centreWidth, left.size.y), glm::vec2(right.uv.left, right.uv.height));
            verts.emplace_back(glm::vec2(position.x + centreWidth, left.size.y), glm::vec2(right.uv.left, right.uv.height));
            verts.emplace_back(position, glm::vec2(left.uv.width, left.uv.bottom));
            verts.emplace_back(glm::vec2(position.x + centreWidth, position.y), glm::vec2(right.uv.left, right.uv.bottom));


            position.x += centreWidth;
            verts.emplace_back(glm::vec2(position.x, right.size.y), glm::vec2(right.uv.left, right.uv.height));
            verts.emplace_back(position, glm::vec2(right.uv.left, right.uv.bottom));
            verts.emplace_back(glm::vec2(position.x + right.size.x, right.size.y), glm::vec2(right.uv.width, right.uv.height));
            verts.emplace_back(glm::vec2(position.x + right.size.x, right.size.y), glm::vec2(right.uv.width, right.uv.height));
            verts.emplace_back(position, glm::vec2(right.uv.left, right.uv.bottom));
            verts.emplace_back(glm::vec2(position.x + right.size.x, position.y), glm::vec2(right.uv.width, right.uv.bottom));
        };

    calcVerts(m_itemSection[0], m_itemSection[1]);
    m_itemBackground.setVertexData(verts);

    verts.clear();
    calcVerts(m_itemActiveSection[0], m_itemActiveSection[1]);
    m_itemBackgroundActive.setVertexData(verts);

    verts.clear();
    calcVerts(m_itemActiveHighlightSection[0], m_itemActiveHighlightSection[1]);
    m_itemBackgroundActiveHighlight.setVertexData(verts);

    verts.clear();
    calcVerts(m_itemHighlightSection[0], m_itemHighlightSection[1]);
    m_itemBackgroundHighlight.setVertexData(verts);

    verts.clear();
    calcVerts(m_itemTitleSection[0], m_itemTitleSection[1]);
    m_itemBackgroundTitle.setVertexData(verts);


    //update detail background
    //TODO this only needs to be a vec2...
    cro::FloatRect backgroundArea = { 0.f, 0.f,
                        static_cast<float>(cro::App::getWindow().getSize().x / 2u),
                        (m_tabBar.background.getComponent<cro::Transform>().getPosition().y - (InfoBarHeight * viewScale)) + (cro::App::getWindow().getSize().y / 2) };

    static constexpr float Padding = 16.f;
    backgroundArea.width -= (Padding * viewScale);
    backgroundArea.height -= (Padding * viewScale);

    backgroundArea.width /= viewScale;
    backgroundArea.height /= viewScale;

    backgroundArea.width = std::round(backgroundArea.width / 2.f);
    backgroundArea.height = std::round(backgroundArea.height / 2.f);

    const float CentreWidth = backgroundArea.width - m_backgroundSections[BackgroundSection::TL].size.x;
    const float CentreHeight = backgroundArea.height - m_backgroundSections[BackgroundSection::TL].size.y;

    verts.clear();

    const auto addQuad = 
        [&](glm::vec2 position, glm::vec2 size, cro::FloatRect uv)
        {
            verts.emplace_back(glm::vec2(position.x, position.y + size.y), glm::vec2(uv.left, uv.height));
            verts.emplace_back(position, glm::vec2(uv.left, uv.bottom));
            verts.emplace_back(position + size, glm::vec2(uv.width, uv.height));

            verts.emplace_back(position + size, glm::vec2(uv.width, uv.height));
            verts.emplace_back(position, glm::vec2(uv.left, uv.bottom));
            verts.emplace_back(glm::vec2(position.x + size.x, position.y), glm::vec2(uv.width, uv.bottom));
        };

    //top left
    glm::vec2 p(-backgroundArea.width, CentreHeight);
    addQuad(p, m_backgroundSections[BackgroundSection::TL].size, m_backgroundSections[BackgroundSection::TL].uv);

    //top right
    p = { CentreWidth, CentreHeight };
    addQuad(p, m_backgroundSections[BackgroundSection::TR].size, m_backgroundSections[BackgroundSection::TR].uv);

    //bottom left
    p = { -backgroundArea.width, -backgroundArea.height };
    addQuad(p, m_backgroundSections[BackgroundSection::BL].size, m_backgroundSections[BackgroundSection::BL].uv);

    //bottom right
    p = { CentreWidth, -backgroundArea.height };
    addQuad(p, m_backgroundSections[BackgroundSection::BR].size, m_backgroundSections[BackgroundSection::BR].uv);


    //top
    p = { -CentreWidth, CentreHeight };
    glm::vec2 size = { CentreWidth * 2.f, m_backgroundSections[BackgroundSection::Top].size.y };
    cro::FloatRect uv = 
    {
        m_backgroundSections[BackgroundSection::TL].uv.width,
        m_backgroundSections[BackgroundSection::TL].uv.bottom,
        m_backgroundSections[BackgroundSection::TR].uv.left,
        m_backgroundSections[BackgroundSection::TR].uv.height
    };
    addQuad(p, size, uv);

    //bottom
    p = { -CentreWidth, -backgroundArea.height };
    uv =
    {
        m_backgroundSections[BackgroundSection::BL].uv.width,
        m_backgroundSections[BackgroundSection::BL].uv.bottom,
        m_backgroundSections[BackgroundSection::BR].uv.left,
        m_backgroundSections[BackgroundSection::BR].uv.height
    };
    addQuad(p, size, uv);

    //left
    p = { -backgroundArea.width, -CentreHeight };
    size = { m_backgroundSections[BackgroundSection::Left].size.x, CentreHeight * 2.f };
    uv =
    {
        m_backgroundSections[BackgroundSection::BL].uv.left,
        m_backgroundSections[BackgroundSection::BL].uv.height,
        m_backgroundSections[BackgroundSection::TL].uv.width,
        m_backgroundSections[BackgroundSection::TL].uv.bottom
    };
    addQuad(p, size, uv);

    //right
    p = { CentreWidth, -CentreHeight };
    uv =
    {
        m_backgroundSections[BackgroundSection::BR].uv.left,
        m_backgroundSections[BackgroundSection::BR].uv.height,
        m_backgroundSections[BackgroundSection::TR].uv.width,
        m_backgroundSections[BackgroundSection::TR].uv.bottom
    };
    addQuad(p, size, uv);

    //centre
    p = { -CentreWidth, -CentreHeight };
    size = { CentreWidth * 2.f, CentreHeight * 2.f };
    addQuad(p, size, m_backgroundSections[BackgroundSection::Centre].uv);

    m_detailsPane.background.getComponent<cro::Drawable2D>().setVertexData(verts);
}

void OptionsStateV2::updateSliderGraphic(std::int32_t amt, std::int32_t total)
{
    std::vector<cro::Vertex2D> verts;

    if (total)
    {
        static constexpr float SliderWidth = 40.f;
        static constexpr float SliderHeight = 6.f;
        const float amtNorm = static_cast<float>(amt) / total;

        const float width = std::round(amtNorm * (SliderWidth * 2.f));
        static constexpr cro::Colour c = CD32::Colours[CD32::Red];
        static constexpr cro::Colour d = CD32::Colours[CD32::BlueDarkest];

        verts =
        {
            cro::Vertex2D(glm::vec2(-(SliderWidth + 1.f), SliderHeight + 1.f), CD32::Colours[CD32::TanDarkest]),
            cro::Vertex2D(glm::vec2(-(SliderWidth + 1.f), -1.f), CD32::Colours[CD32::TanDarkest]),
            cro::Vertex2D(glm::vec2((SliderWidth + 1.f), SliderHeight + 1.f), CD32::Colours[CD32::TanDarkest]),
            cro::Vertex2D(glm::vec2((SliderWidth + 1.f), SliderHeight + 1.f), CD32::Colours[CD32::TanDarkest]),
            cro::Vertex2D(glm::vec2(-(SliderWidth + 1.f), -1.f), CD32::Colours[CD32::TanDarkest]),
            cro::Vertex2D(glm::vec2((SliderWidth + 1.f), -1.f), CD32::Colours[CD32::TanDarkest]),

            cro::Vertex2D(glm::vec2(-(SliderWidth), SliderHeight), CD32::Colours[CD32::Olive]),
            cro::Vertex2D(glm::vec2(-(SliderWidth), -1.f), CD32::Colours[CD32::Olive]),
            cro::Vertex2D(glm::vec2((SliderWidth + 1.f), SliderHeight), CD32::Colours[CD32::Olive]),
            cro::Vertex2D(glm::vec2((SliderWidth + 1.f), SliderHeight), CD32::Colours[CD32::Olive]),
            cro::Vertex2D(glm::vec2(-(SliderWidth), -1.f), CD32::Colours[CD32::Olive]),
            cro::Vertex2D(glm::vec2((SliderWidth + 1.f), -1.f), CD32::Colours[CD32::Olive]),


            cro::Vertex2D(glm::vec2(-SliderWidth, SliderHeight), c),
            cro::Vertex2D(glm::vec2(-SliderWidth, 0.f), c),
            cro::Vertex2D(glm::vec2(-SliderWidth + width, SliderHeight), c),
            cro::Vertex2D(glm::vec2(-SliderWidth + width, SliderHeight), c),
            cro::Vertex2D(glm::vec2(-SliderWidth, 0.f), c),
            cro::Vertex2D(glm::vec2(-SliderWidth + width, 0.f), c),

            cro::Vertex2D(glm::vec2(-SliderWidth + width, SliderHeight), d),
            cro::Vertex2D(glm::vec2(-SliderWidth + width, 0.f), d),
            cro::Vertex2D(glm::vec2(SliderWidth, SliderHeight), d),
            cro::Vertex2D(glm::vec2(SliderWidth, SliderHeight), d),
            cro::Vertex2D(glm::vec2(-SliderWidth + width, 0.f), d),
            cro::Vertex2D(glm::vec2(SliderWidth, 0.f), d)
        };
    }
    m_itemSlider.setVertexData(verts);
}

void OptionsStateV2::updateMenuItems()
{
    //NOTE this is all done 1:1 scale and the resulting sprite set to window scale
    auto& items = m_menuLayout.items[m_tabBar.activeIndex];
    const auto viewScale = cro::UIElementSystem::getViewScale();


    //if we didn't resize the actual size might be bigger than we expect
    //on other tabs...
    glm::vec2 renderSize = glm::vec2(m_menuLayout.texture.getSize());
    renderSize.x = std::round(renderSize.x * m_tabBar.items[m_tabBar.activeIndex].displayWidth);

    m_menuLayout.sprite.getComponent<cro::Sprite>().setTexture(m_menuLayout.texture.getTexture());
    m_menuLayout.sprite.getComponent<cro::Transform>().setScale(glm::vec2(viewScale));

    cro::FloatRect crop = { 0.f, InfoBarHeight * viewScale,
                            static_cast<float>(cro::App::getWindow().getSize().x),
                            (m_tabBar.background.getComponent<cro::Transform>().getPosition().y - (InfoBarHeight * viewScale)) + (cro::App::getWindow().getSize().y / 2)};
    m_menuLayout.sprite.getComponent<cro::Drawable2D>().setCroppingArea(crop, true);

    m_menuText.setFillColour(TextNormalColour);

    constexpr float LineSpacing = 12.f;
    const auto renderItem =
        [&](Menu::Item& item, glm::vec2 pos, std::int32_t idx)
        {
            auto* background = &m_itemBackground;
            if (idx == m_menuLayout.hoveredIndex
                && m_sharedData.activeInput == SharedStateData::ActiveInput::Keyboard)
            {
                background = idx == m_menuLayout.itemIndex ? &m_itemBackgroundActiveHighlight : &m_itemBackgroundHighlight;
            }
            else if (idx == m_menuLayout.itemIndex)
            {
                background = &m_itemBackgroundActive;
            }

            if (item.displayType == Menu::Item::Heading)
            {
                m_itemBackgroundTitle.setPosition(pos);
                m_itemBackgroundTitle.draw();
            }
            else
            {
                background->setPosition(pos);
                background->draw();
            }

            if (item.texture)
            {
                m_menuQuad.setTexture(*item.texture);
                m_menuQuad.setScale(ItemImage / glm::vec2(item.uv.width, item.uv.height));
                m_menuQuad.setPosition(pos + glm::vec2(ItemSpacing, ItemSpacing));
                m_menuQuad.setTextureRect(item.uv);

                m_menuQuad.draw();
                pos.x += (ItemSpacing * 2.f) + ItemImage.x;
            }

            pos.x += ItemSpacing;
            pos.y += ItemHeight - LineSpacing;

            /*if (idx == m_menuLayout.hoveredIndex)
            {
                m_menuText.setFillColour(CD32::Colours[CD32::Black]);
                m_menuTextLarge.setFillColour(CD32::Colours[CD32::Black]);
            }
            else */
            if (idx == m_menuLayout.itemIndex
                || idx == m_menuLayout.hoveredIndex)
            {
                m_menuText.setFillColour(CD32::Colours[CD32::Yellow]);
                m_menuTextLarge.setFillColour(CD32::Colours[CD32::Yellow]);
            }
            else
            {
                m_menuText.setFillColour(TextNormalColour);
                m_menuTextLarge.setFillColour(TextNormalColour);
            }

            if (item.displayType != Menu::Item::Heading)
            {
                m_menuText.setPosition(pos);
                m_menuText.setString(item.title);
                m_menuText.draw();
            }

            switch (item.displayType)
            {
            case Menu::Item::Slider:
                updateSliderGraphic(item.selectedIndex, item.count - 1);
                m_itemSlider.setPosition({ renderSize.x / 2.f, pos.y - (LineSpacing * 1.7f) });
                m_itemSlider.draw();
                [[fallthrough]];
            default:
                if (item.displayType == Menu::Item::Slider)
                {
                    m_menuTextLarge.setPosition({ renderSize.x / 2.f, pos.y - (LineSpacing - 4.f) });
                }
                else
                {
                    m_menuTextLarge.setPosition({ renderSize.x / 2.f, pos.y - (LineSpacing * 1.7f) });
                }

                if (item.labels.size() > 1)
                {
                    m_menuTextLarge.setString("< " + item.labels[item.selectedIndex] + " >");
                }
                else
                {
                    //this is a button
                    m_menuTextLarge.setString(item.labels[item.selectedIndex]);
                }
                m_menuTextLarge.draw();

                {
                    static constexpr float HitPadding = 4.f;
                    item.hitbox = m_menuTextLarge.getLocalBounds();
                    item.hitbox.left += m_menuTextLarge.getPosition().x;
                    item.hitbox.left -= HitPadding;
                    item.hitbox.bottom += m_menuTextLarge.getPosition().y;
                    item.hitbox.bottom -= HitPadding;
                    item.hitbox.width += (2.f * HitPadding);
                    item.hitbox.height += (2.f * HitPadding);
                }
                break;
            case Menu::Item::TextOnly:
                if (!item.description.empty())
                {
                    m_menuTextLarge.setPosition({ renderSize.x / 2.f, pos.y - (LineSpacing * 1.7f) });
                    m_menuTextLarge.setString(item.description);
                    m_menuTextLarge.draw();
                }
                else
                {
                    m_menuText.move({ 0.f, -(LineSpacing - 1.f) });
                    m_menuText.setString(item.subTitle);
                    m_menuText.draw();
                }
                break;
            case Menu::Item::Heading:
                m_menuTextLarge.setPosition({ renderSize.x / 2.f, pos.y - std::round(LineSpacing * 1.7f) });
                m_menuTextLarge.setString(item.title);
                m_menuTextLarge.setFillColour(TextNormalColour);
                m_menuTextLarge.draw();
                break;
            }
            
        };

    constexpr float Stride = ItemHeight + ItemSpacing;
    glm::vec2 pos = { ItemSpacing, renderSize.y - Stride };

    //hide the preview image and let the selection callback
    //display/update it as needed.
    m_detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);

    m_menuLayout.texture.clear(cro::Colour::Transparent);
    //render current item selection to render texture
    //this includes either setting item highlight colour or rendering a highlight box
    auto i = 0;
    for (auto& item : items)
    {
        if (i == m_menuLayout.itemIndex)
        {
            m_detailsPane.text.getComponent<cro::Text>().setString(item.description);
            if (item.selected)
            {
                item.selected(item);
            }
        }

        //TODO we could skip rendering if this is outside
        //the visible area, but it's not presenting a problem yet.
        renderItem(item, pos, i++);
        pos.y -= Stride;
    }

    m_menuLayout.texture.display();

    m_menuLayout.itemBox = { 0.f, 0.f, renderSize.x - (ItemSpacing * 2.f), ItemHeight };
    m_menuLayout.itemBox *= viewScale;
}

void OptionsStateV2::nextItem()
{
    //reset mouse hover highlight
    m_menuLayout.hoveredIndex = -1;

    m_menuLayout.itemIndex = (m_menuLayout.itemIndex + 1) % m_menuLayout.items[m_tabBar.activeIndex].size();
    updateMenuItems();

    playSound(MenuSoundEvent::Switch);
}

void OptionsStateV2::prevItem()
{
    //reset mouse hover highlight
    m_menuLayout.hoveredIndex = -1;

    m_menuLayout.itemIndex = static_cast<std::uint32_t>((m_menuLayout.itemIndex + (m_menuLayout.items[m_tabBar.activeIndex].size() - 1)) % m_menuLayout.items[m_tabBar.activeIndex].size());
    updateMenuItems();

    playSound(MenuSoundEvent::Switch);
}

void OptionsStateV2::activateLeft()
{
    //reset mouse hover highlight
    m_menuLayout.hoveredIndex = -1;

    if (m_menuLayout.items[m_tabBar.activeIndex][m_menuLayout.itemIndex].activateLeft())
    {
        updateMenuItems();
        playSound(MenuSoundEvent::Cancel);
    }
}

void OptionsStateV2::activateRight()
{
    //reset mouse hover highlight
    m_menuLayout.hoveredIndex = -1;

    if (m_menuLayout.items[m_tabBar.activeIndex][m_menuLayout.itemIndex].activateRight())
    {
        updateMenuItems();
        playSound(MenuSoundEvent::Activate);
    }
}

void OptionsStateV2::activate()
{
    if (m_menuLayout.items[m_tabBar.activeIndex][m_menuLayout.itemIndex].activate())
    {
        playSound(MenuSoundEvent::Activate);
    }
}

void OptionsStateV2::checkMouseOver(glm::vec2 screenPos)
{
    std::int32_t selectedTab = -1;
    std::int32_t selectedItem = -1;

    if (screenPos.y > m_tabBar.background.getComponent<cro::Transform>().getWorldPosition().y)
    {
        //check the tab bar
        for (auto i = 0u; i < m_tabBar.items.size(); ++i)
        {
            if (m_tabBar.items[i].hitbox.contains(screenPos))
            {
                selectedTab = static_cast<std::int32_t>(i);
                break;
            }
        }   
    }
    else
    {
        const auto viewScale = cro::UIElementSystem::getViewScale();

        //check the item list - TODO only check against visible
        const glm::vec2 WindowOffset = cro::App::getWindow().getSize() / 2u;
        glm::vec2 basePos = m_menuLayout.sprite.getComponent<cro::Transform>().getPosition();
        basePos += WindowOffset;
        basePos.y -= m_menuLayout.sprite.getComponent<cro::Transform>().getOrigin().y * viewScale;

        const auto menuHeight = static_cast<float>(m_menuLayout.texture.getSize().y);

        for (auto i = 0u; i < m_menuLayout.items[m_tabBar.activeIndex].size(); ++i)
        {
            //TODO skip this if it's outside the drawable area
            const float vertOffset = (menuHeight - ((i * (ItemHeight + ItemSpacing))) - (ItemHeight + ItemSpacing)) * viewScale;
            auto testBox = m_menuLayout.itemBox;
            testBox.left += basePos.x;
            testBox.bottom += basePos.y + vertOffset;

            if (testBox.contains(screenPos))
            {
                selectedItem = i;
                break;
            }
        }
    }


    //we may have switched from tab to item list so we still need to redraw
    if (selectedTab != m_tabBar.hoveredIndex)
    {
        m_tabBar.hoveredIndex = selectedTab;
        updateTabBar();
    }

    if (selectedItem != m_menuLayout.hoveredIndex)
    {
        m_menuLayout.hoveredIndex = selectedItem;
        updateMenuItems();
    }
}

void OptionsStateV2::doMouseClick(glm::vec2 mousePos)
{
    if (m_tabBar.hoveredIndex != -1)
    {
        m_tabBar.activeIndex = m_tabBar.hoveredIndex;
        m_tabBar.hoveredIndex = -1;
        m_menuLayout.itemIndex = 0;
        updateTabBar();

        playSound(MenuSoundEvent::Activate);
    }
    else
    {
        if (m_menuLayout.hoveredIndex != -1)
        {
            m_menuLayout.itemIndex = m_menuLayout.hoveredIndex;
            m_menuLayout.hoveredIndex = -1;
            updateMenuItems();

            playSound(MenuSoundEvent::Activate);
        }
        //else
        {
            //this is the active item, test for activation click
            const auto testbox = m_menuLayout.sprite.getComponent<cro::Transform>().getWorldTransform() *
                m_menuLayout.items[m_tabBar.activeIndex][m_menuLayout.itemIndex].hitbox;
            const auto testpos = m_scene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(mousePos);

            if (testbox.contains(testpos))
            {
                activate();

                const float xPos = testpos.x - testbox.left;
                if (xPos < testbox.width / 2.f)
                {
                    activateLeft();
                }
                else
                {
                    activateRight();
                }
            }
        }
    }
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
        m_detailsPane.text.getComponent<cro::Text>().setString(msg);

        return;
    }


    auto& keys = m_sharedData.inputBinding.keys;
    if (auto result = std::find(keys.begin(), keys.end(), key); result != keys.end())
    {
        cro::String msg = cro::Keyboard::keyString(key);
        msg += " is already bound. Press a key";
        cro::Util::String::wordWrap(msg, 36);
        m_detailsPane.text.getComponent<cro::Text>().setString(msg);

        return;
    }


    keys[m_keybindIndex] = key;

    //m_detailsPane.text.getComponent<cro::Text>().setString(
    //    "Key: " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[m_keybindIndex]));
    
    m_menuLayout.items[TabBar::Item::Keyboard][m_keybindItemIndex].labels[0] = 
        "Key: " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[m_keybindIndex]);

    playSound(MenuSoundEvent::Activate);
    updateMenuItems();

    m_keybindIndex = -1;
    m_keybindItemIndex = -1;
}

void OptionsStateV2::cancelKeybind()
{
    playSound(MenuSoundEvent::Cancel);

    m_detailsPane.text.getComponent<cro::Text>().setString("Press Enter to select a new key");

    m_keybindIndex = -1;
    m_keybindItemIndex = -1;
}

void OptionsStateV2::refreshAudioDevices(Menu::Item& item)
{
    item.labels.clear();

    std::string str;
    auto deviceList = cro::AudioDevice::getDeviceList();
    if (deviceList.empty())
    {
        item.count = 0;
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

    item.count = static_cast<std::int32_t>(deviceList.size());
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

void OptionsStateV2::refreshView()
{
    updateTabBar();
}

void OptionsStateV2::quitState()
{
    m_rootNode.getComponent<cro::Callback>().active = true;
    playSound(MenuSoundEvent::Cancel);
}