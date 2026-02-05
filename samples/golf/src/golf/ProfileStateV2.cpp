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

#include "ProfileStateV2.hpp"
#include "SharedStateData.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "MessageIDs.hpp"
#include "CallbackData.hpp"
#include "../GolfGame.hpp"

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
#include <crogine/ecs/components/ParticleEmitter.hpp>

#include <crogine/ecs/systems/UIElementSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/ParticleSystem.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>

#include <crogine/util/Easings.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/OpenGL.hpp>

#include <filesystem>

namespace
{
#include "shaders/ProgressShader.inl"
    constexpr glm::vec3 BallPos = glm::vec3({ 10.f, 0.f, 0.f });
    constexpr glm::vec3 CamPosAvatar = glm::vec3({ 0.f, 1.f, -1.5f });
    constexpr glm::vec3 CamPosHead = glm::vec3({ 0.f, 1.6f, -0.35f });

    //static const cro::String XboxInfo = cro::String(ButtonX) + " Show Credits   " + cro::String(ButtonY) + " How To Play   " + cro::String(ButtonB) + " Close";
    //static const cro::String PSInfo = cro::String(ButtonSquare) + " Show Credits   " + cro::String(ButtonCross) + " How To Play   " + cro::String(ButtonCircle) + " Close";
    static const cro::String KeyInfo = "LCtrl (Hold) - Save and Close   ESC (Hold) - Cancel Changes";

    const std::array ItemLabels =
    {
        "Avatar", "Headwear",
        "Equipment", "Loadouts", "Details"
    };

    constexpr auto BackgroundDark = cro::Colour(0xc8b89faf);
    constexpr auto BackgroundYellow = cro::Colour(0xf2cf5caf);
    constexpr auto BackgroundRed = cro::Colour(0xb83530af);

    constexpr float DetailBackgroundPadding = 16.f;
    constexpr float DetailBackgroundOffset = DetailBackgroundPadding / 4.f;

    constexpr std::size_t WordWrapLarge = 42;
    constexpr std::size_t WordWrapSmall = 36;

    static constexpr cro::Time RepeatTimeLong = cro::seconds(0.5f);
    static constexpr cro::Time RepeatTimeShort = cro::seconds(0.05f);

    void playSound(std::int32_t id)
    {
        cro::App::postMessage<MenuSoundEvent>(cl::MessageID::MenuSoundMessage)->type = id;
    }
}

using namespace UI;

ProfileStateV2::ProfileStateV2(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd, SharedProfileData& profileData)
    : cro::State        (ss, ctx),
    m_scene             (ctx.appInstance.getMessageBus(), 192),
    m_previewScene      (ctx.appInstance.getMessageBus(), 192),
    m_sharedData        (sd),
    m_profileData       (profileData),
    m_exitHoldTimer     (0.f),
    m_exitFlags         (0),
    m_progressUniform   (-1),
    m_avatarIndex       (0),
    m_lockedAvatarCount (0),
    m_uiTexture         (nullptr)
{
    ctx.mainWindow.setMouseCaptured(false);

    std::fill(m_controllerMasks.begin(), m_controllerMasks.end(), 0);
    std::fill(m_controllerPrevMasks.begin(), m_controllerPrevMasks.end(), 0);

    m_tabBar.items.resize(TabID::Count);
    m_menuLayout.items.resize(TabID::Count);

    loadAssets();
    buildPreviewScene(); //make sure models are loaded first so menu creation can read the data
    buildScene();
}

//public
bool ProfileStateV2::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse
        || m_rootNode.getComponent<cro::Callback>().active)
    {
        return false;
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

                m_tabBar.navLeft.getComponent<cro::Text>().setString("< " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::PrevClub]));
                m_tabBar.navRight.getComponent<cro::Text>().setString(cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::NextClub]) + " >");

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


    if (evt.type == SDL_KEYUP)
    {
        setActiveInput(true, 0);

        if (evt.key.keysym.sym == SDLK_BACKSPACE
            || evt.key.keysym.sym == SDLK_ESCAPE)
        {
            /*quitState();
            return false;*/
            m_exitFlags &= ~ExitFlagQuit;
        }
        else if (evt.key.keysym.sym == SDLK_LCTRL)
        {
            m_exitFlags &= ~ExitFlagSave;
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::NextClub])
        {
            nextTab();
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::PrevClub])
        {
            prevTab();
        }

        //done on key down event for repeat when held
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

        else if (evt.key.keysym.sym == SDLK_BACKSPACE
            || evt.key.keysym.sym == SDLK_ESCAPE)
        {
            /*quitState();
            return false;*/
            m_exitFlags |= ExitFlagQuit;
        }
        else if (evt.key.keysym.sym == SDLK_LCTRL)
        {
            m_exitFlags |= ExitFlagSave;
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
            prevItem();
            resetRepeatTimer(controllerID, RepeatTimeLong);
            break;
        case cro::GameController::DPadDown:
            nextItem();
            resetRepeatTimer(controllerID, RepeatTimeLong);
            break;
        case cro::GameController::ButtonB:
            m_exitFlags |= ExitFlagQuit;
            break;
        case cro::GameController::ButtonX:
            m_exitFlags |= ExitFlagSave;
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
        case cro::GameController::ButtonA:
            activate();
            break;
        case cro::GameController::ButtonB:
            /*quitState();
            return false;*/
            m_exitFlags &= ~ExitFlagQuit;
            break;
        case cro::GameController::ButtonX:
            m_exitFlags &= ~ExitFlagSave;
            break;
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
            //quitState();
            //return false;
            m_exitFlags &= ~ExitFlagQuit;
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONDOWN)
    {
        if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            m_exitFlags |= ExitFlagQuit;
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
        constexpr std::int16_t Threshold = std::numeric_limits<std::int16_t>::max() / 2;// cro::GameController::LeftThumbDeadZone * 2;// 15000;
        const auto controllerID = cro::GameController::controllerID(evt.caxis.which);
        
        if (std::abs(evt.caxis.value) > Threshold)
        {
            setActiveInput(false, controllerID);
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
            prevItem();
        }
        else if (evt.wheel.y < 0)
        {
            nextItem();
        }
    }

    else if (evt.type == SDL_CONTROLLERDEVICEADDED
        || evt.type == SDL_CONTROLLERDEVICEREMOVED)
    {
        //refreshControllerDevices();
        //*sigh* the names aren't updated until AFTER the event
        //so we have to delay a frame or 2.
        //auto entity = m_scene.createEntity();
        //entity.addComponent<cro::Callback>().active = true;
        //entity.getComponent<cro::Callback>().setUserData<std::int32_t>(2);
        //entity.getComponent<cro::Callback>().function =
        //    [&](cro::Entity e, float)
        //    {
        //        auto& c = e.getComponent<cro::Callback>().getUserData<std::int32_t>();
        //        c--;

        //        if (c == 0)
        //        {
        //            refreshControllerDevices();

        //            e.getComponent<cro::Callback>().active = false;
        //            m_scene.destroyEntity(e);
        //        }
        //    };

        ////make sure to reset all timers etc
        //std::fill(m_controllerMasks.begin(), m_controllerMasks.end(), 0);
        //std::fill(m_controllerPrevMasks.begin(), m_controllerMasks.end(), 0);

        //for (auto i = 0; i < 4; ++i)
        //{
        //    resetRepeatTimer(i, RepeatTimeLong);
        //}
    }

    //m_scene.getSystem<cro::UISystem>()->handleEvent(evt);
    m_previewScene.forwardEvent(evt);
    m_scene.forwardEvent(evt);
    return false;
}

void ProfileStateV2::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            //hack to force the texture to resize properly
            m_menuLayout.texture.create(1, 1, false);
            refreshView();
        }
    }
    m_previewScene.forwardMessage(msg);
    m_scene.forwardMessage(msg);
}

