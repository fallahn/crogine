/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
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

#include "ProfileState.hpp"
#include "SharedStateData.hpp"
#include "CommandIDs.hpp"
#include "GameConsts.hpp"
#include "TextAnimCallback.hpp"
#include "MessageIDs.hpp"
#include "BallSystem.hpp"
#include "SharedProfileData.hpp"
#include "CallbackData.hpp"
#include "PlayerColours.hpp"
#include "ButtonHoldSystem.hpp"
#include "ProfileEnum.inl"
#include "../GolfGame.hpp"
#include "../Colordome-32.hpp"

#include <crogine/core/Window.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/core/Mouse.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>

#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/SpriteAnimator.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/ParticleSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>

#include <crogine/util/Easings.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/gui/Gui.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

#include "../ErrorCheck.hpp"

#include <sstream>

namespace
{
    struct MenuID final
    {
        enum
        {
            Dummy, Main,

            //these are the colour palettes
            Hair, Skin, TopL, TopD,
            BottomL, BottomD,

            //BallThumb not used now...
            BallThumb, BallColour,

            //browser windows
            BallSelect, HairSelect,
            
            HairEditor
        };
    };
    
    constexpr std::uint32_t ThumbTextureScale = 4;

    //UI selection indices
    constexpr std::size_t PrevArrow = 2000;
    constexpr std::size_t NextArrow = 2001;
    constexpr std::size_t CloseButton = 2002;

    constexpr glm::uvec2 BallTexSize(96u, 110u);
    constexpr glm::uvec2 AvatarTexSize(130u, 202u);
    constexpr glm::uvec2 MugshotTexSize(192u, 96u);
    constexpr glm::uvec2 HairEditTexSize(200u, 200u);

    constexpr std::size_t ThumbColCount = 8;
    constexpr std::size_t ThumbRowCount = 4;
    constexpr glm::uvec2 BallThumbSize = BallTexSize / 2u;

    constexpr glm::vec3 CameraBasePosition({ -0.867f, 1.325f, -1.68f });
    constexpr glm::vec3 CameraZoomPosition({ -0.867f, 1.625f, -0.58f });
    const glm::vec3 CameraZoomVector = glm::normalize(CameraZoomPosition - CameraBasePosition);
    constexpr glm::vec3 MugCameraPosition({ -0.843f, 1.61f, -0.35f });

    const cro::String XboxString("LB/LT - RB/RT Rotate/Zoom");
    const cro::String PSString("L1/L2 - R1/R2 Rotate/Zoom");

    static constexpr std::size_t MaxBioChars = 512;
    constexpr cro::FloatRect BioCrop =
    {
        0.f, -104.f, 90.f, 104.f
    };

    constexpr std::size_t PaletteColumnCount = 8;
    constexpr std::array SwatchPositions =
    {
        glm::vec2(392.f, 130.f),
        glm::vec2(37.f, 139.f),
        glm::vec2(21.f, 107.f),
        glm::vec2(53.f, 107.f),
        glm::vec2(21.f, 74.f),
        glm::vec2(53.f, 74.f)
    };

    namespace Flyout
    {
        //8x5
        static constexpr glm::vec2 IconSize(8.f);
        static constexpr float IconPadding = 4.f;

        static constexpr float BgWidth = (PaletteColumnCount * (IconSize.x + IconPadding)) + IconPadding;
    }
}

ProfileState::ProfileState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd, SharedProfileData& sp)
    : cro::State        (ss, ctx),
    m_uiScene           (ctx.appInstance.getMessageBus(), 1024),
    m_modelScene        (ctx.appInstance.getMessageBus(), 1024), //just because someone might be daft enough to install ALL the workshop items
    m_sharedData        (sd),
    m_profileData       (sp),
    m_resources         (*sd.activeResources),
    m_viewScale         (2.f),
    m_ballIndex         (0),
    m_ballHairIndex     (0),
    m_avatarIndex       (0),
    m_lockedAvatarCount (0),
    m_lastSelected      (0),
    m_avatarRotation    (0.f),
    m_headwearID        (HeadwearID::Hair),
    m_mugshotUpdated    (false)
{
    ctx.mainWindow.setMouseCaptured(false);

    m_activeProfile = sp.playerProfiles[sp.activeProfileIndex];

    addSystems();
    loadResources();
    buildPreviewScene();
    buildScene();

    m_modelScene.simulate(0.f);
    m_uiScene.simulate(0.f);

    //registerWindow([&]()
    //    {
    //        if (ImGui::Begin("Flaps"))
    //        {
    //            /*auto r = m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Transform>().getRotation();
    //            ImGui::Text("X: %3.2f Y: %3.2f W: %3.2f", r.x, r.y, r.w);
    //            ImGui::Text("AV Rot: %3.2f", m_avatarRotation);*/

    //            auto pos = m_uiScene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition());
    //            ImGui::Text("Mouse Pos: %3.2f, %3.2f", pos.x, pos.y);

    //            bool contains = false;

    //            for (const auto& box : m_transformBoxes)
    //            {
    //                const auto b = cro::Text::getLocalBounds(box.entity).transform(box.entity.getComponent<cro::Transform>().getWorldTransform());
    //                ImGui::Text("Box: %3.3f, %3.3f, %3.3f, %3.3f", b.left, b.bottom, b.width, b.height);

    //                if (b.contains(pos))
    //                {
    //                    contains = true;
    //                }
    //            }

    //            if (contains)
    //            {
    //                ImGui::Text("Contains true");
    //            }
    //            else
    //            {
    //                ImGui::Text("Contains false");
    //            }
    //        }
    //        ImGui::End();
    //    });
}

//public
bool ProfileState::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse
        || m_menuEntities[EntityID::Root].getComponent<cro::Callback>().active)
    {
        return false;
    }

    const auto updateHelpString = [&](std::int32_t controllerID)
    {
        if (controllerID > -1)
        {
            if (hasPSLayout(controllerID))
            {
                m_menuEntities[EntityID::HelpText].getComponent<cro::Text>().setString(PSString);
                m_menuEntities[EntityID::HairHelp].getComponent<cro::Text>().setString("L1/R1 Rotate");
            }
            else
            {
                m_menuEntities[EntityID::HelpText].getComponent<cro::Text>().setString(XboxString);
                m_menuEntities[EntityID::HairHelp].getComponent<cro::Text>().setString("LB/RB Rotate");
            }
        }
        else
        {
            cro::String str(cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Left]));
            str += ", ";
            str += cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Right]);
            str += ", ";
            str += cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Up]);
            str += ", ";
            str += cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Down]);
            str += " Rotate/Zoom";
            m_menuEntities[EntityID::HelpText].getComponent<cro::Text>().setString(str);


            str = cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Left]);
            str += ", ";
            str += cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Right]);
            str += " Rotate";
            m_menuEntities[EntityID::HairHelp].getComponent<cro::Text>().setString(str);
        }
        centreText(m_menuEntities[EntityID::HelpText]);
        centreText(m_menuEntities[EntityID::HairHelp]);
    };

    const auto quitMenu = [&]()
        {
            const auto groupID = m_uiScene.getSystem<cro::UISystem>()->getActiveGroup();

            if (groupID == MenuID::Main)
            {
                quitState();
            }
            else if (groupID == MenuID::BallSelect)
            {
                m_menuEntities[EntityID::BallBrowser].getComponent<cro::Callback>().active = true;
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
            else if (groupID == MenuID::HairSelect)
            {
                m_menuEntities[EntityID::HairBrowser].getComponent<cro::Callback>().active = true;
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
            else if (groupID == MenuID::HairEditor)
            {
                m_menuEntities[EntityID::HairEditor].getComponent<cro::Callback>().active = true;
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
        };

    const auto closeFlyout = [&](std::size_t flyoutID)
        {
            m_flyouts[flyoutID].background.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(flyoutID == PaletteID::Hair ? MenuID::HairEditor : MenuID::Main);
            m_uiScene.getSystem<cro::UISystem>()->setColumnCount(1);
            m_uiScene.getSystem<cro::UISystem>()->selectAt(m_lastSelected);

            //restore model preview colours
            m_profileTextures[m_avatarIndex].setColour(pc::ColourKey::Index(flyoutID), m_activeProfile.avatarFlags[flyoutID]);
            m_profileTextures[m_avatarIndex].apply();

            //refresh hair colour
            if (m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].isValid())
            {
                m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", pc::Palette[m_activeProfile.avatarFlags[pc::ColourKey::Hair]]);
                m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].getComponent<cro::Model>().setMaterialProperty(1, "u_hairColour", pc::Palette[m_activeProfile.avatarFlags[pc::ColourKey::Hair]]);
            }

            if (m_avatarHairModels[m_avatarModels[m_avatarIndex].hatIndex].isValid())
            {
                m_avatarHairModels[m_avatarModels[m_avatarIndex].hatIndex].getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", pc::Palette[m_activeProfile.avatarFlags[pc::ColourKey::Hat]]);
                m_avatarHairModels[m_avatarModels[m_avatarIndex].hatIndex].getComponent<cro::Model>().setMaterialProperty(1, "u_hairColour", pc::Palette[m_activeProfile.avatarFlags[pc::ColourKey::Hat]]);
            }        
        };

    if (evt.type == SDL_KEYUP)
    {
        handleTextEdit(evt);
        switch (evt.key.keysym.sym)
        {
        default:
            updateHelpString(-1);
            break;
        case SDLK_ESCAPE:
        case SDLK_BACKSPACE:
        if (m_textEdit.string == nullptr)
        {
            auto currentMenu = m_uiScene.getSystem<cro::UISystem>()->getActiveGroup();
            switch (currentMenu)
            {
            default:
                quitMenu();
                return false;
            case MenuID::Hair:
            case MenuID::BottomD:
            case MenuID::BottomL:
            case MenuID::Skin:
            case MenuID::TopL:
            case MenuID::TopD:
            {
                auto flyoutID = currentMenu - MenuID::Hair;
                closeFlyout(flyoutID);
                return false;
            }
            }
        }
            break;
        case SDLK_RETURN:
        case SDLK_RETURN2:
        case SDLK_KP_ENTER:
            if (m_textEdit.string)
            {
                applyTextEdit();
            }
            break;
        /*case SDLK_p:
            renderBallFrames();
            break;*/
        //case SDLK_k:
        //    m_menuEntities[EntityID::BioText].getComponent<cro::Callback>().getUserData<std::int32_t>()++;
        //    m_menuEntities[EntityID::BioText].getComponent<cro::Callback>().active = true;
        //    break;
        //case SDLK_l:
        //    m_menuEntities[EntityID::BioText].getComponent<cro::Callback>().getUserData<std::int32_t>()--;
        //    m_menuEntities[EntityID::BioText].getComponent<cro::Callback>().active = true;
        //    break;
        }
    }
    else if (evt.type == SDL_KEYDOWN)
    {
        handleTextEdit(evt);
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_UP:
        case SDLK_DOWN:
        case SDLK_LEFT:
        case SDLK_RIGHT:
            cro::App::getWindow().setMouseCaptured(true);
            break;
        }
    }
    else if (evt.type == SDL_TEXTINPUT)
    {
        handleTextEdit(evt);
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        cro::App::getWindow().setMouseCaptured(true);

        if (m_textEdit.string == nullptr)
        {
            switch (evt.cbutton.button)
            {
            default: break;
            case cro::GameController::ButtonRightShoulder:
            {
                const auto group = m_uiScene.getSystem<cro::UISystem>()->getActiveGroup();
                if (group == MenuID::BallSelect)
                {
                    nextPage(PaginationID::Balls);
                }
                else if (group == MenuID::HairSelect)
                {
                    nextPage(PaginationID::Hair);
                }
            }
                break;
            case cro::GameController::ButtonLeftShoulder:
            {
                const auto group = m_uiScene.getSystem<cro::UISystem>()->getActiveGroup();
                if (group == MenuID::BallSelect)
                {
                    prevPage(PaginationID::Balls);
                }
                else if (group == MenuID::HairSelect)
                {
                    prevPage(PaginationID::Hair);
                }
            }
                break;
            case cro::GameController::ButtonB:
            {
                auto currentMenu = m_uiScene.getSystem<cro::UISystem>()->getActiveGroup();
                switch (currentMenu)
                {
                default:
                    quitMenu();
                    return false;
                case MenuID::Hair:
                case MenuID::BottomD:
                case MenuID::BottomL:
                case MenuID::Skin:
                case MenuID::TopL:
                case MenuID::TopD:
                {
                    auto flyoutID = currentMenu - MenuID::Hair;
                    closeFlyout(flyoutID);
                    return false;
                }
                break;
                }
            }
                return false;
            }
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONUP)
    {
        auto currentMenu = m_uiScene.getSystem<cro::UISystem>()->getActiveGroup();

        switch (currentMenu)
        {
        default:
        {
            updateHelpString(-1);
            if (evt.button.button == SDL_BUTTON_RIGHT)
            {
                quitState();
                return false;
            }
            else if (evt.button.button == SDL_BUTTON_LEFT)
            {
                if (applyTextEdit())
                {
                    //we applied a text edit so don't update the
                    //UISystem
                    return false;
                }
            }
        }
            break;
        case MenuID::BallSelect:
            if (evt.button.button == SDL_BUTTON_RIGHT)
            {
                m_menuEntities[EntityID::BallBrowser].getComponent<cro::Callback>().active = true;
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
            break;
        case MenuID::HairSelect:
            if (evt.button.button == SDL_BUTTON_RIGHT)
            {
                m_menuEntities[EntityID::HairBrowser].getComponent<cro::Callback>().active = true;
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
            break;
        case MenuID::HairEditor:
            if (evt.button.button == SDL_BUTTON_RIGHT)
            {
                m_menuEntities[EntityID::HairEditor].getComponent<cro::Callback>().active = true;
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
            break;
        case MenuID::BallColour:
        {
            auto bounds = m_ballColourFlyout.background.getComponent<cro::Drawable2D>().getLocalBounds();
            bounds = m_ballColourFlyout.background.getComponent<cro::Transform>().getWorldTransform() * bounds;

            if (!bounds.contains(m_uiScene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition())))
            {
                m_ballColourFlyout.background.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Main);
                m_uiScene.getSystem<cro::UISystem>()->setColumnCount(1);
                m_uiScene.getSystem<cro::UISystem>()->selectAt(m_lastSelected);

                m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setMaterialProperty(0, "u_ballColour", m_activeProfile.ballColour);
                return false;
            }
        }
            break;
        case MenuID::BallThumb:
        {
            auto bounds = m_flyouts[PaletteID::BallThumb].background.getComponent<cro::Drawable2D>().getLocalBounds();
            bounds = m_flyouts[PaletteID::BallThumb].background.getComponent<cro::Transform>().getWorldTransform() * bounds;

            if (!bounds.contains(m_uiScene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition())))
            {
                m_flyouts[PaletteID::BallThumb].background.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Main);
                m_uiScene.getSystem<cro::UISystem>()->setColumnCount(1);
                m_uiScene.getSystem<cro::UISystem>()->selectAt(m_lastSelected);
                return false;
            }
        }
        break;
        case MenuID::Hair:
        case MenuID::BottomD:
        case MenuID::BottomL:
        case MenuID::Skin:
        case MenuID::TopL:
        case MenuID::TopD:
        {
            auto flyoutID = currentMenu - MenuID::Hair;
            auto bounds = m_flyouts[flyoutID].background.getComponent<cro::Drawable2D>().getLocalBounds();
            bounds = m_flyouts[flyoutID].background.getComponent<cro::Transform>().getWorldTransform() * bounds;

            if (!bounds.contains(m_uiScene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition())))
            {
                closeFlyout(flyoutID);

                //don't forward this to the menu system
                return false;
            }
        }
            break;
        }        
    }
    else if (evt.type == SDL_CONTROLLERAXISMOTION)
    {
        if (evt.caxis.value > LeftThumbDeadZone)
        {
            cro::App::getWindow().setMouseCaptured(true);

            updateHelpString(cro::GameController::controllerID(evt.caxis.which));
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONDOWN)
    {
        updateHelpString(cro::GameController::controllerID(evt.cbutton.which));
    }
    else if (evt.type == SDL_MOUSEMOTION)
    {
        cro::App::getWindow().setMouseCaptured(false);
        updateHelpString(-1);

        auto mousePos = m_uiScene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords({ evt.motion.x, evt.motion.y });
        auto bounds = m_menuEntities[EntityID::AvatarPreview].getComponent<cro::Sprite>().getTextureBounds();
        bounds = m_menuEntities[EntityID::AvatarPreview].getComponent<cro::Transform>().getWorldTransform() * bounds;

        if (bounds.contains(mousePos)
            && cro::Mouse::isButtonPressed(cro::Mouse::Button::Left))
        {
            m_avatarRotation += static_cast<float>(evt.motion.xrel) * (1.f / 60.f);
            m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, m_avatarRotation);
            updateGizmo();
        }
    }
    else if (evt.type == SDL_MOUSEWHEEL)
    {
        const auto group = m_uiScene.getSystem<cro::UISystem>()->getActiveGroup();
        if (group == MenuID::BallSelect)
        {
            if (evt.wheel.y > 0)
            {
                prevPage(PaginationID::Balls);
            }
            else if(evt.wheel.y < 0)
            {
                nextPage(PaginationID::Balls);
            }
        }
        else if (group == MenuID::HairSelect)
        {
            if (evt.wheel.y > 0)
            {
                prevPage(PaginationID::Hair);
            }
            else if (evt.wheel.y < 0)
            {
                nextPage(PaginationID::Hair);
            }
        }
        else if (group == MenuID::HairEditor)
        {
            auto point = m_uiScene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords({ evt.wheel.mouseX, evt.wheel.mouseY });
            for (const auto& box : m_transformBoxes)
            {
                const auto worldBounds = cro::Text::getLocalBounds(box.entity).transform(box.entity.getComponent<cro::Transform>().getWorldTransform());

                if (worldBounds.contains(point))
                {
                    auto idx = (PlayerData::HeadwearOffset::HairTx + box.index) + (PlayerData::HeadwearOffset::HatTx * m_headwearID);

                    float change = static_cast<float>(evt.wheel.y) * box.step;
                    if (cro::Keyboard::isKeyPressed(SDLK_LSHIFT))
                    {
                        change *= 10.f;
                    }
                    m_activeProfile.headwearOffsets[idx][box.offset] = 
                        std::clamp(m_activeProfile.headwearOffsets[idx][box.offset] + change, box.minVal, box.maxVal);

                    updateHeadwearTransform();
                    break;
                }
            }
        }        
        else
        {
            auto mousePos = m_uiScene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition());
            auto bounds = m_menuEntities[EntityID::AvatarPreview].getComponent<cro::Sprite>().getTextureBounds();
            bounds = m_menuEntities[EntityID::AvatarPreview].getComponent<cro::Transform>().getWorldTransform() * bounds;

            if (bounds.contains(mousePos))
            {
                float zoom = evt.wheel.preciseY * (1.f / 20.f);
                auto pos = m_cameras[CameraID::Avatar].getComponent<cro::Transform>().getPosition();
                if (glm::dot(CameraZoomPosition - pos, CameraZoomVector) > zoom
                    && glm::dot(CameraBasePosition - pos, CameraZoomVector) < zoom)
                {
                    pos += CameraZoomVector * zoom;
                    m_cameras[CameraID::Avatar].getComponent<cro::Transform>().setPosition(pos);
                }
            }
            else
            {
                //check bio for scroll
                bounds = cro::Text::getLocalBounds(m_menuEntities[EntityID::BioText]);
                bounds = m_menuEntities[EntityID::BioText].getComponent<cro::Transform>().getWorldTransform() * bounds;

                if (bounds.contains(mousePos))
                {
                    m_menuEntities[EntityID::BioText].getComponent<cro::Callback>().getUserData<std::int32_t>() -= evt.wheel.y;
                    m_menuEntities[EntityID::BioText].getComponent<cro::Callback>().active = true;
                }
            }
        }
    }

    m_uiScene.getSystem<cro::UISystem>()->handleEvent(evt);
    m_uiScene.forwardEvent(evt);
    m_modelScene.forwardEvent(evt);
    return false;
}

