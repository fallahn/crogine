/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
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
#include "ProfileEnum.inl"
#include "../GolfGame.hpp"
#include "../Colordome-32.hpp"

#include <crogine/core/Window.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/core/Mouse.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
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

            Hair, Skin, TopL, TopD,
            BottomL, BottomD,

            BallThumb
        };
    };
    
    constexpr glm::uvec2 BallTexSize(96u, 110u);
    constexpr glm::uvec2 AvatarTexSize(130u, 202u);
    constexpr glm::uvec2 MugshotTexSize(192u, 96u);

    constexpr std::size_t BallColCount = 6;
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
        glm::vec2(37.f, 171.f),
        glm::vec2(37.f, 139.f),
        glm::vec2(21.f, 107.f),
        glm::vec2(53.f, 107.f),
        glm::vec2(21.f, 74.f),
        glm::vec2(53.f, 74.f)
    };
}

ProfileState::ProfileState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd, SharedProfileData& sp)
    : cro::State        (ss, ctx),
    m_uiScene           (ctx.appInstance.getMessageBus(), 384u),
    m_modelScene        (ctx.appInstance.getMessageBus(), 1024), //just because someone might be daft enough to install ALL the workshop items
    m_sharedData        (sd),
    m_profileData       (sp),
    m_viewScale         (2.f),
    m_ballIndex         (0),
    m_ballHairIndex     (0),
    m_avatarIndex       (0),
    m_lastSelected      (0),
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
    //            if (m_ballThumbs.available())
    //            {
    //                glm::vec2 size(m_ballThumbs.getSize());
    //                ImGui::Image(m_ballThumbs.getTexture(), { size.x, size.y }, { 0.f, 1.f }, { 1.f, 0.f });
    //            }
    //            
    //            /*if (m_mugshotTexture.available())
    //            {
    //                ImGui::Image(m_mugshotTexture.getTexture(), { 192.f, 96.f }, { 0.f, 1.f }, { 1.f, 0.f });
    //            }*/

    //            /*auto pos = m_cameras[CameraID::Mugshot].getComponent<cro::Transform>().getPosition();
    //            if (ImGui::SliderFloat("Height", &pos.y, 0.f, 2.f))
    //            {
    //                m_cameras[CameraID::Mugshot].getComponent<cro::Transform>().setPosition(pos);
    //            }
    //            if (ImGui::SliderFloat("Depth", &pos.z, -2.f, 4.f))
    //            {
    //                m_cameras[CameraID::Mugshot].getComponent<cro::Transform>().setPosition(pos);
    //            }*/
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
            if (cro::GameController::hasPSLayout(controllerID))
            {
                m_menuEntities[EntityID::HelpText].getComponent<cro::Text>().setString(PSString);
            }
            else
            {
                m_menuEntities[EntityID::HelpText].getComponent<cro::Text>().setString(XboxString);
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
        }
        centreText(m_menuEntities[EntityID::HelpText]);
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
            if (m_textEdit.string == nullptr
                && m_uiScene.getSystem<cro::UISystem>()->getActiveGroup() == MenuID::Main)
            {
                quitState();
                return false;
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
        if (evt.cbutton.button == cro::GameController::ButtonB
            && m_uiScene.getSystem<cro::UISystem>()->getActiveGroup() == MenuID::Main)
        {
            quitState();
            return false;
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
                m_flyouts[flyoutID].background.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Main);
                m_uiScene.getSystem<cro::UISystem>()->setColumnCount(1);
                m_uiScene.getSystem<cro::UISystem>()->selectAt(m_lastSelected);

                //restore model preview colours
                m_profileTextures[m_avatarIndex].setColour(pc::ColourKey::Index(flyoutID), m_activeProfile.avatarFlags[flyoutID]);
                m_profileTextures[m_avatarIndex].apply();

                //refresh hair colour
                if (m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].isValid())
                {
                    m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", pc::Palette[m_activeProfile.avatarFlags[0]]);
                }

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
            m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, static_cast<float>(evt.motion.xrel) * (1.f/60.f));
        }
    }
    else if (evt.type == SDL_MOUSEWHEEL)
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
        float rotation = 0.f;

        if (cro::GameController::isButtonPressed(0, cro::GameController::ButtonLeftShoulder)
            || cro::Keyboard::isKeyPressed(m_sharedData.inputBinding.keys[InputBinding::Left]))
        {
            rotation -= dt;
        }
        if (cro::GameController::isButtonPressed(0, cro::GameController::ButtonRightShoulder)
            || cro::Keyboard::isKeyPressed(m_sharedData.inputBinding.keys[InputBinding::Right]))
        {
            rotation += dt;
        }
        m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, rotation);


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
    m_ballTexture.clear(cro::Colour::Transparent);
    m_modelScene.setActiveCamera(m_cameras[CameraID::Ball]);
    m_modelScene.render();
    m_ballTexture.display();

    m_avatarTexture.clear(cro::Colour::Transparent);
    m_modelScene.setActiveCamera(m_cameras[CameraID::Avatar]);
    m_modelScene.render();
    m_avatarTexture.display();

    m_uiScene.render();

}