bool ProfileStateV2::simulate(float dt)
{
    //press/hold to exist
    static constexpr float MaxHoldTime = 0.5f;
    if (m_exitFlags)
    {
        m_exitHoldTimer = std::min(m_exitHoldTimer + dt, MaxHoldTime);
        
        if (m_exitHoldTimer >= MaxHoldTime)
        {
            if (m_exitFlags == ExitFlagQuit)
            {
                quitState();
            }
            else if (m_exitFlags == ExitFlagSave)
            {
                //copy av data back to proper data and write files                
                m_profileData.playerProfiles[m_profileData.activeProfileIndex] = m_activeProfile;
                m_profileData.playerProfiles[m_profileData.activeProfileIndex].playerData.saveProfile();

                /*if (m_mugshotUpdated)
                {
                    auto path = Content::getUserContentPath(Content::UserContent::Profile) + m_activeProfile.playerData.profileID + "/mug.png";
                    m_mugshotTexture.getTexture().saveToFile(path);

                    m_activeProfile.playerData.mugshot = path;
                    m_profileData.playerProfiles[m_profileData.activeProfileIndex].playerData.mugshot = path;
                    m_profileData.playerProfiles[m_profileData.activeProfileIndex].playerData.saveProfile();

                    m_mugshotUpdated = false;
                }

                if (m_activeProfile.playerData.isSteamID
                    && m_activeProfile.playerData.name != Social::getPlayerName())
                {
                    Social::setPlayerName(m_activeProfile.playerData.name);
                }
                */
                LogI << FILE_LINE << " TODO" << std::endl;
                quitState();
            }
            else
            {
                //mode than one button held, so invalid
                m_exitFlags = 0;
                m_exitHoldTimer = 0.f;
            }
        }
    }
    else
    {
        m_exitHoldTimer = 0.f;
    }

    glUseProgram(m_progressShader.getGLHandle());
    glUniform1f(m_progressUniform, m_exitHoldTimer / MaxHoldTime);


    //rotate preview
    const auto rotateModel =
        [&](float v)
        {
            m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, v);
        };
    constexpr auto threshold = std::numeric_limits<std::int16_t>::max() / 2;
    if (const auto v = cro::GameController::getAxisPosition(0, cro::GameController::TriggerLeft);
                v > threshold)
    {
        const float speed = static_cast<float>(v) / std::numeric_limits<std::uint16_t>::max();
        rotateModel(-dt * speed * 1.2f);
    }
    if (const auto v = cro::GameController::getAxisPosition(0, cro::GameController::TriggerRight);
                v > threshold)
    {
        const float speed = static_cast<float>(v) / std::numeric_limits<std::uint16_t>::max();
        rotateModel(dt * speed * 1.2f);
    }
    if (cro::Keyboard::isKeyPressed(SDLK_1))
    {
        rotateModel(-dt);
    }
    if (cro::Keyboard::isKeyPressed(SDLK_2))
    {
        rotateModel(dt);
    }


    scrollToTarget(m_tabBar, m_menuLayout, dt);

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
            resetRepeatTimer(i, RepeatTimeLong);
        }

        if (maskTest(i, InputFlag::Down))
        {
            nextItem();
            resetRepeatTimer(i, RepeatTimeLong);
        }

        m_controllerPrevMasks[i] = m_controllerMasks[i];
        
        //check for repeat inputs
        if (cro::GameController::isButtonPressed(i, cro::GameController::DPadDown)
            || (m_controllerMasks[i] & InputFlag::Down))
        {
            if (m_inputRepeatClocks[i].elapsed() > m_repeatTimes[i])
            {
                nextItem();
                resetRepeatTimer(i, RepeatTimeShort);
            }
        }

        if (cro::GameController::isButtonPressed(i, cro::GameController::DPadUp)
            || (m_controllerMasks[i] & InputFlag::Up))
        {
            if (m_inputRepeatClocks[i].elapsed() > m_repeatTimes[i])
            {
                prevItem();
                resetRepeatTimer(i, RepeatTimeShort);
            }
        }
    }

    m_previewScene.simulate(dt);
    m_scene.simulate(dt);
    return true;
}

void ProfileStateV2::render()
{
    //TODO select camera based on tab
    m_previewTexture.clear(cro::Colour::Transparent);
    m_previewScene.render();
    m_previewTexture.display();

    m_scene.render();
}

//private
void ProfileStateV2::loadAssets()
{
    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);
    m_menuText.setFont(font);
    m_menuText.setCharacterSize(InfoTextSize);

    const auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    m_menuTextLarge.setFont(largeFont);
    m_menuTextLarge.setCharacterSize(UITextSize);
    m_menuTextLarge.setAlignment(cro::SimpleText::Alignment::Centre);

    m_itemSlider.setPrimitiveType(GL_TRIANGLES);


    if (m_progressShader.loadFromString(cro::RenderSystem2D::getDefaultVertexShader(), ProgressFrag))
    {
        m_progressUniform = m_progressShader.getUniformID("u_progress");
    }

    m_previewTexture.create(2, 2);


    cro::Image img;
    img.create(1, 1, cro::Colour::White);
    m_colourPreview.loadFromImage(img);

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

        /*m_tabBar.items[TabID::Settings].sprite = spriteSheet.getSprite("settings_icon");
        m_tabBar.items[TabID::Display].sprite = spriteSheet.getSprite("graphics_icon");
        m_tabBar.items[TabID::Keyboard].sprite = spriteSheet.getSprite("keyboard_icon");*/
    }

    /*if (spriteSheet.loadFromFile("assets/golf/sprites/options_images.spt", m_sharedData.sharedResources->textures))
    {
        m_optionIcons[OptionIcon::GridDensity] = spriteSheet.getSprite("grid_density");
    }*/

    //this sets the default image shown on the right when the tab is selected
    //note that if a specific menu item has a Selected callback that it'll override this
    //even if the item sprite itself is set to nothing (therefore the image is hidden)
    m_tabBar.items[TabID::Body].sprite.setTexture(m_previewTexture.getTexture());
    m_tabBar.items[TabID::Headwear].sprite.setTexture(m_previewTexture.getTexture());
}