void ProfileState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::StateMessage)
    {
        const auto& data = msg.getData<cro::Message::StateEvent>();
        if (data.action == cro::Message::StateEvent::Popped)
        {
            switch (data.id)
            {
            default: break;
            case StateID::Keyboard:
                applyTextEdit();
                break;
            }
        }
    }

    m_uiScene.forwardMessage(msg);
    m_modelScene.forwardMessage(msg);
}

bool ProfileState::simulate(float dt)
{
    //rotate/zoom avatar
    if (!m_textEdit.entity.isValid())
    {
        bool refresh = false;
        if (cro::GameController::isButtonPressed(0, cro::GameController::ButtonLeftShoulder)
            || cro::Keyboard::isKeyPressed(m_sharedData.inputBinding.keys[InputBinding::Left]))
        {
            m_avatarRotation -= dt;
            refresh = true;
        }
        if (cro::GameController::isButtonPressed(0, cro::GameController::ButtonRightShoulder)
            || cro::Keyboard::isKeyPressed(m_sharedData.inputBinding.keys[InputBinding::Right]))
        {
            refresh = true;
            m_avatarRotation += dt;
        }

        if (refresh)
        {
            m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, m_avatarRotation);
            updateGizmo();
        }

        float zoom = 0.f;
        if (cro::GameController::getAxisPosition(0, cro::GameController::TriggerLeft) > TriggerDeadZone
            || cro::Keyboard::isKeyPressed(m_sharedData.inputBinding.keys[InputBinding::Down]))
        {
            zoom -= dt;
        }
        if (cro::GameController::getAxisPosition(0, cro::GameController::TriggerRight) > TriggerDeadZone
            || cro::Keyboard::isKeyPressed(m_sharedData.inputBinding.keys[InputBinding::Up]))
        {
            zoom += dt;
        }
        if (zoom != 0)
        {
            auto pos = m_cameras[CameraID::Avatar].getComponent<cro::Transform>().getPosition();
            if (glm::dot(CameraZoomPosition - pos, CameraZoomVector) > zoom
                && glm::dot(CameraBasePosition - pos, CameraZoomVector) < zoom)
            {
                pos += CameraZoomVector * zoom;
                m_cameras[CameraID::Avatar].getComponent<cro::Transform>().setPosition(pos);
            }
        }
    }

    m_modelScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void ProfileState::render()
{
    if (m_uiScene.getSystem<cro::UISystem>()->getActiveGroup() == MenuID::HairEditor)
    {
        m_hairEditorTexture.clear(cro::Colour::Transparent);
        m_modelScene.setActiveCamera(m_cameras[CameraID::HairEdit]);
        m_modelScene.render();
        m_hairEditorTexture.display();
    }
    else
    {
        m_ballTexture.clear(cro::Colour::Transparent);
        m_modelScene.setActiveCamera(m_cameras[CameraID::Ball]);
        m_modelScene.render();
        m_ballTexture.display();

        m_avatarTexture.clear(cro::Colour::Transparent);
        m_modelScene.setActiveCamera(m_cameras[CameraID::Avatar]);
        m_modelScene.render();
        m_avatarTexture.display();
    }
    m_uiScene.render();

}

//private
void ProfileState::addSystems()
{
    //registerWindow([&]() 
    //    {
    //        ImGui::Begin("Camera");
    //        auto pos = m_cameras[CameraID::Ball].getComponent<cro::Transform>().getWorldPosition();
    //        ImGui::Text("Pos: %3.2f, %3.2f, %3.2f", pos.x, pos.y, pos.z);

    //        if (ImGui::SliderFloat("Y", &pos.y, 0.01f, 0.1f))
    //        {
    //            m_cameras[CameraID::Ball].getComponent<cro::Transform>().setPosition(pos);
    //        }

    //        if (ImGui::SliderFloat("Z", &pos.z, 0.01f, 0.1f))
    //        {
    //            m_cameras[CameraID::Ball].getComponent<cro::Transform>().setPosition(pos);
    //        }

    //        ImGui::End();
    //    });


    auto& mb = getContext().appInstance.getMessageBus();
    m_uiScene.addSystem<cro::UISystem>(mb)->setMouseScrollNavigationEnabled(false);
    m_uiScene.addSystem<ButtonHoldSystem>(mb);
    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::SpriteAnimator>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
    m_uiScene.addSystem<cro::AudioPlayerSystem>(mb);

    m_uiScene.setSystemActive<cro::AudioPlayerSystem>(false);

    m_modelScene.addSystem<cro::CallbackSystem>(mb);
    m_modelScene.addSystem<cro::SkeletalAnimator>(mb);
    m_modelScene.addSystem<cro::CameraSystem>(mb);
    m_modelScene.addSystem<cro::ModelRenderer>(mb);
    m_modelScene.addSystem<cro::ParticleSystem>(mb);

    //m_uiScene.getSystem<cro::UISystem>()->initDebug("Profile UI");
}

void ProfileState::loadResources()
{
    //button audio
    m_menuSounds.loadFromFile("assets/golf/sound/menu.xas", m_resources.audio);
    m_audioEnts[AudioID::Accept] = m_uiScene.createEntity();
    m_audioEnts[AudioID::Accept].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("accept");
    m_audioEnts[AudioID::Back] = m_uiScene.createEntity();
    m_audioEnts[AudioID::Back].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("back");
    m_audioEnts[AudioID::Snap] = m_uiScene.createEntity();
    m_audioEnts[AudioID::Snap].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("snapshot");

    m_audioEnts[AudioID::Select] = m_uiScene.createEntity();
    m_audioEnts[AudioID::Select].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");

    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();


    m_mugshotTexture.create(MugshotTexSize.x, MugshotTexSize.y);
}

void ProfileState::buildScene()
{
    struct RootCallbackData final
    {
        enum
        {
            FadeIn, FadeOut
        }state = FadeIn;
        float currTime = 0.f;
    };

    auto rootNode = m_uiScene.createEntity();
    rootNode.addComponent<cro::Transform>();
    rootNode.addComponent<cro::CommandTarget>().ID = CommandID::Menu::RootNode;
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
            e.getComponent<cro::Transform>().setScale(m_viewScale * cro::Util::Easing::easeOutQuint(currTime));
            if (currTime == 1)
            {
                state = RootCallbackData::FadeOut;
                e.getComponent<cro::Callback>().active = false;

                m_uiScene.setSystemActive<cro::AudioPlayerSystem>(true);
                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Main);

                //assume we launched from a cached state and update
                //local profile data if necessary
                //if (m_activeProfile.profileID != m_profileData.playerProfiles[m_profileData.activeProfileIndex].profileID) //this returns wrong result if the profile has the same id but needs refreshing, eg player name was changed
                if (m_activeProfile.name != m_profileData.playerProfiles[m_profileData.activeProfileIndex].name)
                {
                    m_activeProfile = m_profileData.playerProfiles[m_profileData.activeProfileIndex];

                    //refresh the avatar settings
                    setAvatarIndex(indexFromAvatarID(m_activeProfile.skinID));
                    setHairIndex(indexFromHairID(m_activeProfile.hairID));
                    setHatIndex(indexFromHairID(m_activeProfile.hatID));
                    setBallIndex(indexFromBallID(m_activeProfile.ballID) % m_ballModels.size());
                    refreshMugshot();
                    refreshSwatch();
                }
                m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setMaterialProperty(0, "u_ballColour", m_activeProfile.ballColour);

                refreshNameString();
                m_previousName = m_activeProfile.name;

                refreshBio();
            }
            break;
        case RootCallbackData::FadeOut:
            currTime = std::max(0.f, currTime - (dt * 2.f));
            e.getComponent<cro::Transform>().setScale(m_viewScale * cro::Util::Easing::easeOutQuint(currTime));
            if (currTime == 0)
            {
                requestStackPop();

                state = RootCallbackData::FadeIn;
                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
                m_uiScene.setSystemActive<cro::AudioPlayerSystem>(false);
            }
            break;
        }

    };

    m_menuEntities[EntityID::Root] = rootNode;


    //quad to darken the screen
    auto entity = m_uiScene.createEntity();
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
        scale = std::min(1.f, scale / m_viewScale.x);

        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        for (auto& v : verts)
        {
            v.colour.setAlpha(BackgroundAlpha * scale);
        }
    };

    //prevents input when text input is active.
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Dummy);


    //background
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/avatar_edit.spt", m_resources.textures);

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 10.f, -0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    rootNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());

    auto bgEnt = entity;

    const auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);

    //active profile name
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 382.f, 226.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString(m_activeProfile.name);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.addComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        if (m_textEdit.string != nullptr)
        {
            auto str = *m_textEdit.string;
            if (str.size() == 0)
            {
                str += "_";
            }
            e.getComponent<cro::Text>().setString(str);

            centreText(e);
            /*bounds = cro::Text::getLocalBounds(e);
            bounds.left = (bounds.width - NameWidth) / 2.f;
            bounds.width = NameWidth;
            e.getComponent<cro::Drawable2D>().setCroppingArea(bounds);*/
        }
    };
    centreText(entity);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_menuEntities[EntityID::NameText] = entity;

    auto& uiSystem = *m_uiScene.getSystem<cro::UISystem>();
    auto selected = uiSystem.addCallback([&](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::Callback>().active = true;
            m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

            const auto& str = e.getLabel();
            if (str.empty())
            {
                m_menuEntities[EntityID::TipText].getComponent<cro::Text>().setString(" ");
            }
            else
            {
                m_menuEntities[EntityID::TipText].getComponent<cro::Text>().setString(str);
            }
        });
    auto unselected = uiSystem.addCallback([&](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent); 
            m_menuEntities[EntityID::TipText].getComponent<cro::Text>().setString(" ");
        });

    const auto createButton = [&](const std::string& spriteID, glm::vec2 position, std::int32_t selectionIndex)
    {
        auto bounds = spriteSheet.getSprite(spriteID).getTextureBounds();

        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(position, 0.4f));
        entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
        entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite(spriteID);
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        entity.addComponent<cro::Callback>().function = MenuTextCallback();
        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);
        entity.getComponent<cro::UIInput>().setSelectionIndex(selectionIndex);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selected;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselected;
        bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        return entity;
    };

#ifdef USE_GNS
    //add workshop button
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 17.f, 202.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("workshop_button");
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createButton("workshop_highlight", { 15.f, 200.f }, ButtonWorkshop);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt) 
            {
                if (activated(evt))
                {
                    Social::showWorkshop();
                }
            });

    entity.getComponent<cro::UIInput>().setNextIndex(ButtonHairBrowse, ButtonHairBrowse);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonName, ButtonRandomise);
    entity.setLabel("Open Steam Workshop In Overlay");
#endif

    //colour swatch - this has its own update function
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.2f });
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_menuEntities[EntityID::Swatch] = entity;
    refreshSwatch();

    //colour buttons
    auto skinColour = createButton("colour_highlight", glm::vec2(33.f, 135.f), ButtonSkinTone);
    skinColour.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    m_flyouts[PaletteID::Skin].background.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Skin);
                    m_uiScene.getSystem<cro::UISystem>()->setColumnCount(PaletteColumnCount);
                    m_uiScene.getSystem<cro::UISystem>()->selectAt(fetchUIIndexFromColour(m_activeProfile.avatarFlags[1], pc::ColourKey::Skin));

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    m_lastSelected = e.getComponent<cro::UIInput>().getSelectionIndex();
                }
            });
    skinColour.getComponent<cro::UIInput>().setNextIndex(ButtonPrevBody, ButtonTopLight);
    skinColour.getComponent<cro::UIInput>().setPrevIndex(ButtonNextBall, ButtonHairBrowse);


    auto topLightColour = createButton("colour_highlight", glm::vec2(17.f, 103.f), ButtonTopLight);
    topLightColour.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    m_flyouts[PaletteID::TopL].background.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::TopL);
                    m_uiScene.getSystem<cro::UISystem>()->setColumnCount(PaletteColumnCount);
                    m_uiScene.getSystem<cro::UISystem>()->selectAt(fetchUIIndexFromColour(m_activeProfile.avatarFlags[2], pc::ColourKey::TopLight));

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    m_lastSelected = e.getComponent<cro::UIInput>().getSelectionIndex();
                }
            });
    topLightColour.getComponent<cro::UIInput>().setNextIndex(ButtonTopDark, ButtonBottomLight);
    topLightColour.getComponent<cro::UIInput>().setPrevIndex(ButtonNextBall, ButtonSkinTone);


    auto topDarkColour = createButton("colour_highlight", glm::vec2(49.f, 103.f), ButtonTopDark);
    topDarkColour.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    m_flyouts[PaletteID::TopD].background.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::TopD);
                    m_uiScene.getSystem<cro::UISystem>()->setColumnCount(PaletteColumnCount);
                    m_uiScene.getSystem<cro::UISystem>()->selectAt(fetchUIIndexFromColour(m_activeProfile.avatarFlags[3], pc::ColourKey::TopDark));

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    m_lastSelected = e.getComponent<cro::UIInput>().getSelectionIndex();
                }
            });
    topDarkColour.getComponent<cro::UIInput>().setNextIndex(ButtonPrevBody, ButtonBottomDark);
    topDarkColour.getComponent<cro::UIInput>().setPrevIndex(ButtonTopLight, ButtonSkinTone);


    auto bottomLightColour = createButton("colour_highlight", glm::vec2(17.f, 70.f), ButtonBottomLight);
    bottomLightColour.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    m_flyouts[PaletteID::BotL].background.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::BottomL);
                    m_uiScene.getSystem<cro::UISystem>()->setColumnCount(PaletteColumnCount);
                    m_uiScene.getSystem<cro::UISystem>()->selectAt(fetchUIIndexFromColour(m_activeProfile.avatarFlags[4], pc::ColourKey::BottomLight));

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    m_lastSelected = e.getComponent<cro::UIInput>().getSelectionIndex();
                }
            });
    bottomLightColour.getComponent<cro::UIInput>().setNextIndex(ButtonBottomDark, ButtonSouthPaw);
    bottomLightColour.getComponent<cro::UIInput>().setPrevIndex(ButtonDescDown, ButtonTopLight);

    auto bottomDarkColour = createButton("colour_highlight", glm::vec2(49.f, 70.f), ButtonBottomDark);
    bottomDarkColour.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    m_flyouts[PaletteID::BotD].background.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::BottomD);
                    m_uiScene.getSystem<cro::UISystem>()->setColumnCount(PaletteColumnCount);
                    m_uiScene.getSystem<cro::UISystem>()->selectAt(fetchUIIndexFromColour(m_activeProfile.avatarFlags[5], pc::ColourKey::BottomDark));

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    m_lastSelected = e.getComponent<cro::UIInput>().getSelectionIndex();
                }
            });
    bottomDarkColour.getComponent<cro::UIInput>().setNextIndex(ButtonDescDown, ButtonRandomise);
    bottomDarkColour.getComponent<cro::UIInput>().setPrevIndex(ButtonBottomLight, ButtonTopDark);


    //hat select button
    entity = m_uiScene.createEntity();
    entity.setLabel("Open Headwear Browser");
    entity.addComponent<cro::Transform>().setPosition({47.f, 180.f, 0.5f});
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("hat_select");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    auto rect = entity.getComponent<cro::Sprite>().getTextureRect();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);
    entity.getComponent<cro::UIInput>().setSelectionIndex(ButtonHairBrowse);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonNextBody, ButtonSkinTone);
#ifdef USE_GNS
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonDescUp, ButtonWorkshop);
#else
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonDescUp, ButtonRandomise);
#endif
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = 
        uiSystem.addCallback([rect](cro::Entity e) {e.getComponent<cro::Sprite>().setTextureRect(rect); });
    rect.bottom += rect.height;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] =
        uiSystem.addCallback([&,rect](cro::Entity e)
            {
                e.getComponent<cro::Sprite>().setTextureRect(rect);
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                m_menuEntities[EntityID::TipText].getComponent<cro::Text>().setString(e.getLabel());
            });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt) 
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    m_menuEntities[EntityID::HairEditor].getComponent<cro::Callback>().active = true;
                    m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Skeleton>().stop();
                    m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Skeleton>().gotoFrame(0);

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    const auto expandHitbox = 
        [](cro::Entity e)
        {
            auto bounds = e.getComponent<cro::UIInput>().area;
            bounds.bottom -= 16.f;
            bounds.height += 32.f;
            e.getComponent<cro::UIInput>().area = bounds;
        };

    auto avatarLeft = createButton("arrow_left", glm::vec2(87.f, 133.f), ButtonPrevBody);
    avatarLeft.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    setAvatarIndex((m_avatarIndex + (m_avatarModels.size() - 1)) % m_avatarModels.size());
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    avatarLeft.getComponent<cro::UIInput>().setNextIndex(ButtonNextBody, ButtonHairBrowse);
    avatarLeft.getComponent<cro::UIInput>().setPrevIndex(ButtonTopDark, ButtonTopDark);
    avatarLeft.setLabel("Previous Avatar");
    expandHitbox(avatarLeft);

    auto avatarRight = createButton("arrow_right", glm::vec2(234.f, 133.f), ButtonNextBody);
    avatarRight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    setAvatarIndex((m_avatarIndex + 1) % m_avatarModels.size());
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    avatarRight.getComponent<cro::UIInput>().setNextIndex(ButtonPrevBall, ButtonPrevBall);
    avatarRight.getComponent<cro::UIInput>().setPrevIndex(ButtonPrevBody, ButtonPrevBall);
    avatarRight.setLabel("Next Avatar");
    expandHitbox(avatarRight);

    //southpaw checkbox
    auto southPaw = createButton("check_highlight", glm::vec2(17.f, 50.f), ButtonSouthPaw);
    southPaw.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_activeProfile.flipped = !m_activeProfile.flipped;
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    southPaw.getComponent<cro::UIInput>().setNextIndex(ButtonSaveClose, ButtonRandomise);
    southPaw.getComponent<cro::UIInput>().setPrevIndex(ButtonSaveClose, ButtonBottomLight);
    southPaw.getComponent<cro::UIInput>().area.width *= 7.f;
    southPaw.setLabel("Use Left Handed Avatar");
    
    auto innerEnt = m_uiScene.createEntity();
    innerEnt.addComponent<cro::Transform>().setPosition(glm::vec3(19.f, 52.f, 0.1f));
    innerEnt.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, 5.f), cro::Colour::Transparent),
            cro::Vertex2D(glm::vec2(0.f), cro::Colour::Transparent),
            cro::Vertex2D(glm::vec2(5.f), cro::Colour::Transparent),
            cro::Vertex2D(glm::vec2(5.f, 0.f), cro::Colour::Transparent)
        }
    );
    innerEnt.getComponent<cro::Drawable2D>().updateLocalBounds();
    innerEnt.addComponent<cro::Callback>().active = true;
    innerEnt.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        auto c = m_activeProfile.flipped ? TextGoldColour : cro::Colour::Transparent;
        for (auto& v : e.getComponent<cro::Drawable2D>().getVertexData())
        {
            v.colour = c;
        }
    };
    bgEnt.getComponent<cro::Transform>().addChild(innerEnt.getComponent<cro::Transform>());

    //randomise button
    auto randomButton = createButton("random_highlight", { 9.f, 24.f }, ButtonRandomise);
    randomButton.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    //randomise hair
                    setHairIndex(cro::Util::Random::value(0u, m_avatarHairModels.size() - 1));
                    setHatIndex(0);

                    //randomise avatar
                    setAvatarIndex(cro::Util::Random::value(0u, m_sharedData.avatarInfo.size() - 1));

                    //randomise colours
                    for (auto i = 0; i < PaletteID::BallThumb; ++i)
                    {
                        m_activeProfile.avatarFlags[i] = static_cast<std::uint8_t>(cro::Util::Random::value(0u, pc::PairCounts[i] - 1));
                        m_profileTextures[m_avatarIndex].setColour(pc::ColourKey::Index(i), m_activeProfile.avatarFlags[i]);
                    }

                    //update texture
                    m_profileTextures[m_avatarIndex].apply();

                    //update hair
                    if (m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].isValid())
                    {
                        m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", pc::Palette[m_activeProfile.avatarFlags[0]]);
                        m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].getComponent<cro::Model>().setMaterialProperty(1, "u_hairColour", pc::Palette[m_activeProfile.avatarFlags[0]]);
                    }

                    //update swatch
                    refreshSwatch();
                }
            });
