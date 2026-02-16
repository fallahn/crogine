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
#include "shaders/2DBorderShader.inl"
#include "shaders/ProgressShader.inl"
    constexpr glm::vec3 BallPos = glm::vec3({ 10.f, 0.f, 0.f });
    constexpr glm::vec3 CamPosAvatar = glm::vec3({ 0.f, 1.f, -1.5f });
    constexpr glm::vec3 CamPosHead = glm::vec3({ 0.f, 1.61f, -0.35f });
    constexpr glm::vec3 MugCameraPosition = CamPosHead;

    constexpr glm::uvec2 MugshotTexSize(192u, 96u);
    constexpr std::size_t MaxBioChars = 512;

    static constexpr std::uint32_t DownArrow = 0x2193;
    static const cro::String KeyInfo = cro::String(DownArrow) + cro::String(" LCtrl Save & Close   ") + cro::String(DownArrow) + cro::String(" LAlt Randomise   ") + cro::String(DownArrow) + cro::String(" ESC Close");

    const std::array ItemLabels =
    {
        "Avatar", "Headwear",
        "Equipment", "Loadout", "Biography"
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

    const std::array ManufacturerText =
    {
        cro::String("The original Scottish golf manufacturers,\nGallawent have only the firmest of woods\nand toughest of drivers to back them up.\n\nGrab a caber, aye?"),
        cro::String("Sonorous by name and sonorous by nature,\nthe satisfying thunk of a Dong is all\nyou need to get to the bottom of a hole."),
        cro::String("Hand crafted by authentic one-eyed, three\nfingered craftsmen since the 1700s,\nFellowCraft avoid the hazards so you don't\nhave to."),
        cro::String("Whether you're a proponent of Imperial or\nMetric Akrun only deal in feet, three of\nwhich are gaurenteed to fit comfortably in\nyour grip."),
        cro::String("Though Dannis may sound like a different\nsport, their high quality equipment ensures\nyou won't be calling out for New Balls\nPlease!"),
        cro::String("Clix, inspired by the sound of every great\ngolfer's shoulder, promise the only thing\nyou'll be shouting on the fairway is FORE!"),
        cro::String("For over 100 years BeyTree, the makers of\nsome of the world's finest sporting\nequipment, have been lamenting a single typo."),
        cro::String("Tunnelrock Balls, a name synonymous with\nspelunking, are carefully vacuum packed\nat the source to preserve maximum\nfreshness. From field to freezer in under\nan hour."),
        cro::String("Woven from the finest golden retreiver hair,\nFlaxen make sure their balls use\nonly the softest clippings to ensure the\nswiftest of flights."),
        cro::String("Hardings Balls, both notorious and revered,\nhave a heart of gold and a west\ncountry accent that would turn any bushel\nof apples into the sweetest of ciders."),
        cro::String("The only splinters here are those from the\ncourse record, as Woodgear Balls are\nthe epitome of driving long, hard, and fast.\n\nMay not contain actual wood."),
        cro::String("Brilton & Stockley started in the soup\nindustry, nearly 200 years ago, before\nbranching out to manufacturing sports\nequipment after a rogue accident involving\na yard long spoon."),
        cro::String("There's nothing assigned to this slot.\nGo to the Equipment Counter to find\nupgrades!")
    };

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
    m_statScene         (ctx.appInstance.getMessageBus(), 192),
    m_sharedData        (sd),
    m_profileData       (profileData),
    m_exitHoldTimer     (0.f),
    m_exitFlags         (0),
    m_progressUniform   (-1),
    m_avatarIndex       (0),
    m_lockedAvatarCount (0),
    m_ballIndex         (0),
    m_lockedBallCount   (0),
    m_particleIndex     (0),
    m_lockedClubCount   (0),
    m_clubIndex         (0),
    m_showNameInput     (false),
    m_voiceIndex        (0),
    m_saveMugshotOnExit (false),
    m_uiTexture         (nullptr)
{
    ctx.mainWindow.setMouseCaptured(false);

    std::fill(m_controllerMasks.begin(), m_controllerMasks.end(), 0);
    std::fill(m_controllerPrevMasks.begin(), m_controllerPrevMasks.end(), 0);

    m_tabBar.items.resize(TabID::Count);
    m_menuLayout.items.resize(TabID::Count);

    registerWindow(std::bind(&ProfileStateV2::nameInputWindow, this));

    loadAssets();
    buildPreviewScene(); //make sure models are loaded first so menu creation can read the data
    buildStatScene();
    buildScene();
}

//public
bool ProfileStateV2::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse
        || m_rootNode.getComponent<cro::Callback>().active
        || m_showNameInput)
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
        else if (evt.key.keysym.sym == SDLK_LALT)
        {
            m_exitFlags &= ~ExitFlagRandomise;
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
        else if (evt.key.keysym.sym == SDLK_LALT
            && evt.key.repeat == 0)
        {
            m_exitFlags |= ExitFlagRandomise;
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
        case cro::GameController::ButtonY:
            m_exitFlags |= ExitFlagRandomise;
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
        case cro::GameController::ButtonY:
            m_exitFlags &= ~ExitFlagRandomise;
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
    m_statScene.forwardEvent(evt);
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

            //realigns the current menu to the new screen size
            cro::Entity entity = m_scene.createEntity();
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [&](cro::Entity e, float)
                {
                    m_menuLayout.itemIndex = 0;
                    focusToIndex(m_tabBar, m_menuLayout);

                    e.getComponent<cro::Callback>().active = false;
                    m_scene.destroyEntity(e);
                };
        }
    }
    else if (msg.id == cl::MessageID::SystemMessage)
    {
        const auto& data = msg.getData<SystemEvent>();
        if (data.type == SystemEvent::CancelOSK)
        {
            m_sharedData.useOSKBuffer = false;
            //m_showOSK = false;
        }
        else if (data.type == SystemEvent::SubmitOSK)
        {
            m_sharedData.useOSKBuffer = false;
            //m_showOSK = false;

            if (!m_sharedData.OSKBuffer.empty())
            {
                m_activeProfile.playerData.name = m_sharedData.OSKBuffer;
                applyNameString();
            }
        }
    }
    else if (msg.id == cro::Message::StateMessage)
    {
        const auto& data = msg.getData<cro::Message::StateEvent>();
        if (data.action == cro::Message::StateEvent::Popped)
        {
            switch (data.id)
            {
            default: break;
            case StateID::Shop:
                LogI << FILE_LINE << " update locked item counts and refresh menu" << std::endl;
                
                createEquipmentItems();
                createLoadoutItems();
                break;
            }
        }
    }
    m_statScene.forwardMessage(msg);
    m_previewScene.forwardMessage(msg);
    m_scene.forwardMessage(msg);
}