void ProfileStateV2::buildScene()
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
    const float Spacing = 1.f / (TabID::Count + 1); //leave equivalent of half a tab either end
    for (auto i = 0; i < TabID::Count; ++i)
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
        const float offset = (Spacing/* * 1.5f*/) + (Spacing * i);
        uiElement.resizeCallback = 
            [&, offset](cro::Entity e)
            {
                const auto x = std::ceil((static_cast<float>(cro::App::getWindow().getSize().x) / cro::UIElementSystem::getViewScale()) * offset) + 2.f;
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
            const auto x = std::floor((static_cast<float>(cro::App::getWindow().getSize().x) / cro::UIElementSystem::getViewScale()) * (Spacing / 4.f));
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
            const auto offset = (Spacing * m_tabBar.items.size()) + (Spacing * 0.75f);
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
            const auto x = std::floor((static_cast<float>(cro::App::getWindow().getSize().x) / cro::UIElementSystem::getViewScale()) * (Spacing / 4.f));
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
            const auto offset = (Spacing * m_tabBar.items.size()) + (Spacing * 0.75f);
            const auto x = std::floor((static_cast<float>(cro::App::getWindow().getSize().x) / cro::UIElementSystem::getViewScale()) * offset);
            const auto y = 10.f;
            e.getComponent<cro::UIElement>().absolutePosition = { x,y };
        };
    m_tabBar.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_tabBar.navRightSprite = entity;

    m_menuLayout.sprite = m_scene.createEntity();
    m_menuLayout.sprite.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.2f });
    m_menuLayout.sprite.addComponent<cro::Drawable2D>();
    m_menuLayout.sprite.addComponent<cro::Sprite>();
    rootNode.getComponent<cro::Transform>().addChild(m_menuLayout.sprite.getComponent<cro::Transform>());

    //details window on right side
    m_detailsPane.root = m_scene.createEntity();
    m_detailsPane.root.addComponent<cro::Transform>();
    m_detailsPane.root.addComponent<cro::UIElement>(cro::UIElement::Position, false);
    m_detailsPane.root.getComponent<cro::UIElement>().relativePosition = { 0.f, 0.f }; //this is set set when updating the active tab, might be right or left aligned
    rootNode.getComponent<cro::Transform>().addChild(m_detailsPane.root.getComponent<cro::Transform>());

    //text
    m_detailsPane.text = m_scene.createEntity();
    m_detailsPane.text.addComponent<cro::Transform>();
    m_detailsPane.text.addComponent<cro::Drawable2D>();
    m_detailsPane.text.addComponent<cro::Text>(largeFont);
    m_detailsPane.text.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    m_detailsPane.text.getComponent<cro::Text>().setFillColour(TextNormalColour);
    m_detailsPane.text.addComponent<cro::UIElement>(cro::UIElement::Text, true);
    m_detailsPane.text.getComponent<cro::UIElement>().absolutePosition = { DetailBackgroundOffset, -82.f }; //90
    m_detailsPane.text.getComponent<cro::UIElement>().characterSize = UITextSize;
    m_detailsPane.text.getComponent<cro::UIElement>().verticalSpacing = 3.f;
    m_detailsPane.text.getComponent<cro::UIElement>().depth = 0.2f;
    m_detailsPane.root.getComponent<cro::Transform>().addChild(m_detailsPane.text.getComponent<cro::Transform>());

    //image
    m_detailsPane.image = m_scene.createEntity();
    m_detailsPane.image.addComponent<cro::Transform>();
    m_detailsPane.image.addComponent<cro::Drawable2D>();
    m_detailsPane.image.addComponent<cro::Sprite>();
    m_detailsPane.image.addComponent<cro::UIElement>(cro::UIElement::Sprite, true);
    m_detailsPane.image.getComponent<cro::UIElement>().absolutePosition = { DetailBackgroundOffset, -60.f };
    m_detailsPane.image.getComponent<cro::UIElement>().depth = 0.2f;
    m_detailsPane.root.getComponent<cro::Transform>().addChild(m_detailsPane.image.getComponent<cro::Transform>());

    //background/9 patch
    m_detailsPane.background = m_scene.createEntity();
    m_detailsPane.background.addComponent<cro::Transform>().setOrigin({ 0.f, InfoBarHeight / 2.f });
    m_detailsPane.background.addComponent<cro::Drawable2D>().setTexture(m_uiTexture);
    m_detailsPane.background.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    m_detailsPane.background.addComponent<cro::UIElement>(cro::UIElement::Sprite, true);
    m_detailsPane.background.getComponent<cro::UIElement>().absolutePosition = { DetailBackgroundOffset, 8.f };
    m_detailsPane.background.getComponent<cro::UIElement>().resizeCallback =
        [&](cro::Entity e) 
        {

        };
    m_detailsPane.background.getComponent<cro::UIElement>().depth = -0.3f;
    m_detailsPane.root.getComponent<cro::Transform>().addChild(m_detailsPane.background.getComponent<cro::Transform>());


    //displays an Apply icon if an item requests it
    m_detailsPane.applyButton = m_scene.createEntity();
    m_detailsPane.applyButton.addComponent<cro::Transform>();
    m_detailsPane.applyButton.addComponent<cro::Callback>().active = true;
    m_detailsPane.applyButton.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            e.getComponent<cro::Transform>().setPosition(-(m_detailsPane.backgroundSize / 2.f) * cro::UIElementSystem::getViewScale());
        };
    m_detailsPane.root.getComponent<cro::Transform>().addChild(m_detailsPane.applyButton.getComponent<cro::Transform>());

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
    m_detailsPane.applyButton.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


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
    m_detailsPane.applyButton.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

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
    m_detailsPane.applyButton.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());




    //displays zoom/rotate controls
    auto msgRoot = m_scene.createEntity();
    msgRoot.addComponent<cro::Transform>();
    msgRoot.addComponent<cro::UIElement>(cro::UIElement::Position, false);
    msgRoot.getComponent<cro::UIElement>().relativePosition = { -0.03f, -0.37f };
    //msgRoot.getComponent<cro::UIElement>().absolutePosition = { 0.f, 32.f };
    msgRoot.addComponent<cro::Callback>().active = true;
    msgRoot.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            const float scale = (m_tabBar.activeIndex == TabID::Body
                || m_tabBar.activeIndex == TabID::Headwear)
                ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };
    rootNode.getComponent<cro::Transform>().addChild(msgRoot.getComponent<cro::Transform>());

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("Press 1 or 2 to Rotate");
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

    //TODO set these sprites
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
    


    //menu layouts - needs to be done before updating Tab Bar
    createBodyItems();
    createHeadwearItems();
    createEquipmentItems();
    createLoadoutItems();
    createDetailItems();

    updateTabBar(); //this also updates the menu items


    //info string at the bottom
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    entity.addComponent<cro::Text>(largeFont).setString(KeyInfo);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIElement>(cro::UIElement::Text, true).characterSize = UITextSize;
    entity.getComponent<cro::UIElement>().depth = 0.1f;
    entity.getComponent<cro::UIElement>().absolutePosition = { 22.f, 14.f };
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
    entity.getComponent<cro::UIElement>().absolutePosition = { 20.f, 2.f };
    entity.getComponent<cro::UIElement>().resizeCallback =
        [&](cro::Entity e)
        {
            //0,0 is screen centre so we need to offset origin by half screen size
            auto o = (glm::vec2(cro::App::getWindow().getSize()) / 2.f) / cro::UIElementSystem::getViewScale();
            o.x = std::round(o.x);
            o.y = std::round(o.y);
            e.getComponent<cro::Transform>().setOrigin(o);
        };
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_infoSprite = entity;

    //progress for hold-to-quit
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setShader(&m_progressShader);
    //entity.getComponent<cro::Drawable2D>().setTexture(m_uiTexture);
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
    entity.getComponent<cro::UIElement>().absolutePosition = { 2.f, 2.f };
    entity.getComponent<cro::UIElement>().resizeCallback =
        [&](cro::Entity e)
        {
            auto o = (glm::vec2(cro::App::getWindow().getSize()) / 2.f) / cro::UIElementSystem::getViewScale();
            o.x = std::round(o.x);
            o.y = std::round(o.y);
            e.getComponent<cro::Transform>().setOrigin(o);
        };
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


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


    //tab bar selection callbacks
    m_tabBar.items[TabID::Body].selected =
        [&]()
        {
            for (auto c : m_previewCameras)
            {
                c.getComponent<cro::Camera>().active = false;
            }

            m_previewCameras[PreviewCamera::Avatar].getComponent<cro::Camera>().active = true;
            m_previewScene.setActiveCamera(m_previewCameras[PreviewCamera::Avatar]);

            //transitions the camera if not already in position
            auto ent = m_previewScene.createEntity();
            ent.addComponent<cro::Callback>().active = true;
            ent.getComponent<cro::Callback>().function =
                [&](cro::Entity e, float dt) mutable
                {
                    const auto pos = m_previewCameras[PreviewCamera::Avatar].getComponent<cro::Transform>().getPosition();
                    const auto dir = CamPosAvatar - pos;
                    if (glm::length2(dir) < (0.01f * 0.01f))
                    {
                        m_previewCameras[PreviewCamera::Avatar].getComponent<cro::Transform>().setPosition(CamPosAvatar);
                        e.getComponent<cro::Callback>().active = false;
                        m_previewScene.destroyEntity(e);
                    }
                    else
                    {
                        m_previewCameras[PreviewCamera::Avatar].getComponent<cro::Transform>().move(dir * dt * 10.f);
                    }
                };
            const auto idx = m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Skeleton>().getAnimationIndex("idle_standing");
            m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Skeleton>().play(idx);
        };
    m_tabBar.items[TabID::Headwear].selected =
        [&]()
        {
            for (auto c : m_previewCameras)
            {
                c.getComponent<cro::Camera>().active = false;
            }

            m_previewCameras[PreviewCamera::Avatar].getComponent<cro::Camera>().active = true;
            m_previewScene.setActiveCamera(m_previewCameras[PreviewCamera::Avatar]);

            auto ent = m_previewScene.createEntity();
            ent.addComponent<cro::Callback>().active = true;
            ent.getComponent<cro::Callback>().function =
                [&](cro::Entity e, float dt) mutable
                {
                    const auto pos = m_previewCameras[PreviewCamera::Avatar].getComponent<cro::Transform>().getPosition();
                    const auto dir = CamPosHead - pos;
                    if (glm::length2(dir) < (0.01f * 0.01f))
                    {
                        m_previewCameras[PreviewCamera::Avatar].getComponent<cro::Transform>().setPosition(CamPosHead);
                        e.getComponent<cro::Callback>().active = false;
                        m_previewScene.destroyEntity(e);
                    }
                    else
                    {
                        m_previewCameras[PreviewCamera::Avatar].getComponent<cro::Transform>().move(dir * dt * 20.f);
                    }
                };
            m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Skeleton>().stop();
            m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Skeleton>().gotoFrame(0);
        };
    m_tabBar.items[TabID::Equipment].selected =
        [&]()
        {
            for (auto c : m_previewCameras)
            {
                c.getComponent<cro::Camera>().active = false;
            }

            m_previewCameras[PreviewCamera::Ball].getComponent<cro::Camera>().active = true;
            m_previewScene.setActiveCamera(m_previewCameras[PreviewCamera::Ball]);
        };
}

void ProfileStateV2::buildPreviewScene()
{
    auto& mb = cro::App::getInstance().getMessageBus();
    m_previewScene.addSystem<cro::CallbackSystem>(mb);
    m_previewScene.addSystem<cro::SkeletalAnimator>(mb);
    m_previewScene.addSystem<cro::CameraSystem>(mb);
    m_previewScene.addSystem<cro::ModelRenderer>(mb);
    m_previewScene.addSystem<cro::ParticleSystem>(mb);

    loadAvatarPreviews();
    loadAvatarTextures();
    loadHairModels();
    loadBallModels();

    const auto resize = 
        [&](cro::Camera& cam)
        {
            const auto vpSize = glm::vec2(m_previewTexture.getSize());
            cam.setPerspective(70.f * cro::Util::Const::degToRad, vpSize.x / vpSize.y, 0.01f, 10.f);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };

            //TODO we might have a split view for clubs/balls so this will be camera specific
        };
    auto camEnt = m_previewScene.getActiveCamera();
    camEnt.getComponent<cro::Transform>().setPosition(CamPosAvatar);
    camEnt.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI);
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.05f);

    auto& cam = camEnt.getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    //TODO could set to isStatic to disable continual culling
    //although the tab selected callback should disable all but active
    //cameras anyway.
    resize(cam);
    m_previewCameras[PreviewCamera::Avatar] = camEnt;


    camEnt = m_previewScene.createEntity();
    camEnt.addComponent<cro::Transform>().setPosition({ BallPos.x, 0.03f, -0.05f });
    camEnt.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI);
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.05f);
    auto& ballCam = camEnt.addComponent<cro::Camera>();
    ballCam.resizeCallback = resize;
    resize(ballCam);
    m_previewCameras[PreviewCamera::Ball] = camEnt;


    auto lightEnt = m_previewScene.getSunlight();
    lightEnt.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 2.8f);
    lightEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.8f);
}