#ifdef USE_GNS
    randomButton.getComponent<cro::UIInput>().setNextIndex(ButtonCancel, ButtonWorkshop);
#else
    randomButton.getComponent<cro::UIInput>().setNextIndex(ButtonCancel, ButtonHairBrowse);
#endif
    if (!Social::isSteamdeck())
    {
        randomButton.getComponent<cro::UIInput>().setPrevIndex(ButtonMugshot, ButtonSouthPaw);
    }
    else
    {
        randomButton.getComponent<cro::UIInput>().setPrevIndex(ButtonCancel, ButtonSouthPaw);
    }

    //name button
    auto nameButton = createButton("name_highlight", glm::vec2(264.f, 213.f), ButtonName);
    nameButton.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (/*!m_activeProfile.isSteamID &&*/
                    activated(evt))
                {
                    auto& callback = m_menuEntities[EntityID::NameText].getComponent<cro::Callback>();
                    callback.active = !callback.active;
                    if (callback.active)
                    {
                        beginTextEdit(m_menuEntities[EntityID::NameText], &m_activeProfile.name, ConstVal::MaxStringChars);
                        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                        if (evt.type == SDL_CONTROLLERBUTTONUP)
                        {
#ifdef USE_GNS
                            if (Social::isSteamdeck())
                            {
                                const auto cb =
                                    [&](bool submitted, const char* buffer)
                                    {
                                        if (submitted)
                                        {
                                            *m_textEdit.string = cro::String::fromUtf8(buffer, buffer + std::strlen(buffer));
                                            *m_textEdit.string = m_textEdit.string->substr(0, ConstVal::MaxStringChars);
                                            applyTextEdit();
                                            refreshNameString();
                                        }
                                        else
                                        {
                                            cancelTextEdit();
                                        }
                                    };

                                //this only shows the overlay as Steam takes care of dismissing it
                                const auto utf = m_activeProfile.name.toUtf8();
                                Social::showTextInput(cb, "Enter Name", ConstVal::MaxStringChars * 2, reinterpret_cast<const char*>(utf.data()));
                            }
                            else
#endif
                            {
                                auto* msg = postMessage<SystemEvent>(cl::MessageID::SystemMessage);
                                msg->type = SystemEvent::RequestOSK;
                                msg->data = 0;
                            }
                        }
                    }
                    else
                    {
                        applyTextEdit();
                        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();                
                    }
                }
            });
#ifdef USE_GNS
    nameButton.getComponent<cro::UIInput>().setNextIndex(ButtonWorkshop, ButtonBallSelect);
    nameButton.getComponent<cro::UIInput>().setPrevIndex(ButtonHairBrowse, ButtonCancel);
#else
    nameButton.getComponent<cro::UIInput>().setNextIndex(ButtonDescUp, ButtonBallSelect);
    nameButton.getComponent<cro::UIInput>().setPrevIndex(ButtonHairBrowse, ButtonBallSelect);
#endif

    //ball arrow buttons
    auto ballLeft = createButton("arrow_left", glm::vec2(267.f, 144.f), ButtonPrevBall);
    ballLeft.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    setBallIndex((m_ballIndex + (m_ballModels.size() - 1)) % m_ballModels.size());
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    ballLeft.getComponent<cro::UIInput>().setNextIndex(ButtonBallSelect, ButtonBallColour);
    ballLeft.getComponent<cro::UIInput>().setPrevIndex(ButtonNextBody, ButtonName);
    ballLeft.setLabel("Previous Ball");
    expandHitbox(ballLeft);

    //toggles the ball browser
    auto ballThumb = createButton("ballthumb_highlight", { 279.f , 93.f }, ButtonBallSelect);
    ballThumb.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
                    m_menuEntities[EntityID::BallBrowser].getComponent<cro::Callback>().active = true;

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    m_lastSelected = e.getComponent<cro::UIInput>().getSelectionIndex();
                }
            });
    ballThumb.getComponent<cro::UIInput>().setNextIndex(ButtonNextBall, ButtonBallColour);
    ballThumb.getComponent<cro::UIInput>().setPrevIndex(ButtonPrevBall, ButtonName);
    ballThumb.setLabel("Open Ball Browser");

    auto ballRight = createButton("arrow_right", glm::vec2(382.f, 144.f), ButtonNextBall);
    ballRight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    setBallIndex((m_ballIndex + 1) % m_ballModels.size());
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    ballRight.getComponent<cro::UIInput>().setNextIndex(ButtonDescUp, ButtonBallColourReset);
    ballRight.getComponent<cro::UIInput>().setPrevIndex(ButtonBallSelect, ButtonName);
    ballRight.setLabel("Next Ball");
    expandHitbox(ballRight);



    //ball colour button
    auto ballColour = createButton("ball_colour_highlight", glm::vec2(313.f, 75.f), ButtonBallColour);
    ballColour.getComponent<cro::UIInput>().setNextIndex(ButtonBallColourReset, ButtonPickClubs);
    ballColour.getComponent<cro::UIInput>().setPrevIndex(ButtonNextBody, ButtonBallSelect);
    ballColour.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    m_ballColourFlyout.background.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::BallColour);
                    m_uiScene.getSystem<cro::UISystem>()->setColumnCount(PaletteColumnCount);

                    if (m_activeProfile.ballColourIndex < pc::Palette.size())
                    {
                        m_uiScene.getSystem<cro::UISystem>()->selectAt(fetchUIIndexFromColour(m_activeProfile.ballColourIndex, pc::ColourKey::Hair));
                    }
                    else
                    {
                        m_uiScene.getSystem<cro::UISystem>()->selectAt(0);
                    }

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    m_lastSelected = e.getComponent<cro::UIInput>().getSelectionIndex();
                }
            });
    ballColour.setLabel("Choose Ball Tint");

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 316.f, 78.f, 0.1f });
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, 10.f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(0.f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(10.f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(10.f, 0.f), cro::Colour::White)
        });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
            for (auto& v : verts)
            {
                v.colour = m_activeProfile.ballColour;
            }
        };
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //ball colour reset button
    auto ballColourReset = createButton("ball_reset_highlight", glm::vec2(338.f, 76.f), ButtonBallColourReset);
    ballColourReset.getComponent<cro::UIInput>().setNextIndex(ButtonDescDown, ButtonPickClubs);
    ballColourReset.getComponent<cro::UIInput>().setPrevIndex(ButtonBallColour, ButtonNextBall);
    ballColourReset.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    m_activeProfile.ballColour = cro::Colour::White;
                    m_activeProfile.ballColourIndex = 255;

                    m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setMaterialProperty(0, "u_ballColour", m_activeProfile.ballColour);

                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    ballColourReset.setLabel("Reset Ball Tint");



    //browse club models
    auto clubs = createButton("button_highlight", glm::vec2(277.f, 61.f), ButtonPickClubs);
    bounds = clubs.getComponent<cro::Sprite>().getTextureBounds();
    bounds.left += 2.f;
    bounds.bottom += 2.f;
    bounds.width -= 4.f;
    bounds.height -= 4.f;
    clubs.getComponent<cro::UIInput>().area = bounds;
    clubs.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });
    clubs.getComponent<cro::UIInput>().setNextIndex(ButtonDescDown, ButtonUpdateIcon);
    clubs.getComponent<cro::UIInput>().setPrevIndex(ButtonSouthPaw, ButtonBallColour);



    //updates the profile icon
    auto profileIcon = createButton("button_highlight", glm::vec2(277.f, 48.f), ButtonUpdateIcon);
    bounds = profileIcon.getComponent<cro::Sprite>().getTextureBounds();
    bounds.left += 2.f;
    bounds.bottom += 2.f;
    bounds.width -= 4.f;
    bounds.height -= 4.f;
    profileIcon.getComponent<cro::UIInput>().area = bounds;
    profileIcon.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    generateMugshot();
                    m_audioEnts[AudioID::Snap].getComponent<cro::AudioEmitter>().play();
                }
            });
    profileIcon.getComponent<cro::UIInput>().setNextIndex(ButtonDescDown, ButtonSaveClose);
    profileIcon.getComponent<cro::UIInput>().setPrevIndex(ButtonSouthPaw, ButtonPickClubs);

    //save/quit buttons
    auto saveQuit = createButton("button_highlight", glm::vec2(277.f, 35.f), ButtonSaveClose);
    bounds = saveQuit.getComponent<cro::Sprite>().getTextureBounds();
    bounds.left += 2.f;
    bounds.bottom += 2.f;
    bounds.width -= 4.f;
    bounds.height -= 4.f;
    saveQuit.getComponent<cro::UIInput>().area = bounds;
    saveQuit.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_profileData.playerProfiles[m_profileData.activeProfileIndex] = m_activeProfile;
                    m_profileData.playerProfiles[m_profileData.activeProfileIndex].saveProfile();

                    if (m_mugshotUpdated)
                    {
                        auto path = Social::getUserContentPath(Social::UserContent::Profile) + m_activeProfile.profileID + "/mug.png";
                        m_mugshotTexture.getTexture().saveToFile(path);

                        m_activeProfile.mugshot = path;
                        m_profileData.playerProfiles[m_profileData.activeProfileIndex].mugshot = path;
                        m_profileData.playerProfiles[m_profileData.activeProfileIndex].saveProfile();

                        m_mugshotUpdated = false;
                    }

                    if (m_activeProfile.isSteamID
                        && m_activeProfile.name != Social::getPlayerName())
                    {
                        Social::setPlayerName(m_activeProfile.name);
                    }

                    quitState();
                }
            });
    saveQuit.getComponent<cro::UIInput>().setNextIndex(ButtonSouthPaw, ButtonCancel);
    saveQuit.getComponent<cro::UIInput>().setPrevIndex(ButtonSouthPaw, ButtonUpdateIcon);


    auto quit = createButton("button_highlight", glm::vec2(277.f, 22.f), ButtonCancel);
    bounds = quit.getComponent<cro::Sprite>().getTextureBounds();
    bounds.left += 2.f;
    bounds.bottom += 2.f;
    bounds.width -= 4.f;
    bounds.height -= 4.f;
    quit.getComponent<cro::UIInput>().area = bounds;
    quit.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_activeProfile.name = m_previousName;
                    quitState();
                }
            });
    if (!Social::isSteamdeck())
    {
        quit.getComponent<cro::UIInput>().setNextIndex(ButtonMugshot, ButtonName);
    }
    else
    {
        quit.getComponent<cro::UIInput>().setNextIndex(ButtonRandomise, ButtonName);
    }
    quit.getComponent<cro::UIInput>().setPrevIndex(ButtonRandomise, ButtonSaveClose);



    auto addCorners = [&](cro::Entity p, cro::Entity q)
    {
        auto bounds = q.getComponent<cro::Sprite>().getTextureBounds() * q.getComponent<cro::Transform>().getScale().x;
        auto offset = q.getComponent<cro::Transform>().getPosition();

        auto cornerEnt = m_uiScene.createEntity();
        cornerEnt.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.3f });
        cornerEnt.getComponent<cro::Transform>().move(glm::vec2(offset));
        cornerEnt.addComponent<cro::Drawable2D>();
        cornerEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("corner_bl");
        p.getComponent<cro::Transform>().addChild(cornerEnt.getComponent<cro::Transform>());

        auto cornerBounds = cornerEnt.getComponent<cro::Sprite>().getTextureBounds();

        cornerEnt = m_uiScene.createEntity();
        cornerEnt.addComponent<cro::Transform>().setPosition({ 0.f, bounds.height - cornerBounds.height, 0.3f });
        cornerEnt.getComponent<cro::Transform>().move(glm::vec2(offset));
        cornerEnt.addComponent<cro::Drawable2D>();
        cornerEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("corner_tl");
        p.getComponent<cro::Transform>().addChild(cornerEnt.getComponent<cro::Transform>());

        cornerEnt = m_uiScene.createEntity();
        cornerEnt.addComponent<cro::Transform>().setPosition({ bounds.width - cornerBounds.width, bounds.height - cornerBounds.height, 0.3f });
        cornerEnt.getComponent<cro::Transform>().move(glm::vec2(offset));
        cornerEnt.addComponent<cro::Drawable2D>();
        cornerEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("corner_tr");
        p.getComponent<cro::Transform>().addChild(cornerEnt.getComponent<cro::Transform>());

        cornerEnt = m_uiScene.createEntity();
        cornerEnt.addComponent<cro::Transform>().setPosition({ bounds.width - cornerBounds.width, 0.f, 0.3f });
        cornerEnt.getComponent<cro::Transform>().move(glm::vec2(offset));
        cornerEnt.addComponent<cro::Drawable2D>();
        cornerEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("corner_br");
        p.getComponent<cro::Transform>().addChild(cornerEnt.getComponent<cro::Transform>());
    };


    //avatar preview
    glm::vec3 previewPos({ 98.f, 27.f, 0.1f });
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(previewPos);
    if (!m_sharedData.pixelScale)
    {
        entity.getComponent<cro::Transform>().setScale(glm::vec2(1.f / getViewScale()));
    }
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_avatarTexture.getTexture());
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_menuEntities[EntityID::AvatarPreview] = entity;
    addCorners(bgEnt, entity);

    //displays an icon based on whether the model is custom or built-in etc
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(previewPos + glm::vec3({ 108.f, 180.f, 0.1f }));
    entity.getComponent<cro::Transform>().setOrigin({ 8.f, 8.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("asset_type");
    entity.addComponent<cro::SpriteAnimation>();

    struct AnimData final
    {
        std::size_t m_lastIndex = 0;
        float progress = 0.f;
    };
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<AnimData>();
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& [lastIndex, progress] = e.getComponent<cro::Callback>().getUserData<AnimData>();
            
            if (m_avatarIndex != lastIndex)
            {
                //shrink
                progress = std::max(0.f, progress - (dt * 6.f));
                if(progress == 0)
                {
                    lastIndex = m_avatarIndex;
                    e.getComponent<cro::SpriteAnimation>().play(m_avatarModels[m_avatarIndex].type);
                }
            }
            else
            {
                if (progress != 1)
                {
                    //grow
                    progress = std::min(1.f, progress + (dt * 6.f));
                }
            }
            const float scale = cro::Util::Easing::easeOutSine(progress);
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale, 1.f));
        };

    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //displays the index of the current avatar model
    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(previewPos + glm::vec3(65.f, 9.f, 0.1f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setString("1/" + std::to_string(m_avatarModels.size() - m_lockedAvatarCount));
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<std::size_t>(0);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            auto& lastIdx = e.getComponent<cro::Callback>().getUserData<std::size_t>();
            if (m_avatarIndex != lastIdx)
            {
                lastIdx = m_avatarIndex;
                auto str = std::to_string(m_avatarModels[m_avatarIndex].previewIndex) + "/" + std::to_string(m_avatarModels.size() - m_lockedAvatarCount);
                e.getComponent<cro::Text>().setString(str);
            }
        };
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //ball preview
    previewPos = { 279.f, 93.f, 0.1f };
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(previewPos);
    if (!m_sharedData.pixelScale)
    {
        entity.getComponent<cro::Transform>().setScale(glm::vec2(1.f / getViewScale()));
    }
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_ballTexture.getTexture());
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    addCorners(bgEnt, entity);

    //displays an icon based on whether the model is custom or built-in etc
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(previewPos + glm::vec3({ 74.f, 88.f, 0.1f }));
    entity.getComponent<cro::Transform>().setOrigin({ 8.f, 8.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("asset_type");
    entity.addComponent<cro::SpriteAnimation>();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<AnimData>();
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& [lastIndex, progress] = e.getComponent<cro::Callback>().getUserData<AnimData>();

            if (m_ballIndex != lastIndex)
            {
                //shrink
                progress = std::max(0.f, progress - (dt * 6.f));
                if (progress == 0)
                {
                    lastIndex = m_ballIndex;
                    e.getComponent<cro::SpriteAnimation>().play(m_ballModels[m_ballIndex].type);
                }
            }
            else
            {
                if (progress != 1)
                {
                    //grow
                    progress = std::min(1.f, progress + (dt * 6.f));
                }
            }
            const float scale = cro::Util::Easing::easeOutSine(progress);
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale, 1.f));
        };
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //displays selected index
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(previewPos + glm::vec3(48.f, 9.f, 0.1f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setString("1/" + std::to_string(m_ballModels.size()));
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<std::size_t>(0);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            auto& lastIdx = e.getComponent<cro::Callback>().getUserData<std::size_t>();
            if (m_ballIndex != lastIdx)
            {
                lastIdx = m_ballIndex;
                auto str = std::to_string(m_ballIndex + 1) + "/" + std::to_string(m_ballModels.size());
                e.getComponent<cro::Text>().setString(str);
            }
        };
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //mugshot
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 396.f, 24.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>();
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_menuEntities[EntityID::Mugshot] = entity;

    refreshMugshot();

    //opens the profile folder for easier adding of mugshot
    //but only in windowed mode and not on steamdeck
    if (!Social::isSteamdeck())
    {
        //open the profile folder to copy mugshot image
        entity = createButton("profile_highlight", {393.f, 21.f}, ButtonMugshot);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
            uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
                {
                    if (activated(evt))
                    {
                        auto path = Social::getUserContentPath(Social::UserContent::Profile) + m_activeProfile.profileID;
                        if (cro::FileSystem::directoryExists(path)
                            && !cro::App::getWindow().isFullscreen())
                        {
                            cro::Util::String::parseURL(path);
                        }
                    }
                });
        entity.getComponent<cro::UIInput>().setNextIndex(ButtonRandomise, ButtonName);
        entity.getComponent<cro::UIInput>().setPrevIndex(ButtonCancel, ButtonDescDown);


        //*sigh* entity already has a callback for animation (which by this point should probably be its own system)
        //so hack around this with another entity that enables the button when !full screen
        auto b = entity;
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [b](cro::Entity, float) mutable
        {
            b.getComponent<cro::UIInput>().enabled = !cro::App::getWindow().isFullscreen();
        };
    }

    entity = createButton("scroll_up", { 441.f, 202.f }, ButtonDescUp);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_menuEntities[EntityID::BioText].getComponent<cro::Callback>().getUserData<std::int32_t>()--;
                    m_menuEntities[EntityID::BioText].getComponent<cro::Callback>().active = true;
                }
            });
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonHairBrowse, ButtonDescDown);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonNextBall, ButtonName);
    expandHitbox(entity);

    entity = createButton("scroll_down", { 441.f, 91.f }, ButtonDescDown);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_menuEntities[EntityID::BioText].getComponent<cro::Callback>().getUserData<std::int32_t>()++;
                    m_menuEntities[EntityID::BioText].getComponent<cro::Callback>().active = true;
                }
            });
    if (!Social::isSteamdeck())
    {
        entity.getComponent<cro::UIInput>().setNextIndex(ButtonBottomLight, ButtonMugshot);
    }
    else
    {
        entity.getComponent<cro::UIInput>().setNextIndex(ButtonBottomLight, ButtonDescUp);
    }
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonNextBall, ButtonDescUp);
    expandHitbox(entity);


    //bio string
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 399.f, 200.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont);// .setString("Cleftwhistle");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    
    entity.addComponent<cro::Callback>().setUserData<std::int32_t>(0);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        static constexpr float RowHeight = 8.f;
        const auto bounds = cro::Text::getLocalBounds(e);
        const auto maxHeight = -bounds.height - BioCrop.bottom;

        auto& idx = e.getComponent<cro::Callback>().getUserData<std::int32_t>();
        idx = std::clamp(idx, 0, std::abs(static_cast<std::int32_t>(maxHeight / RowHeight)) + 1);

        const float target = idx * -RowHeight;
        auto org = e.getComponent<cro::Transform>().getOrigin();
        org.y = target;// std::min(target, org.y - (target - org.y) * dt);

        auto rect = BioCrop;
        rect.bottom += org.y;
        e.getComponent<cro::Drawable2D>().setCroppingArea(rect);
        e.getComponent<cro::Transform>().setOrigin(org);

        if (org.y == target)
        {
            e.getComponent<cro::Callback>().active = false;
        }
    };

    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_menuEntities[EntityID::BioText] = entity;
    setBioString(generateRandomBio());

    //help string
    bounds = bgEnt.getComponent<cro::Sprite>().getTextureBounds();
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({164.f, 14.f, 0.1f});
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_menuEntities[EntityID::HelpText] = entity;


    //button help string
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({bounds.width - 126.f, 14.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    //entity.getComponent<cro::Text>().setString("This is some test text. Hello!");
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_menuEntities[EntityID::TipText] = entity;


    CallbackContext ctx;
    ctx.spriteSheet.loadFromFile("assets/golf/sprites/avatar_browser.spt", m_resources.textures);
    ctx.createArrow = 
        [&](std::int32_t left)
        {
            auto ent = m_uiScene.createEntity();
            ent.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.2f });
            ent.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
            ent.addComponent<cro::Drawable2D>();
            ent.addComponent<cro::Sprite>() = ctx.spriteSheet.getSprite(left ? "arrow_left" : "arrow_right");
            ent.addComponent<cro::Callback>().setUserData<float>(ent.getComponent<cro::Sprite>().getTextureRect().left);
            ent.addComponent<cro::UIInput>().area = ent.getComponent<cro::Sprite>().getTextureBounds();
            ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = ctx.arrowSelected;
            ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = ctx.arrowUnselected;
            return ent;
        };
    ctx.arrowSelected = uiSystem.addCallback([&](cro::Entity e) mutable
        {
            e.getComponent<cro::AudioEmitter>().play();
            //we hacked in the base pos here
            const float left = e.getComponent<cro::Callback>().getUserData<float>();
            auto bounds = e.getComponent<cro::Sprite>().getTextureRect();
            bounds.left = bounds.width + left;
            e.getComponent<cro::Sprite>().setTextureRect(bounds);

            //TODO we need to know the type of item we're paging
            for (auto& page : m_pageContexts[PaginationID::Balls].pageList)
            {
                page.highlight.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            }
            //instead of hacking this here
            for (auto& page : m_pageContexts[PaginationID::Hair].pageList)
            {
                page.highlight.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            }
        });
    ctx.arrowUnselected = uiSystem.addCallback([](cro::Entity e)
        {
            const float left = e.getComponent<cro::Callback>().getUserData<float>();
            auto bounds = e.getComponent<cro::Sprite>().getTextureRect();
            bounds.left = left;
            e.getComponent<cro::Sprite>().setTextureRect(bounds);
        });
    ctx.closeSelected = uiSystem.addCallback([&](cro::Entity e) mutable
        {
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::Callback>().active = true;

            //TODO get the item ID of what we're paging
            for (auto& page : m_pageContexts[PaginationID::Balls].pageList)
            {
                page.highlight.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            }
            //instead of hacking this here
            for (auto& page : m_pageContexts[PaginationID::Hair].pageList)
            {
                page.highlight.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            }
        });
    ctx.closeUnselected = uiSystem.addCallback([](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });
    ctx.onClose = [&]()
        {
            m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::HairEditor);

            m_menuEntities[EntityID::HairPreview].getComponent<cro::Sprite>().setTextureRect(m_headwearPreviewRects[m_headwearID]);
        };

    createPalettes(bgEnt);
    createHairBrowser(rootNode, ctx);

    ctx.onClose = [&]()
        {
            m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Main);
            m_uiScene.getSystem<cro::UISystem>()->selectAt(m_lastSelected);

            if (m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Skeleton>().getState() == cro::Skeleton::Stopped)
            {
                auto idx = m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Skeleton>().getAnimationIndex("idle_standing");
                m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Skeleton>().play(idx);
            }
        };
    createBallBrowser(rootNode, ctx);

    CallbackContext ctx2;
    ctx2.closeUnselected = ctx.closeUnselected;
    ctx2.closeSelected = uiSystem.addCallback([&](cro::Entity e) mutable
        {
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::Callback>().active = true;
        });
    ctx2.closeButtonPosition = { 438.f, 116.f, 0.1f };
    ctx2.spriteSheet.loadFromFile("assets/golf/sprites/hair_editor.spt", m_resources.textures);
    ctx2.onClose = ctx.onClose;
    createHairEditor(rootNode, ctx2);

    auto updateView = [&, rootNode](cro::Camera& cam) mutable
    {
        glm::vec2 size(GolfGame::getActiveTarget()->getSize());

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -12.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        m_viewScale = glm::vec2(getViewScale());
        rootNode.getComponent<cro::Transform>().setScale(m_viewScale);
        rootNode.getComponent<cro::Transform>().setPosition(size / 2.f);

        //updates any text objects / buttons with a relative position
        cro::Command cmd;
        cmd.targetFlags = CommandID::Menu::UIElement;
        cmd.action =
            [&, size](cro::Entity e, float)
        {
            const auto& element = e.getComponent<UIElement>();
            auto pos = element.absolutePosition;
            pos += element.relativePosition * size / m_viewScale;

            pos.x = std::floor(pos.x);
            pos.y = std::floor(pos.y);

            e.getComponent<cro::Transform>().setPosition(glm::vec3(pos, element.depth));
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    };

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().resizeCallback = updateView;
    m_uiScene.setActiveCamera(entity);
    updateView(entity.getComponent<cro::Camera>());
}