bool ProfileStateV2::simulate(float dt)
{
    //press/hold to exist
    static constexpr float MaxHoldTime = 0.35f;
    if (m_exitFlags)
    {
        m_exitHoldTimer = std::min(m_exitHoldTimer + dt, MaxHoldTime);
        
        if (m_exitHoldTimer >= MaxHoldTime)
        {
            switch (m_exitFlags)
            {
            default:
                //mode than one button held, so invalid
                m_exitHoldTimer = 0.f;
                break;
            case ExitFlagQuit:
                quitState();
                break;
            case ExitFlagSave:
                if (m_saveMugshotOnExit)
                {
                    const auto path = Content::getUserContentPath(Content::UserContent::Profile) + m_activeProfile.playerData.profileID + "/mug.png";
                    m_mugshotTexture.getTexture().saveToFile(path);
                    m_activeProfile.playerData.mugshot = path;
                    m_saveMugshotOnExit = false;
                }

                if (m_activeProfile.playerData.isSteamID
                    && m_activeProfile.playerData.name != Social::getPlayerName())
                {
                    Social::setPlayerName(m_activeProfile.playerData.name);
                }

                //copy av data back to proper data and write files     
                m_activeProfile.loadout.write(m_activeProfile.playerData.profileID);
                m_profileData.playerProfiles[m_profileData.activeProfileIndex] = m_activeProfile;
                m_profileData.playerProfiles[m_profileData.activeProfileIndex].playerData.saveProfile();

                quitState();
                break;
            case ExitFlagRandomise:
                randomise();
                break;
            }
            m_exitFlags = 0;
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
    constexpr auto threshold = std::numeric_limits<std::int16_t>::max() / 6;
    constexpr float SpeedMultiplier = 2.f;
    if (const auto v = cro::GameController::getAxisPosition(0, cro::GameController::TriggerLeft);
                v > threshold)
    {
        const float speed = cro::Util::Easing::easeInQuart(static_cast<float>(v) / std::numeric_limits<std::int16_t>::max());
        rotateModel(-dt * speed * SpeedMultiplier);
    }
    if (const auto v = cro::GameController::getAxisPosition(0, cro::GameController::TriggerRight);
                v > threshold)
    {
        const float speed = cro::Util::Easing::easeInQuart(static_cast<float>(v) / std::numeric_limits<std::int16_t>::max());
        rotateModel(dt * speed * SpeedMultiplier);
    }
    if (cro::Keyboard::isKeyPressed(SDLK_1))
    {
        rotateModel(-dt * SpeedMultiplier);
    }
    if (cro::Keyboard::isKeyPressed(SDLK_2))
    {
        rotateModel(dt * SpeedMultiplier);
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

    m_statScene.simulate(dt);
    m_previewScene.simulate(dt);
    m_scene.simulate(dt);
    return true;
}

void ProfileStateV2::render()
{
    std::int32_t colourIndex = 0;
    switch (m_tabBar.activeIndex)
    {
    default:
        colourIndex = CD32::BlueLight;
        break;
    case TabID::Loadout:
        /*colourIndex = CD32::BeigeDarkest;
        break;*/
    case TabID::Details:
        colourIndex = CD32::Brown;
        break;
    }

    m_previewTexture.clear(CD32::Colours[colourIndex]);
    if (m_tabBar.activeIndex == TabID::Loadout)
    {
        //this seems a fart arse way of rendering stat graphics
        //but I guess this is where we are now...
        m_statScene.render();
    }
    else
    {
        m_previewScene.render();

        //render palette preview if necessary (these drawable types automatically skip drawing if there are no verts)
        m_palettePreview.draw();
    }
    m_previewTexture.display();

    m_scene.render();
}

//private
void ProfileStateV2::loadAssets()
{
    m_mugshotTexture.create(MugshotTexSize.x, MugshotTexSize.y);
    m_mugshotShader.loadFromString(cro::RenderSystem2D::getDefaultVertexShader(), BorderFrag, "#define TEXTURED\n");

    m_palettePreview.setPrimitiveType(GL_TRIANGLES);

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

        m_itemIcons[ItemIcon::UnlockedItem] = spriteSheet.getSprite("item_unlocked");
        m_itemIcons[ItemIcon::WorkshopItem] = spriteSheet.getSprite("item_workshop");
        m_itemIcons[ItemIcon::WorkshopButton] = spriteSheet.getSprite("workshop_button");
        m_itemIcons[ItemIcon::EquipButton] = spriteSheet.getSprite("item_equip_counter");
        m_itemIcons[ItemIcon::Locked] = spriteSheet.getSprite("locked_icon");
    }

    if (spriteSheet.loadFromFile("assets/golf/sprites/shop_badges.spt", m_sharedData.sharedResources->textures))
    {
        m_itemIcons[ItemIcon::Gallawent]   = spriteSheet.getSprite("small_01");
        m_itemIcons[ItemIcon::Dong]        = spriteSheet.getSprite("small_02");
        m_itemIcons[ItemIcon::Fellowcraft] = spriteSheet.getSprite("small_03");
        m_itemIcons[ItemIcon::Akrun]       = spriteSheet.getSprite("small_04");
        m_itemIcons[ItemIcon::Dannis]      = spriteSheet.getSprite("small_05");
        m_itemIcons[ItemIcon::Clix]        = spriteSheet.getSprite("small_06");
        m_itemIcons[ItemIcon::Beytree]     = spriteSheet.getSprite("small_07");
        m_itemIcons[ItemIcon::Tunnelrock]  = spriteSheet.getSprite("small_08");
        m_itemIcons[ItemIcon::Flaxen]      = spriteSheet.getSprite("small_09");
        m_itemIcons[ItemIcon::Hardings]    = spriteSheet.getSprite("small_10");
        m_itemIcons[ItemIcon::Woodgear]    = spriteSheet.getSprite("small_11");
        m_itemIcons[ItemIcon::BnS]         = spriteSheet.getSprite("small_12");
    }

    //this sets the default image shown on the right when the tab is selected
    //note that if a specific menu item has a Selected callback that it'll override this
    //even if the item sprite itself is set to nothing (therefore the image is hidden)
    m_tabBar.items[TabID::Body].sprite.setTexture(m_previewTexture.getTexture());
    m_tabBar.items[TabID::Headwear].sprite.setTexture(m_previewTexture.getTexture());

    loadClubData();
    loadVoiceData();
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
    m_detailsPane.text.getComponent<cro::UIElement>().absolutePosition = { DetailBackgroundOffset, -98.f }; //90
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
    /*m_detailsPane.background.getComponent<cro::UIElement>().resizeCallback =
        [&](cro::Entity e) 
        {

        };*/
    m_detailsPane.background.getComponent<cro::UIElement>().depth = -0.3f;
    m_detailsPane.root.getComponent<cro::Transform>().addChild(m_detailsPane.background.getComponent<cro::Transform>());


    //displays the selected club set on the equipment tab
    if (!m_clubData.empty())
    {
        static constexpr std::int32_t Cols = 5;
        std::int32_t rows = (static_cast<std::int32_t>(m_clubData.size() - 1) / Cols) + 1;

        cro::Texture tempTexture;
        cro::SimpleQuad tempQuad;

        tempTexture.loadFromFile(m_clubData[0].thumbnail);
        tempQuad.setTexture(tempTexture);
        m_clubTexture.create(Cols * tempTexture.getSize().x, rows * tempTexture.getSize().y, false);
        m_clubTexture.setSmooth(false);
        m_clubData[0].uv = { glm::vec2(0.f), glm::vec2(tempTexture.getSize()) };

        m_clubTexture.clear(CD32::Colours[CD32::BlueLight]);
        tempQuad.draw();
        for (auto i = 1u; i < m_clubData.size(); ++i)
        {
            const float x = static_cast<float>(std::floor((i % Cols) * tempTexture.getSize().x));
            const float y = static_cast<float>(std::floor((i / Cols) * tempTexture.getSize().y));

            tempTexture.loadFromFile(m_clubData[i].thumbnail);
            tempQuad.setTexture(tempTexture);
            tempQuad.setPosition({x,y});
            tempQuad.draw();

            m_clubData[i].uv = { glm::vec2(x,y), glm::vec2(tempTexture.getSize()) };
        }
        m_clubTexture.display();

        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.05f });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>(m_clubTexture.getTexture());
        m_detailsPane.image.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_detailsPane.clubsetImage = entity;
    }

    //displays the mugshot if available
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.05f });
    entity.addComponent<cro::Drawable2D>().setShader(&m_mugshotShader);
    entity.addComponent<cro::Sprite>();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(1.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            if (m_tabBar.activeIndex == TabID::Details)
            {
                e.getComponent<cro::Transform>().setScale(glm::vec2(e.getComponent<cro::Callback>().getUserData<float>()));
                /*e.getComponent<cro::Drawable2D>().setFacing(m_activeProfile.playerData.mugshot.empty() 
                    ? cro::Drawable2D::Facing::Back : cro::Drawable2D::Facing::Front);*/
            }
            else
            {
                e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            }
        };
    m_detailsPane.image.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_detailsPane.mugshotImage = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    m_detailsPane.mugshotImage.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_detailsPane.bioString = entity;


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
    entity.addComponent<cro::Text>(largeFont).setString("Enter - Play Voice");
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
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("voice_xbox");
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
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("voice_ps");
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
    msgRoot.addComponent<cro::Callback>().active = true;
    msgRoot.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            const float scale = (m_tabBar.activeIndex == TabID::Body
                || m_tabBar.activeIndex == TabID::Headwear)
                ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));

            e.getComponent<cro::Transform>().setPosition(-(m_detailsPane.backgroundSize / 2.f) * cro::UIElementSystem::getViewScale());
        };
    m_detailsPane.root.getComponent<cro::Transform>().addChild(msgRoot.getComponent<cro::Transform>());

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("Press 1 or 2 to Rotate");
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
    msgRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("rotate_xbox");
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
    msgRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("rotate_ps");
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

    m_infoRects[0] = spriteSheet.getSprite("info_profile_ps").getTextureRect();
    m_infoRects[1] = spriteSheet.getSprite("info_profile_xbox").getTextureRect();

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("info_profile_xbox");
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

            //hide club preview
            if (m_detailsPane.clubsetImage.isValid())
            {
                m_detailsPane.clubsetImage.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            }

            //hide mugshot
            m_detailsPane.mugshotImage.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
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

            //hide club preview
            if (m_detailsPane.clubsetImage.isValid())
            {
                m_detailsPane.clubsetImage.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            }

            //hide mugshot
            m_detailsPane.mugshotImage.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
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

            if (m_detailsPane.clubsetImage.isValid())
            {
                m_detailsPane.clubsetImage.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
            }

            //hide mugshot
            m_detailsPane.mugshotImage.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        };
    m_tabBar.items[TabID::Loadout].selected =
        [&]()
        {
            for (auto c : m_previewCameras)
            {
                c.getComponent<cro::Camera>().active = false;
            }

            if (m_detailsPane.clubsetImage.isValid())
            {
                m_detailsPane.clubsetImage.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            }

            //hide mugshot
            m_detailsPane.mugshotImage.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        };
    m_tabBar.items[TabID::Details].selected =
        [&]()
        {
            for (auto c : m_previewCameras)
            {
                c.getComponent<cro::Camera>().active = false;
            }
            m_previewCameras[PreviewCamera::MugShot].getComponent<cro::Camera>().active = true;
            m_previewCameras[PreviewCamera::Biog].getComponent<cro::Camera>().active = true;
            m_previewScene.setActiveCamera(m_previewCameras[PreviewCamera::Biog]);

            const auto idx = m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Skeleton>().getAnimationIndex("idle_standing");
            m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Skeleton>().play(idx);

            if (m_detailsPane.clubsetImage.isValid())
            {
                m_detailsPane.clubsetImage.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            }

            //show the mughot if the profile currently has one
            if (!m_activeProfile.playerData.mugshot.empty())
            {
                m_detailsPane.mugshotImage.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
            }
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

    m_previewScene.enableSkybox();
    m_previewScene.setCubemap("assets/golf/images/skybox/dusk/sky.ccm");

    loadAvatarPreviews();
    loadAvatarTextures();
    loadHairModels();
    loadBallModels();

    const auto resize = 
        [&](cro::Camera& cam)
        {
            const auto vpSize = glm::vec2(m_previewTexture.getSize());
            cam.setPerspective(70.f * cro::Util::Const::degToRad, vpSize.x / vpSize.y, 0.01f, 25.f);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
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


    //this needs its own callback with narrower FOV and split screen
    //for club thumbnails
    const auto resize2 =
        [&](cro::Camera& cam)
        {
            const auto vpSize = glm::vec2(m_previewTexture.getSize());
            cam.setPerspective(60.f * cro::Util::Const::degToRad, (vpSize.x / 2.f) / vpSize.y, 0.01f, 10.f);
            cam.viewport = { 0.5f, 0.f, 0.5f, 1.f };
        };

    camEnt = m_previewScene.createEntity();
    camEnt.addComponent<cro::Transform>().setPosition({ BallPos.x, 0.03f, -0.1f });
    camEnt.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI);
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.05f);
    auto& ballCam = camEnt.addComponent<cro::Camera>();
    ballCam.resizeCallback = resize2;
    resize2(ballCam);
    m_previewCameras[PreviewCamera::Ball] = camEnt;


    const auto resize3 =
        [&](cro::Camera& cam)
        {
            const auto vpSize = glm::vec2(m_previewTexture.getSize());
            cam.setPerspective(70.f * cro::Util::Const::degToRad, (vpSize.x / 2.f) / vpSize.y, 0.01f, 25.f);
            cam.viewport = { 0.f, 0.f, 0.5f, 1.f };
        };
    camEnt = m_previewScene.createEntity();
    camEnt.addComponent<cro::Transform>().setPosition(CamPosAvatar);
    camEnt.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI);
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.05f);

    auto& bioCam = camEnt.addComponent<cro::Camera>();
    bioCam.resizeCallback = resize3;
    resize3(bioCam);
    m_previewCameras[PreviewCamera::Biog] = camEnt;



    //doesn't use a callback because the mugshot texture doesn't resize
    camEnt = m_previewScene.createEntity();
    camEnt.addComponent<cro::Transform>().setPosition(MugCameraPosition);
    camEnt.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI);
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.057f);
    auto& cam2 = camEnt.addComponent<cro::Camera>();
    cam2.setPerspective(60.f * cro::Util::Const::degToRad, 1.f, 0.1f, 6.f);
    cam2.viewport = { 0.f, 0.f, 0.5f, 1.f };
    //cam2.setRenderFlags(cro::Camera::Pass::Final, ~(1 << 1));
    m_previewCameras[PreviewCamera::MugShot] = camEnt;


    auto lightEnt = m_previewScene.getSunlight();
    lightEnt.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 2.8f);
    lightEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.8f);
}