void ProfileStateV2::createBodyItems()
{
    m_menuLayout.items[TabID::Body].clear();

    auto* item = &m_menuLayout.items[TabID::Body].emplace_back();
    item->title = "Avatar Settings";
    item->displayType = Menu::Item::Heading;
    item->description = "Customise your avatar";

    //avatar
    item = &m_menuLayout.items[TabID::Body].emplace_back();
    item->title = "Body Type";
    item->activated =
        [&](Menu::Item& i)
        {
            setAvatarIndex(i.selectedIndex);
            i.description = i.labels[i.selectedIndex] + "/" + std::to_string(m_avatarModels.size() - m_lockedAvatarCount);
            m_detailsPane.text.getComponent<cro::Text>().setString(i.description);
            //TODO could add if this is an unlock/workshop model here
        };
    for (auto i = 0u; i < m_avatarModels.size(); ++i)
    {
        if (m_avatarModels[i].previewModel.isValid())
        {
            item->labels.push_back(std::to_string(i + 1));
        }
    }
    item->selectedIndex = m_avatarIndex;
    item->description = item->labels[item->selectedIndex] + "/" + std::to_string(m_avatarModels.size() - m_lockedAvatarCount);

    //colours - somehow Hair ended up with index 0 but hats on index 6...
    for (std::int32_t c = pc::ColourKey::Skin; c < pc::ColourKey::Hat; ++c)
    {
        item = &m_menuLayout.items[TabID::Body].emplace_back();
        item->title = "Colour " + std::to_string(c);
        item->description = "Choose a colour";
        item->activated =
            [&, c](Menu::Item& i)
            {
                m_activeProfile.playerData.avatarFlags[c] = i.selectedIndex;
                const cro::Colour colour = pc::Palette[m_activeProfile.playerData.avatarFlags[c]];

                //set the preview colour
                i.previewColour = colour;

                //update the texture
                m_profileTextures[m_avatarIndex].setColour(pc::ColourKey::Index(c), i.selectedIndex);
                m_profileTextures[m_avatarIndex].apply();
            };

        for (auto i = 0u; i < pc::PairCounts[c]; ++i)
        {
            item->labels.push_back(std::to_string(i + 1));
        }

        item->selectedIndex = m_activeProfile.playerData.avatarFlags[c];
        item->displayType = Menu::Item::Slider;
        item->previewColour = pc::Palette[item->selectedIndex];
        item->texture = &m_colourPreview;
        item->uv = { 0.f, 0.f, 1.f, 1.f };
    }

#ifdef USE_GNS
    //workshop button if steam
    LogI << "Add workshop button!" << std::endl;
#endif
}

void ProfileStateV2::createHeadwearItems()
{
    m_menuLayout.items[TabID::Headwear].clear();

    //hair title
    auto* item = &m_menuLayout.items[TabID::Headwear].emplace_back();
    item->title = "Hair Settings";
    item->displayType = Menu::Item::Heading;
    item->description = "Choose headwear model 01";


    const auto createItems = 
        [&](std::int32_t keyIndex)
        {
            //model selection
            item = &m_menuLayout.items[TabID::Headwear].emplace_back();
            item->title = "Appearance";
            item->activated =
                [&, keyIndex](Menu::Item& i)
                {
                    //hmm we're just creating the same callback twice with
                    //this branch rather than unique callbacks... meh
                    if (keyIndex == pc::ColourKey::Hair)
                    {
                        setHairIndex(i.selectedIndex);
                    }
                    else
                    {
                        setHatIndex(i.selectedIndex);
                    }

                    i.description = i.labels[i.selectedIndex] + "/" + std::to_string(m_avatarHairModels.size());
                    if (!m_sharedData.hairInfo[i.selectedIndex].label.empty())
                    {
                        i.description += " " + m_sharedData.hairInfo[i.selectedIndex].label;
                    }

                    m_detailsPane.text.getComponent<cro::Text>().setString(i.description);
                    //TODO could add if this is an unlock/workshop model here
                    //m_sharedData.hairInfo[i.selectedIndex].type == 1; //1 unlock 2 workshop
                };
            for (auto i = 0u; i < m_avatarHairModels.size(); ++i)
            {
                item->labels.push_back(std::to_string(i + 1));
            }
            item->selectedIndex = keyIndex == pc::ColourKey::Hair ? m_avatarModels[m_avatarIndex].hairIndex : m_avatarModels[m_avatarIndex].hatIndex;
            item->description = item->labels[item->selectedIndex] + "/" + std::to_string(m_avatarHairModels.size());
            if (!m_sharedData.hairInfo[item->selectedIndex].label.empty())
            {
                item->description += " " + m_sharedData.hairInfo[item->selectedIndex].label;
            }

            //colour property
            item = &m_menuLayout.items[TabID::Headwear].emplace_back();
            item->title = "Colour";
            item->description = "Choose a colour";
            item->activated =
                [&, keyIndex](Menu::Item& i)
                {
                    m_activeProfile.playerData.avatarFlags[keyIndex] = i.selectedIndex;
                    const cro::Colour colour = pc::Palette[m_activeProfile.playerData.avatarFlags[keyIndex]];

                    //set the preview colour
                    i.previewColour = colour;

                    //update the shader uniform
                    const auto index = keyIndex == pc::ColourKey::Hair
                        ? m_avatarModels[m_avatarIndex].hairIndex
                        : m_avatarModels[m_avatarIndex].hatIndex;
                    m_avatarHairModels[index].getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", pc::Palette[m_activeProfile.playerData.avatarFlags[keyIndex]]);
                    m_avatarHairModels[index].getComponent<cro::Model>().setMaterialProperty(1, "u_hairColour", pc::Palette[m_activeProfile.playerData.avatarFlags[keyIndex]]);

                };

            for (auto i = 0u; i < pc::PairCounts[keyIndex]; ++i)
            {
                item->labels.push_back(std::to_string(i + 1));
            }

            item->selectedIndex = m_activeProfile.playerData.avatarFlags[keyIndex];
            item->displayType = Menu::Item::Slider;
            item->previewColour = pc::Palette[item->selectedIndex];
            item->texture = &m_colourPreview;
            item->uv = { 0.f, 0.f, 1.f, 1.f };

            //transform properties
            auto offset = PlayerData::HeadwearOffset::HairTx
                + keyIndex == pc::ColourKey::Hair ? 0 : PlayerData::HeadwearOffset::HatTx;

            const std::array<std::string, 3u> LabelA = { "Position","Rotation","Scale" };
            const std::array<std::string, 3u> LabelB = { " X"," Y"," Z" };
            constexpr std::array<float, 3u> MaxValues = { 0.05f, cro::Util::Const::PI, 1.5f };
            constexpr std::array<float, 3u> MinValues = { -0.05f, -cro::Util::Const::PI, 0.05f };
            static constexpr std::array<float, 3u> CentreValues = { 0.f, 0.f, 1.f };
            static constexpr std::int32_t SelectionCount = 24; //number of selections is +/- this

            const auto indexToValue =
                [&](std::int32_t type, std::int32_t selectedIndex)
                {
                    //type is pos/rot/scale selectedIndex is the selection in the widget
                    const auto offsetIndex = selectedIndex - SelectionCount;
                    if (offsetIndex < 0)
                    {
                        return (((CentreValues[type] - MinValues[type]) / SelectionCount) * offsetIndex) + CentreValues[type];
                    }
                    else if (offsetIndex > 0)
                    {
                        return (((MaxValues[type] - CentreValues[type]) / SelectionCount) * offsetIndex) + CentreValues[type];
                    }
                    else
                    {
                        return CentreValues[type];
                    }
                };

            const auto valueToIndex =
                [indexToValue](std::int32_t type, float value)
                {
                    //hmm. would be nice to keep these values somewhere save regenerating them...
                    std::vector<float> values;
                    for (auto i = 0; i < (SelectionCount * 2) + 1; ++i)
                    {
                        values.push_back(indexToValue(type,i));
                    }
                    const auto res = std::lower_bound(values.cbegin(), values.cend(), value);
                    return static_cast<std::size_t>(std::distance(values.cbegin(), res));
                };

            for (auto i = 0; i < 3; ++i)
            {
                //pos, rot, scale
                for (auto j = 0; j < 3; ++j)
                {
                    //x,y,z
                    item = &m_menuLayout.items[TabID::Headwear].emplace_back();
                    item->title = LabelA[i] + LabelB[j];
                    item->displayType = Menu::Item::Slider;

                    for (auto k = 0; k < (SelectionCount * 2) + 1; ++k)
                    {
                        const auto val = indexToValue(i,k);
                        
                        std::stringstream ss;
                        ss.precision(3);
                        ss << std::fixed << val;
                        item->labels.push_back(ss.str());
                    }

                    item->selectedIndex = std::min(item->labels.size() - 1, valueToIndex(i, m_activeProfile.playerData.headwearOffsets[offset][j]));
                    item->activated = 
                        [&, indexToValue, i, j, offset, keyIndex](Menu::Item& item)
                        {
                            const auto val = indexToValue(i, item.selectedIndex);
                            m_activeProfile.playerData.headwearOffsets[offset][j] = val;
                            
                            if (keyIndex == pc::ColourKey::Hair)
                            {
                                applyHeadwearTransform(m_avatarModels[m_avatarIndex].hairIndex, PlayerData::HeadwearOffset::HairTx);
                            }
                            else
                            {
                                applyHeadwearTransform(m_avatarModels[m_avatarIndex].hatIndex, PlayerData::HeadwearOffset::HatTx);
                            }
                        };
                }
                //reset
                auto resetIndex = m_menuLayout.items[TabID::Headwear].size();
                item = &m_menuLayout.items[TabID::Headwear].emplace_back();
                item->title = LabelA[i];
                item->labels.push_back("Reset");
                item->activated =
                    [&,resetIndex,i,offset](Menu::Item& item)
                    {
                        m_activeProfile.playerData.headwearOffsets[offset] = glm::vec3(CentreValues[i]);
                        if (keyIndex == pc::ColourKey::Hair)
                        {
                            applyHeadwearTransform(m_avatarModels[m_avatarIndex].hairIndex, PlayerData::HeadwearOffset::HairTx);
                        }
                        else
                        {
                            applyHeadwearTransform(m_avatarModels[m_avatarIndex].hatIndex, PlayerData::HeadwearOffset::HatTx);
                        }

                        //reset the selected indices of the transform items
                        for (auto j = 1; j < 4; ++j)
                        {
                            m_menuLayout.items[TabID::Headwear][resetIndex - j].selectedIndex = SelectionCount;
                        }
                        updateMenuItems();
                    };

                offset++;
            }
        };
    createItems(pc::ColourKey::Hair);


    //again for hat
    item = &m_menuLayout.items[TabID::Headwear].emplace_back();
    item->title = "Hat Settings";
    item->displayType = Menu::Item::Heading;
    item->description = "Choose headwear model 02";

    createItems(pc::ColourKey::Hat);


#ifdef USE_GNS
        //workshop button if steam
        LogI << FILE_LINE << " implement" << std::endl;
#endif
}