void ProfileState::buildPreviewScene()
{
    CRO_ASSERT(!m_profileData.ballDefs.empty(), "Must load this state on top of menu");

    cro::EmitterSettings emitterSettings;
    emitterSettings.loadFromFile("assets/golf/particles/puff_small.cps", m_resources.textures);

    //this has all been parsed by the menu state - so we're assuming
    //all the models etc are fine and load without chicken
    std::int32_t c = 0;
    static constexpr glm::vec3 BallPos({ 10.f, 0.f, 0.f });
    for (auto& ballDef : m_profileData.ballDefs)
    {
        auto entity = m_modelScene.createEntity();
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
            entity.getComponent<cro::Model>().setMaterial(0, m_profileData.profileMaterials.ball);
        }
        entity.getComponent<cro::Model>().setMaterial(1, m_profileData.profileMaterials.ballReflection);
        entity.addComponent<cro::Callback>().active = true;

        //if (m_sharedData.ballInfo[c].rollAnimation)
        //{
        //    entity.getComponent<cro::Callback>().function =
        //        [](cro::Entity e, float dt)
        //    {
        //        e.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -dt * 6.f);
        //    };
        //    entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, /*cro::Util::Const::PI + */m_sharedData.ballInfo[c].previewRotation);
        //    entity.getComponent<cro::Transform>().move({ 0.f, Ball::Radius, 0.f });
        //    entity.getComponent<cro::Transform>().setOrigin({ 0.f, Ball::Radius, 0.f });
        //}
        //else
        {
            entity.getComponent<cro::Callback>().function =
                [](cro::Entity e, float dt)
            {
                e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt);
            };
        }

        auto& preview = m_ballModels.emplace_back();
        preview.ball = entity;
        preview.type = m_sharedData.ballInfo[c].type;
        preview.root = m_modelScene.createEntity();
        preview.root.addComponent<cro::Transform>().setPosition(BallPos);
        preview.root.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, m_sharedData.ballInfo[c].previewRotation);
        preview.root.getComponent<cro::Transform>().addChild(preview.ball.getComponent<cro::Transform>());
        preview.root.addComponent<cro::ParticleEmitter>().settings = emitterSettings;
        
        ++c;
    }
    
    auto entity = m_modelScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(BallPos + glm::vec3(0.f, -0.01f, 0.f));
    m_profileData.shadowDef->createModel(entity);

    entity = m_modelScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(BallPos + glm::vec3(0.f, -0.15f, 0.f));
    entity.getComponent<cro::Transform>().setScale(glm::vec3(5.f));
    m_profileData.grassDef->createModel(entity);

    static constexpr glm::vec3 AvatarPos({ CameraBasePosition.x, 0.f, 0.f });
    std::size_t previewIndex = 1;
    for (auto i = 0u; i < m_profileData.avatarDefs.size(); ++i)
    {
        //need to pad this out regardless for correct indexing
        auto& avt = m_avatarModels.emplace_back();
        if (!m_sharedData.avatarInfo[i].locked)
        {
            auto& avatar = m_profileData.avatarDefs[i];

            auto entity = m_modelScene.createEntity();
            entity.addComponent<cro::Transform>().setOrigin(AvatarPos);
            entity.getComponent<cro::Transform>().setPosition(entity.getComponent<cro::Transform>().getOrigin());
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

    entity = m_modelScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(AvatarPos + glm::vec3(0.f, -0.1f, -0.08f));
    entity.getComponent<cro::Transform>().setScale(glm::vec3(16.f));
    m_profileData.shadowDef->createModel(entity);

    entity = m_modelScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(AvatarPos + glm::vec3(0.f, -0.15f, 0.f));
    entity.getComponent<cro::Transform>().setScale(glm::vec3(60.f));
    m_profileData.grassDef->createModel(entity);
    entity.getComponent<cro::Model>().setRenderFlags((1 << 1));
    
    //update the model textures with the current colour settings
    //and any available audio
    const std::array<std::string, 3u> emitterNames =
    {
        "bunker", "fairway", "green"
    };
    for (auto i = 0u; i < m_sharedData.avatarInfo.size(); ++i)
    {
        //need to pad out the vector anyway for correct indexing
        auto& t = m_profileTextures.emplace_back(m_sharedData.avatarInfo[i].texturePath);
        
        if (!m_sharedData.avatarInfo[i].locked)
        {
            for (auto j = 0; j < pc::ColourKey::Count; ++j)
            {
                t.setColour(pc::ColourKey::Index(j), m_activeProfile.avatarFlags[j]);
            }
            t.apply();

            m_avatarModels[i].previewModel.getComponent<cro::Model>().setMaterialProperty(0, "u_diffuseMap", t.getTextureID());

            cro::AudioScape as;
            if (!m_sharedData.avatarInfo[i].audioscape.empty() &&
                as.loadFromFile(m_sharedData.avatarInfo[i].audioscape, m_resources.audio))
            {
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
            }
        }
    }

    //empty at front for 'bald'
    m_avatarHairModels.push_back({});
    m_previewHairModels.push_back({});
    for (auto& hair : m_profileData.hairDefs)
    {
        entity = m_modelScene.createEntity();
        entity.addComponent<cro::Transform>();
        hair.createModel(entity);

        auto material = m_profileData.profileMaterials.hair;
        applyMaterialData(hair, material);

        entity.getComponent<cro::Model>().setHidden(true);
        entity.getComponent<cro::Model>().setMaterial(0, material);

        if (hair.getMaterialCount() == 2)
        {
            auto material2 = m_profileData.profileMaterials.hairReflection;
            applyMaterialData(hair, material2, 1);
            entity.getComponent<cro::Model>().setMaterial(1, material2);
        }


        m_avatarHairModels.push_back(entity);
        
        //these arent ball hair, they're preview for thumbs
        entity = m_modelScene.createEntity();
        entity.addComponent<cro::Transform>().setScale(PreviewHairScale);
        entity.getComponent<cro::Transform>().setOrigin(PreviewHairOffset);
        entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 0.3f);
        hair.createModel(entity);
        entity.getComponent<cro::Model>().setMaterial(0, material);
        entity.getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", CD32::Colours[CD32::Orange]);

        if (hair.getMaterialCount() == 2)
        {
            auto material2 = m_profileData.profileMaterials.hairReflection;
            applyMaterialData(hair, material2, 1);
            entity.getComponent<cro::Model>().setMaterial(1, material2);
            entity.getComponent<cro::Model>().setMaterialProperty(2, "u_hairColour", CD32::Colours[CD32::Orange]);
        }

        entity.getComponent<cro::Model>().setHidden(true);
        m_previewHairModels.push_back(entity);
    }

    m_avatarIndex = indexFromAvatarID(m_activeProfile.skinID);
    m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Model>().setHidden(false);
    m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Callback>().active = true;

    m_ballIndex = indexFromBallID(m_activeProfile.ballID);
    m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setHidden(false);

    setHairIndex(indexFromHairID(m_activeProfile.hairID));
    setHatIndex(indexFromHairID(m_activeProfile.hatID));

    auto ballTexCallback = [&](cro::Camera& cam)
    {
        //std::uint32_t samples = m_sharedData.pixelScale ? 0 :
        //    m_sharedData.antialias ? m_sharedData.multisamples : 0;

        //auto windowSize = static_cast<float>(cro::App::getWindow().getSize().x);

        float windowScale = getViewScale();
        float scale = m_sharedData.pixelScale ? windowScale : 1.f;

        auto invScale = static_cast<std::uint32_t>((windowScale + 1.f) - scale);
        auto size = BallTexSize * invScale;
        m_ballTexture.create(size.x, size.y, true, false, /*samples*/0);

        size = AvatarTexSize * invScale;
        m_avatarTexture.create(size.x, size.y, true, false, /*samples*/0);


        cam.setPerspective(1.1f, static_cast<float>(BallTexSize.x) / BallTexSize.y, 0.001f, 2.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };
    m_cameras[CameraID::Ball] = m_modelScene.createEntity();
    m_cameras[CameraID::Ball].addComponent<cro::Transform>().setPosition({ 10.f, 0.045f, 0.099f });
    m_cameras[CameraID::Ball].addComponent<cro::Camera>().resizeCallback = ballTexCallback;
    ballTexCallback(m_cameras[CameraID::Ball].getComponent<cro::Camera>());

    createItemThumbs();

    m_cameras[CameraID::Avatar] = m_modelScene.createEntity();
    m_cameras[CameraID::Avatar].addComponent<cro::Transform>().setPosition(CameraBasePosition);
    m_cameras[CameraID::Avatar].getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI);
    m_cameras[CameraID::Avatar].getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.157f);
    auto& cam = m_cameras[CameraID::Avatar].addComponent<cro::Camera>();
    cam.setPerspective(70.f * cro::Util::Const::degToRad, static_cast<float>(AvatarTexSize.x) / AvatarTexSize.y, 0.1f, 6.f);
    cam.viewport = { 0.f, 0.f, 1.f, 1.f };

    m_cameras[CameraID::Mugshot] = m_modelScene.createEntity();
    m_cameras[CameraID::Mugshot].addComponent<cro::Transform>().setPosition(MugCameraPosition);
    m_cameras[CameraID::Mugshot].getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI);
    m_cameras[CameraID::Mugshot].getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.057f);
    auto& cam2 = m_cameras[CameraID::Mugshot].addComponent<cro::Camera>();
    cam2.setPerspective(60.f * cro::Util::Const::degToRad, 1.f, 0.1f, 6.f);
    cam2.viewport = { 0.f, 0.f, 0.5f, 1.f };
    cam2.setRenderFlags(cro::Camera::Pass::Final, ~(1 << 1));


    m_cameras[CameraID::HairEdit] = m_modelScene.createEntity();
    m_cameras[CameraID::HairEdit].addComponent<cro::Transform>().setPosition(MugCameraPosition);
    m_cameras[CameraID::HairEdit].getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI);
    m_cameras[CameraID::HairEdit].getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.057f);
    auto& cam3 = m_cameras[CameraID::HairEdit].addComponent<cro::Camera>();
    cam3.setPerspective(60.f * cro::Util::Const::degToRad, 1.f, 0.1f, 6.f);
    cam3.viewport = { 0.f, 0.f, 1.f, 1.f };


    m_modelScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 96.f * cro::Util::Const::degToRad);
    m_modelScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -39.f * cro::Util::Const::degToRad);
}