//private
void ProfileState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_uiScene.addSystem<cro::UISystem>(mb)->setMouseScrollNavigationEnabled(false);
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
                if (m_activeProfile.profileID != m_profileData.playerProfiles[m_profileData.activeProfileIndex].profileID)
                {
                    m_activeProfile = m_profileData.playerProfiles[m_profileData.activeProfileIndex];

                    //refresh the avatar settings
                    setAvatarIndex(indexFromAvatarID(m_activeProfile.skinID));
                    setHairIndex(indexFromHairID(m_activeProfile.hairID));
                    setBallIndex(indexFromBallID(m_activeProfile.ballID) % m_ballModels.size());
                    refreshMugshot();
                    refreshNameString();
                    refreshSwatch();
                }
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

    auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);

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
        });
    auto unselected = uiSystem.addCallback([](cro::Entity e) {e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent); });

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

    entity.getComponent<cro::UIInput>().setNextIndex(ButtonName, ButtonHairColour);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonName, ButtonRandomise);
#endif

    //colour swatch - this has its own update function
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_menuEntities[EntityID::Swatch] = entity;
    refreshSwatch();

    auto fetchUIIndexFromColour =
        [&](std::uint8_t colourIndex, std::int32_t paletteIndex) 
    {
        const auto rowCount = m_uiScene.getSystem<cro::UISystem>()->getGroupSize() / PaletteColumnCount;
        const auto x = colourIndex % PaletteColumnCount;
        auto y = colourIndex / PaletteColumnCount;

        y = (rowCount - 1) - y;
        return (y * PaletteColumnCount + x) % pc::PairCounts[paletteIndex];
    };

    //colour buttons
    auto hairColour = createButton("colour_highlight", glm::vec2(33.f, 167.f), ButtonHairColour);
    hairColour.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&, fetchUIIndexFromColour](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    //show palette menu
                    m_flyouts[PaletteID::Hair].background.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                    //set column/row count for menu
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Hair);
                    m_uiScene.getSystem<cro::UISystem>()->setColumnCount(PaletteColumnCount);
                    m_uiScene.getSystem<cro::UISystem>()->selectAt(fetchUIIndexFromColour(m_activeProfile.avatarFlags[0], pc::ColourKey::Hair));

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    m_lastSelected = e.getComponent<cro::UIInput>().getSelectionIndex();
                }
            });
    hairColour.getComponent<cro::UIInput>().setNextIndex(ButtonPrevHair, ButtonSkinTone);
#ifdef USE_GNS
    hairColour.getComponent<cro::UIInput>().setPrevIndex(ButtonDescUp, ButtonWorkshop);
#else
    hairColour.getComponent<cro::UIInput>().setPrevIndex(ButtonDescUp, ButtonRandomise);