void ProfileStateV2::createEquipmentItems()
{
    m_menuLayout.items[TabID::Equipment].clear();

    auto* item = &m_menuLayout.items[TabID::Equipment].emplace_back();
    item->title = "Equipment Appearance";
    item->displayType = Menu::Item::Heading;
    item->description = "Choose how your equipment appears";

    //TODO puff particles when switching balls

    //ball model
    //ball colour
    //club model
#ifdef USE_GNS
    //workshop button if steam
    LogI << FILE_LINE << " implement me" << std::endl;
#endif
}

void ProfileStateV2::createLoadoutItems()
{
    m_menuLayout.items[TabID::Loadout].clear();

    auto* item = &m_menuLayout.items[TabID::Loadout].emplace_back();
    item->title = "Select Loadout";
    item->displayType = Menu::Item::Heading;
    //item->description = "Choose headwear model 01";

    //each club
    //balls
    //equipment counter
}

void ProfileStateV2::createDetailItems()
{
    m_menuLayout.items[TabID::Details].clear();

    auto* item = &m_menuLayout.items[TabID::Details].emplace_back();
    item->title = "Profile Details";
    item->displayType = Menu::Item::Heading;
    //item->description = "Choose headwear model 01";

    //name
    //description
    //mugshot
    //voice

    //workshop button if steam
}

void ProfileStateV2::onCachedPush()
{
    m_exitFlags = 0;
    m_exitHoldTimer = 0.f;

    //we make a copy of this so we can cancel any modifications
    m_activeProfile = m_profileData.playerProfiles[m_profileData.activeProfileIndex];
    m_activeProfile.loadout.read(m_activeProfile.playerData.profileID);

    //set any initial previews
    setAvatarIndex(indexFromAvatarID(m_activeProfile.playerData.skinID));
    setHairIndex(indexFromHairID(m_activeProfile.playerData.hairID));
    setHatIndex(indexFromHairID(m_activeProfile.playerData.hatID));


    //do this here so each tab is refreshed on push by reading the active profile
    createBodyItems();
    createHeadwearItems();
    createEquipmentItems();
    createLoadoutItems();
    createDetailItems();

    refreshView();

    m_rootNode.getComponent<cro::Callback>().active = true;
}

void ProfileStateV2::onCachedPop()
{

}

void ProfileStateV2::resetRepeatTimer(std::int32_t i, cro::Time resetTime)
{
    m_inputRepeatClocks[i].restart();
    m_repeatTimes[i] = resetTime;
}

void ProfileStateV2::updateTabBar()
{
    const glm::vec2 WindowSize = cro::App::getWindow().getSize();

    const float Spacing = 1.f / (TabID::Count + 1); //leave equivalent of half a tab either end
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
            glm::vec2 position = { (std::round(TabWidth / 2.f) + kludgeOffset) + ((i * TabWidth) + viewScale), 0.f };
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

            glm::vec2 position = { std::round(TabWidth / 2.f) + (i * TabWidth), 0.f };
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

    const auto DetailOffset = (((1.f - m_tabBar.items[m_tabBar.activeIndex].displayWidth) / 2.f) + m_tabBar.items[m_tabBar.activeIndex].displayWidth) - 0.5f;

    switch (m_tabBar.items[m_tabBar.activeIndex].alignment)
    {
    default:
    case TabBar::Item::Left:
        m_menuLayout.sprite.getComponent<cro::Transform>().setPosition({ 0.f, 0.f });

        m_detailsPane.root.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        m_detailsPane.root.getComponent<cro::UIElement>().relativePosition.x = DetailOffset;
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
        const float x = std::round(WindowSize.x - ((static_cast<float>(m_menuLayout.texture.getSize().x) * m_tabBar.items[m_tabBar.activeIndex].displayWidth) * cro::UIElementSystem::getViewScale()));
        m_menuLayout.sprite.getComponent<cro::Transform>().setPosition({ x, 0.f });

        m_detailsPane.root.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        m_detailsPane.root.getComponent<cro::UIElement>().relativePosition.x = -DetailOffset;
    }
        break;
    }
    m_menuLayout.sprite.getComponent<cro::Transform>().move(-WindowSize / 2.f);
    

    //set the detail text alignment based on active tab
    //switch (m_tabBar.activeIndex)
    //{
    //default:
    //    m_detailsPane.text.getComponent<cro::Transform>().setOrigin({ 0.f, 0.f });
    //    break;
    //case TabID::Controller:
    //{
    //    //this is hacky but it means the text only goes out of bounds in the edge
    //    //case where there are 4 controllers and the resolution of the window is one
    //    //of 3 obscure sizes (1176x664, 1600x1024 and 1680x1050 - that I know of)
    //    /*const float Offset = cro::GameController::getControllerCount() > 3 ? 16.f : 0.f;
    //    m_detailsPane.text.getComponent<cro::Transform>().setOrigin({ 0.f, Offset });*/
    //}
    //    break;
    //}


    resizeItemGraphics();
    updateMenuItems();
}

void ProfileStateV2::nextTab()
{
    if (m_detailsPane.tabDetails[m_tabBar.activeIndex].isValid())
    {
        m_detailsPane.tabDetails[m_tabBar.activeIndex].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    }

    m_tabBar.activeIndex = (m_tabBar.activeIndex + 1) % TabID::Count;
    m_menuLayout.itemIndex = 0;

    if (m_detailsPane.tabDetails[m_tabBar.activeIndex].isValid())
    {
        m_detailsPane.tabDetails[m_tabBar.activeIndex].getComponent<cro::Transform>().setScale(glm::vec2(1.f));
    }

    if (m_tabBar.items[m_tabBar.activeIndex].selected)
    {
        m_tabBar.items[m_tabBar.activeIndex].selected();
    }

    refreshView();
    
    playSound(MenuSoundEvent::Activate);
}

