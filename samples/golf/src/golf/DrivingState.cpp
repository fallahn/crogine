/*-----------------------------------------------------------------------

Matt Marchant 2021
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include "DrivingState.hpp"
#include "SharedStateData.hpp"
#include "GameConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "FpsCameraSystem.hpp"
#include "../GolfGame.hpp"

#include <crogine/gui/Gui.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/AudioListener.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/BillboardSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/SpriteAnimator.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/AudioSystem.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>

#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
#include "TerrainShader.inl"

    bool useFreeCam = false;
    constexpr glm::vec3 PlayerPosition(0.f, 0.f, 123.f);
}

DrivingState::DrivingState(cro::StateStack& stack, cro::State::Context context, SharedStateData& sd)
    : cro::State    (stack, context),
    m_sharedData    (sd),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus()),
    m_viewScale     (1.f)
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    });

#ifdef CRO_DEBUG_
    registerWindow([&]()
        {
            if (ImGui::Begin("Window"))
            {
                /*auto camPos = m_freeCam.getComponent<cro::Transform>().getPosition();
                ImGui::Text("Cam pos %3.3f, %3.3f, %3.3f", camPos.x, camPos.y, camPos.z);*/
            }
            ImGui::End();
        });
#endif
}

//public
bool DrivingState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_p:
        case SDLK_ESCAPE:
        case SDLK_BACKSPACE:
        case SDLK_PAUSE:
            requestStackPush(StateID::Pause);
            break;
            //make sure sysem buttons don't do anything
        case SDLK_F1:
        case SDLK_F5:

            break;
#ifdef CRO_DEBUG_
        case SDLK_INSERT:
            toggleFreeCam();
            break;
#endif
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        if (evt.cbutton.which == cro::GameController::deviceID(0))
        {
            switch (evt.cbutton.button)
            {
            default: break;
            case cro::GameController::ButtonStart:
                requestStackPush(StateID::Pause);
                break;
            }
        }
    }

#ifdef CRO_DEBUG_
    m_gameScene.getSystem<FpsCameraSystem>()->handleEvent(evt);
#endif

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void DrivingState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool DrivingState::simulate(float dt)
{
    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void DrivingState::render()
{
    m_backgroundTexture.clear();
    m_gameScene.render(m_backgroundTexture);
    m_backgroundTexture.display();

    m_uiScene.render(*GolfGame::getActiveTarget());
}

//private
void DrivingState::toggleFreeCam()
{
    useFreeCam = !useFreeCam;
    if (useFreeCam)
    {
        m_defaultCam = m_gameScene.setActiveCamera(m_freeCam);
        m_gameScene.setActiveListener(m_freeCam);
    }
    else
    {
        m_gameScene.setActiveCamera(m_defaultCam);
        m_gameScene.setActiveListener(m_defaultCam);
    }

    m_gameScene.setSystemActive<FpsCameraSystem>(useFreeCam);
    //m_inputParser.setActive(!useFreeCam);
    cro::App::getWindow().setMouseCaptured(useFreeCam);
}

void DrivingState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::BillboardSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
    m_gameScene.addSystem<cro::AudioSystem>(mb);
#ifdef  CRO_DEBUG_
    m_gameScene.addSystem<FpsCameraSystem>(mb);

    m_gameScene.setSystemActive<FpsCameraSystem>(false);
#endif

    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::UISystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::SpriteAnimator>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
    m_uiScene.addSystem<cro::AudioPlayerSystem>(mb);
}