void ProfileStateV2::buildStatScene()
{
    auto& mb = cro::App::getInstance().getMessageBus();
    m_statScene.addSystem<cro::CallbackSystem>(mb);
    m_statScene.addSystem<cro::TextSystem>(mb);
    m_statScene.addSystem<cro::SpriteSystem2D>(mb);
    m_statScene.addSystem<cro::CameraSystem>(mb);
    m_statScene.addSystem<cro::RenderSystem2D>(mb);


    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/gear_editor.spt", m_resources.textures);

    auto sprite = spriteSheet.getSprite("stat_bar");
    const auto spriteSize = sprite.getTextureBounds();
    const auto spriteUV = sprite.getTextureRectNormalised();
    static constexpr float StatWidth = 132.f;
    static constexpr float EndWidth = 10.f;

    const float UVWidth = spriteUV.width * (EndWidth / spriteSize.width);
    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    const auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);

    const auto createStatBar = [&](glm::vec2 pos)
        {
            auto& statBar = m_statLayout.statBars.emplace_back();

            auto entity = m_statScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(glm::vec3(pos, 0.2f));
            entity.addComponent<cro::Drawable2D>().setVertexData(
                {
                    cro::Vertex2D(glm::vec2(0.f, spriteSize.height),                  glm::vec2(spriteUV.left, spriteUV.bottom + spriteUV.height)),
                    cro::Vertex2D(glm::vec2(0.f),                                     glm::vec2(spriteUV.left, spriteUV.bottom)),

                    cro::Vertex2D(glm::vec2(EndWidth, spriteSize.height),             glm::vec2(spriteUV.left + UVWidth, spriteUV.bottom + spriteUV.height)),
                    cro::Vertex2D(glm::vec2(EndWidth, 0.f),                           glm::vec2(spriteUV.left + UVWidth, spriteUV.bottom)),

                    cro::Vertex2D(glm::vec2(StatWidth / 2.f, spriteSize.height),      glm::vec2(spriteUV.left + (spriteUV.width / 2.f), spriteUV.bottom + spriteUV.height)),
                    cro::Vertex2D(glm::vec2(StatWidth / 2.f, 0.f),                    glm::vec2(spriteUV.left + (spriteUV.width / 2.f), spriteUV.bottom)),

                    cro::Vertex2D(glm::vec2(StatWidth - EndWidth, spriteSize.height), glm::vec2(spriteUV.left + (spriteUV.width - UVWidth), spriteUV.bottom + spriteUV.height)),
                    cro::Vertex2D(glm::vec2(StatWidth - EndWidth, 0.f),               glm::vec2(spriteUV.left + (spriteUV.width - UVWidth), spriteUV.bottom)),

                    cro::Vertex2D(glm::vec2(StatWidth, spriteSize.height),            glm::vec2(spriteUV.left + spriteUV.width, spriteUV.bottom + spriteUV.height)),
                    cro::Vertex2D(glm::vec2(StatWidth, 0.f),                          glm::vec2(spriteUV.left + spriteUV.width, spriteUV.bottom))
                });
            entity.getComponent<cro::Drawable2D>().setTexture(sprite.getTexture());

            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().setUserData<std::int32_t>(6);
            entity.getComponent<cro::Callback>().function =
                [](cro::Entity e, float dt)
                {
                    const auto val = e.getComponent<cro::Callback>().getUserData<std::int32_t>();
                    static constexpr float size = (StatWidth - (EndWidth / 2.f));
                    static constexpr float SegmentCount = 20.f;

                    const float segSize = size / SegmentCount;
                    const float target = ((size / 2.f) + (val * segSize));

                    auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
                    const float pos = verts[4].position.x;
                    const float move = (target - pos) * dt * 5.f;

                    verts[4].position.x += move;
                    verts[5].position.x = verts[4].position.x;
                };

            statBar.bgEnt = entity;

            entity = m_statScene.createEntity();
            entity.addComponent<cro::Transform>();
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("stat_pointer");
            const auto r = entity.getComponent<cro::Sprite>().getTextureBounds();
            entity.getComponent<cro::Transform>().setOrigin({ std::ceil(r.width / 2.f) + 1.f, r.height, -0.1f });

            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().setUserData<std::int32_t>(6);
            entity.getComponent<cro::Callback>().function =
                [](cro::Entity e, float dt)
                {
                    const auto val = e.getComponent<cro::Callback>().getUserData<std::int32_t>();
                    static constexpr float BasePos = StatWidth / 2.f;
                    const float offset = (BasePos / 10.f) * val;

                    e.getComponent<cro::Transform>().setPosition({ BasePos + offset, -2.f });
                };

            statBar.bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

            statBar.pointer = entity;

            entity = m_statScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(glm::vec3(glm::vec2(6.f, InfoTextSize + 5.f), 0.1f));
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Text>(smallFont).setFillColour(TextNormalColour);
            entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
            entity.getComponent<cro::Text>().setString("Default: 0");
            statBar.bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

            statBar.text = entity;
        };

    createStatBar(glm::vec2(20.f, 178.f));
    createStatBar(glm::vec2(20.f, 178.f - (spriteSize.height + 22.f)));

    auto entity = m_statScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 20.f, 214.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setFillColour(TextNormalColour);
    //entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    m_statLayout.statTitle = entity;

    entity = m_statScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 160.f, 214.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setString(ManufacturerText.back());
    m_statLayout.manufacturerName = entity;

    entity = m_statScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 160.f, 214.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setString(ManufacturerText.back());
    m_statLayout.manufacturerInfo = entity;

    entity = m_statScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>();
    m_statLayout.manufacturerIcon = entity;


    auto camEnt = m_statScene.getActiveCamera();
    auto& cam = camEnt.getComponent<cro::Camera>();
    cam.resizeCallback = 
        [&](cro::Camera& c)
        {
            const auto viewScale = cro::UIElementSystem::getViewScale();
            const glm::vec2 viewSize = m_previewTexture.getSize();
            c.setOrthographic(0.f, std::floor(viewSize.x / viewScale), 0.f, std::floor(viewSize.y / viewScale), -1.f, 10.f);
            c.viewport = { 0.f, 0.f, 1.f, 1.f };
        };
    cam.resizeCallback(cam);
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
    item->title = "Appearance";
    item->displayType = Menu::Item::Slider;
    item->activated =
        [&](Menu::Item& i)
        {
            setAvatarIndex(i.selectedIndex);
            i.description = i.labels[i.selectedIndex] + "/" + std::to_string(m_avatarModels.size() - m_lockedAvatarCount);
            m_detailsPane.text.getComponent<cro::Text>().setString(i.description);

            switch (m_avatarModels[m_avatarIndex].type)
            {
            default:
                i.texture = nullptr;
                break;
            case 1: //unlocked
                i.texture = m_itemIcons[ItemIcon::UnlockedItem].getTexture();
                i.uv = m_itemIcons[ItemIcon::UnlockedItem].getTextureRect();
                break;
            case 2: //from workshop
                i.texture = m_itemIcons[ItemIcon::WorkshopItem].getTexture();
                i.uv = m_itemIcons[ItemIcon::WorkshopItem].getTextureRect();
                break;
            }
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
    switch (m_avatarModels[m_avatarIndex].type)
    {
    default:
        item->texture = nullptr;
        break;
    case 1: //unlocked
        item->texture = m_itemIcons[ItemIcon::UnlockedItem].getTexture();
        item->uv = m_itemIcons[ItemIcon::UnlockedItem].getTextureRect();
        break;
    case 2: //from workshop
        item->texture = m_itemIcons[ItemIcon::WorkshopItem].getTexture();
        item->uv = m_itemIcons[ItemIcon::WorkshopItem].getTextureRect();
        break;
    }



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
                updatePalettePreview(pc::ColourKey::Index(c), i.selectedIndex);

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
        item->selected =
            [&, c](const Menu::Item& i)
            {
                updatePalettePreview(pc::ColourKey::Index(c), i.selectedIndex);

                //hmm seems silly to repeat this here just because the existence
                //of this callback prevents it from happening when the items are updated
                m_detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
                m_detailsPane.image.getComponent<cro::Sprite>() = m_tabBar.items[m_tabBar.activeIndex].sprite;
                const auto bounds = m_detailsPane.image.getComponent<cro::Sprite>().getTextureBounds();
                m_detailsPane.image.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f,0.f });
            };
        item->displayType = Menu::Item::Slider;
        item->previewColour = pc::Palette[item->selectedIndex];
        item->texture = &m_colourPreview;
        item->uv = { 0.f, 0.f, 1.f, 1.f };
    }

#ifdef USE_GNS
    //workshop button if steam
    item = &m_menuLayout.items[TabID::Body].emplace_back();
    item->title = "Steam Workshop";
    item->description = "Opens the Steam Workshop in the overlay";
    item->labels.push_back("Find More Avatars");
    item->activated = 
        [](Menu::Item&)
        {
            Social::showWorkshop();
        };
    item->texture = m_itemIcons[ItemIcon::WorkshopButton].getTexture();
    item->uv = m_itemIcons[ItemIcon::WorkshopButton].getTextureRect();
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
            item->displayType = Menu::Item::Slider;
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
                    
                    switch (m_sharedData.hairInfo[i.selectedIndex].type)
                    {
                    default: 
                        i.texture = nullptr;
                        break;
                    case 1:
                        i.texture = m_itemIcons[ItemIcon::UnlockedItem].getTexture();
                        i.uv = m_itemIcons[ItemIcon::UnlockedItem].getTextureRect();
                        break;
                    case 2:
                        i.texture = m_itemIcons[ItemIcon::WorkshopItem].getTexture();
                        i.uv = m_itemIcons[ItemIcon::WorkshopItem].getTextureRect();
                        break;
                    }
                };
            for (auto i = 0u; i < m_avatarHairModels.size(); ++i)
            {
                item->labels.push_back(std::to_string(i + 1));
            }
            item->selectedIndex = keyIndex == pc::ColourKey::Hair ? m_avatarModels[m_avatarIndex].hairIndex : m_avatarModels[m_avatarIndex].hatIndex;
            switch (m_sharedData.hairInfo[item->selectedIndex].type)
            {
            default:
                item->texture = nullptr;
                break;
            case 1:
                item->texture = m_itemIcons[ItemIcon::UnlockedItem].getTexture();
                item->uv = m_itemIcons[ItemIcon::UnlockedItem].getTextureRect();
                break;
            case 2:
                item->texture = m_itemIcons[ItemIcon::WorkshopItem].getTexture();
                item->uv = m_itemIcons[ItemIcon::WorkshopItem].getTextureRect();
                break;
            }
            
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
                    item->description = "Adjust the " + LabelA[i] + " " + LabelB[j] + " axis";

                    for (auto k = 0; k < (SelectionCount * 2) + 1; ++k)
                    {
                        const auto val = indexToValue(i,k);
                        
                        std::stringstream ss;
                        ss.precision(3);
                        ss << std::fixed << val;
                        item->labels.push_back(ss.str());
                    }

                    item->selectedIndex = static_cast<std::int32_t>(std::min(item->labels.size() - 1, valueToIndex(i, m_activeProfile.playerData.headwearOffsets[offset][j])));
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
                item->title = "Reset " + LabelA[i];
                item->description = "Resets the " + LabelA[i] + " to its default value";
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
    item = &m_menuLayout.items[TabID::Headwear].emplace_back();
    item->title = "Steam Workshop";
    item->description = "Opens the Steam Workshop in the overlay";
    item->labels.push_back("Find More Items");
    item->activated =
        [](Menu::Item&)
        {
            Social::showWorkshop();
        };
    item->texture = m_itemIcons[ItemIcon::WorkshopButton].getTexture();
    item->uv = m_itemIcons[ItemIcon::WorkshopButton].getTextureRect();
#endif
}