void ProfileStateV2::prevTab()
{
    if (m_detailsPane.tabDetails[m_tabBar.activeIndex].isValid())
    {
        m_detailsPane.tabDetails[m_tabBar.activeIndex].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    }

    m_tabBar.activeIndex = (m_tabBar.activeIndex + (TabID::Count - 1)) % TabID::Count;
    m_menuLayout.itemIndex = 0;
    
    if (m_detailsPane.tabDetails[m_tabBar.activeIndex].isValid())
    {
        m_detailsPane.tabDetails[m_tabBar.activeIndex].getComponent<cro::Transform>().setScale(glm::vec2(1.f));
    }

    if (m_tabBar.items[m_tabBar.activeIndex].selected)
    {
        m_tabBar.items[m_tabBar.activeIndex].selected();
    }

    refreshView();
    playSound(MenuSoundEvent::Cancel);
}

void ProfileStateV2::resizeItemGraphics()
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
    float renderSize = static_cast<float>(m_menuLayout.texture.getSize().x);
    renderSize = std::round(renderSize * m_tabBar.items[m_tabBar.activeIndex].displayWidth);

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
            const auto centreWidth = renderSize - (left.size.x * 2.f);
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
    glm::vec2 backgroundArea = { static_cast<float>(cro::App::getWindow().getSize().x - (renderSize * viewScale)),
                        (m_tabBar.background.getComponent<cro::Transform>().getPosition().y - (InfoBarHeight * viewScale)) + (cro::App::getWindow().getSize().y / 2) };

    backgroundArea.x -= (DetailBackgroundPadding * viewScale);
    backgroundArea.y -= (DetailBackgroundPadding * viewScale);

    backgroundArea.x /= viewScale;
    backgroundArea.y /= viewScale;

    backgroundArea.x = std::round(backgroundArea.x / 2.f);
    backgroundArea.y = std::round(backgroundArea.y / 2.f);
    m_detailsPane.backgroundSize = backgroundArea * 2.f;

    const float CentreWidth = backgroundArea.x - m_backgroundSections[BackgroundSection::TL].size.x;
    const float CentreHeight = backgroundArea.y - m_backgroundSections[BackgroundSection::TL].size.y;

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
    glm::vec2 p(-backgroundArea.x, CentreHeight);
    addQuad(p, m_backgroundSections[BackgroundSection::TL].size, m_backgroundSections[BackgroundSection::TL].uv);

    //top right
    p = { CentreWidth, CentreHeight };
    addQuad(p, m_backgroundSections[BackgroundSection::TR].size, m_backgroundSections[BackgroundSection::TR].uv);

    //bottom left
    p = { -backgroundArea.x, -backgroundArea.y };
    addQuad(p, m_backgroundSections[BackgroundSection::BL].size, m_backgroundSections[BackgroundSection::BL].uv);

    //bottom right
    p = { CentreWidth, -backgroundArea.y };
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
    p = { -CentreWidth, -backgroundArea.y };
    uv =
    {
        m_backgroundSections[BackgroundSection::BL].uv.width,
        m_backgroundSections[BackgroundSection::BL].uv.bottom,
        m_backgroundSections[BackgroundSection::BR].uv.left,
        m_backgroundSections[BackgroundSection::BR].uv.height
    };
    addQuad(p, size, uv);

    //left
    p = { -backgroundArea.x, -CentreHeight };
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

    //resize the preview graphic to fit
    m_previewTexture.create(static_cast<std::uint32_t>(CentreWidth*2.f * viewScale), static_cast<std::uint32_t>(CentreHeight*1.25f * viewScale));
    m_tabBar.items[m_tabBar.activeIndex].sprite.setTexture(m_previewTexture.getTexture());

    for (auto camEnt : m_previewCameras)
    {
        //TODO we shouldn't need this test once all cameras are created
        //but we need to do this to make sure the cameras have the correct
        //viewport for the resized preview texture
        if (camEnt.isValid())
        {
            auto& cam = camEnt.getComponent<cro::Camera>();
            cam.resizeCallback(cam);
        }
    }
}

void ProfileStateV2::updateSliderGraphic(std::int32_t amt, std::int32_t total)
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