void ProfileState::createPalettes(cro::Entity parent)
{
    static const std::array<std::int32_t, PaletteID::BallThumb> PreviousMenus =
    {
        MenuID::HairEditor, MenuID::Main, MenuID::Main,
        MenuID::Main, MenuID::Main, MenuID::Main
    };

    for (auto i = 0u; i < PaletteID::BallThumb; ++i)
    {
        auto entity = createPaletteBackground(parent, m_flyouts[i], i);

        //buttons/menu items
        m_flyouts[i].activateCallback = m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, i](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                auto colourKey = i;
                if (i == PaletteID::Hair
                    && m_headwearID == HeadwearID::Hat)
                {
                    colourKey = pc::ColourKey::Hat;
                }

                auto quitMenu = [&]()
                {
                    m_flyouts[i].background.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(PreviousMenus[i]);
                    m_uiScene.getSystem<cro::UISystem>()->setColumnCount(1);
                    m_uiScene.getSystem<cro::UISystem>()->selectAt(m_lastSelected);

                    //update texture
                    m_profileTextures[m_avatarIndex].setColour(pc::ColourKey::Index(colourKey), m_activeProfile.avatarFlags[colourKey]);
                    m_profileTextures[m_avatarIndex].apply();

                    //refresh hair colour
                    if (m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].isValid())
                    {
                        m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", pc::Palette[m_activeProfile.avatarFlags[pc::ColourKey::Hair]]);
                        m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].getComponent<cro::Model>().setMaterialProperty(1, "u_hairColour", pc::Palette[m_activeProfile.avatarFlags[pc::ColourKey::Hair]]);
                    }

                    if (m_avatarHairModels[m_avatarModels[m_avatarIndex].hatIndex].isValid())
                    {
                        m_avatarHairModels[m_avatarModels[m_avatarIndex].hatIndex].getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", pc::Palette[m_activeProfile.avatarFlags[pc::ColourKey::Hat]]);
                        m_avatarHairModels[m_avatarModels[m_avatarIndex].hatIndex].getComponent<cro::Model>().setMaterialProperty(1, "u_hairColour", pc::Palette[m_activeProfile.avatarFlags[pc::ColourKey::Hat]]);
                    }

                    if (m_headwearID == HeadwearID::Hair)
                    {
                        m_menuEntities[EntityID::HairColourPreview].getComponent<cro::Sprite>().setColour(pc::Palette[m_activeProfile.avatarFlags[pc::ColourKey::Hair]]);
                    }
                    else
                    {
                        m_menuEntities[EntityID::HairColourPreview].getComponent<cro::Sprite>().setColour(pc::Palette[m_activeProfile.avatarFlags[pc::ColourKey::Hat]]);
                    }
                };

                if (activated(evt))
                {
                    m_activeProfile.avatarFlags[colourKey] = e.getComponent<cro::Callback>().getUserData<std::uint8_t>();
                    quitMenu();

                    refreshSwatch();
                }
                else if (deactivated(evt))
                {
                    quitMenu();
                }
            });
        m_flyouts[i].selectCallback = m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, entity, i](cro::Entity e) mutable
            {
                auto colourKey = i;
                if (i == PaletteID::Hair)
                {
                    if (m_headwearID == HeadwearID::Hair)
                    {
                        if (m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].isValid())
                        {
                            m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", pc::Palette[e.getComponent<cro::Callback>().getUserData<std::uint8_t>()]);
                            m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].getComponent<cro::Model>().setMaterialProperty(1, "u_hairColour", pc::Palette[e.getComponent<cro::Callback>().getUserData<std::uint8_t>()]);
                        }
                    }
                    else
                    {
                        if (m_avatarHairModels[m_avatarModels[m_avatarIndex].hatIndex].isValid())
                        {
                            m_avatarHairModels[m_avatarModels[m_avatarIndex].hatIndex].getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", pc::Palette[e.getComponent<cro::Callback>().getUserData<std::uint8_t>()]);
                            m_avatarHairModels[m_avatarModels[m_avatarIndex].hatIndex].getComponent<cro::Model>().setMaterialProperty(1, "u_hairColour", pc::Palette[e.getComponent<cro::Callback>().getUserData<std::uint8_t>()]);
                            colourKey = pc::ColourKey::Hat;
                        }
                    }
                }

                m_profileTextures[m_avatarIndex].setColour(pc::ColourKey::Index(colourKey), e.getComponent<cro::Callback>().getUserData<std::uint8_t>());
                m_profileTextures[m_avatarIndex].apply();

                entity.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition());
                m_audioEnts[AudioID::Select].getComponent<cro::AudioEmitter>().play();
                m_audioEnts[AudioID::Select].getComponent<cro::AudioEmitter>().setPlayingOffset(cro::Time());
            });

        const auto menuID = i + MenuID::Hair;
        const std::size_t IndexOffset = (i * 100) + 200;
        createPaletteButtons(m_flyouts[i], i, menuID, IndexOffset);
    }

    auto entity = createPaletteBackground(parent, m_ballColourFlyout, PaletteID::Hair);
    m_ballColourFlyout.background.getComponent<cro::Transform>().setPosition(glm::vec2(320.f, 12.f));

    //activate and select callbacks
    m_ballColourFlyout.activateCallback = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt) mutable
        {
            auto quitMenu = [&]()
                {
                    m_ballColourFlyout.background.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Main);
                    m_uiScene.getSystem<cro::UISystem>()->setColumnCount(1);
                    m_uiScene.getSystem<cro::UISystem>()->selectAt(m_lastSelected);

                    m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setMaterialProperty(0, "u_ballColour", m_activeProfile.ballColour);
                };

            if (activated(evt))
            {
                m_activeProfile.ballColourIndex = e.getComponent<cro::Callback>().getUserData<std::uint8_t>();
                m_activeProfile.ballColour = pc::Palette[m_activeProfile.ballColourIndex];
                quitMenu();
            }
            else if (deactivated(evt))
            {
                quitMenu();
            }
        });

    m_ballColourFlyout.selectCallback = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&, entity](cro::Entity e) mutable
        {
            m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setMaterialProperty(0, "u_ballColour", pc::Palette[e.getComponent<cro::Callback>().getUserData<std::uint8_t>()]);


            entity.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition());
            m_audioEnts[AudioID::Select].getComponent<cro::AudioEmitter>().play();
            m_audioEnts[AudioID::Select].getComponent<cro::AudioEmitter>().setPlayingOffset(cro::Time());
        });

    createPaletteButtons(m_ballColourFlyout, PaletteID::Hair, MenuID::BallColour, (PaletteID::Count * 100) + 200);
}

void ProfileState::createItemThumbs()
{
    glm::uvec2 texSize(std::min(ThumbColCount, m_ballModels.size()) * BallThumbSize.x, ((m_ballModels.size() / ThumbColCount) + 1) * BallThumbSize.y);
    texSize *= ThumbTextureScale;

    cro::FloatRect vp = { 0.f, 0.f, static_cast<float>(BallThumbSize.x * ThumbTextureScale) / texSize.x, static_cast<float>(BallThumbSize.y * ThumbTextureScale) / texSize.y };
    auto& cam = m_cameras[CameraID::Ball].getComponent<cro::Camera>();

    auto oldIndex = m_ballIndex;

    glm::quat oldRot = cro::Transform::QUAT_IDENTITY;
    const auto showBall = [&](std::size_t idx)
        {
            m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setHidden(true);
            m_ballIndex = idx;
            m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setHidden(false);

            oldRot = m_ballModels[m_ballIndex].ball.getComponent<cro::Transform>().getRotation();
            m_ballModels[m_ballIndex].ball.getComponent<cro::Transform>().setRotation(cro::Transform::QUAT_IDENTITY);
        };

    m_modelScene.setActiveCamera(m_cameras[CameraID::Ball]);
    m_modelScene.simulate(0.f);

    const auto& ident = m_resources.textures.get("assets/golf/images/ident.png");
    cro::SimpleQuad keyIcon(ident);
    cro::SimpleQuad spannerIcon(ident);

    const auto identSize = glm::vec2(ident.getSize());
    const cro::FloatRect key(0.f, 0.f, identSize.x / 2.f, identSize.y);
    const cro::FloatRect spanner(identSize.x / 2.f, 0.f, identSize.x / 2.f, identSize.y);
    const glm::vec2 iconOffset = (glm::vec2(BallThumbSize) - glm::vec2(17.f)) * ThumbTextureScale;
    keyIcon.setTextureRect(key);
    keyIcon.setScale(glm::vec2(ThumbTextureScale));
    spannerIcon.setTextureRect(spanner);
    spannerIcon.setScale(glm::vec2(ThumbTextureScale));


    //ball thumbs
    m_pageContexts[PaginationID::Balls].thumbnailTexture.create(texSize.x, texSize.y);
    m_pageContexts[PaginationID::Balls].thumbnailTexture.clear(CD32::Colours[CD32::BlueLight]);

    for (auto i = 0u; i < m_ballModels.size(); ++i)
    {
        const auto x = (i % ThumbColCount);
        const auto y = (i / ThumbColCount);

        vp.left = x * vp.width;
        vp.bottom = y * vp.height;
        cam.viewport = vp;

        showBall(i);

        m_modelScene.simulate(0.f);
        m_modelScene.render();
        m_ballModels[m_ballIndex].ball.getComponent<cro::Transform>().setRotation(oldRot);

        if (m_ballModels[i].type == 1)
        {
            //unlocked ball
            keyIcon.setPosition(glm::vec2(x * BallThumbSize.x * ThumbTextureScale, y * BallThumbSize.y * ThumbTextureScale) + iconOffset);
            keyIcon.draw();
        }
        else if (m_ballModels[i].type == 2)
        {
            //custom model
            spannerIcon.setPosition(glm::vec2(x * BallThumbSize.x * ThumbTextureScale, y * BallThumbSize.y * ThumbTextureScale) + iconOffset);
            spannerIcon.draw();
        }
    }

    m_pageContexts[PaginationID::Balls].thumbnailTexture.display();
    showBall(oldIndex);


    cro::ModelDefinition md(m_resources);
    md.loadFromFile("assets/models/bust.cmt");
    auto ent = m_modelScene.createEntity();
    ent.addComponent<cro::Transform>();
    md.createModel(ent);
    ent.getComponent<cro::Model>().setMaterial(0, m_profileData.profileMaterials.hair);
    ent.getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", CD32::Colours[CD32::BeigeLight]);
    m_modelScene.simulate(0.f);

    const auto showHair = [&](std::size_t idx)
        {
            //first item is bald so there's no model ent
            if (m_previewHairModels[idx].isValid())
            {
                m_ballModels[m_ballIndex].root.getComponent<cro::Transform>().addChild(m_previewHairModels[idx].getComponent<cro::Transform>());
                m_ballModels[m_ballIndex].root.getComponent<cro::Transform>().setRotation(cro::Transform::QUAT_IDENTITY); //this will have been set when rendering balls
                m_previewHairModels[idx].getComponent<cro::Model>().setHidden(false);
                m_previewHairModels[idx].getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
            }
        };

    const auto hideHair = [&](std::size_t idx)
        {
            if (m_previewHairModels[idx].isValid())
            {
                m_ballModels[m_ballIndex].root.getComponent<cro::Transform>().removeChild(m_previewHairModels[idx].getComponent<cro::Transform>());
                m_previewHairModels[idx].getComponent<cro::Model>().setHidden(true);
                m_previewHairModels[idx].getComponent<cro::Transform>().removeChild(ent.getComponent<cro::Transform>());
                m_modelScene.simulate(0.f);
            }
        };

    //hair thumbs
    texSize = glm::uvec2(std::min(ThumbColCount, m_previewHairModels.size()) * BallThumbSize.x, ((m_previewHairModels.size() / ThumbColCount) + 1) * BallThumbSize.y);
    texSize *= ThumbTextureScale;
    vp = { 0.f, 0.f, static_cast<float>(BallThumbSize.x * ThumbTextureScale) / texSize.x, static_cast<float>(BallThumbSize.y * ThumbTextureScale) / texSize.y };
    cam.viewport = vp;

    m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setHidden(true);

    m_pageContexts[PaginationID::Hair].thumbnailTexture.create(texSize.x, texSize.y);
    m_pageContexts[PaginationID::Hair].thumbnailTexture.clear(CD32::Colours[CD32::BlueLight]);
    
    //fudge to render bald bust
    ent.getComponent<cro::Transform>().setPosition(m_ballModels[m_ballIndex].root.getComponent<cro::Transform>().getWorldPosition() - (PreviewHairOffset * PreviewHairScale));
    ent.getComponent<cro::Transform>().setScale(PreviewHairScale);
    ent.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, 0.3f);
    m_modelScene.simulate(0.f);
    m_modelScene.render();
    ent.getComponent<cro::Transform>().setPosition(glm::vec3(0.f));
    ent.getComponent<cro::Transform>().setScale(glm::vec3(1.f));
    ent.getComponent<cro::Transform>().setRotation(cro::Transform::QUAT_IDENTITY);

    //don't include bald at the front
    for (auto i = 1u; i < m_previewHairModels.size(); ++i)
    {
        const auto x = (i % ThumbColCount);
        const auto y = (i / ThumbColCount);

        vp.left = x * vp.width;
        vp.bottom = y * vp.height;
        cam.viewport = vp;

        showHair(i);

        m_modelScene.simulate(0.f);
        m_modelScene.render();

        hideHair(i);

        if (m_sharedData.hairInfo[i].type == 1)
        {
            //unlocked hair
            keyIcon.setPosition(glm::vec2(x * BallThumbSize.x * ThumbTextureScale, y * BallThumbSize.y * ThumbTextureScale) + iconOffset);
            keyIcon.draw();
        }
        else if (m_sharedData.hairInfo[i].type == 2)
        {
            //custom model
            spannerIcon.setPosition(glm::vec2(x * BallThumbSize.x * ThumbTextureScale, y * BallThumbSize.y * ThumbTextureScale) + iconOffset);
            spannerIcon.draw();
        }
    }

    m_pageContexts[PaginationID::Hair].thumbnailTexture.display();
    
    m_modelScene.destroyEntity(ent);
    m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setHidden(false);

    cam.viewport = { 0.f, 0.f , 1.f, 1.f };
}

void ProfileState::createItemPage(cro::Entity parent, std::int32_t page, std::int32_t itemID)
{
    CRO_ASSERT(m_pageContexts[itemID].pageList.size() == page, "");
    static constexpr glm::vec2 IconSize(BallThumbSize);
    static constexpr float IconPadding = 6.f;

    static constexpr float ThumbWidth = (ThumbColCount * (IconSize.x + IconPadding)) + IconPadding;
    const float BgWidth = parent.getComponent<cro::Sprite>().getTextureBounds().width;

    const std::size_t RangeStart = page * ThumbRowCount * ThumbColCount;
    const std::size_t RangeEnd = RangeStart + std::min(ThumbRowCount * ThumbColCount, m_pageContexts[itemID].itemCount - RangeStart);
    const std::size_t ItemCount = RangeEnd - RangeStart;

    const auto RowCount = std::min(ThumbRowCount, (ItemCount / ThumbColCount) + std::min(std::size_t(1), ItemCount % ThumbColCount));
    const float ThumbHeight = (RowCount * IconSize.y) + (RowCount * IconPadding) + IconPadding;

    auto& browserPage = m_pageContexts[itemID].pageList.emplace_back();
    browserPage.background = m_uiScene.createEntity();
    browserPage.background.addComponent<cro::Transform>().setPosition({ (BgWidth - ThumbWidth) / 2.f, 290.f - ThumbHeight, 0.2f});
    parent.getComponent<cro::Transform>().addChild(browserPage.background.getComponent<cro::Transform>());

    //thumbnails
    std::vector<cro::Vertex2D> verts;
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.1f });
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    entity.getComponent<cro::Drawable2D>().setTexture(&m_pageContexts[itemID].thumbnailTexture.getTexture());

    const glm::vec2 TexSize(m_pageContexts[itemID].thumbnailTexture.getSize());
    cro::FloatRect textureBounds = { 0.f, 0.f, static_cast<float>(BallThumbSize.x * ThumbTextureScale) / TexSize.x, static_cast<float>(BallThumbSize.y * ThumbTextureScale) / TexSize.y };

    for (auto j = RangeStart; j < RangeEnd; ++j)
    {
        std::size_t x = (j - RangeStart) % ThumbColCount;
        std::size_t y = (j - RangeStart) / ThumbColCount;
        textureBounds.left = (j%ThumbColCount) * textureBounds.width;
        textureBounds.bottom = (j/ThumbColCount) * textureBounds.height;

        glm::vec2 pos = { x * (IconSize.x + IconPadding), ((RowCount - y) - 1) * (IconSize.y + IconPadding) };
        pos += IconPadding;

        verts.emplace_back(glm::vec2(pos.x, pos.y + IconSize.y), glm::vec2(textureBounds.left, textureBounds.bottom + textureBounds.height));
        verts.emplace_back(pos, glm::vec2(textureBounds.left, textureBounds.bottom));
        verts.emplace_back(pos + IconSize, glm::vec2(textureBounds.left + textureBounds.width, textureBounds.bottom + textureBounds.height));

        verts.emplace_back(pos + IconSize, glm::vec2(textureBounds.left + textureBounds.width, textureBounds.bottom + textureBounds.height));
        verts.emplace_back(pos, glm::vec2(textureBounds.left, textureBounds.bottom));
        verts.emplace_back(glm::vec2(pos.x + IconSize.x, pos.y), glm::vec2(textureBounds.left + textureBounds.width, textureBounds.bottom));
    }
    entity.getComponent<cro::Drawable2D>().setVertexData(verts);
    browserPage.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //highlight
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ IconPadding, IconPadding + ((RowCount - 1) * (IconPadding + IconSize.y)), 0.1f});
    entity.addComponent<cro::Drawable2D>().setVertexData(createMenuHighlight(IconSize, TextGoldColour));
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);

    browserPage.highlight = entity;
    browserPage.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //buttons
    
    //TODO rather than capture this ent use a single callback which indexes into the page items array
    //browserPage.selectCallback = m_uiScene.getSystem<cro::UISystem>()->addCallback(
    //    [&, itemID, page](cro::Entity e) mutable
    //    {
    //        m_pageContexts[itemID].pageList[page].highlight.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition());
    //        m_pageContexts[itemID].pageList[page].highlight.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);

    //        m_audioEnts[AudioID::Select].getComponent<cro::AudioEmitter>().play();
    //        m_audioEnts[AudioID::Select].getComponent<cro::AudioEmitter>().setPlayingOffset(cro::Time());
    //    });

    constexpr std::size_t IndexOffset = 100;
    for (auto j = RangeStart; j < RangeEnd; ++j)
    {
        //the Y order is reversed so that the navigation
        //direction of keys/controller is correct in the grid
        std::size_t x = (j - RangeStart) % ThumbColCount;
        std::size_t y = (RowCount - 1) - ((j - RangeStart) / ThumbColCount);

        glm::vec2 pos = { x * (IconSize.x + IconPadding), y * (IconSize.y + IconPadding) };
        pos += IconPadding;

        auto inputIndex = ((((RowCount - y) - 1) * ThumbColCount) + x) + RangeStart;

        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(pos, 0.1f));
        entity.addComponent<cro::UIInput>().setGroup(m_pageContexts[itemID].menuID);
        entity.getComponent<cro::UIInput>().area = { glm::vec2(0.f), IconSize };
        entity.getComponent<cro::UIInput>().setSelectionIndex(IndexOffset + inputIndex);

        std::size_t leftIndex = inputIndex - 1;
        std::size_t rightIndex = inputIndex + 1;
        std::size_t upIndex = inputIndex - ThumbColCount;
        std::size_t downIndex = inputIndex + ThumbColCount;

        //if moving down takes us off the end of the array
        //ie the next row isn't complete, skip to the nav arrow
        if (downIndex > (RangeEnd - 1))
        {
            if (m_pageContexts[itemID].pageHandles.pageTotal > 1)
            {
                downIndex = x < 4 ? PrevArrow : NextArrow;
            }
            else
            {
                //wrap back to top (assumes we never only have one rown on the first page)
                downIndex -= ((RowCount - 1) * ThumbColCount);
            }
        }
        else
        {
            downIndex += IndexOffset;
        }

        const auto itemsThisRow = std::min(ThumbColCount, (RangeEnd - RangeStart) - (((RowCount - y) - 1) * ThumbColCount));

        //these might be the same col (eg if there's only 1 entry this page)
        if (x == 0)
        {
            //left hand column
            leftIndex += itemsThisRow;
        }
        if (x == (itemsThisRow - 1))
        {
            //right hand column
            rightIndex -= itemsThisRow;
        }
        leftIndex += IndexOffset;
        rightIndex += IndexOffset;
        upIndex += IndexOffset;
        
        //these might be the same row
        if (y == 0
            || RowCount == 1)
        {
            //bottom row
            if (m_pageContexts[itemID].pageHandles.pageTotal > 1)
            {
                downIndex = x < 4 ? PrevArrow : NextArrow;
            }
            else
            {
                //wrap back to top (assumes we never only have one row on the first page)
                downIndex = (inputIndex - ((RowCount - 1) * ThumbColCount)) + IndexOffset;
            }
        }
        if (y == (RowCount - 1)
            || RowCount == 1)
        {
            upIndex = CloseButton;
        }

        entity.getComponent<cro::UIInput>().setPrevIndex(leftIndex, upIndex);
        entity.getComponent<cro::UIInput>().setNextIndex(rightIndex, downIndex);

        //this needs to be set per-pagination type so we know which model array to index for description labels
        //entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = browserPage.selectCallback;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_pageContexts[itemID].activateCallback;
        entity.addComponent<cro::Callback>().setUserData<std::uint8_t>(static_cast<std::uint8_t>(inputIndex));

        browserPage.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        browserPage.items.push_back(entity);
    }


    //hide this page by default
    browserPage.background.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    browserPage.highlight.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    for (auto item : browserPage.items)
    {
        item.getComponent<cro::UIInput>().enabled = false;
    }
}