void ProfileStateV2::createEquipmentItems()
{
    m_lockedBallCount = 0;
    //for some reason (I forget) the ballInfo size
    //is different to the number of model defs
    std::int32_t c = 0;
    for (auto& ballDef : m_profileData.ballDefs)
    {
        if (m_sharedData.ballInfo[c++].locked)
        {
            m_lockedBallCount++;
        }
    }

    m_lockedClubCount = 0;
    for (const auto& data : m_clubData)
    {
        if (data.locked)
        {
            m_lockedClubCount++;
        }
    }

    m_menuLayout.items[TabID::Equipment].clear();

    auto* item = &m_menuLayout.items[TabID::Equipment].emplace_back();
    item->title = "Equipment Appearance";
    item->displayType = Menu::Item::Heading;
    item->description = "Choose how your equipment appears";


    //clubs
    item = &m_menuLayout.items[TabID::Equipment].emplace_back();
    item->title = "Clubset Appearance";
    item->displayType = Menu::Item::Slider;
    item->activated =
        [&](Menu::Item& i)
        {
            setClubIndex(i.selectedIndex);
            //setClubIndex() may have adjusted thit ti account for locked clubs
            i.selectedIndex = m_clubIndex;

            i.description = i.labels[i.selectedIndex] + "/" + std::to_string(m_clubData.size() - m_lockedClubCount);
            if (!m_clubData[i.selectedIndex].name.empty())
            {
                i.description += " " + m_clubData[i.selectedIndex].name;
            }

            m_detailsPane.text.getComponent<cro::Text>().setString(i.description);
            //hmm, we don't appear to be tracking unlocked items
            //for clubs, just if they're workshop clubs...
            if (m_clubData[i.selectedIndex].userItem)
            {
                i.texture = m_itemIcons[ItemIcon::WorkshopItem].getTexture();
                i.uv = m_itemIcons[ItemIcon::WorkshopItem].getTextureRect();
            }
            else
            {
                i.texture = nullptr;
            }
        };

    //see skipping locked items, below
    std::int32_t j = 1;
    for (auto i = 0u; i < m_clubData.size(); ++i)
    {
        if (m_clubData[i].locked)
        {
            item->labels.push_back("Locked");
        }
        else
        {
            item->labels.push_back(std::to_string(j++));
        }
    }
    item->selectedIndex = indexFromClubID(m_activeProfile.playerData.clubID);
    if (m_clubData[item->selectedIndex].userItem)
    {
        item->texture = m_itemIcons[ItemIcon::WorkshopItem].getTexture();
        item->uv = m_itemIcons[ItemIcon::WorkshopItem].getTextureRect();
    }
    else
    {
        item->texture = nullptr;
    }
    item->description = item->labels[item->selectedIndex] + "/" + std::to_string(m_clubData.size() - m_lockedClubCount);
    if (!m_clubData[item->selectedIndex].name.empty())
    {
        item->description += " " + m_clubData[item->selectedIndex].name;
    }
    setClubIndex(item->selectedIndex);



    //TODO we can't preview locked balls now there are no thumbnails :/

    //ball model selection
    item = &m_menuLayout.items[TabID::Equipment].emplace_back();
    item->title = "Ball Appearance";
    item->displayType = Menu::Item::Slider;
    item->activated =
        [&](Menu::Item& i)
        {
            setBallIndex(i.selectedIndex);

            //setBallIndex() may skip locked balls so we need to re-sync...
            i.selectedIndex = m_ballIndex;

            i.description = i.labels[i.selectedIndex] + "/" + std::to_string(m_ballModels.size() - m_lockedBallCount);
            if (!m_sharedData.ballInfo[i.selectedIndex].label.empty())
            {
                i.description += " " + m_sharedData.ballInfo[i.selectedIndex].label;
            }

            m_detailsPane.text.getComponent<cro::Text>().setString(i.description);
            
            switch (m_sharedData.ballInfo[i.selectedIndex].type)
            {
            default:
                i.texture = nullptr;
                break;
            case 1: //unlocked
                i.texture = m_itemIcons[ItemIcon::UnlockedItem].getTexture();
                i.uv = m_itemIcons[ItemIcon::UnlockedItem].getTextureRect();
                break;
            case 2: //from workshop
                i.texture = m_itemIcons[ItemIcon::WorkshopItem].getTexture();
                i.uv = m_itemIcons[ItemIcon::WorkshopItem].getTextureRect();
                break;
            }
        };
    
    //to hide locked balls we'll use a separate counter
    //then skip locked items in setBallIndex()
    j = 1;
    for (auto i = 0u; i < m_ballModels.size(); ++i)
    {
        if (m_sharedData.ballInfo[i].locked)
        {
            item->labels.push_back("Locked");
        }
        else
        {
            item->labels.push_back(std::to_string(j++));
        }
    }
    item->selectedIndex = indexFromBallID(m_activeProfile.playerData.ballID);
    switch (m_sharedData.ballInfo[item->selectedIndex].type)
    {
    default:
        item->texture = nullptr;
        break;
    case 1: //unlocked
        item->texture = m_itemIcons[ItemIcon::UnlockedItem].getTexture();
        item->uv = m_itemIcons[ItemIcon::UnlockedItem].getTextureRect();
        break;
    case 2: //from workshop
        item->texture = m_itemIcons[ItemIcon::WorkshopItem].getTexture();
        item->uv = m_itemIcons[ItemIcon::WorkshopItem].getTextureRect();
        break;
    }
    item->description = item->labels[item->selectedIndex] + "/" + std::to_string(m_ballModels.size() - m_lockedBallCount);
    if (!m_sharedData.ballInfo[item->selectedIndex].label.empty())
    {
        item->description += " " + m_sharedData.ballInfo[item->selectedIndex].label;
    }
    setBallIndex(item->selectedIndex);

    //colour property
    item = &m_menuLayout.items[TabID::Equipment].emplace_back();
    item->title = "Ball Tint";
    item->displayType = Menu::Item::Slider;
    item->description = "Choose ball colour";
    item->activated =
        [&](Menu::Item& i)
        {
            //*sigh* unfortunately this colour hack means we can't
            //show the palette on the preview withoput even more hack-arounds
            const auto idx = i.selectedIndex - 1 < 0 ? 255 : i.selectedIndex - 1;
            m_activeProfile.playerData.ballColourIndex = idx; //hack because white is index 0 but expects invalid idx
            const cro::Colour colour = idx < pc::Palette.size() ? pc::Palette[idx] : cro::Colour::White;

            //set the preview colour
            i.previewColour = colour;
            m_activeProfile.playerData.ballColour = colour;

            //update the shader uniform
            m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setMaterialProperty(0, "u_ballColour", m_activeProfile.playerData.ballColour);
        };

    //front is white/no colour
    item->labels.push_back("1");
    for (auto i = 0u; i < pc::PairCounts[0]; ++i)
    {
        item->labels.push_back(std::to_string(i + 2));
    }

    item->selectedIndex = m_activeProfile.playerData.ballColourIndex < pc::Palette.size() ? m_activeProfile.playerData.ballColourIndex + 1 : 0;
    item->displayType = Menu::Item::Slider;
    item->previewColour = item->selectedIndex == 0 ? cro::Colour::White : pc::Palette[item->selectedIndex-1];
    item->texture = &m_colourPreview;
    item->uv = { 0.f, 0.f, 1.f, 1.f };
    

#ifdef USE_GNS
    //workshop button if steam
    item = &m_menuLayout.items[TabID::Equipment].emplace_back();
    item->title = "Steam Workshop";
    item->description = "Opens the Steam Workshop in the overlay";
    item->labels.push_back("Find More Items");
    item->activated =
        [](Menu::Item&)
        {
            Social::showWorkshop();
        };
    item->texture = m_itemIcons[ItemIcon::WorkshopButton].getTexture();
    item->uv = m_itemIcons[ItemIcon::WorkshopButton].getTextureRect();
#endif
}

void ProfileStateV2::createLoadoutItems()
{
    m_menuLayout.items[TabID::Loadout].clear();

    auto* item = &m_menuLayout.items[TabID::Loadout].emplace_back();
    item->title = "Select Loadout";
    item->displayType = Menu::Item::Heading;
    item->description = "Select your loadout from equipment bought at the Equipment Counter";
    item->selected = [](const Menu::Item&) {}; //empty just to hide the stats sprite

    const auto itemAvailable = [](std::int32_t i)
        {
            switch (i)
            {
            default: return true;
            case GearID::FiveW:
                return Social::getLevel() >= ClubID::getUnlockLevel(ClubID::FiveWood);
            case GearID::FourI:
                return Social::getLevel() >= ClubID::getUnlockLevel(ClubID::FourIron);
            case GearID::SixI:
                return Social::getLevel() >= ClubID::getUnlockLevel(ClubID::SixIron);
            case GearID::SevenI:
                return Social::getLevel() >= ClubID::getUnlockLevel(ClubID::SevenIron);
            case GearID::NineI:
                return Social::getLevel() >= ClubID::getUnlockLevel(ClubID::NineIron);
            }
            return true;
        };
    
    //TODO slight problem here in that the equipment counter doesn't cover the lob wedge...
    const std::array titles =
    {
        std::string("Driver"),
        std::string("3 Wood"),
        std::string("5 Wood"),
        std::string("4 Iron"),
        std::string("5 Iron"),
        std::string("6 Iron"),
        std::string("7 Iron"),
        std::string("8 Iron"),
        std::string("9 Iron"),
        std::string("Pitch Wedge"),
        std::string("Gap Wedge"),
        std::string("Sand Wedge"),
        std::string("Balls"),
    };

    //sort inventory into sub-lists of things that we own
    struct SubItem final
    {
        std::string name;
        std::int32_t idx = -1;
        SubItem(const std::string& s) :name(s) {}
    };
    std::array<std::vector<SubItem>, GearID::Count> subItems;
    for (auto& list : subItems)
    {
        list.emplace_back("Default");
    }

    for (auto i = 0u; i < m_sharedData.inventory.inventory.size(); ++i)
    {
        const auto idx = m_sharedData.inventory.inventory[i];
        if (idx != -1)
        {
            //we own this
            const auto& item = inv::Items[i];
            subItems[item.type].emplace_back(inv::Manufacturers[item.manufacturer]).idx = i;
        }
    }


    for (auto i = 0u; i < GearID::Count; ++i)
    {
        const auto available = itemAvailable(i);

        item = &m_menuLayout.items[TabID::Loadout].emplace_back();
        item->title = titles[i];

        if (available)
        {
            std::vector<std::int32_t> itemIndices;

            const auto& items = subItems[i];
            for (const auto& subItem : items)
            {
                item->labels.push_back(subItem.name);
                itemIndices.push_back(subItem.idx);
            }
            item->displayType = Menu::Item::Slider;
            item->activated = 
                [&, itemIndices, i](Menu::Item& menuItem)
                {
                    m_activeProfile.loadout.items[i] = itemIndices[menuItem.selectedIndex];
                    refreshStat(i, itemIndices[menuItem.selectedIndex], true);
                };
            const auto res = std::find(itemIndices.cbegin(), itemIndices.cend(), m_activeProfile.loadout.items[i]);
            if (res != itemIndices.cend())
            {
                item->selectedIndex = static_cast<std::int32_t>(std::distance(itemIndices.cbegin(), res));
            }
            item->selected = 
                [&, itemIndices, i](const Menu::Item& menuItem) 
                {
                    //update stats window
                    refreshStat(i, itemIndices[menuItem.selectedIndex], true);
                    m_detailsPane.image.getComponent<cro::Sprite>().setTexture(m_previewTexture.getTexture());
                    m_detailsPane.image.getComponent<cro::Transform>().setOrigin({ m_detailsPane.image.getComponent<cro::Sprite>().getTextureBounds().width / 2.f, 0.f });
                    m_detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
                };
        }
        else
        {
            //WARNING this relies on i matching ClubIDs
            item->labels.push_back("Unlocked at level " + std::to_string(ClubID::getUnlockLevel(i)));
            item->activated = [](Menu::Item&) {};
            item->selected = [&](const Menu::Item&)
                {
                    //this is an awful icon. Maybe we're better off adding it to the item instead
                    /*m_detailsPane.image.getComponent<cro::Sprite>() = m_itemIcons[ItemIcon::Locked];
                    m_detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);*/
                };
            item->texture = m_itemIcons[ItemIcon::Locked].getTexture();
            item->uv = m_itemIcons[ItemIcon::Locked].getTextureRect();
        }
    }
    

    //equipment counter
    item = &m_menuLayout.items[TabID::Loadout].emplace_back();
    item->title = "Go To";
    item->labels.push_back("Equipment Counter");
    item->description = "Find more upgrades at the Equipment Counter";
    item->activated =
        [&](Menu::Item&)
        {
            requestStackPush(StateID::Shop);
        };
    item->texture = m_itemIcons[ItemIcon::EquipButton].getTexture();
    item->uv = m_itemIcons[ItemIcon::EquipButton].getTextureRect();
    item->selected = [&](const Menu::Item&) {}; //hides stat image
}