#endif

    auto skinColour = createButton("colour_highlight", glm::vec2(33.f, 135.f), ButtonSkinTone);
    skinColour.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&, fetchUIIndexFromColour](cro::Entity e, const cro::ButtonEvent& evt)
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
    skinColour.getComponent<cro::UIInput>().setNextIndex(ButtonPrevBall, ButtonTopLight);
    skinColour.getComponent<cro::UIInput>().setPrevIndex(ButtonNextBall, ButtonHairColour);


    auto topLightColour = createButton("colour_highlight", glm::vec2(17.f, 103.f), ButtonTopLight);
    topLightColour.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&, fetchUIIndexFromColour](cro::Entity e, const cro::ButtonEvent& evt)
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
        uiSystem.addCallback([&, fetchUIIndexFromColour](cro::Entity e, const cro::ButtonEvent& evt)
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


    auto bottomLightColour = createButton("colour_highlight", glm::vec2(17.f, 69.f), ButtonBottomLight);
    bottomLightColour.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&, fetchUIIndexFromColour](cro::Entity e, const cro::ButtonEvent& evt)
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

    auto bottomDarkColour = createButton("colour_highlight", glm::vec2(49.f, 69.f), ButtonBottomDark);
    bottomDarkColour.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&, fetchUIIndexFromColour](cro::Entity e, const cro::ButtonEvent& evt)
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


    //avatar arrow buttons
    auto hairLeft = createButton("arrow_left", glm::vec2(87.f, 156.f), ButtonPrevHair);
    hairLeft.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    auto hairIndex = (m_avatarModels[m_avatarIndex].hairIndex + (m_avatarHairModels.size() - 1)) % m_avatarHairModels.size();
                    setHairIndex(hairIndex);

                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    hairLeft.getComponent<cro::UIInput>().setNextIndex(ButtonNextHair, ButtonPrevBody);
    hairLeft.getComponent<cro::UIInput>().setPrevIndex(ButtonHairColour, ButtonPrevBody);
    
    auto hairRight = createButton("arrow_right", glm::vec2(234.f, 156.f), ButtonNextHair);
    hairRight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    auto hairIndex = (m_avatarModels[m_avatarIndex].hairIndex + 1) % m_avatarHairModels.size();
                    setHairIndex(hairIndex);
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    hairRight.getComponent<cro::UIInput>().setNextIndex(ButtonPrevBall, ButtonNextBody);
    hairRight.getComponent<cro::UIInput>().setPrevIndex(ButtonPrevHair, ButtonNextBody);
    
    auto avatarLeft = createButton("arrow_left", glm::vec2(87.f, 110.f), ButtonPrevBody);
    avatarLeft.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    setAvatarIndex((m_avatarIndex + (m_avatarModels.size() - 1)) % m_avatarModels.size());
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    avatarLeft.getComponent<cro::UIInput>().setNextIndex(ButtonNextBody, ButtonPrevHair);
    avatarLeft.getComponent<cro::UIInput>().setPrevIndex(ButtonTopDark, ButtonPrevHair);
    
    auto avatarRight = createButton("arrow_right", glm::vec2(234.f, 110.f), ButtonNextBody);
    avatarRight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    setAvatarIndex((m_avatarIndex + 1) % m_avatarModels.size());
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    avatarRight.getComponent<cro::UIInput>().setNextIndex(ButtonPrevBall, ButtonNextHair);
    avatarRight.getComponent<cro::UIInput>().setPrevIndex(ButtonPrevBody, ButtonNextHair);


    //checkbox
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
                    setHairIndex(cro::Util::Random::value(0u, m_sharedData.hairInfo.size() - 1));

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
                    }

                    //update swatch
                    refreshSwatch();
                }
            });
#ifdef USE_GNS
    randomButton.getComponent<cro::UIInput>().setNextIndex(ButtonCancel, ButtonWorkshop);