void ProfileState::updateGizmo()
{
    const float scaleX = std::cos(m_avatarRotation);
    const float scaleZ = std::sin(m_avatarRotation);

    //by default these are in triangle strips
    //0   2
    //| / |
    //1   3

    std::vector<cro::Vertex2D> verts;
    verts.push_back(m_gizmo.y[0]);
    verts.push_back(m_gizmo.y[1]);
    verts.push_back(m_gizmo.y[2]);
    verts.push_back(m_gizmo.y[2]);
    verts.push_back(m_gizmo.y[1]);
    verts.push_back(m_gizmo.y[3]);

    float c = std::clamp(0.4f + (0.6f * std::abs(scaleX)), 0.f, 1.f);
    cro::Colour col(c, c, c, 1.f);
    for (auto& v : m_gizmo.x)
    {
        v.colour = col;
    }

    if (scaleX > 0)
    {
        verts.push_back(m_gizmo.x[0]);
        verts.push_back(m_gizmo.x[1]);
        verts.push_back(m_gizmo.x[2]);
        verts.back().position.x *= scaleX;
        verts.push_back(m_gizmo.x[2]);
        verts.back().position.x *= scaleX;
        verts.push_back(m_gizmo.x[1]);
        verts.push_back(m_gizmo.x[3]);
        verts.back().position.x *= scaleX;
    }
    else //fudge cos winding direction
    {
        verts.push_back(m_gizmo.x[2]);
        verts.back().position.x *= scaleX;
        verts.push_back(m_gizmo.x[3]);
        verts.back().position.x *= scaleX;
        verts.push_back(m_gizmo.x[0]);
        verts.push_back(m_gizmo.x[0]);
        verts.push_back(m_gizmo.x[3]);
        verts.back().position.x *= scaleX;
        verts.push_back(m_gizmo.x[1]);
    }

    c = std::clamp(0.4f + (0.6f * std::abs(scaleZ)), 0.f, 1.f);
    col = cro::Colour(c, c, c, 1.f);
    for (auto& v : m_gizmo.z)
    {
        v.colour = col;
    }

    if (scaleZ > 0)
    {
        verts.push_back(m_gizmo.z[0]);
        verts.push_back(m_gizmo.z[1]);
        verts.push_back(m_gizmo.z[2]);
        verts.back().position.x *= scaleZ;
        verts.push_back(m_gizmo.z[2]);
        verts.back().position.x *= scaleZ;
        verts.push_back(m_gizmo.z[1]);
        verts.push_back(m_gizmo.z[3]);
        verts.back().position.x *= scaleZ;
    }
    else
    {
        verts.push_back(m_gizmo.z[2]);
        verts.back().position.x *= scaleZ;
        verts.push_back(m_gizmo.z[3]);
        verts.back().position.x *= scaleZ;
        verts.push_back(m_gizmo.z[0]);
        verts.push_back(m_gizmo.z[0]);
        verts.push_back(m_gizmo.z[3]);
        verts.back().position.x *= scaleZ;
        verts.push_back(m_gizmo.z[1]);
    }
    m_gizmo.entity.getComponent<cro::Drawable2D>().setVertexData(verts);
}

void ProfileState::updateHeadwearTransform()
{
    auto idx = 0;
    auto idxOffset = PlayerData::HeadwearOffset::HatTx * m_headwearID;
    if (m_headwearID == HeadwearID::Hair)
    {
        idx = m_avatarModels[m_avatarIndex].hairIndex;
    }
    else
    {
        idx = m_avatarModels[m_avatarIndex].hatIndex;
    }

    if (m_avatarHairModels[idx].isValid())
    {
        const auto rot = m_activeProfile.headwearOffsets[PlayerData::HeadwearOffset::HairRot + idxOffset] * cro::Util::Const::PI;
        m_avatarHairModels[idx].getComponent<cro::Transform>().setPosition(m_activeProfile.headwearOffsets[PlayerData::HeadwearOffset::HairTx + idxOffset]);
        m_avatarHairModels[idx].getComponent<cro::Transform>().setRotation(cro::Transform::Z_AXIS, rot.z);
        m_avatarHairModels[idx].getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, rot.y);
        m_avatarHairModels[idx].getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, rot.x);
        m_avatarHairModels[idx].getComponent<cro::Transform>().setScale(m_activeProfile.headwearOffsets[PlayerData::HeadwearOffset::HairScale + idxOffset]);
    }
}

void ProfileState::createBallBrowser(cro::Entity parent, const CallbackContext& ctx)
{
    auto [bgEnt, _] = createBrowserBackground(MenuID::BallSelect, ctx);
    m_menuEntities[EntityID::BallBrowser] = bgEnt;
    parent.getComponent<cro::Transform>().addChild(bgEnt.getComponent<cro::Transform>());

    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    const auto bgSize = bgEnt.getComponent<cro::Sprite>().getTextureBounds();

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(std::floor(bgSize.width / 2.f), bgSize.height - 38.f, 0.1f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Choose Your Ball");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(glm::vec2(1.f, -1.f));
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //calc balls per page
    static constexpr auto BallsPerPage = ThumbColCount * ThumbRowCount;

    //calc number of pages
    const auto PageCount = (m_ballModels.size() / BallsPerPage) + std::min(std::size_t(1), m_ballModels.size() % BallsPerPage);

    //add arrows
    if (PageCount > 1)
    {
        //this func sets up the input callback, and stores
        //UV data in cro::Callback::setUserData() - don't modify this!
        auto& ballPageHandles = m_pageContexts[PaginationID::Balls].pageHandles;

        ballPageHandles.prevButton = ctx.createArrow(1);
        ballPageHandles.prevButton.getComponent<cro::UIInput>().setGroup(MenuID::BallSelect);
        ballPageHandles.prevButton.getComponent<cro::UIInput>().setSelectionIndex(PrevArrow);
        ballPageHandles.prevButton.getComponent<cro::UIInput>().setNextIndex(NextArrow);
        ballPageHandles.prevButton.getComponent<cro::UIInput>().setPrevIndex(NextArrow);
        ballPageHandles.prevButton.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback(
                [&](cro::Entity, const cro::ButtonEvent& evt) 
                {
                    if (activated(evt))
                    {
                        prevPage(PaginationID::Balls);
                    }
                });
        ballPageHandles.prevButton.getComponent<cro::Transform>().setPosition(glm::vec2(192.f, 25.f));
        bgEnt.getComponent<cro::Transform>().addChild(ballPageHandles.prevButton.getComponent<cro::Transform>());

        ballPageHandles.nextButton = ctx.createArrow(0);
        ballPageHandles.nextButton.getComponent<cro::UIInput>().setGroup(MenuID::BallSelect);
        ballPageHandles.nextButton.getComponent<cro::UIInput>().setSelectionIndex(NextArrow);
        ballPageHandles.nextButton.getComponent<cro::UIInput>().setNextIndex(PrevArrow, CloseButton);
        ballPageHandles.nextButton.getComponent<cro::UIInput>().setPrevIndex(PrevArrow);
        ballPageHandles.nextButton.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback(
                [&](cro::Entity, const cro::ButtonEvent& evt)
                {
                    if (activated(evt))
                    {
                        nextPage(PaginationID::Balls);
                    }
                });
        ballPageHandles.nextButton.getComponent<cro::Transform>().setPosition(glm::vec2(bgSize.width - 192.f - 16.f, 25.f));
        bgEnt.getComponent<cro::Transform>().addChild(ballPageHandles.nextButton.getComponent<cro::Transform>());

        ballPageHandles.pageTotal = PageCount;
        ballPageHandles.pageCount = m_uiScene.createEntity();
        ballPageHandles.pageCount.addComponent<cro::Transform>().setPosition(glm::vec3(std::floor(bgSize.width / 2.f), 37.f, 0.1f));
        ballPageHandles.pageCount.addComponent<cro::Drawable2D>();
        ballPageHandles.pageCount.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
        ballPageHandles.pageCount.getComponent<cro::Text>().setFillColour(TextNormalColour);
        ballPageHandles.pageCount.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
        ballPageHandles.pageCount.getComponent<cro::Text>().setString("1/" + std::to_string(PageCount));
        bgEnt.getComponent<cro::Transform>().addChild(ballPageHandles.pageCount.getComponent<cro::Transform>());
    }


    //item label
    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bgSize.width / 2.f, 13.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString(" ");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_pageContexts[PaginationID::Balls].pageHandles.itemLabel = entity;

    m_pageContexts[PaginationID::Balls].itemCount = m_ballModels.size();
    m_pageContexts[PaginationID::Balls].menuID = MenuID::BallSelect;
    m_pageContexts[PaginationID::Balls].activateCallback = 
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            auto quitMenu = [&]()
                {
                    m_menuEntities[EntityID::BallBrowser].getComponent<cro::Callback>().active = true;
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                };

            if (activated(evt))
            {
                //apply selection
                setBallIndex(e.getComponent<cro::Callback>().getUserData<std::uint8_t>());
                quitMenu();
            }
            else if (deactivated(evt))
            {
                quitMenu();
            }
        });

    //for each page - this tests if arrows were created (above)
    for (auto i = 0u; i < PageCount; ++i)
    {
        createItemPage(bgEnt, i, PaginationID::Balls);

        //shame model arrays are unique, else we could
        //recycle this for all paging types...
        auto selectCallback = m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, i](cro::Entity e) mutable
            {
                m_pageContexts[PaginationID::Balls].pageList[i].highlight.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition());
                m_pageContexts[PaginationID::Balls].pageList[i].highlight.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);

                const auto itemIndex = e.getComponent<cro::Callback>().getUserData<std::uint8_t>();
                if (!m_sharedData.ballInfo[itemIndex].label.empty())
                {
                    m_pageContexts[PaginationID::Balls].pageHandles.itemLabel.getComponent<cro::Text>().setString(m_sharedData.ballInfo[itemIndex].label);
                }
                else
                {
                    m_pageContexts[PaginationID::Balls].pageHandles.itemLabel.getComponent<cro::Text>().setString(" ");
                }

                m_audioEnts[AudioID::Select].getComponent<cro::AudioEmitter>().play();
                m_audioEnts[AudioID::Select].getComponent<cro::AudioEmitter>().setPlayingOffset(cro::Time());
            });
        for (auto& item : m_pageContexts[PaginationID::Balls].pageList[i].items)
        {
            item.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectCallback;
        }
    }
    activatePage(PaginationID::Balls, m_pageContexts[PaginationID::Balls].pageIndex, true);
}

void ProfileState::createHairBrowser(cro::Entity parent, const CallbackContext& ctx)
{
    auto [bgEnt, _] = createBrowserBackground(MenuID::HairSelect, ctx);
    bgEnt.getComponent<cro::Transform>().move({ 0.f, 0.f, 0.85f });
    m_menuEntities[EntityID::HairBrowser] = bgEnt;
    parent.getComponent<cro::Transform>().addChild(bgEnt.getComponent<cro::Transform>());

    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    const auto bgSize = bgEnt.getComponent<cro::Sprite>().getTextureBounds();

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(std::floor(bgSize.width / 2.f), bgSize.height - 38.f, 0.1f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Choose Your Headwear");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(glm::vec2(1.f, -1.f));
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //calc hair per page
    static constexpr auto HairPerPage = ThumbColCount * ThumbRowCount;

    //calc number of pages
    const auto PageCount = (m_avatarHairModels.size() / HairPerPage) + std::min(std::size_t(1), m_avatarHairModels.size() % HairPerPage);

    //add arrows
    if (PageCount > 1)
    {
        //this func sets up the input callback, and stores
        //UV data in cro::Callback::setUserData() - don't modify this!
        auto& pageHandles = m_pageContexts[PaginationID::Hair].pageHandles;

        pageHandles.prevButton = ctx.createArrow(1);
        pageHandles.prevButton.getComponent<cro::UIInput>().setGroup(MenuID::HairSelect);
        pageHandles.prevButton.getComponent<cro::UIInput>().setSelectionIndex(PrevArrow);
        pageHandles.prevButton.getComponent<cro::UIInput>().setNextIndex(NextArrow);
        pageHandles.prevButton.getComponent<cro::UIInput>().setPrevIndex(NextArrow);
        pageHandles.prevButton.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback(
                [&](cro::Entity, const cro::ButtonEvent& evt)
                {
                    if (activated(evt))
                    {
                        prevPage(PaginationID::Hair);
                    }
                });
        pageHandles.prevButton.getComponent<cro::Transform>().setPosition(glm::vec2(192.f, 25.f));
        bgEnt.getComponent<cro::Transform>().addChild(pageHandles.prevButton.getComponent<cro::Transform>());

        pageHandles.nextButton = ctx.createArrow(0);
        pageHandles.nextButton.getComponent<cro::UIInput>().setGroup(MenuID::HairSelect);
        pageHandles.nextButton.getComponent<cro::UIInput>().setSelectionIndex(NextArrow);
        pageHandles.nextButton.getComponent<cro::UIInput>().setNextIndex(PrevArrow, CloseButton);
        pageHandles.nextButton.getComponent<cro::UIInput>().setPrevIndex(PrevArrow);
        pageHandles.nextButton.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback(
                [&](cro::Entity, const cro::ButtonEvent& evt)
                {
                    if (activated(evt))
                    {
                        nextPage(PaginationID::Hair);
                    }
                });
        pageHandles.nextButton.getComponent<cro::Transform>().setPosition(glm::vec2(bgSize.width - 192.f - 16.f, 25.f));
        bgEnt.getComponent<cro::Transform>().addChild(pageHandles.nextButton.getComponent<cro::Transform>());

        pageHandles.pageTotal = PageCount;
        pageHandles.pageCount = m_uiScene.createEntity();
        pageHandles.pageCount.addComponent<cro::Transform>().setPosition(glm::vec3(std::floor(bgSize.width / 2.f), 37.f, 0.1f));
        pageHandles.pageCount.addComponent<cro::Drawable2D>();
        pageHandles.pageCount.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
        pageHandles.pageCount.getComponent<cro::Text>().setFillColour(TextNormalColour);
        pageHandles.pageCount.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
        pageHandles.pageCount.getComponent<cro::Text>().setString("1/" + std::to_string(PageCount));
        bgEnt.getComponent<cro::Transform>().addChild(pageHandles.pageCount.getComponent<cro::Transform>());
    }

    //item label - TODO we should probably be creating this as part of the background
    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({bgSize.width / 2.f, 13.f, 0.1f});
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString(" ");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_pageContexts[PaginationID::Hair].pageHandles.itemLabel = entity;

    m_pageContexts[PaginationID::Hair].itemCount = m_avatarHairModels.size();
    m_pageContexts[PaginationID::Hair].menuID = MenuID::HairSelect;
    m_pageContexts[PaginationID::Hair].activateCallback =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                auto quitMenu = [&]()
                    {
                        m_menuEntities[EntityID::HairBrowser].getComponent<cro::Callback>().active = true;
                        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                    };

                if (activated(evt))
                {
                    //apply selection
                    if (m_headwearID == HeadwearID::Hair)
                    {
                        setHairIndex(e.getComponent<cro::Callback>().getUserData<std::uint8_t>());
                    }
                    else
                    {
                        setHatIndex(e.getComponent<cro::Callback>().getUserData<std::uint8_t>());
                    }
                    quitMenu();
                }
                else if (deactivated(evt))
                {
                    quitMenu();
                }
            });

    //for each page - this tests if arrows were created (above)
    for (auto i = 0u; i < PageCount; ++i)
    {
        createItemPage(bgEnt, i, PaginationID::Hair);
        //TODO if we only had one highlight we could do this for
        //all items in the browser, rather than per-page
        auto selectCallback = m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, i](cro::Entity e) mutable
            {
                m_pageContexts[PaginationID::Hair].pageList[i].highlight.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition());
                m_pageContexts[PaginationID::Hair].pageList[i].highlight.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);

                const auto itemIndex = e.getComponent<cro::Callback>().getUserData<std::uint8_t>();
                if (!m_sharedData.hairInfo[itemIndex].label.empty())
                {
                    m_pageContexts[PaginationID::Hair].pageHandles.itemLabel.getComponent<cro::Text>().setString(m_sharedData.hairInfo[itemIndex].label);
                }
                else
                {
                    m_pageContexts[PaginationID::Hair].pageHandles.itemLabel.getComponent<cro::Text>().setString(" ");
                }
                m_audioEnts[AudioID::Select].getComponent<cro::AudioEmitter>().play();
                m_audioEnts[AudioID::Select].getComponent<cro::AudioEmitter>().setPlayingOffset(cro::Time());
            });

        for (auto& item : m_pageContexts[PaginationID::Hair].pageList[i].items)
        {
            item.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectCallback;
        }
    }
    activatePage(PaginationID::Hair, m_pageContexts[PaginationID::Hair].pageIndex, true);
}