void ProfileStateV2::createDetailItems()
{
    m_menuLayout.items[TabID::Details].clear();

    auto* item = &m_menuLayout.items[TabID::Details].emplace_back();
    item->title = "Profile Details";
    item->displayType = Menu::Item::Heading;

    //name - sigh applyNameString() has this index hardcoded...
    item = &m_menuLayout.items[TabID::Details].emplace_back();
    item->title = "Profile Name";
    item->activated = 
        [&](Menu::Item& i)
        {
            if (m_sharedData.activeInput != SharedStateData::ActiveInput::Keyboard)
            {
#ifdef USE_GNS
                if (Social::isSteamdeck(true))
                {
                    //OSK
                    const auto cb =
                        [&](bool submitted, const char* buffer)
                        {
                            if (submitted)
                            {
                                m_activeProfile.playerData.name = cro::String::fromUtf8(buffer, buffer + std::strlen(buffer));
                                applyNameString();
                            }
                        };

                    //this only shows the overlay as Steam takes care of dismissing it
                    const auto utf = m_activeProfile.playerData.name.toUtf8Char();
                    Social::showTextInput(cb, "Profile Name", ConstVal::MaxStringChars * 2, utf.data());
                }
                else
#endif
                {
                    //m_showOSK = true; // hmm this is used to block input, but OSK state shouldn't be forwarding it?
                    m_sharedData.useOSKBuffer = true;
                    m_sharedData.OSKBuffer = m_activeProfile.playerData.name;
                    requestStackPush(StateID::Keyboard);
                }
            }
            else
            {
                //show ImGuiWindow
                cro::App::getWindow().setMouseCaptured(false);
                m_nameBuffer = m_activeProfile.playerData.name.toUtf8Char();
                m_showNameInput = true;
            }
        };
    item->labels.push_back(m_activeProfile.playerData.name);
    item->description = "Choose a profile name";

    //TODO description
    //hmmm is this something we really want to bother editing?


    //load the current mugshot image if available
    if (!m_activeProfile.playerData.mugshot.empty())
    {
        const auto& tex = m_sharedData.sharedResources->textures.get(m_activeProfile.playerData.mugshot);

        //const glm::vec2 texSize(tex.getSize());
        //const glm::vec2 scale = glm::vec2(96.f, 48.f) / texSize;
        m_detailsPane.mugshotImage.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        m_detailsPane.mugshotImage.getComponent<cro::Sprite>().setTexture(tex);
    }
    else
    {
        m_detailsPane.mugshotImage.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    }


    //update mugshot
    item = &m_menuLayout.items[TabID::Details].emplace_back();
    item->title = "Mugshot";
    item->activated =
        [&](Menu::Item& i)
        {
            updateMugshot();
        };
    item->labels.push_back("Update");
    item->description = "Update the player icon displayed for this profile.";
    item->activatedAudioID = MenuSoundEvent::Snapshot;

    //remove mugshot
    item = &m_menuLayout.items[TabID::Details].emplace_back();
    item->title = "Mugshot";
    item->activated =
        [&](Menu::Item& i)
        {
            clearMugshot();
        };
    item->labels.push_back("Remove");
#ifdef USE_GNS
    item->description = "Remove the player icon displayed for this profile. Defaults to your Steam avatar icon.";
#else
    item->description = "Remove the player icon displayed for this profile.";
#endif
    item->activatedAudioID = MenuSoundEvent::Crumple;

    //voice type
    if (const auto v = std::find_if(m_voices.begin(), m_voices.end(),
        [&](const VoiceData& as) {return as.audioScape.getUID() == m_activeProfile.playerData.voiceID; });
        v != m_voices.end())
    {
        m_voiceIndex = static_cast<std::int32_t>(std::distance(m_voices.begin(), v));
    };

    item = &m_menuLayout.items[TabID::Details].emplace_back();
    item->title = "Voice Type";
    item->activated =
        [&](Menu::Item& i)
        {
            m_voiceIndex = i.selectedIndex;
            m_activeProfile.playerData.voiceID = m_voices[m_voiceIndex].audioScape.getUID();
            playPreviewAudio();

            i.description = "Voice: " + m_voices[m_voiceIndex].audioScape.getName();
            m_detailsPane.text.getComponent<cro::Text>().setString(i.description);

            if (m_voices[m_voiceIndex].isWorkshop)
            {
                i.texture = m_itemIcons[ItemIcon::WorkshopItem].getTexture();
                i.uv = m_itemIcons[ItemIcon::WorkshopItem].getTextureRect();
            }
            else
            {
                i.texture = nullptr;
            }
        };
    item->selectedIndex = m_voiceIndex;
    if (m_voices[m_voiceIndex].isWorkshop)
    {
        item->texture = m_itemIcons[ItemIcon::WorkshopItem].getTexture();
        item->uv = m_itemIcons[ItemIcon::WorkshopItem].getTextureRect();
    }
    else
    {
        item->texture = nullptr;
    }
    item->description = "Voice: " + m_voices[m_voiceIndex].audioScape.getName();
    for (auto i = 0u; i < m_voices.size(); ++i)
    {
        item->labels.push_back(std::to_string(i + 1));
    }
    item->displayType = Menu::Item::Slider;
    item->alwaysActivate = true;
    item->selected =
        [&](const Menu::Item&)
        {
            m_detailsPane.applyButton.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
            
            m_detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
            m_detailsPane.image.getComponent<cro::Sprite>() = m_tabBar.items[m_tabBar.activeIndex].sprite;
            const auto bounds = m_detailsPane.image.getComponent<cro::Sprite>().getTextureBounds();
            m_detailsPane.image.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f,0.f });
        };
    item->activatedAudioID = -1; //mutes the menu sound

    //voice pitch
    item = &m_menuLayout.items[TabID::Details].emplace_back();
    item->title = "Voice Pitch";
    item->activated =
        [&](Menu::Item& i)
        {
            m_activeProfile.playerData.voicePitch = static_cast<std::int8_t>(i.selectedIndex - 2);
            playPreviewAudio();
        };
    item->selectedIndex = m_activeProfile.playerData.voicePitch + 2;
    item->labels = { "-2", "-1", "Default", "+1", "+2" };
    item->displayType = Menu::Item::Slider;
    item->alwaysActivate = true;
    item->selected =
        [&](const Menu::Item&)
        {
            m_detailsPane.applyButton.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

            m_detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
            m_detailsPane.image.getComponent<cro::Sprite>() = m_tabBar.items[m_tabBar.activeIndex].sprite;
            const auto bounds = m_detailsPane.image.getComponent<cro::Sprite>().getTextureBounds();
            m_detailsPane.image.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f,0.f });
        };
    item->activatedAudioID = -1; //mutes the menu sound

#ifdef USE_GNS
    //workshop button if steam
    item = &m_menuLayout.items[TabID::Details].emplace_back();
    item->title = "Steam Workshop";
    item->description = "Opens the Steam Workshop in the overlay";
    item->labels.push_back("Find More Voices");
    item->activated =
        [](Menu::Item&)
        {
            Social::showWorkshop();
        };
    item->texture = m_itemIcons[ItemIcon::WorkshopButton].getTexture();
    item->uv = m_itemIcons[ItemIcon::WorkshopButton].getTextureRect();
#endif
}

void ProfileStateV2::onCachedPush()
{
    m_exitFlags = 0;
    m_exitHoldTimer = 0.f;

    //we make a copy of this so we can cancel any modifications
    m_activeProfile = m_profileData.playerProfiles[m_profileData.activeProfileIndex];
    m_activeProfile.loadout.read(m_activeProfile.playerData.profileID);
    refreshBio();

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

    activateTab(0);
    //refreshView(); //done by activateTab();

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
    activateTab((m_tabBar.activeIndex + 1) % TabID::Count);
    playSound(MenuSoundEvent::Activate);
}

void ProfileStateV2::prevTab()
{
    activateTab((m_tabBar.activeIndex + (TabID::Count - 1)) % TabID::Count);
    playSound(MenuSoundEvent::Cancel);
}

void ProfileStateV2::activateTab(std::int32_t idx)
{
    if (m_detailsPane.tabDetails[m_tabBar.activeIndex].isValid())
    {
        m_detailsPane.tabDetails[m_tabBar.activeIndex].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    }

    m_tabBar.activeIndex = idx;
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

    //set item 0 as focused
    focusToIndex(m_tabBar, m_menuLayout);
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
    m_tabBar.items[m_tabBar.activeIndex].renderWidth = static_cast<float>(m_menuLayout.texture.getSize().x);
    m_tabBar.items[m_tabBar.activeIndex].renderWidth = std::round(m_tabBar.items[m_tabBar.activeIndex].renderWidth * m_tabBar.items[m_tabBar.activeIndex].displayWidth);

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
            const auto centreWidth = m_tabBar.items[m_tabBar.activeIndex].renderWidth - (left.size.x * 2.f);
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
    glm::vec2 backgroundArea = { static_cast<float>(cro::App::getWindow().getSize().x - (m_tabBar.items[m_tabBar.activeIndex].renderWidth * viewScale)),
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
            //mugshot cam doesn't have one of these
            if (cam.resizeCallback)
            {
                cam.resizeCallback(cam);
            }
        }
    }
    m_statScene.getActiveCamera().getComponent<cro::Camera>().resizeCallback(m_statScene.getActiveCamera().getComponent<cro::Camera>());

    //reposition club sprite
    const glm::vec2 bgSize = m_previewTexture.getSize();
    if (m_detailsPane.clubsetImage.isValid())
    {
        //m_detailsPane.clubsetImage.getComponent<cro::Transform>().setScale(glm::vec2(viewScale));
        const glm::vec2 thumbSize = { m_clubData[0].uv.width /** viewScale*/, m_clubData[0].uv.height /** viewScale*/ };
        glm::vec2 pos = { ((bgSize.x / 2.f) - thumbSize.x) / 2.f, (bgSize.y - thumbSize.y) / 2.f };
        pos.x = std::floor(pos.x);
        pos.y = std::floor(pos.y);
        m_detailsPane.clubsetImage.getComponent<cro::Transform>().setPosition(pos);
        m_detailsPane.clubsetImage.getComponent<cro::Drawable2D>().setCroppingArea({ std::abs(std::min(pos.x, 0.f)), std::abs(std::min(pos.y, 0.f)),
                                                                                        bgSize.x / 2.f, bgSize.y });
    }

    //and mugshot
    const glm::vec2 texSize = glm::vec2(MugshotTexSize.x, MugshotTexSize.y) * viewScale;
    glm::vec2 pos = { (((bgSize.x / 2.f) - texSize.x) / 2.f) + (bgSize.x / 2.f), (bgSize.y - texSize.y) };
    pos.x = std::floor(pos.x);
    pos.y = std::floor(pos.y);
    m_detailsPane.mugshotImage.getComponent<cro::Transform>().setPosition(pos);
    m_detailsPane.mugshotImage.getComponent<cro::Callback>().setUserData<float>(viewScale);

    cro::FloatRect crop = { glm::vec2(0.f), glm::vec2(MugshotTexSize) };
    crop.left = std::abs(std::min(0.f, pos.x - (bgSize.x / 2.f))) * 2.f;
    crop.width -= (crop.left * 2.f);

    m_detailsPane.mugshotImage.getComponent<cro::Drawable2D>().setCroppingArea(crop);
    m_detailsPane.mugshotImage.getComponent<cro::Drawable2D>().bindUniform("u_croppingArea", glm::vec4(crop.left, crop.bottom, crop.width, crop.height));

    //and update the layout of stat items
    //confusingly these are done pre-scale whereas
    //the texture size itself is scaled...
    const glm::vec2 previewSize(CentreWidth * 2.f, std::floor(CentreHeight * 1.25f));
    m_statLayout.statTitle.getComponent<cro::Transform>().setPosition({ 6.f, previewSize.y - 24.f });
    m_statLayout.manufacturerIcon.getComponent<cro::Transform>().setPosition({ (previewSize.x / 2.f) - 42.f, previewSize.y - 62.f });
    //this is done in refreshStat()
    //m_statLayout.manufacturerName.getComponent<cro::Transform>().setPosition({ (previewSize.x / 2.f) - 26.f, previewSize.y - 54.f });
    m_statLayout.manufacturerInfo.getComponent<cro::Transform>().setPosition({ (previewSize.x / 2.f) - 40.f, previewSize.y - 64.f });
    m_statLayout.statBars[0].bgEnt.getComponent<cro::Transform>().setPosition({ 2.f, previewSize.y - 60.f });
    m_statLayout.statBars[1].bgEnt.getComponent<cro::Transform>().setPosition({ 2.f, previewSize.y - 106.f });
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
                if (item.displayType == Menu::Item::TextOnly)
                {
                    //achievement icon
                    m_menuQuad.setPosition(pos + glm::vec2(ItemSpacing, ItemSpacing));
                    pos.x += ItemSpacing + ItemImage.x; //moves title text over
                }
                else
                {
                    //align to the right
                    m_menuQuad.setPosition(pos + glm::vec2(m_tabBar.items[m_tabBar.activeIndex].renderWidth - (ItemSpacing + ItemImage.x), ItemSpacing));
                }
                m_menuQuad.setTexture(*item.texture);
                m_menuQuad.setScale(ItemImage / glm::vec2(item.uv.width, item.uv.height));
                m_menuQuad.setTextureRect(item.uv);
                m_menuQuad.setColour(item.previewColour);

                m_menuQuad.draw();
            }

            pos.x += ItemSpacing;
            pos.y += ItemHeight - LineSpacing;

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
                updateSliderGraphic(item.selectedIndex, static_cast<std::int32_t>(item.labels.size() - 1));
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
    updatePalettePreview(-1, -1); //reset this allow item selected callback to update it

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

    if (m_menuLayout.itemIndex < m_menuLayout.items[m_tabBar.activeIndex].size() - 1)
    {
        do
        {
            m_menuLayout.itemIndex++;
        } while (m_menuLayout.itemIndex < m_menuLayout.items[m_tabBar.activeIndex].size() - 1
            && m_menuLayout.items[m_tabBar.activeIndex][m_menuLayout.itemIndex].displayType == Menu::Item::Heading);
        updateMenuItems();

        playSound(MenuSoundEvent::Switch);
    }
}