#else
    randomButton.getComponent<cro::UIInput>().setNextIndex(ButtonCancel, ButtonHairColour);
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
                if (!m_activeProfile.isSteamID &&
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
                            auto* msg = postMessage<SystemEvent>(cl::MessageID::SystemMessage);
                            msg->type = SystemEvent::RequestOSK;
                            msg->data = 0;
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
    nameButton.getComponent<cro::UIInput>().setPrevIndex(ButtonWorkshop, ButtonCancel);
#else
    nameButton.getComponent<cro::UIInput>().setNextIndex(ButtonDescUp, ButtonBallSelect);
    nameButton.getComponent<cro::UIInput>().setPrevIndex(ButtonNextHair, ButtonBallSelect);
#endif

    //ball arrow buttons
    auto ballLeft = createButton("arrow_left", glm::vec2(267.f, 134.f), ButtonPrevBall);
    ballLeft.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    setBallIndex((m_ballIndex + (m_ballModels.size() - 1)) % m_ballModels.size());
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    ballLeft.getComponent<cro::UIInput>().setNextIndex(ButtonBallSelect, ButtonUpdateIcon);
    ballLeft.getComponent<cro::UIInput>().setPrevIndex(ButtonNextHair, ButtonName);


    //toggles the preview to display ball thumbs
    auto ballThumb = createButton("ballthumb_highlight", { 279.f , 83.f }, ButtonBallSelect);
    ballThumb.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    m_flyouts[PaletteID::BallThumb].background.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::BallThumb);
                    m_uiScene.getSystem<cro::UISystem>()->setColumnCount(BallColCount);
                    m_uiScene.getSystem<cro::UISystem>()->selectAt(m_ballIndex + 1);

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    m_lastSelected = e.getComponent<cro::UIInput>().getSelectionIndex();
                }
            });
    ballThumb.getComponent<cro::UIInput>().setNextIndex(ButtonNextBall, ButtonUpdateIcon);
    ballThumb.getComponent<cro::UIInput>().setPrevIndex(ButtonPrevBall, ButtonName);

    auto ballRight = createButton("arrow_right", glm::vec2(382.f, 134.f), ButtonNextBall);
    ballRight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    setBallIndex((m_ballIndex + 1) % m_ballModels.size());
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    ballRight.getComponent<cro::UIInput>().setNextIndex(ButtonDescUp, ButtonUpdateIcon);
    ballRight.getComponent<cro::UIInput>().setPrevIndex(ButtonBallSelect, ButtonName);

    //updates the profile icon
    auto profileIcon = createButton("button_highlight", glm::vec2(269.f, 55.f), ButtonUpdateIcon);
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
    profileIcon.getComponent<cro::UIInput>().setPrevIndex(ButtonSouthPaw, ButtonBallSelect);

    //save/quit buttons
    auto saveQuit = createButton("button_highlight", glm::vec2(269.f, 38.f), ButtonSaveClose);
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

                    quitState();
                }
            });
    saveQuit.getComponent<cro::UIInput>().setNextIndex(ButtonSouthPaw, ButtonCancel);
    saveQuit.getComponent<cro::UIInput>().setPrevIndex(ButtonSouthPaw, ButtonUpdateIcon);


    auto quit = createButton("button_highlight", glm::vec2(269.f, 21.f), ButtonCancel);
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
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 98.f, 27.f, 0.1f });
    if (!m_sharedData.pixelScale)
    {
        entity.getComponent<cro::Transform>().setScale(glm::vec2(1.f / getViewScale()));
    }
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_avatarTexture.getTexture());
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_menuEntities[EntityID::AvatarPreview] = entity;
    addCorners(bgEnt, entity);

    //ball preview
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 279.f, 83.f, 0.1f });
    if (!m_sharedData.pixelScale)
    {
        entity.getComponent<cro::Transform>().setScale(glm::vec2(1.f / getViewScale()));
    }
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_ballTexture.getTexture());
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    addCorners(bgEnt, entity);


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

    entity = createButton("scroll_up", { 441.f, 192.f }, ButtonDescUp);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_menuEntities[EntityID::BioText].getComponent<cro::Callback>().getUserData<std::int32_t>()--;
                    m_menuEntities[EntityID::BioText].getComponent<cro::Callback>().active = true;
                }
            });
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonHairColour, ButtonDescDown);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonNextBall, ButtonName);

    entity = createButton("scroll_down", { 441.f, 81.f }, ButtonDescDown);
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

    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

    //bio string
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 399.f, 190.f, 0.1f });
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
    entity.addComponent<cro::Transform>().setPosition({bounds.width / 2.f, 14.f, 0.1f});
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_menuEntities[EntityID::HelpText] = entity;


    createPalettes(bgEnt);
    createBallFlyout(bgEnt);

    auto updateView = [&, rootNode](cro::Camera& cam) mutable
    {
        glm::vec2 size(GolfGame::getActiveTarget()->getSize());

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
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
    std::int32_t i = 0;
    static constexpr glm::vec3 BallPos({ 10.f, 0.f, 0.f });
    for (auto& ballDef : m_profileData.ballDefs)
    {
        auto entity = m_modelScene.createEntity();
        entity.addComponent<cro::Transform>();
        ballDef.createModel(entity);
        entity.getComponent<cro::Model>().setHidden(true);
        entity.getComponent<cro::Model>().setMaterial(0, m_profileData.profileMaterials.ball);
        entity.getComponent<cro::Model>().setMaterial(1, m_profileData.profileMaterials.ballReflection);
        entity.addComponent<cro::Callback>().active = true;

        if (m_sharedData.ballInfo[i].rollAnimation)
        {
            entity.getComponent<cro::Callback>().function =
                [](cro::Entity e, float dt)
            {
                e.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -dt * 6.f);
            };
            entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, -cro::Util::Const::PI * 0.85f);
            entity.getComponent<cro::Transform>().move({ 0.f, Ball::Radius, 0.f });
            entity.getComponent<cro::Transform>().setOrigin({ 0.f, Ball::Radius, 0.f });
        }
        else
        {
            entity.getComponent<cro::Callback>().function =
                [](cro::Entity e, float dt)
            {
                e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt);
            };
        }
        ++i;

        auto& preview = m_ballModels.emplace_back();
        preview.ball = entity;
        preview.root = m_modelScene.createEntity();
        preview.root.addComponent<cro::Transform>().setPosition(BallPos);
        preview.root.getComponent<cro::Transform>().addChild(preview.ball.getComponent<cro::Transform>());
        preview.root.addComponent<cro::ParticleEmitter>().settings = emitterSettings;
    }
    
    auto entity = m_modelScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(BallPos + glm::vec3(0.f, -0.01f, 0.f));
    m_profileData.shadowDef->createModel(entity);

    entity = m_modelScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(BallPos + glm::vec3(0.f, -0.15f, 0.f));
    entity.getComponent<cro::Transform>().setScale(glm::vec3(5.f));
    m_profileData.grassDef->createModel(entity);

    static constexpr glm::vec3 AvatarPos({ CameraBasePosition.x, 0.f, 0.f });
    for (auto& avatar : m_profileData.avatarDefs)
    {
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

        auto& avt = m_avatarModels.emplace_back();
        avt.previewModel = entity;

        //these are unique models from the menu so we'll 
        //need to capture their attachment points once again...
        if (entity.hasComponent<cro::Skeleton>())
        {
            //this should never not be true as the models were validated
            //in the menu state - but 
            auto id = entity.getComponent<cro::Skeleton>().getAttachmentIndex("head");
            if (id > -1)
            {
                //hair is optional so OK if this doesn't exist
                avt.hairAttachment = &entity.getComponent<cro::Skeleton>().getAttachments()[id];
            }

            entity.getComponent<cro::Skeleton>().play(entity.getComponent<cro::Skeleton>().getAnimationIndex("idle_standing"));
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
        auto& t = m_profileTextures.emplace_back(m_sharedData.avatarInfo[i].texturePath);

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
                    auto entity = m_uiScene.createEntity();
                    entity.addComponent<cro::Transform>();
                    entity.addComponent<cro::AudioEmitter>() = as.getEmitter(name);
                    entity.getComponent<cro::AudioEmitter>().setLooped(false);
                    m_avatarModels[i].previewAudio.push_back(entity);
                }
            }
        }
    }

    //empty at front for 'bald'
    m_avatarHairModels.push_back({});
    m_ballHairModels.push_back({});
    for (auto& hair : m_profileData.hairDefs)
    {
        auto entity = m_modelScene.createEntity();
        entity.addComponent<cro::Transform>();
        hair.createModel(entity);
        entity.getComponent<cro::Model>().setHidden(true);
        entity.getComponent<cro::Model>().setMaterial(0, m_profileData.profileMaterials.hair);
        m_avatarHairModels.push_back(entity);

        entity = m_modelScene.createEntity();
        entity.addComponent<cro::Transform>().setScale(BallHairScale);
        entity.getComponent<cro::Transform>().setOrigin(BallHairOffset);
        hair.createModel(entity);
        entity.getComponent<cro::Model>().setMaterial(0, m_profileData.profileMaterials.hair);
        entity.getComponent<cro::Model>().setHidden(true);
        m_ballHairModels.push_back(entity);
    }

    m_avatarIndex = indexFromAvatarID(m_activeProfile.skinID);
    m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Model>().setHidden(false);
    m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Callback>().active = true;

    m_ballIndex = indexFromBallID(m_activeProfile.ballID);
    m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setHidden(false);

    setHairIndex(indexFromHairID(m_activeProfile.hairID));

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

    createBallThumbs();

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


    m_modelScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 96.f * cro::Util::Const::degToRad);
    m_modelScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -39.f * cro::Util::Const::degToRad);
}