void ProfileState::createHairEditor(cro::Entity parent, const CallbackContext& ctx)
{
    constexpr std::size_t IndexThumb = 10000;
    constexpr std::size_t IndexCol   = 10001;
    constexpr std::size_t IndexHat   = 10002;
    constexpr std::size_t IndexClose = 10003;
    constexpr std::size_t IndexTrans = 10004;
    constexpr std::size_t IndexReset = 10022;

    auto [bgEnt, closeButtonEnt] = createBrowserBackground(MenuID::HairEditor, ctx);
    bgEnt.getComponent<cro::Transform>().move(glm::vec2(0.f, 10.f));
    m_menuEntities[EntityID::HairEditor] = bgEnt;
    parent.getComponent<cro::Transform>().addChild(bgEnt.getComponent<cro::Transform>());

    closeButtonEnt.getComponent<cro::UIInput>().setSelectionIndex(IndexClose);
    closeButtonEnt.getComponent<cro::UIInput>().setNextIndex(IndexThumb, IndexTrans + 5);
    closeButtonEnt.getComponent<cro::UIInput>().setPrevIndex(IndexThumb, IndexHat);

    //avatar preview
    m_hairEditorTexture.create(HairEditTexSize.x, HairEditTexSize.y);

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 28.f, 29.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_hairEditorTexture.getTexture());
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto avEnt = entity;
    //gizmo
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 26.f, 4.f, 0.1f });
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    entity.getComponent<cro::Drawable2D>().setTexture(ctx.spriteSheet.getTexture());
    avEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    ctx.spriteSheet.getSprite("x").getVertexData(m_gizmo.x);
    ctx.spriteSheet.getSprite("y").getVertexData(m_gizmo.y);
    ctx.spriteSheet.getSprite("z").getVertexData(m_gizmo.z);
    m_gizmo.entity = entity;
    updateGizmo();

    //help string
    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    auto bounds = bgEnt.getComponent<cro::Sprite>().getTextureBounds();
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({130.f, 14.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_menuEntities[EntityID::HairHelp] = entity;


    m_headwearPreviewRects[HeadwearID::Hair] = getHeadwearTextureRect(m_avatarModels[m_avatarIndex].hairIndex);
    m_headwearPreviewRects[HeadwearID::Hat] = getHeadwearTextureRect(m_avatarModels[m_avatarIndex].hatIndex);
    

    //hair / bust preview
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 276.f, 109.f, 0.1f });
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.5f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_pageContexts[PaginationID::Hair].thumbnailTexture.getTexture());
    entity.getComponent<cro::Sprite>().setTextureRect(m_headwearPreviewRects[HeadwearID::Hair]);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_menuEntities[EntityID::HairPreview] = entity;

    //hair button
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 274.f, 107.f, 0.15f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = ctx.spriteSheet.getSprite("hair_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::HairEditor);
    entity.getComponent<cro::UIInput>().setSelectionIndex(IndexThumb);
    entity.getComponent<cro::UIInput>().setNextIndex(IndexCol, IndexTrans);
    entity.getComponent<cro::UIInput>().setPrevIndex(IndexHat, IndexReset);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = ctx.closeSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = ctx.closeUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([&](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_menuEntities[EntityID::HairBrowser].getComponent<cro::Callback>().active = true;
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });
    entity.addComponent<cro::Callback>().function = MenuTextCallback();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //colour preview
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 379.f, 187.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = ctx.spriteSheet.getSprite("item_colour");
    entity.getComponent<cro::Sprite>().setColour(pc::Palette[m_activeProfile.avatarFlags[pc::ColourKey::Hair]]);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto colourPreview = entity;
    m_menuEntities[EntityID::HairColourPreview] = entity;

    //colour button
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 375.f, 183.f, 0.15f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = ctx.spriteSheet.getSprite("palette_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::HairEditor);
    entity.getComponent<cro::UIInput>().setSelectionIndex(IndexCol);
    entity.getComponent<cro::UIInput>().setNextIndex(IndexHat, IndexClose);
    entity.getComponent<cro::UIInput>().setPrevIndex(IndexThumb, IndexReset + 1);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = ctx.closeSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = ctx.closeUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    //show palette menu
                    m_flyouts[PaletteID::Hair].background.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                    //set column/row count for menu
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Hair);
                    m_uiScene.getSystem<cro::UISystem>()->setColumnCount(PaletteColumnCount);

                    //check hair or hat index
                    const auto idx = m_headwearID == HeadwearID::Hair ? pc::ColourKey::Hair : pc::ColourKey::Hat;
                    m_uiScene.getSystem<cro::UISystem>()->selectAt(fetchUIIndexFromColour(m_activeProfile.avatarFlags[idx], idx));

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    m_lastSelected = e.getComponent<cro::UIInput>().getSelectionIndex();
                }
            });
    entity.addComponent<cro::Callback>().function = MenuTextCallback();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //title text
    const auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 437.f, 224.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("Select Hair");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(glm::vec2(1.f, -1.f));
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto textEnt = entity;

    /*entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 437.f, 172.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("SAMPLE\nTEXT");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());*/


    //hair/hat button
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 407.f, 189.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = ctx.spriteSheet.getSprite("select_button");
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    static constexpr std::array ButtonPositions =
    {
        glm::vec2(69.f, 7.f),
        glm::vec2(21.f, 7.f),
    };

    auto buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>().setPosition({ 69.f, 7.f, 0.1f });
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = ctx.spriteSheet.getSprite("select_highlight");
    buttonEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.addComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::HairEditor);
    buttonEnt.getComponent<cro::UIInput>().setSelectionIndex(IndexHat);
    buttonEnt.getComponent<cro::UIInput>().setNextIndex(IndexThumb, IndexClose);
    buttonEnt.getComponent<cro::UIInput>().setPrevIndex(IndexCol, IndexReset + 2);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = ctx.closeSelected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = ctx.closeUnselected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, entity, colourPreview, textEnt](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    auto b = entity.getComponent<cro::Sprite>().getTextureRect();
                    if (m_headwearID == HeadwearID::Hair)
                    {
                        m_headwearID = HeadwearID::Hat;
                        b.bottom += (b.height + 2.f);
                        textEnt.getComponent<cro::Text>().setString("Select Hat");
                    }
                    else
                    {
                        m_headwearID = HeadwearID::Hair;
                        b.bottom -= (b.height + 2.f);
                        textEnt.getComponent<cro::Text>().setString("Select Hair");
                    }
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    e.getComponent<cro::Transform>().setPosition(ButtonPositions[m_headwearID]);
                    entity.getComponent<cro::Sprite>().setTextureRect(b);


                    //refresh colour preview and hair thumbnail with selected settings
                    m_menuEntities[EntityID::HairPreview].getComponent<cro::Sprite>().setTextureRect(m_headwearPreviewRects[m_headwearID]);

                    const auto idx = m_headwearID == HeadwearID::Hair ? pc::ColourKey::Hair : pc::ColourKey::Hat;
                    colourPreview.getComponent<cro::Sprite>().setColour(pc::Palette[m_activeProfile.avatarFlags[idx]]);
                }
            });
    buttonEnt.addComponent<cro::Callback>().function = MenuTextCallback();
    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());


    //transform numbers
    const auto& infoFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    constexpr float YStart = 85.f;
    constexpr float RowSpacing = 14.f;
    constexpr float ColSpacing = 74.f;


    const auto selectedID = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e) 
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
        });
    const auto unselectedID = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);

            auto& context = e.getComponent<ButtonHoldContext>();
            context.isActive = false;
            context.target = nullptr;
        });
    const auto buttonLeftDown = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                auto& context = e.getComponent<ButtonHoldContext>();
                context.timer.restart();
                context.isActive = true;

                auto idx = (PlayerData::HeadwearOffset::HairTx + context.i) + (PlayerData::HeadwearOffset::HatTx * m_headwearID);
                context.target = &m_activeProfile.headwearOffsets[idx][context.j];
                context.step = -context.minStep;
            }
        });
    const auto buttonRightDown = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                auto& context = e.getComponent<ButtonHoldContext>();
                context.timer.restart();
                context.isActive = true;

                auto idx = (PlayerData::HeadwearOffset::HairTx + context.i) + (PlayerData::HeadwearOffset::HatTx * m_headwearID);
                context.target = &m_activeProfile.headwearOffsets[idx][context.j];
                context.step = context.minStep;
            }
        });
    const auto buttonLeftUp = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                auto& context = e.getComponent<ButtonHoldContext>();
                if (context.timer.elapsed().asSeconds() < ButtonHoldContext::MinClickTime)
                {
                    //short click
                    auto idx = (PlayerData::HeadwearOffset::HairTx + context.i) + (PlayerData::HeadwearOffset::HatTx * m_headwearID);
                    float& val = m_activeProfile.headwearOffsets[idx][context.j];
                    val = std::max(context.minVal, val - context.maxStep);

                    updateHeadwearTransform();
                }

                context.isActive = false;
                context.target = nullptr;
            }
        });
    const auto buttonRightUp = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                auto& context = e.getComponent<ButtonHoldContext>();
                if (context.timer.elapsed().asSeconds() < ButtonHoldContext::MinClickTime)
                {
                    //short click
                    auto idx = (PlayerData::HeadwearOffset::HairTx + context.i) + (PlayerData::HeadwearOffset::HatTx * m_headwearID);
                    float& val = m_activeProfile.headwearOffsets[idx][context.j];
                    val = std::min(context.maxVal, val + context.maxStep);

                    updateHeadwearTransform();
                }

                context.isActive = false;
                context.target = nullptr;
            }
        });
    const auto mouseExit = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e, glm::vec2, const cro::MotionEvent&)
        {
            auto& context = e.getComponent<ButtonHoldContext>();
            context.isActive = false;
            context.target = nullptr;
        });

    bounds = ctx.spriteSheet.getSprite("arrow_highlight").getTextureBounds();

    constexpr std::size_t cCount = 3;
    constexpr std::size_t rCount = 3;
    constexpr std::size_t buttonCount = 2; //buttons per item
    constexpr std::size_t rButtonCount = cCount * buttonCount; //buttons per row

    constexpr std::array MinValues = { -0.1f, -1.f, 0.01f };
    constexpr std::array MaxValues = { 0.1f, 1.f, 1.5f };
    constexpr std::array MinSteps = { 0.001f, 0.002f, 0.01f };
    constexpr std::array MaxSteps = { 0.001f, 0.002f, 0.01f };

    glm::vec3 pos(310.f, YStart, 0.1f);
    std::int32_t k = 0;

    for (auto i = 0u; i < cCount; ++i) //cols pos, rot, scale
    {
        for (auto j = 0u; j < rCount; ++j) //rows x, y, z
        {
            entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(pos);
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Text>(infoFont).setString("0.00");
            entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
            entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
            entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [&, i, j](cro::Entity e, float)
                {
                    auto idx = (PlayerData::HeadwearOffset::HairTx + i) + (PlayerData::HeadwearOffset::HatTx * m_headwearID);

                    float val = m_activeProfile.headwearOffsets[idx][j];
                    std::stringstream ss;
                    ss.precision(2);
                    ss << std::fixed << val;
                    e.getComponent<cro::Text>().setString(ss.str());
                };
            bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

            //used to detect mouse scroll
            m_transformBoxes[k].entity = entity;
            m_transformBoxes[k].index = i;
            m_transformBoxes[k].offset = j;
            m_transformBoxes[k].minVal = MinValues[i];
            m_transformBoxes[k].maxVal = MaxValues[i];
            m_transformBoxes[k].step = MinSteps[i];
            k++;

            ButtonHoldContext buttonContext;
            buttonContext.i = i;
            buttonContext.j = j;
            buttonContext.minVal = MinValues[i];
            buttonContext.maxVal = MaxValues[i];
            buttonContext.minStep = MinSteps[i];
            buttonContext.maxStep = MaxSteps[i];
            buttonContext.callback = std::bind(&ProfileState::updateHeadwearTransform, this);

            auto numEnt = entity;
            const auto rowStart = IndexTrans + (j * rButtonCount);
            std::size_t buttonIndex = rowStart + (i * buttonCount);
            std::size_t right = rowStart + (((i * buttonCount) + 1) % rButtonCount);
            std::size_t left = rowStart + (((i * buttonCount) + (rButtonCount - 1)) % rButtonCount);
            
            std::size_t up = 0;
            std::size_t down = 0;

            switch (j)
            {
            default:
            case 0:
                //top row
                if ((i*2) < 3)
                {
                    up = IndexThumb;
                }
                else
                {
                    up = IndexClose;
                }
                down = buttonIndex + rButtonCount;
                break;
            case 1:
                up = buttonIndex - rButtonCount;
                down = buttonIndex + rButtonCount;
                break;
            case 2:
                //bottom row
                down = IndexReset + i;
                up = buttonIndex - rButtonCount;
                break;
            }

            //add up/down buttons
            entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(glm::vec2(-25.f, -9.f));
            entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Sprite>() = ctx.spriteSheet.getSprite("arrow_highlight");
            entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
            entity.addComponent<cro::UIInput>().area = bounds;
            entity.getComponent<cro::UIInput>().setSelectionIndex(buttonIndex);
            entity.getComponent<cro::UIInput>().setNextIndex(right, down);
            entity.getComponent<cro::UIInput>().setPrevIndex(left, up);
            entity.getComponent<cro::UIInput>().setGroup(MenuID::HairEditor);
            entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
            entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
            entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = buttonLeftDown;
            entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = buttonLeftUp;
            entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Exit] = mouseExit;
            entity.addComponent<ButtonHoldContext>() = buttonContext;
            numEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

            buttonIndex++;
            right = (((buttonIndex - rowStart) + 1) % rButtonCount) + rowStart;
            left = (((buttonIndex - rowStart) + (rButtonCount - 1)) % rButtonCount) + rowStart;

            switch (j)
            {
            default:
            case 0:
                //top row
                down = buttonIndex + rButtonCount;
                if (i * 2 == 2)
                {
                    up = IndexClose;
                }
                break;
            case 1:
                up = buttonIndex - rButtonCount;
                down = buttonIndex + rButtonCount;
                break;
            case 2:
                //bottom row
                up = buttonIndex - rButtonCount;
                break;
            }

            entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(glm::vec2(16.f, -9.f));
            entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Sprite>() = ctx.spriteSheet.getSprite("arrow_highlight");
            entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
            entity.addComponent<cro::UIInput>().area = bounds;
            entity.getComponent<cro::UIInput>().setSelectionIndex(buttonIndex);
            entity.getComponent<cro::UIInput>().setNextIndex(right, down);
            entity.getComponent<cro::UIInput>().setPrevIndex(left, up);
            entity.getComponent<cro::UIInput>().setGroup(MenuID::HairEditor);
            entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
            entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
            entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = buttonRightDown;
            entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = buttonRightUp;
            entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Exit] = mouseExit;
            entity.addComponent<ButtonHoldContext>() = buttonContext;
            numEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

            pos.y -= RowSpacing;
        }
        pos.x += ColSpacing;
        pos.y = YStart;
    }


    //tx reset buttons
    glm::vec2 buttonPos(310.f, 35.f);

    for (auto i = 0; i < 3; ++i)
    {
        buttonEnt = m_uiScene.createEntity();
        buttonEnt.addComponent<cro::Transform>().setPosition(glm::vec3(buttonPos, 0.1f));
        buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        buttonEnt.addComponent<cro::Drawable2D>();
        buttonEnt.addComponent<cro::Sprite>() = ctx.spriteSheet.getSprite("select_highlight");
        buttonEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
        buttonEnt.addComponent<cro::UIInput>().area = bounds;
        buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::HairEditor);
        buttonEnt.getComponent<cro::UIInput>().setSelectionIndex(IndexReset + i);
        buttonEnt.getComponent<cro::UIInput>().setNextIndex(IndexReset + ((i + 1) % 3), IndexThumb + i);
        buttonEnt.getComponent<cro::UIInput>().setPrevIndex(IndexReset + ((i + 2) % 3), (IndexReset + i) - (3 + (3 -i)));
        buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = ctx.closeSelected;
        buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = ctx.closeUnselected;
        buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback(
                [&, i](cro::Entity e, const cro::ButtonEvent& evt) mutable
                {
                    if (activated(evt))
                    {
                        auto idx = (PlayerData::HeadwearOffset::HairTx + i) + (m_headwearID * PlayerData::HeadwearOffset::HatTx);
                        m_activeProfile.headwearOffsets[idx] = (idx % 3) == 2 ? glm::vec3(1.f) : glm::vec3(0.f);
                        updateHeadwearTransform();

                        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    }
                });
        buttonEnt.addComponent<cro::Callback>().function = MenuTextCallback();
        buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
        bgEnt.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());

        buttonPos.x += 74.f;
    }
}

cro::FloatRect ProfileState::getHeadwearTextureRect(std::size_t idx)
{
    cro::FloatRect textureBounds = { 0.f, 0.f, static_cast<float>(BallThumbSize.x * ThumbTextureScale), static_cast<float>(BallThumbSize.y * ThumbTextureScale) };

    std::size_t x = idx % ThumbColCount;
    std::size_t y = idx / ThumbColCount;
    textureBounds.left = x * textureBounds.width;
    textureBounds.bottom = y * textureBounds.height;

    return textureBounds;
}

std::size_t ProfileState::fetchUIIndexFromColour(std::uint8_t colourIndex, std::int32_t paletteIndex)
{
    const auto rowCount = m_uiScene.getSystem<cro::UISystem>()->getGroupSize() / PaletteColumnCount;
    const auto x = colourIndex % PaletteColumnCount;
    auto y = colourIndex / PaletteColumnCount;

    y = (rowCount - 1) - y;
    return (y * PaletteColumnCount + x) % pc::PairCounts[paletteIndex];
}

std::pair<cro::Entity, cro::Entity> ProfileState::createBrowserBackground(std::int32_t menuID, const CallbackContext& ctx)
{
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = ctx.spriteSheet.getSprite("background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setPosition({ -std::floor(bounds.width / 2.f), -std::floor(bounds.height / 2.f), 1.f });
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));

    struct BGCallbackData final
    {
        float progress = 0.f;
        std::int32_t direction = 0; //0 grow 1 shrink
    };
    entity.addComponent<cro::Callback>().setUserData<BGCallbackData>();
    entity.getComponent<cro::Callback>().function =
        [&, menuID, ctx](cro::Entity e, float dt)
        {
            glm::vec2 scale(1.f);

            auto& [currTime, dir] = e.getComponent<cro::Callback>().getUserData<BGCallbackData>();
            if (dir == 0)
            {
                //grow
                currTime = std::min(currTime + (dt * 2.f), 1.f);
                scale.x = cro::Util::Easing::easeOutQuint(currTime);

                if (currTime == 1)
                {
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(menuID);
                    dir = 1;
                    e.getComponent<cro::Callback>().active = false;
                }
            }
            else
            {
                //shrink
                currTime = std::max(0.f, currTime - (dt * 2.f));
                scale.y = cro::Util::Easing::easeInQuint(currTime);

                if (currTime == 0)
                {
                    dir = 0;
                    e.getComponent<cro::Callback>().active = false;

                    ctx.onClose();
                }
            }
            e.getComponent<cro::Transform>().setScale(scale);
        };


    auto buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>().setPosition(ctx.closeButtonPosition);
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = ctx.spriteSheet.getSprite("close_button");
    buttonEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    buttonEnt.addComponent<cro::UIInput>().setGroup(menuID);
    buttonEnt.getComponent<cro::UIInput>().setSelectionIndex(CloseButton);
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.getComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = ctx.closeSelected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = ctx.closeUnselected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, entity](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
                    entity.getComponent<cro::Callback>().active = true;
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });

    buttonEnt.addComponent<cro::Callback>().function = MenuTextCallback();
    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());

    return { entity, buttonEnt };
}

void ProfileState::quitState()
{
    auto currentMenu = m_uiScene.getSystem<cro::UISystem>()->getActiveGroup();
    if (currentMenu == MenuID::Main)
    {
        applyTextEdit();

        m_menuEntities[EntityID::Root].getComponent<cro::Callback>().active = true;
        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
    }
    //m_uiScene.setSystemActive<cro::AudioPlayerSystem>(false);
}

cro::Entity ProfileState::createPaletteBackground(cro::Entity parent, FlyoutMenu& target, std::uint32_t i)
{
    const auto RowCount = (pc::PairCounts[i] / PaletteColumnCount);
    const float BgHeight = (RowCount * Flyout::IconSize.y) + (RowCount * Flyout::IconPadding) + Flyout::IconPadding;

    //background
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(SwatchPositions[i], 2.5f));
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);

    auto verts = createMenuBackground({ Flyout::BgWidth, BgHeight });

    for (auto j = 0u; j < pc::PairCounts[i]; ++j)
    {
        std::size_t x = j % PaletteColumnCount;
        std::size_t y = j / PaletteColumnCount;
        auto colour = pc::Palette[j];

        glm::vec2 pos = { x * (Flyout::IconSize.x + Flyout::IconPadding), y * (Flyout::IconSize.y + Flyout::IconPadding) };
        pos += Flyout::IconPadding;

        verts.emplace_back(glm::vec2(pos.x, pos.y + Flyout::IconSize.y), colour);
        verts.emplace_back(pos, colour);
        verts.emplace_back(pos + Flyout::IconSize, colour);

        verts.emplace_back(pos + Flyout::IconSize, colour);
        verts.emplace_back(pos, colour);
        verts.emplace_back(glm::vec2(pos.x + Flyout::IconSize.x, pos.y), colour);
    }
    entity.getComponent<cro::Drawable2D>().setVertexData(verts);

    target.background = entity;
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //highlight
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ Flyout::IconPadding, Flyout::IconPadding, 0.1f });
    entity.addComponent<cro::Drawable2D>().setVertexData(createMenuHighlight(Flyout::IconSize, TextGoldColour));
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);

    target.highlight = entity;
    target.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    return entity;
}

void ProfileState::createPaletteButtons(FlyoutMenu& target, std::uint32_t i, std::uint32_t menuID, std::size_t indexOffset)
{
    const auto RowCount = (pc::PairCounts[i] / PaletteColumnCount);

    for (auto j = 0u; j < pc::PairCounts[i]; ++j)
    {
        //the Y order is reversed so that the navigation
        //direction of keys/controller is correct in the grid
        std::size_t x = j % PaletteColumnCount;
        std::size_t y = (RowCount - 1) - (j / PaletteColumnCount);

        glm::vec2 pos = { x * (Flyout::IconSize.x + Flyout::IconPadding), y * (Flyout::IconSize.y + Flyout::IconPadding) };
        pos += Flyout::IconPadding;

        auto inputIndex = (((RowCount - y) - 1) * PaletteColumnCount) + x;

        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(pos, 0.1f));
        entity.addComponent<cro::UIInput>().setGroup(menuID);
        entity.getComponent<cro::UIInput>().area = { glm::vec2(0.f), Flyout::IconSize };
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = target.selectCallback;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = target.activateCallback;


        entity.getComponent<cro::UIInput>().setSelectionIndex(indexOffset + inputIndex);
        if (x == 0)
        {
            auto prevIndex = std::min(inputIndex + (PaletteColumnCount - 1), pc::PairCounts[i] - 1);
            entity.getComponent<cro::UIInput>().setPrevIndex(indexOffset + prevIndex);
        }
        else if (x == (PaletteColumnCount - 1))
        {
            entity.getComponent<cro::UIInput>().setNextIndex(indexOffset + (inputIndex - (PaletteColumnCount - 1)));
        }
        else if (y == 0
            && x == (pc::PairCounts[i] % PaletteColumnCount) - 1)
        {
            auto nextIndex = inputIndex - ((pc::PairCounts[i] % PaletteColumnCount) - 1);
            entity.getComponent<cro::UIInput>().setNextIndex(indexOffset + nextIndex);
        }


        entity.addComponent<cro::Callback>().setUserData<std::uint8_t>(static_cast<std::uint8_t>((y * PaletteColumnCount) + x));

        target.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }
}