void ProfileStateV2::prevItem()
{
    //reset mouse hover highlight
    m_menuLayout.hoveredIndex = -1;

    //hmm these are uints so we can't use max(0)
    if (m_menuLayout.itemIndex > 0)
    {
        do
        {
            //also hmmm this doesn't work if the heading *is* at 0
            m_menuLayout.itemIndex--;
        } while (m_menuLayout.itemIndex > 0
            && m_menuLayout.items[m_tabBar.activeIndex][m_menuLayout.itemIndex].displayType == Menu::Item::Heading);
        updateMenuItems();

        playSound(MenuSoundEvent::Switch);
    }
}

void ProfileStateV2::activateLeft()
{
    //reset mouse hover highlight
    m_menuLayout.hoveredIndex = -1;

    /*const */auto& item = m_menuLayout.items[m_tabBar.activeIndex][m_menuLayout.itemIndex];
    //ugh activate() ought to be const but that's a whole mess of mutables.
    if (item.activateLeft())
    {
        updateMenuItems();
        playSound(item.activatedAudioID);
    }
}

void ProfileStateV2::activateRight()
{
    //reset mouse hover highlight
    m_menuLayout.hoveredIndex = -1;

    auto& item = m_menuLayout.items[m_tabBar.activeIndex][m_menuLayout.itemIndex];
    if (item.activateRight())
    {
        updateMenuItems();
        playSound(item.activatedAudioID);
    }
}