void ProfileState::createPalettes(cro::Entity parent)
{
    //8x5
    static constexpr glm::vec2 IconSize(8.f);
    static constexpr float IconPadding = 4.f;

    static constexpr float BgWidth = (PaletteColumnCount * (IconSize.x + IconPadding)) + IconPadding;

    for (auto i = 0u; i < PaletteID::BallThumb; ++i)
    {
        const auto RowCount = (pc::PairCounts[i] / PaletteColumnCount);
        const float BgHeight = (RowCount * IconSize.y) + (RowCount * IconPadding) + IconPadding;

        //background
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(SwatchPositions[i], 0.5f));
        entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);

        auto verts = createMenuBackground({ BgWidth, BgHeight });
        
        for (auto j = 0u; j < pc::PairCounts[i]; ++j)
        {
            std::size_t x = j % PaletteColumnCount;
            std::size_t y = j / PaletteColumnCount;
            auto colour = pc::Palette[j];

            glm::vec2 pos = { x * (IconSize.x + IconPadding), y * (IconSize.y + IconPadding) };
            pos += IconPadding;

            verts.emplace_back(glm::vec2(pos.x, pos.y + IconSize.y), colour);
            verts.emplace_back(pos, colour);
            verts.emplace_back(pos + IconSize, colour);

            verts.emplace_back(pos + IconSize, colour);
            verts.emplace_back(pos, colour);
            verts.emplace_back(glm::vec2(pos.x + IconSize.x, pos.y), colour);
        }
        entity.getComponent<cro::Drawable2D>().setVertexData(verts);

        m_flyouts[i].background = entity;
        parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


        //highlight
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ IconPadding, IconPadding, 0.1f });
        entity.addComponent<cro::Drawable2D>().setVertexData(createMenuHighlight(IconSize, TextGoldColour));
        entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);

        m_flyouts[i].highlight = entity;
        m_flyouts[i].background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        //buttons/menu items
        auto menuID = i + MenuID::Hair;
        std::size_t IndexOffset = (i * 100) + 200;

        m_flyouts[i].activateCallback = m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, i](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                auto quitMenu = [&]()
                {
                    m_flyouts[i].background.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Main);
                    m_uiScene.getSystem<cro::UISystem>()->setColumnCount(1);
                    m_uiScene.getSystem<cro::UISystem>()->selectAt(m_lastSelected);

                    //update texture
                    m_profileTextures[m_avatarIndex].setColour(pc::ColourKey::Index(i), m_activeProfile.avatarFlags[i]);
                    m_profileTextures[m_avatarIndex].apply();

                    //refresh hair colour
                    if (m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].isValid())
                    {
                        m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", pc::Palette[m_activeProfile.avatarFlags[0]]);
                    }
                };

                if (activated(evt))
                {
                    m_activeProfile.avatarFlags[i] = e.getComponent<cro::Callback>().getUserData<std::uint8_t>();
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
                if (i == PaletteID::Hair)
                {
                    if (m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].isValid())
                    {
                        m_avatarHairModels[m_avatarModels[m_avatarIndex].hairIndex].getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", pc::Palette[e.getComponent<cro::Callback>().getUserData<std::uint8_t>()]);
                    }
                }

                m_profileTextures[m_avatarIndex].setColour(pc::ColourKey::Index(i), e.getComponent<cro::Callback>().getUserData<std::uint8_t>());
                m_profileTextures[m_avatarIndex].apply();

                entity.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition());
                m_audioEnts[AudioID::Select].getComponent<cro::AudioEmitter>().play();
                m_audioEnts[AudioID::Select].getComponent<cro::AudioEmitter>().setPlayingOffset(cro::Time());
            });

        for (auto j = 0u; j < pc::PairCounts[i]; ++j)
        {
            //the Y order is reversed so that the navigation
            //direction of keys/controller is correct in the grid
            std::size_t x = j % PaletteColumnCount;
            std::size_t y = (RowCount - 1) - (j / PaletteColumnCount);
            //auto colour = pc::Palette[j];

            glm::vec2 pos = { x * (IconSize.x + IconPadding), y * (IconSize.y + IconPadding) };
            pos += IconPadding;

            auto inputIndex = (((RowCount - y) - 1) * PaletteColumnCount) + x;

            entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(glm::vec3(pos, 0.1f));
            entity.addComponent<cro::UIInput>().setGroup(menuID);
            entity.getComponent<cro::UIInput>().area = { glm::vec2(0.f), IconSize };
            entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_flyouts[i].selectCallback;
            entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_flyouts[i].activateCallback;


            entity.getComponent<cro::UIInput>().setSelectionIndex(IndexOffset + inputIndex);
            if (x == 0)
            {
                auto prevIndex = std::min(inputIndex + (PaletteColumnCount - 1), pc::PairCounts[i] - 1);
                entity.getComponent<cro::UIInput>().setPrevIndex(IndexOffset + prevIndex);
            }
            else if (x == (PaletteColumnCount - 1))
            {
                entity.getComponent<cro::UIInput>().setNextIndex(IndexOffset + (inputIndex - (PaletteColumnCount - 1)));
            }
            else if (y == 0
                && x == (pc::PairCounts[i] % PaletteColumnCount) - 1)
            {
                auto nextIndex = inputIndex - ((pc::PairCounts[i] % PaletteColumnCount) - 1);
                entity.getComponent<cro::UIInput>().setNextIndex(IndexOffset + nextIndex);
            }


            entity.addComponent<cro::Callback>().setUserData<std::uint8_t>(static_cast<std::uint8_t>((y * PaletteColumnCount) + x));

            m_flyouts[i].background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        }
    }
}