std::size_t ProfileState::indexFromAvatarID(std::uint32_t skinID) const
{
    const auto& avatarInfo = m_sharedData.avatarInfo;

    if (auto result = std::find_if(avatarInfo.cbegin(), avatarInfo.cend(), 
        [skinID](const SharedStateData::AvatarInfo& a){return a.uid == skinID;}); result != avatarInfo.cend())
    {
        return std::distance(avatarInfo.cbegin(), result);
    }

    return 0;
}

std::size_t ProfileState::indexFromBallID(std::uint32_t ballID) const
{
    const auto& ballInfo = m_sharedData.ballInfo;
    if (auto result = std::find_if(ballInfo.cbegin(), ballInfo.cend(),
        [ballID](const SharedStateData::BallInfo& b)
        {
            return b.uid == ballID;
        }); result != ballInfo.cend())
    {
        return std::distance(ballInfo.cbegin(), result);
    }

    return 0;
}

std::size_t ProfileState::indexFromHairID(std::uint32_t hairID) const
{
    const auto& hairInfo = m_sharedData.hairInfo;
    if (auto result = std::find_if(hairInfo.cbegin(), hairInfo.cend(), 
        [hairID](const SharedStateData::HairInfo& hi) {return hi.uid == hairID;}); result != hairInfo.end())
    {
        return std::distance(hairInfo.begin(), result);
    }
    return 0;
}

void ProfileState::setAvatarIndex(std::size_t idx)
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
            idx = (idx + (m_avatarModels.size() -1)) % m_avatarModels.size();
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

    m_activeProfile.skinID = m_sharedData.avatarInfo[m_avatarIndex].uid;

    for (auto i = 0u; i < m_activeProfile.avatarFlags.size(); ++i)
    {
        m_profileTextures[idx].setColour(pc::ColourKey::Index(i), m_activeProfile.avatarFlags[i]);
    }
    m_profileTextures[idx].apply();


    if (!m_avatarModels[m_avatarIndex].previewAudio.empty())
    {
        m_avatarModels[m_avatarIndex].previewAudio[cro::Util::Random::value(0u, m_avatarModels[m_avatarIndex].previewAudio.size() - 1)].getComponent<cro::AudioEmitter>().play();
    }
}

void ProfileState::setHairIndex(std::size_t idx)
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
        m_avatarHairModels[hairIndex].getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", pc::Palette[m_activeProfile.avatarFlags[pc::ColourKey::Hair]]);
        m_avatarHairModels[hairIndex].getComponent<cro::Model>().setMaterialProperty(1, "u_hairColour", pc::Palette[m_activeProfile.avatarFlags[pc::ColourKey::Hair]]);

        const auto rot = m_activeProfile.headwearOffsets[PlayerData::HeadwearOffset::HairRot] * cro::Util::Const::PI;
        m_avatarHairModels[hairIndex].getComponent<cro::Transform>().setPosition(m_activeProfile.headwearOffsets[PlayerData::HeadwearOffset::HairTx]);
        m_avatarHairModels[hairIndex].getComponent<cro::Transform>().setRotation(cro::Transform::Z_AXIS, rot.z);
        m_avatarHairModels[hairIndex].getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, rot.y);
        m_avatarHairModels[hairIndex].getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, rot.x);
        m_avatarHairModels[hairIndex].getComponent<cro::Transform>().setScale(m_activeProfile.headwearOffsets[PlayerData::HeadwearOffset::HairScale]);
    }
    m_avatarModels[m_avatarIndex].hairIndex = hairIndex;

    m_activeProfile.hairID = m_sharedData.hairInfo[hairIndex].uid;

    m_headwearPreviewRects[HeadwearID::Hair] = getHeadwearTextureRect(hairIndex);
}

void ProfileState::setHatIndex(std::size_t idx)
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
        m_avatarHairModels[hatIndex].getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", pc::Palette[m_activeProfile.avatarFlags[pc::ColourKey::Hat]]);
        m_avatarHairModels[hatIndex].getComponent<cro::Model>().setMaterialProperty(1, "u_hairColour", pc::Palette[m_activeProfile.avatarFlags[pc::ColourKey::Hat]]);

        const auto rot = m_activeProfile.headwearOffsets[PlayerData::HeadwearOffset::HatRot] * cro::Util::Const::PI;
        m_avatarHairModels[hatIndex].getComponent<cro::Transform>().setPosition(m_activeProfile.headwearOffsets[PlayerData::HeadwearOffset::HatTx]);
        m_avatarHairModels[hatIndex].getComponent<cro::Transform>().setRotation(cro::Transform::Z_AXIS, rot.z);
        m_avatarHairModels[hatIndex].getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, rot.y);
        m_avatarHairModels[hatIndex].getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, rot.x);
        m_avatarHairModels[hatIndex].getComponent<cro::Transform>().setScale(m_activeProfile.headwearOffsets[PlayerData::HeadwearOffset::HatScale]);
    }
    m_avatarModels[m_avatarIndex].hatIndex = hatIndex;

    m_activeProfile.hatID = m_sharedData.hairInfo[hatIndex].uid;

    m_headwearPreviewRects[HeadwearID::Hat] = getHeadwearTextureRect(hatIndex);
}

void ProfileState::setBallIndex(std::size_t idx)
{
    CRO_ASSERT(idx < m_ballModels.size(), "");

    if (m_previewHairModels[m_ballHairIndex].isValid())
    {
        m_ballModels[m_ballIndex].root.getComponent<cro::Transform>().removeChild(m_previewHairModels[m_ballHairIndex].getComponent<cro::Transform>());
    }
    m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setHidden(true);

    m_ballIndex = idx;

    if (m_previewHairModels[m_ballHairIndex].isValid())
    {
        m_ballModels[m_ballIndex].root.getComponent<cro::Transform>().addChild(m_previewHairModels[m_ballHairIndex].getComponent<cro::Transform>());
    }
    m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setHidden(false);
    m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setMaterialProperty(0, "u_ballColour", m_activeProfile.ballColour);

    m_ballModels[m_ballIndex].root.getComponent<cro::ParticleEmitter>().start();

    m_activeProfile.ballID = m_sharedData.ballInfo[m_ballIndex].uid;
}

void ProfileState::nextPage(std::int32_t itemID)
{
    const auto page = (m_pageContexts[itemID].pageIndex + 1) % m_pageContexts[itemID].pageList.size();
    
    activatePage(itemID, page, false);
    m_uiScene.getSystem<cro::UISystem>()->selectAt(NextArrow);
}

void ProfileState::prevPage(std::int32_t itemID)
{
    auto page = (m_pageContexts[itemID].pageIndex + (m_pageContexts[itemID].pageList.size() - 1))
        % m_pageContexts[itemID].pageList.size();
    activatePage(itemID, page, false);
    m_uiScene.getSystem<cro::UISystem>()->selectAt(PrevArrow);
}

void ProfileState::activatePage(std::int32_t itemID, std::size_t page, bool forceRefresh)
{
    if (m_uiScene.getSystem<cro::UISystem>()->getActiveGroup() == m_pageContexts[itemID].menuID
        || forceRefresh)
    {
        auto& pages = m_pageContexts[itemID].pageList;
        auto& pageIndex = m_pageContexts[itemID].pageIndex;

        pages[pageIndex].background.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        pages[pageIndex].highlight.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        for (auto item : pages[pageIndex].items)
        {
            item.getComponent<cro::UIInput>().enabled = false;
        }

        pageIndex = page;

        pages[pageIndex].background.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        pages[pageIndex].highlight.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        for (auto item : pages[pageIndex].items)
        {
            item.getComponent<cro::UIInput>().enabled = true;
        }


        auto& pageHandles = m_pageContexts[itemID].pageHandles;
        if (pageHandles.pageCount.isValid())
        {
            pageHandles.pageCount.getComponent<cro::Text>().setString(std::to_string(page + 1) + "/" + std::to_string(pageHandles.pageTotal));
        }

        if (pageHandles.prevButton.isValid())
        {
            auto itemIndex = std::min(std::size_t(3), pages[pageIndex].items.size() - 1);
            auto downIndex = pages[pageIndex].items[itemIndex].getComponent<cro::UIInput>().getSelectionIndex();
            pageHandles.prevButton.getComponent<cro::UIInput>().setNextIndex(NextArrow, downIndex);

            itemIndex += ThumbColCount * 3;
            while (itemIndex >= pages[pageIndex].items.size())
            {
                if (itemIndex > ThumbColCount)
                {
                    itemIndex -= ThumbColCount;
                }
                else
                {
                    itemIndex = std::min(std::size_t(3), pages[pageIndex].items.size() - 1);
                }
            }

            auto upIndex = pages[pageIndex].items[itemIndex].getComponent<cro::UIInput>().getSelectionIndex();
            pageHandles.prevButton.getComponent<cro::UIInput>().setPrevIndex(NextArrow, upIndex);
        }

        if (pageHandles.nextButton.isValid())
        {
            auto itemIndex = std::min(std::size_t(4), pages[pageIndex].items.size() - 1);
            itemIndex += ThumbColCount * 3;
            while (itemIndex >= pages[pageIndex].items.size())
            {
                if (itemIndex > ThumbColCount)
                {
                    itemIndex -= ThumbColCount;
                }
                else
                {
                    itemIndex = std::min(std::size_t(4), pages[pageIndex].items.size() - 1);
                }
            }

            auto upIndex = pages[pageIndex].items[itemIndex].getComponent<cro::UIInput>().getSelectionIndex();
            pageHandles.nextButton.getComponent<cro::UIInput>().setPrevIndex(PrevArrow, upIndex);
        }

        if (!forceRefresh)
        {
            m_uiScene.getSystem<cro::UISystem>()->selectAt(pages[pageIndex].items[0].getComponent<cro::UIInput>().getSelectionIndex());

            m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().setPlayingOffset(cro::seconds(0.f));
        }
    }
}

void ProfileState::refreshMugshot()
{
    if (!m_activeProfile.mugshot.empty())
    {
        auto& tex = m_sharedData.sharedResources->textures.get(m_activeProfile.mugshot);

        glm::vec2 texSize(tex.getSize());
        glm::vec2 scale = glm::vec2(96.f, 48.f) / texSize;
        m_menuEntities[EntityID::Mugshot].getComponent<cro::Transform>().setScale(scale);
        m_menuEntities[EntityID::Mugshot].getComponent<cro::Sprite>().setTexture(tex);
    }
    else
    {
        m_menuEntities[EntityID::Mugshot].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    }
}

void ProfileState::refreshNameString()
{
    m_menuEntities[EntityID::NameText].getComponent<cro::Text>().setString(m_activeProfile.name);
    centreText(m_menuEntities[EntityID::NameText]);
}

void ProfileState::refreshSwatch()
{
    static constexpr glm::vec2 SwatchSize = glm::vec2(20.f);

    std::vector<cro::Vertex2D> verts;
    for (auto i = 1u; i < SwatchPositions.size(); ++i)//skip hair
    {
        cro::Colour c = pc::Palette[m_activeProfile.avatarFlags[i]]; 
        auto pos = SwatchPositions[i];

        verts.emplace_back(glm::vec2(pos.x, pos.y + SwatchSize.y), c);
        verts.emplace_back(pos, c);
        verts.emplace_back(pos + SwatchSize, c);
        
        verts.emplace_back(pos + SwatchSize, c);
        verts.emplace_back(pos, c);
        verts.emplace_back(glm::vec2(pos.x + SwatchSize.x, pos.y), c);
    }
    m_menuEntities[EntityID::Swatch].getComponent<cro::Drawable2D>().setVertexData(verts);
}

void ProfileState::refreshBio()
{
    //look for bio file and load it if it exists
    auto path = Social::getUserContentPath(Social::UserContent::Profile);
    if (cro::FileSystem::directoryExists(path))
    {
        if (m_activeProfile.profileID.empty())
        {
            //this creates a new id
            m_activeProfile.saveProfile();
        }

        path += m_activeProfile.profileID + "/";

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

void ProfileState::beginTextEdit(cro::Entity stringEnt, cro::String* dst, std::size_t maxChars)
{
    *dst = dst->substr(0, maxChars);
    m_previousString = *dst;

    stringEnt.getComponent<cro::Text>().setFillColour(TextEditColour);
    m_textEdit.string = dst;
    m_textEdit.entity = stringEnt;
    m_textEdit.maxLen = maxChars;

    //block input to menu
    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);

    SDL_StartTextInput();
}

void ProfileState::handleTextEdit(const cro::Event& evt)
{
    if (!m_textEdit.string)
    {
        return;
    }

    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_BACKSPACE:
            if (!m_textEdit.string->empty())
            {
                m_textEdit.string->erase(m_textEdit.string->size() - 1);
            }
            break;
        case SDLK_v:
            if (evt.key.keysym.mod & KMOD_CTRL)
            {
                if (SDL_HasClipboardText())
                {
                    char* text = SDL_GetClipboardText();
                    auto codePoints = cro::Util::String::getCodepoints(text);
                    SDL_free(text);

                    cro::String str = cro::String::fromUtf32(codePoints.begin(), codePoints.end());
                    auto len = std::min(str.size(), ConstVal::MaxStringChars - m_textEdit.string->size());

                    *m_textEdit.string += str.substr(0, len);
                }
            }
            break;
        }
    }
    else if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_ESCAPE:
            cancelTextEdit();
            break;
        }
    }
    else if (evt.type == SDL_TEXTINPUT)
    {
        if (m_textEdit.string->size() < ConstVal::MaxStringChars
            && m_textEdit.string->size() < m_textEdit.maxLen)
        {
            auto codePoints = cro::Util::String::getCodepoints(evt.text.text);
            *m_textEdit.string += cro::String::fromUtf32(codePoints.begin(), codePoints.end());
        }
    }
}

void ProfileState::cancelTextEdit()
{
    *m_textEdit.string = m_previousString;
    applyTextEdit();

    //strictly speaking this should be whichever entity
    //that just cancelled editing - but m_textEdit is reset
    //after having applied the edit...
    centreText(m_menuEntities[EntityID::NameText]);

    m_previousString.clear();
}

bool ProfileState::applyTextEdit()
{
    if (m_textEdit.string && m_textEdit.entity.isValid())
    {
        if (m_textEdit.string->empty())
        {
            *m_textEdit.string = "INVALID";
        }

        m_textEdit.entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        m_textEdit.entity.getComponent<cro::Text>().setString(*m_textEdit.string);
        m_textEdit.entity.getComponent<cro::Callback>().active = false;

        //send this as a command to delay it by a frame - doesn't matter who receives it :)
        cro::Command cmd;
        cmd.targetFlags = CommandID::Menu::RootNode;
        cmd.action = [&](cro::Entity, float)
        {
            //commandception
            cro::Command cmd2;
            cmd2.targetFlags = CommandID::Menu::RootNode;
            cmd2.action = [&](cro::Entity, float)
            {
                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Main);
            };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd2);
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

        //SDL isn't really clear why we may want to call this, but
        //it appears it can break the text input of ImGui windows
        //SDL_StopTextInput();
        m_textEdit = {};
        return true;
    }
    m_textEdit = {};
    return false;
}

std::string ProfileState::generateRandomBio() const
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
            u8"Small feet means nothing - not when you can handle your wood like this.";
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

void ProfileState::setBioString(const std::string& s)
{
    if (s.empty())
    {
        return;
    }

    static constexpr std::size_t MaxWidth = 17;

    auto output = cro::Util::String::wordWrap(s, MaxWidth, MaxBioChars);

    m_menuEntities[EntityID::BioText].getComponent<cro::Transform>().setOrigin({ 0.f, -0.f });
    m_menuEntities[EntityID::BioText].getComponent<cro::Text>().setString(output);
    m_menuEntities[EntityID::BioText].getComponent<cro::Drawable2D>().setCroppingArea(BioCrop);
}

void ProfileState::generateMugshot()
{
    m_mugshotTexture.clear({0xa9c0afff});
    auto& cam = m_cameras[CameraID::Mugshot].getComponent<cro::Camera>();
    cam.viewport = { 0.f, 0.f, 0.5f, 1.f };
    m_cameras[CameraID::Mugshot].getComponent<cro::Transform>().setPosition(MugCameraPosition);
    m_cameras[CameraID::Mugshot].getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI);
    m_cameras[CameraID::Mugshot].getComponent<cro::Camera>().updateMatrices(m_cameras[CameraID::Mugshot].getComponent<cro::Transform>());
    m_modelScene.setActiveCamera(m_cameras[CameraID::Mugshot]);
    m_modelScene.render();

    cam.viewport = { 0.5f, 0.f, 0.5f, 1.f };
    m_cameras[CameraID::Mugshot].getComponent<cro::Transform>().setPosition(MugCameraPosition + glm::vec3(-MugCameraPosition.z /*+ 0.05f*/, 0.f, -MugCameraPosition.z));
    m_cameras[CameraID::Mugshot].getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI / 2.f);
    m_cameras[CameraID::Mugshot].getComponent<cro::Camera>().updateMatrices(m_cameras[CameraID::Mugshot].getComponent<cro::Transform>());
    m_modelScene.render();

    m_mugshotTexture.display();

    m_mugshotUpdated = true;
    m_menuEntities[EntityID::Mugshot].getComponent<cro::Sprite>().setTexture(m_mugshotTexture.getTexture());
    m_menuEntities[EntityID::Mugshot].getComponent<cro::Transform>().setScale(glm::vec2(0.5f));
}

void ProfileState::renderBallFrames()
{
    if (!cro::FileSystem::directoryExists("anim"))
    {
        cro::FileSystem::createDirectory("anim");
    }


    const auto oldPos = m_cameras[CameraID::Ball].getComponent<cro::Transform>().getPosition();
    m_cameras[CameraID::Ball].getComponent<cro::Transform>().setPosition({ oldPos.x, 0.026f, 0.058f });

    static constexpr float Angle = cro::Util::Const::PI / 16.f;
    const auto oldCam = m_modelScene.setActiveCamera(m_cameras[CameraID::Ball]);

    m_ballModels[m_ballIndex].ball.getComponent<cro::Callback>().active = false;

    for (auto i = 0; i < 31; ++i)
    {
        //set angle
        m_ballModels[m_ballIndex].ball.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, Angle * i);

        //update scene
        m_modelScene.simulate(0.f);

        //render scene
        m_ballTexture.clear();
        m_modelScene.render();
        m_ballTexture.display();

        //save image
        std::string n = "anim/anim" + std::to_string(m_ballIndex) + "_";
        if (i < 10) n += "0";
        n += std::to_string(i) + ".png";
        m_ballTexture.getTexture().saveToFile(n);
    }


    m_ballModels[m_ballIndex].ball.getComponent<cro::Callback>().active = true;
    m_modelScene.setActiveCamera(oldCam);
    m_cameras[CameraID::Ball].getComponent<cro::Transform>().setPosition(oldPos);
}