void ProfileStateV2::activate()
{
    auto& item = m_menuLayout.items[m_tabBar.activeIndex][m_menuLayout.itemIndex];
    if (item.activate())
    {
        playSound(item.activatedAudioID);
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
        activateTab(m_tabBar.hoveredIndex);
        m_tabBar.hoveredIndex = -1;
        playSound(MenuSoundEvent::Activate);
    }
    else
    {
        if (m_menuLayout.hoveredIndex == -1 ||
            m_menuLayout.items[m_tabBar.activeIndex][m_menuLayout.hoveredIndex].displayType != Menu::Item::Heading)
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

void ProfileStateV2::refreshStat(std::uint32_t catID, std::int32_t invID, bool setPointer)
{
    m_statLayout.statTitle.getComponent<cro::Text>().setString(inv::ItemStrings[catID]);
    const auto baseTextPos = m_statLayout.manufacturerInfo.getComponent<cro::Transform>().getPosition();

    if (invID == -1)
    {
        //nothing assigned to this slot
        m_statLayout.statBars[0].bgEnt.getComponent<cro::Callback>().setUserData<std::int32_t>(0);
        m_statLayout.statBars[1].bgEnt.getComponent<cro::Callback>().setUserData<std::int32_t>(0);

        if (setPointer)
        {
            m_statLayout.statBars[0].pointer.getComponent<cro::Callback>().setUserData<std::int32_t>(0);
            m_statLayout.statBars[1].pointer.getComponent<cro::Callback>().setUserData<std::int32_t>(0);
        }

        m_statLayout.statBars[0].text.getComponent<cro::Text>().setString("Default: 0");
        m_statLayout.manufacturerInfo.getComponent<cro::Text>().setString(ManufacturerText.back());

        m_statLayout.manufacturerIcon.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        m_statLayout.manufacturerName.getComponent<cro::Transform>().setPosition(baseTextPos + glm::vec3(0.f, 12.f, 0.f));
        m_statLayout.manufacturerName.getComponent<cro::Text>().setString("Default");

        if (catID == GearID::Balls)
        {
            m_statLayout.statBars[1].text.getComponent<cro::Text>().setString(" ");
            m_statLayout.statBars[1].bgEnt.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            m_statLayout.statBars[1].pointer.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
        }
        else
        {
            m_statLayout.statBars[1].text.getComponent<cro::Text>().setString("Default: 0");
            m_statLayout.statBars[1].bgEnt.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
            m_statLayout.statBars[1].pointer.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
        }
        return;
    }


    const auto& item = inv::Items[invID];
    m_statLayout.manufacturerInfo.getComponent<cro::Text>().setString(ManufacturerText[item.manufacturer]);
    m_statLayout.manufacturerName.getComponent<cro::Text>().setString(inv::Manufacturers[item.manufacturer]);
    m_statLayout.manufacturerName.getComponent<cro::Transform>().setPosition(baseTextPos + glm::vec3(26.f, 18.f, 0.f));

    m_statLayout.manufacturerIcon.getComponent<cro::Sprite>() =
        m_itemIcons[ItemIcon::Gallawent + item.manufacturer];
    m_statLayout.manufacturerIcon.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

    m_statLayout.statBars[0].bgEnt.getComponent<cro::Callback>().setUserData<std::int32_t>(item.stat01);
    m_statLayout.statBars[1].bgEnt.getComponent<cro::Callback>().setUserData<std::int32_t>(item.stat02);

    if (setPointer)
    {
        m_statLayout.statBars[0].pointer.getComponent<cro::Callback>().setUserData<std::int32_t>(item.stat01);
        m_statLayout.statBars[1].pointer.getComponent<cro::Callback>().setUserData<std::int32_t>(item.stat02);
    }

    std::int32_t category = 0;
    switch (item.type)
    {
    default:
    case inv::ItemType::Driver:
        break;
    case inv::ItemType::FiveW:
    case inv::ItemType::ThreeW:
        category = 1;
        break;
    case inv::ItemType::FourI:
    case inv::ItemType::FiveI:
    case inv::ItemType::SixI:
    case inv::ItemType::SevenI:
    case inv::ItemType::EightI:
    case inv::ItemType::NineI:
        category = 2;
        break;
    case inv::ItemType::GapWedge:
    case inv::ItemType::PitchWedge:
    case inv::ItemType::SandWedge:
        category = 3;
        break;
    case inv::ItemType::Ball:
        category = 4;
        break;
    }

    std::string valStr;
    if (item.stat01 > -1)
    {
        valStr += "+";
    }
    valStr += std::to_string(item.stat01);
    m_statLayout.statBars[0].text.getComponent<cro::Text>().setString(inv::StatLabels[category].stat1 + valStr);


    //second stat might be empty, eg balls
    valStr.clear();
    if (!inv::StatLabels[category].stat2.empty())
    {
        if (item.stat02 > -1)
        {
            valStr += "+";
        }
        valStr += std::to_string(item.stat02);
        m_statLayout.statBars[1].bgEnt.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
        m_statLayout.statBars[1].pointer.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
    }
    else
    {
        //display number of balls remaining
        const auto amt = m_sharedData.inventory.inventory[invID];
        valStr = std::to_string(amt) + " remaining";

        m_statLayout.statBars[1].bgEnt.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
        m_statLayout.statBars[1].pointer.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    }
    m_statLayout.statBars[1].text.getComponent<cro::Text>().setString(inv::StatLabels[category].stat2 + valStr);
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

    //rather than creating an emitter for every single model a single
    //emitter will do (well 2/3 to buffer when switching previews  quickly)
    cro::EmitterSettings emitterSettings;
    emitterSettings.loadFromFile("assets/golf/particles/puff_small.cps", m_resources.textures);

    for (auto i = 0u; i < m_ballParticles.size(); ++i)
    {
        m_ballParticles[i] = m_previewScene.createEntity();
        m_ballParticles[i].addComponent<cro::Transform>().setPosition(BallPos);
        m_ballParticles[i].addComponent<cro::ParticleEmitter>().settings = emitterSettings;
    }


    //this has all been parsed by the menu state - so we're assuming
    //all the models etc are fine and load without chicken
    std::int32_t c = 0;
    for (auto& ballDef : m_profileData.ballDefs)
    {
        //this is superfluous here as we do it each time we
        //lay out the menu items in case a ball was unlocked at the
        //equipment counter
        /*if (m_sharedData.ballInfo[c].locked)
        {
            m_lockedBallCount++;
        }*/
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

        ++c;
    }

    m_ballModels[0].ball.getComponent<cro::Model>().setHidden(false);
}

void ProfileStateV2::loadClubData()
{
    const auto processClubPath =
        [&](const std::string& path, bool isUser)
        {
            ClubData data;
            data.userItem = isUser;

            bool hasModels = true;

            const auto files = cro::FileSystem::listFiles(path);
            for (const auto& f : files)
            {
                if (f == "list.cst")
                {
                    cro::ConfigFile cfg;
                    if (cfg.loadFromFile(path + "/" + f))
                    {
                        const auto& props = cfg.getProperties();
                        for (const auto& p : props)
                        {
                            const auto& name = p.getName();
                            if (name == "name")
                            {
                                data.name = p.getValue<std::string>();
                            }
                            else if (name == "uid")
                            {
                                data.uid = p.getValue<std::uint32_t>();
                            }
                            else if (name == "man")
                            {
                                data.manufacturer = std::clamp(p.getValue<std::int32_t>(), 0, std::int32_t(inv::ManufID::BeyTree));
                                data.locked =
                                    data.manufacturer != -1
                                    && (m_sharedData.inventory.manufacturerFlags & (1 << data.manufacturer)) == 0;

                                //moved to creating menu items
                                //(for one thing this will be wrong if a duplicated
                                //club is counted then removed...)
                                /*if (data.locked)
                                {
                                    m_lockedClubCount++;
                                }*/
                            }
                        }

                        //make sure some models are listed and exist
                        if (const auto* models = cfg.findObjectWithName("models");
                            models == nullptr)
                        {
                            LogE << "No models were listed in " << path + "/" + f;
                            hasModels = false;
                        }
                        else
                        {
                            for (const auto& p : models->getProperties())
                            {
                                if (p.getName() == "path")
                                {
                                    if (!cro::FileSystem::fileExists(path + "/" + p.getValue<std::string>()))
                                    {
                                        LogE << path << " lists model files, but they were not found on disk" << std::endl;
                                        hasModels = false;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
                else if (f == "thumb.png")
                {
                    data.thumbnail = path + "/" + f;
                }
            }

            if (!data.name.empty()
                && !data.thumbnail.empty()
                && hasModels)
            {
                //make sure this ID doesn't already exist!!
                if (std::find_if(m_clubData.begin(), m_clubData.end(), [&data](const ClubData& cd) { return cd.uid == data.uid; }) == m_clubData.end())
                {
                    m_clubData.push_back(data);
                }
                else
                {
                    LogW << "Multiple club sets with UID " << data.uid << " were found." << std::endl;
                }
            }
            else
            {
                LogW << path << ": clubset skipped, invalid file data" << std::endl;
            }
        };

    const auto ContentDirs = Content::getInstallPaths();
    for (const auto& c : ContentDirs)
    {
        const auto basePath = cro::FileSystem::getResourcePath() + c + "clubs/";
        const auto clubsets = cro::FileSystem::listDirectories(basePath);

        for (const auto& s : clubsets)
        {
            processClubPath(basePath + s, false);
        }
    }

    //make sure the default set is first, if it's found
    if (auto res = std::find_if(m_clubData.begin(), m_clubData.end(),
        [](const ClubData& cd)
        {
            return cd.name.find("Default") != std::string::npos;
        }); res != m_clubData.end() && m_clubData.size() > 1)
    {
        std::swap(m_clubData[0], m_clubData[std::distance(m_clubData.begin(), res)]);
    }

    //workshop clubs
    const auto basePath = Content::getUserContentPath(Content::UserContent::Clubs);
    auto clubsets = cro::FileSystem::listDirectories(basePath);

    //remove dirs from this list if it's not from the workshop (rather crudely)
    clubsets.erase(std::remove_if(clubsets.begin(), clubsets.end(), [](const std::string& s) {return s.back() != 'w'; }), clubsets.end());

    if (clubsets.size() > ConstVal::MaxClubsets)
    {
        clubsets.resize(ConstVal::MaxClubsets);
        LogW << "Installed clubsets have been truncated to the maximum 64!" << std::endl;
    }

    for (const auto& s : clubsets)
    {
        processClubPath(basePath + s, true);
    }
}

void ProfileStateV2::loadVoiceData()
{
    //parse all the available audioscapes
    const std::array EmitterNames =
    {
        std::string("bunker"),
        std::string("fairway"),
        std::string("green"),
        std::string("celebrate"),
        std::string("hook"),
        std::string("rough"),
        std::string("scrub"),
        std::string("slice"),
        std::string("water")
    };

    std::vector<std::string> paths;
    const auto ContentDirs = Content::getInstallPaths();

    for (const auto& c : ContentDirs)
    {
        std::string basePath = "sound/avatars/";
        const auto files = cro::FileSystem::listFiles(c + basePath);
        for (const auto& f : files)
        {
            if (cro::FileSystem::getFileExtension(f) == ".xas")
            {
                paths.push_back(c + basePath + f);
            }
        }
    }


    //we can't always gaurantee paths are read in the same order across
    //different OS so let's sort them to be sure
    std::sort(paths.begin(), paths.end());
    const auto next = paths.size();

    const auto basePath = Content::getUserContentPath(Content::UserContent::Voice);
    const auto dirs = cro::FileSystem::listDirectories(basePath);
    for (const auto& dir : dirs)
    {
        const auto files = cro::FileSystem::listFiles(basePath + dir);
        for (const auto& f : files)
        {
            if (cro::FileSystem::getFileExtension(f) == ".xas")
            {
                paths.push_back(basePath + dir + "/" + f);
            }
        }
    }

    if (next < paths.size())
    {
        //more were added
        std::sort(paths.begin() + next, paths.end());
    }

    std::size_t i = 0;
    for (const auto& path : paths)
    {
        cro::AudioScape as;
        as.loadFromFile(path, m_resources.audio);

        bool allEmitters = true;
        for (const auto& emitter : EmitterNames)
        {
            if (!as.hasEmitter(emitter))
            {
                allEmitters = false;
                LogW << "Skipping " << as.getName() << ", missing emitter " << emitter << std::endl;
                break;
            }
        }

        i++;
        if (as.getUID() != 0
            && allEmitters)
        {
            auto& vd = m_voices.emplace_back();
            vd.audioScape = as;
            vd.isWorkshop = i > next;
        }
    }

    if (m_voices.empty())
    {
        //possibly because no audio hardware was found
        m_voices.emplace_back();
    }
}

std::int32_t ProfileStateV2::indexFromAvatarID(std::uint32_t skinID) const
{
    const auto& avatarInfo = m_sharedData.avatarInfo;

    if (auto result = std::find_if(avatarInfo.cbegin(), avatarInfo.cend(),
        [skinID](const SharedStateData::AvatarInfo& a) {return a.uid == skinID; }); result != avatarInfo.cend())
    {
        return static_cast<std::int32_t>(std::distance(avatarInfo.cbegin(), result));
    }

    return 0;
}

std::int32_t ProfileStateV2::indexFromHairID(std::uint32_t hairID) const
{
    const auto& hairInfo = m_sharedData.hairInfo;
    if (auto result = std::find_if(hairInfo.cbegin(), hairInfo.cend(),
        [hairID](const SharedStateData::HairInfo& hi) {return hi.uid == hairID; }); result != hairInfo.end())
    {
        return static_cast<std::int32_t>(std::distance(hairInfo.begin(), result));
    }
    return 0;
}

std::int32_t ProfileStateV2::indexFromBallID(std::uint32_t ballID) const
{
    const auto& ballInfo = m_sharedData.ballInfo;
    if (auto result = std::find_if(ballInfo.cbegin(), ballInfo.cend(),
        [ballID](const SharedStateData::BallInfo& b)
        {
            return b.uid == ballID;
        }); result != ballInfo.cend())
    {
        return static_cast<std::int32_t>(std::distance(ballInfo.cbegin(), result));
    }

    return 0;
}

std::int32_t ProfileStateV2::indexFromClubID(std::uint32_t uid) const
{
    if (auto result = std::find_if(m_clubData.cbegin(), m_clubData.cend(),
        [uid](const ClubData& cd) {return cd.uid == uid; }); result != m_clubData.cend())
    {
        return static_cast<std::int32_t>(std::distance(m_clubData.cbegin(), result));
    }
    return 0;
}

void ProfileStateV2::setAvatarIndex(std::int32_t idx)
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
            idx = static_cast<std::int32_t>((idx + (m_avatarModels.size() - 1)) % m_avatarModels.size());
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
}

void ProfileStateV2::setHairIndex(std::int32_t idx)
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

void ProfileStateV2::setHatIndex(std::int32_t idx)
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

void ProfileStateV2::setBallIndex(std::int32_t idx)
{
    CRO_ASSERT(idx < m_ballModels.size(), "");

    m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setHidden(true);

    //skip over locked balls. This isn't ideal, we want to preview
    //them without making them selectable...
    if (idx > m_ballIndex)
    {
        while (m_sharedData.ballInfo[idx].locked)
        {
            idx = (idx + 1) % m_ballModels.size();
        }
    }
    else if (idx < m_ballIndex)
    {
        while (m_sharedData.ballInfo[idx].locked)
        {
            idx = static_cast<std::int32_t>((idx +( m_ballModels.size() -1)) % m_ballModels.size());
        }
    }

    m_ballIndex = idx;

    m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setHidden(false);
    m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setMaterialProperty(0, "u_ballColour", m_activeProfile.playerData.ballColour);

    m_particleIndex = (m_particleIndex + 1) % m_ballParticles.size();
    m_ballParticles[m_particleIndex].getComponent<cro::ParticleEmitter>().start();

    m_activeProfile.playerData.ballID = m_sharedData.ballInfo[m_ballIndex].uid;
}

void ProfileStateV2::setClubIndex(std::int32_t idx)
{
    if (idx > m_clubIndex)
    {
        while (m_clubData[idx].locked)
        {
            idx = (idx+1) % m_clubData.size();
        }
    }
    else if (idx < m_clubIndex)
    {
        while (m_clubData[idx].locked)
        {
            idx = static_cast<std::int32_t>((idx + (m_clubData.size() - 1)) % m_clubData.size());
        }
    }

    //update the thumbnail preview
    if (m_detailsPane.clubsetImage.isValid())
    {
        m_detailsPane.clubsetImage.getComponent<cro::Sprite>().setTextureRect(m_clubData[idx].uv);
    }

    m_clubIndex = idx;
    m_activeProfile.playerData.clubID = m_clubData[idx].uid;
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

void ProfileStateV2::nameInputWindow()
{
    if (m_showNameInput)
    {
        const float viewScale = getViewScale();

        const auto size = glm::vec2(cro::App::getWindow().getSize());
        const glm::vec2 WindowSize = glm::vec2(200.f, 80.f) * viewScale;
        const auto WindowPos = (size - WindowSize) / 2.f;

        ImGui::SetNextWindowSize({ WindowSize.x, WindowSize.y });
        ImGui::SetNextWindowPos({ WindowPos.x, WindowPos.y });

        ImGui::GetFont()->Scale *= viewScale;
        ImGui::PushFont(ImGui::GetFont());

        ImGui::Begin("Profile Name", &m_showNameInput, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        ImGui::SetNextItemWidth(-1.f);
        if (ImGui::InputText("##input", &m_nameBuffer))
        {
            static constexpr std::size_t MaxChars = ConstVal::MaxStringChars;
            if (m_nameBuffer.length() > MaxChars)
            {
                m_nameBuffer = m_nameBuffer.substr(0, MaxChars);
            }
        }
        if (ImGui::Button("OK", { (WindowSize.x / 2.f) - 12.f, 0.f }))
        {
            m_activeProfile.playerData.name = cro::String::fromUtf8(m_nameBuffer.begin(), m_nameBuffer.end());
            applyNameString();

            m_nameBuffer.clear();
            m_showNameInput = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", { -1.f, 0.f }))
        {
            m_nameBuffer.clear();
            m_showNameInput = false;
        }
        ImGui::End();

        ImGui::GetFont()->Scale = 1.f;
        ImGui::PopFont();
    }
}

void ProfileStateV2::applyNameString()
{
    m_menuLayout.items[TabID::Details][1].labels[0] = m_activeProfile.playerData.name;
    updateMenuItems(); //redraw the new name label
}

void ProfileStateV2::playPreviewAudio()
{
    //I'll leave this here as an I told you so
    //for when you come back to fix using a static var.
    static std::size_t playCount = 0;

    static std::size_t emitterIndex = 0;
    static const std::array<std::string, 8> EmitterNames =
    {
        std::string("celebrate"),
        "slice",
        "hook",
        "green",
        "bunker",
        "fairway",
        "scrub",
        "water"
    };

    if (playCount < 4)
    {
        emitterIndex = (emitterIndex + 1) % EmitterNames.size();

        auto e = m_scene.createEntity();
        e.addComponent<cro::Transform>();
        e.addComponent<cro::AudioEmitter>() = m_voices[m_voiceIndex].audioScape.getEmitter(EmitterNames[emitterIndex]);
        e.getComponent<cro::AudioEmitter>().setPitch(1.f + (static_cast<float>(m_activeProfile.playerData.voicePitch) / VoicePitchDivisor));
        e.getComponent<cro::AudioEmitter>().play();
        e.addComponent<cro::Callback>().active = true;
        e.getComponent<cro::Callback>().function =
            [&](cro::Entity f, float)
            {
                if (f.getComponent<cro::AudioEmitter>().getState() == cro::AudioEmitter::State::Stopped)
                {
                    f.getComponent<cro::Callback>().active = false;
                    m_scene.destroyEntity(f);
                    playCount--;
                }
            };

        playCount++;
    }
}

void ProfileStateV2::updateMugshot()
{
    m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Skeleton>().stop();
    m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Skeleton>().gotoFrame(0);

    m_mugshotTexture.clear({ 0xa9c0afff });
    auto camEnt = m_previewCameras[PreviewCamera::MugShot];
    auto& cam = camEnt.getComponent<cro::Camera>();
    cam.viewport = { 0.f, 0.f, 0.5f, 1.f };
    camEnt.getComponent<cro::Transform>().setPosition(MugCameraPosition);
    camEnt.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI);
    camEnt.getComponent<cro::Camera>().updateMatrices(camEnt.getComponent<cro::Transform>());
    auto oldCam = m_previewScene.setActiveCamera(camEnt);
    m_previewScene.simulate(0.f); //updates all the camera/model matrices
    
    //hack to set the background a solid colour by pointing at the sky
    auto q = glm::rotate(cro::Transform::QUAT_IDENTITY, cro::Util::Const::PI / 2.f, cro::Transform::X_AXIS);
    m_previewScene.setSkyboxOrientation(q);
    m_previewScene.render();

    cam.viewport = { 0.5f, 0.f, 0.5f, 1.f };
    camEnt.getComponent<cro::Transform>().setPosition(MugCameraPosition + glm::vec3(-MugCameraPosition.z /*+ 0.05f*/, 0.f, -MugCameraPosition.z));
    camEnt.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI / 2.f);
    camEnt.getComponent<cro::Camera>().updateMatrices(camEnt.getComponent<cro::Transform>());
    m_previewScene.simulate(0.f);
    
    q = glm::rotate(cro::Transform::QUAT_IDENTITY, cro::Util::Const::PI / 2.f, cro::Transform::Z_AXIS);
    m_previewScene.setSkyboxOrientation(q);
    m_previewScene.render();
    m_previewScene.setSkyboxOrientation(cro::Transform::QUAT_IDENTITY);

    m_mugshotTexture.display();
    m_previewScene.setActiveCamera(oldCam);

    m_saveMugshotOnExit = true;

    const auto idx = m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Skeleton>().getAnimationIndex("idle_standing");
    m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Skeleton>().play(idx);
    m_detailsPane.mugshotImage.getComponent<cro::Sprite>().setTexture(m_mugshotTexture.getTexture());
    //m_detailsPane.mugshotImage.getComponent<cro::Transform>().setScale(glm::vec2(0.5f));
}

void ProfileStateV2::clearMugshot()
{
    const auto path = Content::getUserContentPath(Content::UserContent::Profile) + m_activeProfile.playerData.profileID + "/mug.png";
    if (cro::FileSystem::fileExists(path))
    {
        std::error_code ec;
        std::filesystem::remove(path, ec);

        if (ec)
        {
            LogE << "Unable to remove mugshot file: " << ec.message() << std::endl;
        }
    }

    m_activeProfile.playerData.mugshot.clear();
    m_profileData.playerProfiles[m_profileData.activeProfileIndex].playerData.mugshot.clear();

    m_saveMugshotOnExit = false;

    m_mugshotTexture.clear(cro::Colour::Transparent);
    m_mugshotTexture.display();

    //hide any preview sprite - this is done by the sprite callback now
    //m_detailsPane.mugshotImage.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
}

std::string ProfileStateV2::generateRandomBio() const
{
    switch (cro::Util::Random::value(0, 6))
    {
    default:
    case 0:
        return
            u8"This former house-builder knows all about the benefits of elevation. If they play a good shot, you'll certainly be able to see their big beam and they'll hit the roof if they manage a hole in one! As a low-handicapper, tends to play off the builders tee, and enjoys ladder tournaments.";
    case 1:
        return
            u8"A retired gardener this player knows a thing or two about lying in the rough. Don't underestimate them though - they could be considered the rake in the grass!";
    case 2:
        return
            u8"Small feet mean nothing - not when you can handle your wood like this.";
    case 3:
        return
            u8"Formerly a countryside resident this player moved to the city to experience the thrills of urban golf. Just don't sneak up on them when they're strumming the banjo.";
    case 4:
        return
            u8"\"Good things come in small packages\" is this player's motto. Apparently they diet exclusively on fortune cookies.";
    case 5:
        return
            u8"The clever use of turn signals got this player to where they are today.";
    case 6:
        return
            u8"Nick-named \"The Midwife\" because they always deliver (and \"Postman\" was already taken) here's a player who knows a comfy lie is much better than a water-berth. Takes a cautious approach as they know it's much better to crawl before you can walk. Becoming a golf pro was their crowning achievement";
    }
}

void ProfileStateV2::refreshBio()
{
    //look for bio file and load it if it exists
    auto path = Content::getUserContentPath(Content::UserContent::Profile);
    if (cro::FileSystem::directoryExists(path))
    {
        if (m_activeProfile.playerData.profileID.empty())
        {
            //this creates a new id
            m_activeProfile.playerData.saveProfile();
        }

        path += m_activeProfile.playerData.profileID + "/";

        if (cro::FileSystem::directoryExists(path))
        {
            path += "bio.txt";

            if (cro::FileSystem::fileExists(path))
            {
                std::vector<char> buffer(MaxBioChars + 1);

                cro::RaiiRWops inFile;
                inFile.file = SDL_RWFromFile(path.c_str(), "r");
                if (inFile.file)
                {
                    auto readCount = inFile.file->read(inFile.file, buffer.data(), 1, MaxBioChars);
                    buffer[readCount] = 0; //nullterm
                    setBioString(buffer.data());
                }
            }
            else
            {
                //else set bio to random and write file
                std::string bio = generateRandomBio();

                cro::RaiiRWops outfile;
                outfile.file = SDL_RWFromFile(path.c_str(), "w");
                if (outfile.file)
                {
                    outfile.file->write(outfile.file, bio.data(), bio.size(), 1);
                }
                setBioString(bio);
            }
        }
        else
        {
            setBioString(generateRandomBio());
        }
    }
}

void ProfileStateV2::setBioString(const std::string& str)
{
    //TODO measure space and word wrap

    cro::String s = str;
    cro::Util::String::wordWrap(s, 36);

    m_detailsPane.bioString.getComponent<cro::Text>().setString(s);

    const auto left = m_detailsPane.mugshotImage.getComponent<cro::Drawable2D>().getCroppingArea().left;
    m_detailsPane.bioString.getComponent<cro::Transform>().setPosition(glm::vec2(left/* + 8.f*/, -4.f));

    //TODO set char size? OR should we be using the UIElement here?
}

void ProfileStateV2::randomise()
{
    //randomise hair
    setHairIndex(cro::Util::Random::value(0u, m_avatarHairModels.size() - 1));
    setHatIndex(0);

    //randomise avatar
    setAvatarIndex(cro::Util::Random::value(0u, m_sharedData.avatarInfo.size() - 1));

    m_activeProfile.playerData.voiceID = m_avatarModels[m_avatarIndex].audioUID;
    m_activeProfile.playerData.voicePitch = 0;

    //randomise colours
    for (auto i = 0; i < /*PaletteID::BallThumb*/6; ++i)
    {
        m_activeProfile.playerData.avatarFlags[i] = static_cast<std::uint8_t>(cro::Util::Random::value(0u, (pc::PairCounts[i] / 2) - 1));
        m_profileTextures[m_avatarIndex].setColour(pc::ColourKey::Index(i), m_activeProfile.playerData.avatarFlags[i]);
    }

    //update texture
    m_profileTextures[m_avatarIndex].apply();

    //update hair
    if (m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].isValid())
    {
        m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", pc::Palette[m_activeProfile.playerData.avatarFlags[0]]);
        m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].getComponent<cro::Model>().setMaterialProperty(1, "u_hairColour", pc::Palette[m_activeProfile.playerData.avatarFlags[0]]);
    }

    createBodyItems();
    createHeadwearItems();
    createEquipmentItems();
    createDetailItems();

    activateTab(m_tabBar.activeIndex);
}

void ProfileStateV2::updatePalettePreview(std::int32_t paletteID, std::int32_t selectedIdx)
{
    constexpr float PreviewSize = 16.f;
    constexpr float BorderSize = 1.f;

    std::vector<cro::Vertex2D> verts;

    if (paletteID > -1 && selectedIdx > -1)
    {
        const auto rows = std::min(pc::PairCounts[paletteID] / 2, pc::PairCounts[paletteID] / 4);
        const auto cols = pc::PairCounts[paletteID] / rows;

        const cro::Colour bg = cro::Colour(0.f, 0.f, 0.f, BackgroundAlpha);
        const float top = (rows + 1) * PreviewSize;
        constexpr float bottom = PreviewSize;
        const float width = cols * PreviewSize;
        verts.emplace_back(glm::vec2(0.f, top), bg);
        verts.emplace_back(glm::vec2(0.f, bottom), bg);
        verts.emplace_back(glm::vec2(width, top), bg);

        verts.emplace_back(glm::vec2(width, top), bg);
        verts.emplace_back(glm::vec2(0.f, bottom), bg);
        verts.emplace_back(glm::vec2(width, bottom), bg);

        for (auto y = 0u; y < rows; ++y)
        {
            for (auto x = 0u; x < cols; ++x)
            {
                const auto i = y * cols + x;
                const glm::vec2 pos(x * PreviewSize, (rows * PreviewSize) - (y * PreviewSize));

                if (i == selectedIdx)
                {
                    //draw background tris first
                    verts.emplace_back(pos + glm::vec2(0.f, PreviewSize), CD32::Colours[CD32::Yellow]);
                    verts.emplace_back(pos, CD32::Colours[CD32::Yellow]);
                    verts.emplace_back(pos + glm::vec2(PreviewSize, PreviewSize), CD32::Colours[CD32::Yellow]);

                    verts.emplace_back(pos + glm::vec2(PreviewSize, PreviewSize ), CD32::Colours[CD32::Yellow]);
                    verts.emplace_back(pos, CD32::Colours[CD32::Yellow]);
                    verts.emplace_back(pos + glm::vec2(PreviewSize, 0.f), CD32::Colours[CD32::Yellow]);
                }

                verts.emplace_back(pos + glm::vec2(BorderSize, PreviewSize - BorderSize), pc::Palette[i]);
                verts.emplace_back(pos + glm::vec2(BorderSize, BorderSize), pc::Palette[i]);
                verts.emplace_back(pos + glm::vec2((PreviewSize - BorderSize), PreviewSize - BorderSize), pc::Palette[i]);

                verts.emplace_back(pos + glm::vec2((PreviewSize - BorderSize), PreviewSize - BorderSize), pc::Palette[i]);
                verts.emplace_back(pos + glm::vec2(BorderSize, BorderSize), pc::Palette[i]);
                verts.emplace_back(pos + glm::vec2((PreviewSize - BorderSize), BorderSize), pc::Palette[i]);
            }
        }
        const float scale = cro::UIElementSystem::getViewScale();
        m_palettePreview.setPosition(glm::vec2(0.f, m_previewTexture.getSize().y - (((rows + 1) * PreviewSize) * scale)));
        m_palettePreview.setScale(glm::vec2(scale));
    }
    m_palettePreview.setVertexData(verts);
}