void ProfileState::createBallFlyout(cro::Entity parent)
{
    static constexpr glm::vec2 IconSize(BallThumbSize);
    static constexpr float IconPadding = 4.f;

    static constexpr float BgWidth = (BallColCount * (IconSize.x + IconPadding)) + IconPadding;

    const auto RowCount = (m_ballModels.size() / BallColCount) + 1;
    const float BgHeight = (RowCount * IconSize.y) + (RowCount * IconPadding) + IconPadding;

    //background
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(200.f, 46.f, 0.5f));
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);

    auto verts = createMenuBackground({ BgWidth, BgHeight });

    entity.getComponent<cro::Drawable2D>().setVertexData(verts);

    m_flyouts[PaletteID::BallThumb].background = entity;
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    verts.clear();
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.1f });
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    entity.getComponent<cro::Drawable2D>().setTexture(&m_ballThumbs.getTexture());

    const glm::vec2 TexSize(m_ballThumbs.getSize());
    cro::FloatRect textureBounds = { 0.f, 0.f, static_cast<float>(BallThumbSize.x) / TexSize.x, static_cast<float>(BallThumbSize.y) / TexSize.y };

    for (auto j = 0u; j < m_ballModels.size(); ++j)
    {
        std::size_t x = j % BallColCount;
        std::size_t y = j / BallColCount;
        textureBounds.left = x * textureBounds.width;
        textureBounds.bottom = y * textureBounds.height;

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
    m_flyouts[PaletteID::BallThumb].background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //highlight
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ IconPadding, IconPadding, 0.1f });
    entity.addComponent<cro::Drawable2D>().setVertexData(createMenuHighlight(IconSize, TextGoldColour));
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);

    m_flyouts[PaletteID::BallThumb].highlight = entity;
    m_flyouts[PaletteID::BallThumb].background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //buttons
    auto menuID = MenuID::BallThumb;

    m_flyouts[PaletteID::BallThumb].activateCallback = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            auto quitMenu = [&]()
            {
                m_flyouts[PaletteID::BallThumb].background.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Main);
                m_uiScene.getSystem<cro::UISystem>()->setColumnCount(1);
                m_uiScene.getSystem<cro::UISystem>()->selectAt(m_lastSelected);
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
    m_flyouts[PaletteID::BallThumb].selectCallback = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&, entity](cro::Entity e) mutable
        {
            //test if we intersect the edge of the screen and scroll the flyout vertically
            auto point = e.getComponent<cro::Transform>().getWorldPosition();
            auto screenPos = m_uiScene.getActiveCamera().getComponent<cro::Camera>().coordsToPixel(point);
            while (screenPos.y < 0)
            {
                m_flyouts[PaletteID::BallThumb].background.getComponent<cro::Transform>().move({0.f, (IconSize.y + IconPadding)});

                point = e.getComponent<cro::Transform>().getWorldPosition();
                screenPos = m_uiScene.getActiveCamera().getComponent<cro::Camera>().coordsToPixel(point);
            }

            point = e.getComponent<cro::Transform>().getWorldPosition();
            point.y += IconSize.y;
            screenPos = m_uiScene.getActiveCamera().getComponent<cro::Camera>().coordsToPixel(point);
                
            while (screenPos.y > cro::App::getWindow().getSize().y)
            {
                m_flyouts[PaletteID::BallThumb].background.getComponent<cro::Transform>().move({ 0.f, -(IconSize.y + IconPadding) });
                    
                point = e.getComponent<cro::Transform>().getWorldPosition();
                point.y += IconSize.y;
                screenPos = m_uiScene.getActiveCamera().getComponent<cro::Camera>().coordsToPixel(point);
            }

            entity.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition());
            m_audioEnts[AudioID::Select].getComponent<cro::AudioEmitter>().play();
            m_audioEnts[AudioID::Select].getComponent<cro::AudioEmitter>().setPlayingOffset(cro::Time());
        });

    constexpr std::size_t IndexOffset = 100;
    for (auto j = 0u; j < m_ballModels.size(); ++j)
    {
        //the Y order is reversed so that the navigation
        //direction of keys/controller is correct in the grid
        std::size_t x = j % BallColCount;
        std::size_t y = (RowCount - 1) - (j / BallColCount);

        glm::vec2 pos = { x * (IconSize.x + IconPadding), y * (IconSize.y + IconPadding) };
        pos += IconPadding;

        auto inputIndex = (((RowCount - y) - 1) * BallColCount) + x;

        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(pos, 0.1f));
        entity.addComponent<cro::UIInput>().setGroup(menuID);
        entity.getComponent<cro::UIInput>().area = { glm::vec2(0.f), IconSize };
        entity.getComponent<cro::UIInput>().setSelectionIndex(IndexOffset + inputIndex);
        if (x == 0)
        {
            auto prevIndex = std::min(inputIndex + (BallColCount - 1), m_ballModels.size() - 1);
            entity.getComponent<cro::UIInput>().setPrevIndex(IndexOffset + prevIndex);
        }
        else if (x == (BallColCount - 1))
        {
            entity.getComponent<cro::UIInput>().setNextIndex(IndexOffset + (inputIndex - (BallColCount - 1)));
        }
        else if (y == 0 
            && x == (m_ballModels.size() % BallColCount) - 1)
        {
            auto nextIndex = inputIndex - ((m_ballModels.size() % BallColCount) - 1);
            entity.getComponent<cro::UIInput>().setNextIndex(IndexOffset + nextIndex);
        }
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_flyouts[PaletteID::BallThumb].selectCallback;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_flyouts[PaletteID::BallThumb].activateCallback;
        entity.addComponent<cro::Callback>().setUserData<std::uint8_t>(static_cast<std::uint8_t>(inputIndex));

        m_flyouts[PaletteID::BallThumb].background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }

    //pad out missing cols to make column count work
    const auto PadCount = (BallColCount - (m_ballModels.size() % BallColCount));
    for (auto j = 0u; j < PadCount; ++j)
    {
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>();
        entity.addComponent<cro::UIInput>().setGroup(menuID);
        entity.getComponent<cro::UIInput>().enabled = false;

        m_flyouts[PaletteID::BallThumb].background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }
}