void DrivingState::loadAssets()
{
    m_gameScene.setCubemap("assets/golf/images/skybox/spring/sky.ccm");

    m_resources.shaders.loadFromString(ShaderID::Cel, CelVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n");
    m_resources.shaders.loadFromString(ShaderID::CelTextured, CelVertexShader, CelFragmentShader, "#define TEXTURED\n");

    m_materialIDs[MaterialID::Cel] = m_resources.materials.add(m_resources.shaders.get(ShaderID::Cel));
    m_materialIDs[MaterialID::CelTextured] = m_resources.materials.add(m_resources.shaders.get(ShaderID::CelTextured));
}

void DrivingState::createScene()
{
    //load the course model
    auto texturedMat = m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]);

    cro::ModelDefinition md(m_resources);
    md.loadFromFile("assets/golf/models/driving_range.cmt");
    setTexture(md, texturedMat);

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    md.createModel(entity);
    entity.getComponent<cro::Model>().setMaterial(0, texturedMat);


    //update the 3D view
    auto updateView = [&](cro::Camera& cam)
    {
        auto vpSize = calcVPSize();

        auto winSize = glm::vec2(cro::App::getWindow().getSize());
        float scale = std::floor(winSize.y / vpSize.y);
        auto texSize = vpSize;
        if (texSize.x * scale <= winSize.x)
        {
            texSize *= 1.6f;
        }

        m_backgroundTexture.create(static_cast<std::uint32_t>(texSize.x), static_cast<std::uint32_t>(texSize.y));

        //the resize actually extends the target vertically so we need to maintain a
        //horizontal FOV, not the vertical one expected by default.
        cam.setPerspective(FOV * (vpSize.y / ViewportHeight), vpSize.x / vpSize.y, 0.1f, vpSize.x);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };


        //because we don't know in which order the cam callbacks are raised
        //we need to send the player repos command from here when we know the view is correct
        cro::Command cmd;
        cmd.targetFlags = CommandID::UI::PlayerSprite;
        cmd.action = [&](cro::Entity e, float)
        {
            const auto& camera = m_cameras[CameraID::Player].getComponent<cro::Camera>();
            auto pos = camera.coordsToPixel(PlayerPosition, m_backgroundTexture.getSize());
            e.getComponent<cro::Transform>().setPosition(pos);
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    };

    auto camEnt = m_gameScene.getActiveCamera();
    auto& cam = camEnt.getComponent<cro::Camera>();
    cam.resizeCallback = updateView;
    updateView(cam);
    
    camEnt.getComponent<cro::Transform>().setPosition({ 0.f, 3.f, 128.f });
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -10.f * cro::Util::Const::degToRad);
    m_cameras[CameraID::Player] = camEnt;

#ifdef CRO_DEBUG_
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>();
    camEnt.addComponent<cro::Camera>().resizeCallback = updateView;
    //camEnt.getComponent<cro::Camera>().reflectionBuffer.create(1024, 1024);
    camEnt.addComponent<cro::AudioListener>();
    camEnt.addComponent<FpsCamera>();
    updateView(camEnt.getComponent<cro::Camera>());

    m_freeCam = camEnt;
#endif


    //TODO emulate facing north with sun more or less behind player
}

void DrivingState::createUI()
{
    //displays the game scene
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_backgroundTexture.getTexture());
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    entity.addComponent<cro::Callback>().function =
        [](cro::Entity e, float)
    {
        //this is activated once to make sure the
        //sprite is up to date with any texture buffer resize
        glm::vec2 texSize = e.getComponent<cro::Sprite>().getTexture()->getSize();
        e.getComponent<cro::Sprite>().setTextureRect({ glm::vec2(0.f), texSize });
        e.getComponent<cro::Transform>().setOrigin(texSize / 2.f);
        e.getComponent<cro::Callback>().active = false;
    };
    auto courseEnt = entity;

    //ui is attached to this for relative scaling
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();

    auto rootNode = entity;
    createPlayer(courseEnt);

    //ui viewport is set 1:1 with window, then the scene
    //is scaled to best-fit to maintain pixel accuracy of text.
    auto updateView = [&, rootNode, courseEnt](cro::Camera& cam) mutable
    {
        auto windowSize = GolfGame::getActiveTarget()->getSize();
        glm::vec2 size(windowSize);

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -5.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        auto vpSize = calcVPSize();

        m_viewScale = glm::vec2(std::floor(size.y / vpSize.y));
        rootNode.getComponent<cro::Transform>().setScale(m_viewScale);

        courseEnt.getComponent<cro::Transform>().setScale(m_viewScale);
        courseEnt.getComponent<cro::Callback>().active = true; //makes sure to delay so updating the texture size is complete first
        courseEnt.getComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, -1.f));

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

void DrivingState::createPlayer(cro::Entity courseEnt)
{
    //load sprites from avatar info
    const auto indexFromSkinID = [&](std::uint8_t skinID)->std::size_t
    {
        auto result = std::find_if(m_sharedData.avatarInfo.begin(), m_sharedData.avatarInfo.end(),
            [skinID](const SharedStateData::AvatarInfo& ai)
            {
                return skinID == ai.uid;
            });

        if (result != m_sharedData.avatarInfo.end())
        {
            return std::distance(m_sharedData.avatarInfo.begin(), result);
        }
        return 0;
    };

    auto idx = indexFromSkinID(m_sharedData.localConnectionData.playerData[0].skinID);

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile(m_sharedData.avatarInfo[idx].spritePath, m_resources.textures);

    auto& camera = m_cameras[CameraID::Player].getComponent<cro::Camera>();
    camera.updateMatrices(m_cameras[CameraID::Player].getComponent<cro::Transform>());
    auto pos = camera.coordsToPixel(PlayerPosition, m_backgroundTexture.getSize());


    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(pos);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("wood");
    entity.getComponent<cro::Sprite>().setTexture(m_sharedData.avatarTextures[0][0], false);
    entity.addComponent<cro::SpriteAnimation>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::PlayerSprite;
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width * 0.78f, 0.f, -0.5f });
    courseEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}