void ProfileStateV2::updateMenuItems()
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
                m_menuQuad.setColour(item.previewColour);

                m_menuQuad.draw();
                pos.x += (ItemSpacing) + ItemImage.x;
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
                updateSliderGraphic(item.selectedIndex, item.labels.size() - 1);
                m_itemSlider.setPosition({ std::floor(renderSize.x / 2.f), pos.y - 22.f/*std::floor(LineSpacing * 1.7f)*/ });
                m_itemSlider.draw();
                [[fallthrough]];
            default:
                if (item.displayType == Menu::Item::Slider)
                {
                    m_menuTextLarge.setPosition({ std::round(renderSize.x / 2.f), pos.y - (LineSpacing - 1.f) });
                }
                else
                {
                    m_menuTextLarge.setPosition({ std::round(renderSize.x / 2.f), pos.y - std::round(LineSpacing * 1.7f) });
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
                    m_menuTextLarge.setPosition({ std::round(renderSize.x / 2.f), pos.y - std::round(LineSpacing * 1.7f) });
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
                m_menuTextLarge.setPosition({ std::round(renderSize.x / 2.f), pos.y - std::round(LineSpacing * 1.7f) });
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
    m_detailsPane.image.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
    m_detailsPane.applyButton.getComponent<cro::Transform>().setScale(glm::vec2(0.f));

    m_menuLayout.texture.clear(cro::Colour::Transparent);
    //render current item selection to render texture
    //this includes either setting item highlight colour or rendering a highlight box
    auto i = 0;
    for (auto& item : items)
    {
        if (i == m_menuLayout.itemIndex)
        {
            auto txt = item.description;
            cro::Util::String::wordWrap(txt, WordWrapLarge);
            
            m_detailsPane.text.getComponent<cro::Text>().setString(txt);

            const auto b = (cro::Text::getLocalBounds(m_detailsPane.text).width / viewScale) + DetailBackgroundPadding;
            if (b > m_detailsPane.backgroundSize.x)
            {
                cro::Util::String::wordWrap(txt, WordWrapSmall);
                m_detailsPane.text.getComponent<cro::Text>().setString(txt);
            }

            if (item.selected)
            {
                item.selected(item);
            }
            else if (m_tabBar.items[m_tabBar.activeIndex].sprite.getTexture())
            {
                //set this sprite if it's available
                m_detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
                m_detailsPane.image.getComponent<cro::Sprite>() = m_tabBar.items[m_tabBar.activeIndex].sprite;
                const auto bounds = m_detailsPane.image.getComponent<cro::Sprite>().getTextureBounds();
                m_detailsPane.image.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f,0.f });
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

void ProfileStateV2::nextItem()
{
    //reset mouse hover highlight
    m_menuLayout.hoveredIndex = -1;

    m_menuLayout.itemIndex = (m_menuLayout.itemIndex + 1) % m_menuLayout.items[m_tabBar.activeIndex].size();
    updateMenuItems();

    playSound(MenuSoundEvent::Switch);
}

void ProfileStateV2::prevItem()
{
    //reset mouse hover highlight
    m_menuLayout.hoveredIndex = -1;

    m_menuLayout.itemIndex = static_cast<std::uint32_t>((m_menuLayout.itemIndex + (m_menuLayout.items[m_tabBar.activeIndex].size() - 1)) % m_menuLayout.items[m_tabBar.activeIndex].size());
    updateMenuItems();

    playSound(MenuSoundEvent::Switch);
}

void ProfileStateV2::activateLeft()
{
    //reset mouse hover highlight
    m_menuLayout.hoveredIndex = -1;

    if (m_menuLayout.items[m_tabBar.activeIndex][m_menuLayout.itemIndex].activateLeft())
    {
        updateMenuItems();
        playSound(MenuSoundEvent::Cancel);
    }
}

void ProfileStateV2::activateRight()
{
    //reset mouse hover highlight
    m_menuLayout.hoveredIndex = -1;

    if (m_menuLayout.items[m_tabBar.activeIndex][m_menuLayout.itemIndex].activateRight())
    {
        updateMenuItems();
        playSound(MenuSoundEvent::Activate);
    }
}

void ProfileStateV2::activate()
{
    if (m_menuLayout.items[m_tabBar.activeIndex][m_menuLayout.itemIndex].activate())
    {
        playSound(MenuSoundEvent::Activate);
    }
}

void ProfileStateV2::checkMouseOver(glm::vec2 screenPos)
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

void ProfileStateV2::doMouseClick(glm::vec2 mousePos)
{
    if (m_tabBar.hoveredIndex != -1)
    {
        if (m_detailsPane.tabDetails[m_tabBar.activeIndex].isValid())
        {
            m_detailsPane.tabDetails[m_tabBar.activeIndex].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        }

        m_tabBar.activeIndex = m_tabBar.hoveredIndex;
        m_tabBar.hoveredIndex = -1;
        m_menuLayout.itemIndex = 0;

        if (m_tabBar.items[m_tabBar.activeIndex].selected)
        {
            m_tabBar.items[m_tabBar.activeIndex].selected();
        }

        if (m_detailsPane.tabDetails[m_tabBar.activeIndex].isValid())
        {
            m_detailsPane.tabDetails[m_tabBar.activeIndex].getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        }

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
                //this seems counter intuitive but it stops mouse input
                //automatically activating items like resolution setting
                if (!m_menuLayout.items[m_tabBar.activeIndex][m_menuLayout.itemIndex].alwaysActivate)
                {
                    activate();
                }

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

void ProfileStateV2::refreshView()
{
    //heh OK let's say I had grander plans for this funtion...

    updateTabBar();
}

void ProfileStateV2::quitState()
{
    m_exitFlags = 0;
    m_exitHoldTimer = 0.f;

    m_rootNode.getComponent<cro::Callback>().active = true;
    playSound(MenuSoundEvent::Cancel);
}

void ProfileStateV2::loadAvatarPreviews()
{
    static constexpr glm::vec3 AvatarPos({ -0.867f, 0.f, 0.f });
    std::size_t previewIndex = 1;
    for (auto i = 0u; i < m_profileData.avatarDefs.size(); ++i)
    {
        //need to pad this out regardless for correct indexing
        auto& avt = m_avatarModels.emplace_back();
        if (!m_sharedData.avatarInfo[i].locked)
        {
            auto& avatar = m_profileData.avatarDefs[i];

            auto entity = m_previewScene.createEntity();
            entity.addComponent<cro::Transform>().setOrigin(AvatarPos);
            avatar.createModel(entity);
            entity.getComponent<cro::Model>().setHidden(true);

            auto material = m_profileData.profileMaterials.avatar;
            applyMaterialData(avatar, material);
            entity.getComponent<cro::Model>().setMaterial(0, material);

            entity.addComponent<cro::Callback>().setUserData<AvatarAnimCallbackData>();
            entity.getComponent<cro::Callback>().function =
                [&](cro::Entity e, float dt)
                {
                    auto& [direction, progress] = e.getComponent<cro::Callback>().getUserData<AvatarAnimCallbackData>();
                    const float Speed = dt * 4.f;
                    float rotation = 0.f; //hmm would be nice to rotate in the direction of the index change...

                    if (direction == 0)
                    {
                        //grow
                        progress = std::min(1.f, progress + Speed);
                        rotation = -cro::Util::Const::TAU + (cro::Util::Const::TAU * progress);

                        if (progress == 1)
                        {
                            e.getComponent<cro::Callback>().active = false;
                        }
                    }
                    else
                    {
                        //shrink
                        progress = std::max(0.f, progress - Speed);
                        rotation = cro::Util::Const::TAU * (1.f - progress);

                        if (progress == 0)
                        {
                            e.getComponent<cro::Callback>().active = false;
                            e.getComponent<cro::Model>().setHidden(true);
                        }
                    }

                    glm::vec3 scale(progress, 1.f, progress);
                    e.getComponent<cro::Transform>().setScale(scale);

                    //TODO we want to add initial rotation here ideally...
                    //however it's not easily extractable from the orientation.
                    e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation);
                };

            avt.previewModel = entity;
            avt.type = m_sharedData.avatarInfo[i].type;
            avt.previewIndex = previewIndex++;

            //these are unique models from the menu so we'll 
            //need to capture their attachment points once again...
            if (entity.hasComponent<cro::Skeleton>())
            {
                //this should never not be true as the models were validated
                //in the menu state - but 
                auto id = entity.getComponent<cro::Skeleton>().getAttachmentIndex("head");
                if (id > -1)
                {
                    //duplicate the hat attachment first so we don't invalidate pointers
                    //when adding the attachment resizes the attachment vector
                    auto hatAttachment = entity.getComponent<cro::Skeleton>().getAttachments()[id];
                    auto hatID = entity.getComponent<cro::Skeleton>().addAttachment(hatAttachment);

                    avt.hatAttachment = &entity.getComponent<cro::Skeleton>().getAttachments()[hatID];

                    //hair is optional so OK if this doesn't exist
                    avt.hairAttachment = &entity.getComponent<cro::Skeleton>().getAttachments()[id];
                }

                entity.getComponent<cro::Skeleton>().play(entity.getComponent<cro::Skeleton>().getAnimationIndex("idle_standing"));
            }
        }
        else
        {
            m_lockedAvatarCount++;
        }
    }
}

void ProfileStateV2::loadAvatarTextures()
{
    for (auto i = 0u; i < m_sharedData.avatarInfo.size(); ++i)
    {
        //need to pad out the vector anyway for correct indexing
        auto& t = m_profileTextures.emplace_back(m_sharedData.avatarInfo[i].texturePath);

        if (!m_sharedData.avatarInfo[i].locked)
        {
            //no point applyingthe colours here as we don't have a valid active profile until
            //onCachedPush() has been called.
            /*for (auto j = 0; j < pc::ColourKey::Count; ++j)
            {
                t.setColour(pc::ColourKey::Index(j), m_activeProfile.playerData.avatarFlags[j]);
            }
            t.apply();*/

            m_avatarModels[i].previewModel.getComponent<cro::Model>().setMaterialProperty(0, "u_diffuseMap", t.getTextureID());

            /*cro::AudioScape as;
            if (!m_sharedData.avatarInfo[i].audioscape.empty() &&
                as.loadFromFile(m_sharedData.avatarInfo[i].audioscape, m_resources.audio))
            {
                m_avatarModels[i].audioUID = as.getUID();
                for (const auto& name : emitterNames)
                {
                    if (as.hasEmitter(name))
                    {
                        auto e = m_uiScene.createEntity();
                        e.addComponent<cro::Transform>();
                        e.addComponent<cro::AudioEmitter>() = as.getEmitter(name);
                        e.getComponent<cro::AudioEmitter>().setLooped(false);
                        m_avatarModels[i].previewAudio.push_back(e);
                    }
                }
            }*/
        }
    }
}

void ProfileStateV2::loadHairModels()
{
    //empty at front for 'bald'
    m_avatarHairModels.push_back({});
    for (auto& hair : m_profileData.hairDefs)
    {
        auto entity = m_previewScene.createEntity();
        entity.addComponent<cro::Transform>();
        hair.createModel(entity);

        auto material = m_profileData.profileMaterials.hair;
        applyMaterialData(hair, material);

        entity.getComponent<cro::Model>().setHidden(true);
        entity.getComponent<cro::Model>().setMaterial(0, material);

        if (hair.getMaterialCount() == 2)
        {
            auto material2 = hair.hasTag(1, "glass") ? m_profileData.profileMaterials.hairGlass : m_profileData.profileMaterials.hairReflection;

            applyMaterialData(hair, material2, 1);
            entity.getComponent<cro::Model>().setMaterial(1, material2);
        }

        m_avatarHairModels.push_back(entity);
    }
}

void ProfileStateV2::loadBallModels()
{
    CRO_ASSERT(!m_profileData.ballDefs.empty(), "Must load this state on top of menu");

    //TODO rather than creating an emitter for every single model a single
    //emitter will do (or maybe 2/3 to buffer when switching previews  quickly)
    cro::EmitterSettings emitterSettings;
    emitterSettings.loadFromFile("assets/golf/particles/puff_small.cps", m_resources.textures);

    //this has all been parsed by the menu state - so we're assuming
    //all the models etc are fine and load without chicken
    std::int32_t c = 0;
    for (auto& ballDef : m_profileData.ballDefs)
    {
        auto entity = m_previewScene.createEntity();
        entity.addComponent<cro::Transform>();
        ballDef.createModel(entity);
        entity.getComponent<cro::Model>().setHidden(true);
        if (ballDef.hasSkeleton())
        {
            entity.getComponent<cro::Model>().setMaterial(0, m_profileData.profileMaterials.ballSkinned);
            entity.getComponent<cro::Skeleton>().play(0);
        }
        else
        {
            if (ballDef.getMaterial(0)->properties.count("u_normalMap"))
            {
                auto mat = m_profileData.profileMaterials.ballBumped;
                applyMaterialData(ballDef, mat);
                entity.getComponent<cro::Model>().setMaterial(0, mat);
            }
            else
            {
                entity.getComponent<cro::Model>().setMaterial(0, m_profileData.profileMaterials.ball);
            }
        }
        entity.getComponent<cro::Model>().setMaterial(1, m_profileData.profileMaterials.ballReflection);
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [](cro::Entity e, float dt)
            {
                e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt);
            };

        auto& preview = m_ballModels.emplace_back();
        preview.ball = entity;
        preview.type = m_sharedData.ballInfo[c].type;
        preview.infoIndex = c;
        preview.root = m_previewScene.createEntity();
        preview.root.addComponent<cro::Transform>().setPosition(BallPos);
        preview.root.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, m_sharedData.ballInfo[c].previewRotation);
        preview.root.getComponent<cro::Transform>().addChild(preview.ball.getComponent<cro::Transform>());
        preview.root.addComponent<cro::ParticleEmitter>().settings = emitterSettings;

        ++c;
    }

    m_ballModels[0].ball.getComponent<cro::Model>().setHidden(false);
}

std::size_t ProfileStateV2::indexFromAvatarID(std::uint32_t skinID) const
{
    const auto& avatarInfo = m_sharedData.avatarInfo;

    if (auto result = std::find_if(avatarInfo.cbegin(), avatarInfo.cend(),
        [skinID](const SharedStateData::AvatarInfo& a) {return a.uid == skinID; }); result != avatarInfo.cend())
    {
        return std::distance(avatarInfo.cbegin(), result);
    }

    return 0;
}

std::size_t ProfileStateV2::indexFromHairID(std::uint32_t hairID) const
{
    const auto& hairInfo = m_sharedData.hairInfo;
    if (auto result = std::find_if(hairInfo.cbegin(), hairInfo.cend(),
        [hairID](const SharedStateData::HairInfo& hi) {return hi.uid == hairID; }); result != hairInfo.end())
    {
        return std::distance(hairInfo.begin(), result);
    }
    return 0;
}

void ProfileStateV2::setAvatarIndex(std::size_t idx)
{
    auto hairIdx = m_avatarModels[m_avatarIndex].hairIndex;
    auto hatIdx = m_avatarModels[m_avatarIndex].hatIndex;

    if (m_avatarModels[m_avatarIndex].hairAttachment)
    {
        m_avatarModels[m_avatarIndex].hairAttachment->setModel({});
    }

    if (m_avatarModels[m_avatarIndex].hatAttachment)
    {
        m_avatarModels[m_avatarIndex].hatAttachment->setModel({});
    }

    m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Callback>().active = true;
    m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Callback>().getUserData<AvatarAnimCallbackData>().direction = 1;

    //we might have blank spots so skip these
    if (m_avatarIndex == 0 || idx < m_avatarIndex)
    {
        while (!m_avatarModels[idx].previewModel.isValid()
            && idx != m_avatarIndex)
        {
            idx = (idx + (m_avatarModels.size() - 1)) % m_avatarModels.size();
        }
    }
    else
    {
        while (!m_avatarModels[idx].previewModel.isValid()
            && idx != m_avatarIndex)
        {
            idx = (idx + 1) % m_avatarModels.size();
        }
    }

    m_avatarIndex = idx;

    if (m_avatarModels[m_avatarIndex].hairAttachment)
    {
        m_avatarModels[m_avatarIndex].hairAttachment->setModel(m_avatarHairModels[hairIdx]);
        m_avatarModels[m_avatarIndex].hairIndex = hairIdx;
    }

    if (m_avatarModels[m_avatarIndex].hatAttachment)
    {
        m_avatarModels[m_avatarIndex].hatAttachment->setModel(m_avatarHairModels[hatIdx]);
        m_avatarModels[m_avatarIndex].hatIndex = hatIdx;
    }

    m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Model>().setHidden(false);
    m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Callback>().active = true;
    m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Callback>().getUserData<AvatarAnimCallbackData>().direction = 0;

    m_activeProfile.playerData.skinID = m_sharedData.avatarInfo[m_avatarIndex].uid;


    for (auto i = 0u; i < m_activeProfile.playerData.avatarFlags.size(); ++i)
    {
        m_profileTextures[idx].setColour(pc::ColourKey::Index(i), m_activeProfile.playerData.avatarFlags[i]);
    }
    m_profileTextures[idx].apply();


    //although this should never be true as we
    //assert the selected index when the window opens
    if (m_activeProfile.playerData.voiceID == 0)
    {
        LogI << FILE_LINE << " TODO" << std::endl;
        /*if (!m_avatarModels[m_avatarIndex].previewAudio.empty())
        {
            m_avatarModels[m_avatarIndex].previewAudio[cro::Util::Random::value(0u, m_avatarModels[m_avatarIndex].previewAudio.size() - 1)].getComponent<cro::AudioEmitter>().play();
        }*/
    }
    else
    {
        //playPreviewAudio();
    }
}

void ProfileStateV2::setHairIndex(std::size_t idx)
{
    //don't set the same as the hat
    if (idx == m_avatarModels[m_avatarIndex].hatIndex)
    {
        idx = 0;
    }
    auto hairIndex = m_avatarModels[m_avatarIndex].hairIndex;

    if (m_avatarHairModels[hairIndex].isValid())
    {
        m_avatarHairModels[hairIndex].getComponent<cro::Model>().setHidden(true);
    }
    hairIndex = idx;
    if (m_avatarHairModels[hairIndex].isValid())
    {
        m_avatarHairModels[hairIndex].getComponent<cro::Model>().setHidden(false);
    }

    if (m_avatarModels[m_avatarIndex].hairAttachment
        && m_avatarHairModels[hairIndex].isValid())
    {
        m_avatarModels[m_avatarIndex].hairAttachment->setModel(m_avatarHairModels[hairIndex]);
        m_avatarHairModels[hairIndex].getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", pc::Palette[m_activeProfile.playerData.avatarFlags[pc::ColourKey::Hair]]);
        m_avatarHairModels[hairIndex].getComponent<cro::Model>().setMaterialProperty(1, "u_hairColour", pc::Palette[m_activeProfile.playerData.avatarFlags[pc::ColourKey::Hair]]);

        applyHeadwearTransform(hairIndex, 0);
    }
    m_avatarModels[m_avatarIndex].hairIndex = hairIndex;

    m_activeProfile.playerData.hairID = m_sharedData.hairInfo[hairIndex].uid;

    //m_headwearPreviewRects[HeadwearID::Hair] = getThumbnailTextureRect(hairIndex);
}

void ProfileStateV2::setHatIndex(std::size_t idx)
{
    //don't set the same as hair
    if (idx == m_avatarModels[m_avatarIndex].hairIndex)
    {
        idx = 0;
    }

    auto hatIndex = m_avatarModels[m_avatarIndex].hatIndex;

    if (m_avatarHairModels[hatIndex].isValid())
    {
        m_avatarHairModels[hatIndex].getComponent<cro::Model>().setHidden(true);
    }
    hatIndex = idx;
    if (m_avatarHairModels[hatIndex].isValid())
    {
        m_avatarHairModels[hatIndex].getComponent<cro::Model>().setHidden(false);
    }

    if (m_avatarModels[m_avatarIndex].hatAttachment
        && m_avatarHairModels[hatIndex].isValid())
    {
        m_avatarModels[m_avatarIndex].hatAttachment->setModel(m_avatarHairModels[hatIndex]);
        m_avatarHairModels[hatIndex].getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", pc::Palette[m_activeProfile.playerData.avatarFlags[pc::ColourKey::Hat]]);
        m_avatarHairModels[hatIndex].getComponent<cro::Model>().setMaterialProperty(1, "u_hairColour", pc::Palette[m_activeProfile.playerData.avatarFlags[pc::ColourKey::Hat]]);

        applyHeadwearTransform(hatIndex, PlayerData::HeadwearOffset::HatTx);
    }
    m_avatarModels[m_avatarIndex].hatIndex = hatIndex;

    m_activeProfile.playerData.hatID = m_sharedData.hairInfo[hatIndex].uid;

    //m_headwearPreviewRects[HeadwearID::Hat] = getThumbnailTextureRect(hatIndex);
}

void ProfileStateV2::applyHeadwearTransform(std::size_t idx, std::size_t indexOffset)
{
    if (m_avatarHairModels[idx].isValid()) //'bald' at front has no transform
    {
        const auto rot = m_activeProfile.playerData.headwearOffsets[PlayerData::HeadwearOffset::HairRot + indexOffset] * cro::Util::Const::PI;
        m_avatarHairModels[idx].getComponent<cro::Transform>().setPosition(m_activeProfile.playerData.headwearOffsets[PlayerData::HeadwearOffset::HairTx + indexOffset]);
        m_avatarHairModels[idx].getComponent<cro::Transform>().setRotation(cro::Transform::Z_AXIS, rot.z);
        m_avatarHairModels[idx].getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, rot.y);
        m_avatarHairModels[idx].getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, rot.x);
        m_avatarHairModels[idx].getComponent<cro::Transform>().setScale(m_activeProfile.playerData.headwearOffsets[PlayerData::HeadwearOffset::HairScale + indexOffset]);
    }
}