void ProfileState::createBallThumbs()
{
    //I don't even.. wtf
    const glm::uvec2 TexSize(std::min(BallColCount, m_ballModels.size()) * BallThumbSize.x, ((m_ballModels.size() / BallColCount) + 1) * BallThumbSize.y);

    cro::FloatRect vp = { 0.f, 0.f, static_cast<float>(BallThumbSize.x) / TexSize.x, static_cast<float>(BallThumbSize.y) / TexSize.y };
    auto& cam = m_cameras[CameraID::Ball].getComponent<cro::Camera>();

    auto oldIndex = m_ballIndex;
    
    const auto showBall = [&](std::size_t idx)
    {
        m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setHidden(true);
        m_ballIndex = idx;
        m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setHidden(false);
    };
    
    m_modelScene.setActiveCamera(m_cameras[CameraID::Ball]);
    m_modelScene.simulate(0.f);

    m_ballThumbs.create(TexSize.x, TexSize.y);
    m_ballThumbs.clear(CD32::Colours[CD32::BlueLight]);

    for (auto i = 0u; i < m_ballModels.size(); ++i)
    {
        vp.left = (i % BallColCount) * vp.width;
        vp.bottom = (i / BallColCount) * vp.height;
        cam.viewport = vp;

        showBall(i);
        m_modelScene.simulate(0.f);
        m_modelScene.render();
    }

    m_ballThumbs.display();

    cam.viewport = { 0.f, 0.f , 1.f, 1.f };
    showBall(oldIndex);
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
    m_uiScene.setSystemActive<cro::AudioPlayerSystem>(false);
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
    if (m_avatarModels[m_avatarIndex].hairAttachment)
    {
        m_avatarModels[m_avatarIndex].hairAttachment->setModel({});
    }

    m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Callback>().active = true;
    m_avatarModels[m_avatarIndex].previewModel.getComponent<cro::Callback>().getUserData<AvatarAnimCallbackData>().direction = 1;

    m_avatarIndex = idx;

    if (m_avatarModels[m_avatarIndex].hairAttachment)
    {
        m_avatarModels[m_avatarIndex].hairAttachment->setModel(m_avatarHairModels[hairIdx]);
        m_avatarModels[m_avatarIndex].hairIndex = hairIdx;
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
        m_avatarHairModels[hairIndex].getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", pc::Palette[m_activeProfile.avatarFlags[0]]);
    }
    m_avatarModels[m_avatarIndex].hairIndex = hairIndex;

    m_activeProfile.hairID = m_sharedData.hairInfo[hairIndex].uid;
}

void ProfileState::setBallIndex(std::size_t idx)
{
    CRO_ASSERT(idx < m_ballModels.size(), "");

    if (m_ballHairModels[m_ballHairIndex].isValid())
    {
        m_ballModels[m_ballIndex].root.getComponent<cro::Transform>().removeChild(m_ballHairModels[m_ballHairIndex].getComponent<cro::Transform>());
    }
    m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setHidden(true);

    m_ballIndex = idx;

    if (m_ballHairModels[m_ballHairIndex].isValid())
    {
        m_ballModels[m_ballIndex].root.getComponent<cro::Transform>().addChild(m_ballHairModels[m_ballHairIndex].getComponent<cro::Transform>());
    }
    m_ballModels[m_ballIndex].ball.getComponent<cro::Model>().setHidden(false);

    m_ballModels[m_ballIndex].root.getComponent<cro::ParticleEmitter>().start();

    m_activeProfile.ballID = m_sharedData.ballInfo[m_ballIndex].uid;
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
    for (auto i = 0u; i < SwatchPositions.size(); ++i)
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


        SDL_StopTextInput();
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
