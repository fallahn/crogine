/*-----------------------------------------------------------------------

Matt Marchant - 2022
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

#include "BilliardsState.hpp"
#include "SharedStateData.hpp"
#include "PacketIDs.hpp"
#include "CommandIDs.hpp"
#include "MessageIDs.hpp"
#include "GameConsts.hpp"
#include "BilliardsSystem.hpp"
#include "InterpolationSystem.hpp"
#include "server/ServerPacketData.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>

#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/AudioSystem.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Easings.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/util/Network.hpp>

namespace
{
    const cro::Time ReadyPingFreq = cro::seconds(1.f);
}

BilliardsState::BilliardsState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State        (ss, ctx),
    m_sharedData        (sd),
    m_gameScene         (ctx.appInstance.getMessageBus()),
    m_uiScene           (ctx.appInstance.getMessageBus()),
    m_inputParser       (sd.inputBinding, ctx.appInstance.getMessageBus()),
    m_currentPlayer     (0),
    m_scaleBuffer       ("PixelScale", sizeof(float)),
    m_resolutionBuffer  ("ScaledResolution", sizeof(glm::vec2)),
    m_viewScale         (2.f),
    m_ballDefinition    (m_resources),
    m_wantsGameState    (true),
    m_activeCamera      (CameraID::Side),
    m_gameMode          (TableData::Void)
{
    ctx.mainWindow.loadResources([&]() 
        {
            loadAssets();
            addSystems();
            buildScene();
        });

#ifdef CRO_DEBUG_
    registerWindow([&]() 
        {
            if (ImGui::Begin("Buns"))
            {
                ImGui::Text("Power %3.3f", m_inputParser.getPower());

                /*auto pos = m_cameras[CameraID::Player].getComponent<cro::Transform>().getPosition();
                if (ImGui::SliderFloat("Height", &pos.y, 0.1f, 1.f))
                {
                    m_cameras[CameraID::Player].getComponent<cro::Transform>().setPosition(pos);
                }
                
                if (ImGui::SliderFloat("Distance", &pos.z, 0.1f, 1.f))
                {
                    m_cameras[CameraID::Player].getComponent<cro::Transform>().setPosition(pos);
                }

                ImGui::Text("Position %3.3f, %3.3f", pos.y, pos.z);

                static float rotation = -10.f;
                if (ImGui::SliderFloat("Rotation", &rotation, -90.f, 0.f))
                {
                    m_cameras[CameraID::Player].getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, rotation * cro::Util::Const::degToRad);
                }*/
            }
            ImGui::End();
        });
#endif
}

//public
bool BilliardsState::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse)
    {
        if (evt.type == SDL_MOUSEMOTION
            && cro::App::getWindow().getMouseCaptured())
        {
            cro::App::getWindow().setMouseCaptured(false);
        }

        return false;
    }

#ifdef CRO_DEBUG_
    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_F2:
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint8_t(ServerCommand::SpawnBall), cro::NetFlag::Reliable);
            break;
        case SDLK_F3:
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint8_t(ServerCommand::StrikeBall), cro::NetFlag::Reliable);
            break;
        }
    }
    else 
#endif

    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_ESCAPE:
        case SDLK_p:
        case SDLK_PAUSE:
            requestStackPush(StateID::Pause);
            break;
        case SDLK_SPACE:
            //toggleQuitReady();
            break;
        case SDLK_KP_1:
        case SDLK_1:
            setActiveCamera(CameraID::Side);
            break;
        case SDLK_KP_2:
        case SDLK_2:
            setActiveCamera(CameraID::Front);
            break;
        case SDLK_KP_3:
        case SDLK_3:
            setActiveCamera(CameraID::Overhead);
            break;
        case SDLK_KP_4:
        case SDLK_4:
            setActiveCamera(CameraID::Player);
            break;
#ifdef CRO_DEBUG_
        case SDLK_TAB:
            //if (m_activeCamera == CameraID::Player)
            {
                cro::App::getWindow().setMouseCaptured(false);
            }
            break;
#endif //CRO_DEBUG_

        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONDOWN
        && evt.cbutton.which == cro::GameController::deviceID(m_sharedData.inputBinding.controllerID))
    {
        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonA:
            //toggleQuitReady();
            break;
        case cro::GameController::ButtonLeftShoulder:
            setActiveCamera((m_activeCamera + (CameraID::Count - 1)) % CameraID::Count);
            break;
        case cro::GameController::ButtonRightShoulder:
            setActiveCamera((m_activeCamera + 1) % CameraID::Count);
            break;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP
        && evt.cbutton.which == cro::GameController::deviceID(m_sharedData.inputBinding.controllerID))
    {
        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonStart:
            requestStackPush(StateID::Pause);
            break;
        }
    }
    else if (evt.type == SDL_CONTROLLERDEVICEREMOVED)
    {
        //check if any players are using the controller
        //and reassign any still connected devices
        for (auto i = 0; i < 4; ++i)
        {
            if (evt.cdevice.which == cro::GameController::deviceID(i))
            {
                for (auto& idx : m_sharedData.controllerIDs)
                {
                    idx = std::max(0, i - 1);
                }
                //update the input parser in case this player is active
                m_sharedData.inputBinding.controllerID = m_sharedData.controllerIDs[m_currentPlayer];
                break;
            }
        }
    }

    m_inputParser.handleEvent(evt);
    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return false;
}

void BilliardsState::handleMessage(const cro::Message& msg)
{
    switch (msg.id)
    {
    default: break;
    case cro::Message::WindowMessage:
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            resizeBuffers();
        }
    }
        break;
    case cro::Message::SkeletalAnimationMessage:
    {
        const auto& data = msg.getData<cro::Message::SkeletalAnimationEvent>();
        if (data.userType == 0)
        {
            BilliardBallInput input;
            auto [impulse, offset] = m_inputParser.getImpulse();
            input.impulse = impulse;
            input.offset = offset;
            input.client = m_sharedData.localConnectionData.connectionID;
            input.player = m_currentPlayer;

            m_sharedData.clientConnection.netClient.sendPacket(PacketID::InputUpdate, input, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
    }
    break;
    case MessageID::BilliardsMessage:
    {
        const auto& data = msg.getData<BilliardBallEvent>();
        if (data.type == BilliardBallEvent::BallPlaced)
        {
            BilliardBallInput input;
            input.offset = data.position;
            input.client = m_sharedData.localConnectionData.connectionID;
            input.player = m_currentPlayer;

            m_sharedData.clientConnection.netClient.sendPacket(PacketID::BallPlaced, input, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
    }
        break;
    }

    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool BilliardsState::simulate(float dt)
{
    if (m_sharedData.clientConnection.connected)
    {
        cro::NetEvent evt;
        while (m_sharedData.clientConnection.netClient.pollEvent(evt))
        {
            //handle events
            handleNetEvent(evt);
        }

        if (m_wantsGameState)
        {
            if (m_readyClock.elapsed() > ReadyPingFreq)
            {
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::ClientReady, m_sharedData.clientConnection.connectionID, cro::NetFlag::Reliable);
                m_readyClock.restart();
            }
        }
    }
    else
    {
        //we've been disconnected somewhere - push error state
        m_sharedData.errorMessage = "Lost connection to host.";
        requestStackPush(StateID::Error);
    }

    m_inputParser.update(dt);

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return false;
}

void BilliardsState::render()
{
    m_gameSceneTexture.clear(/*cro::Colour::AliceBlue*/);
    m_gameScene.render();
    m_gameSceneTexture.display();

    m_uiScene.render();
}

//private
void BilliardsState::loadAssets()
{
    m_ballDefinition.loadFromFile("assets/golf/models/hole_19/billiard_ball.cmt");
}

void BilliardsState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_gameScene.addSystem<cro::CommandSystem>(mb);
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::SkeletalAnimator>(mb);
    m_gameScene.addSystem<InterpolationSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
    m_gameScene.addSystem<cro::AudioSystem>(mb);

    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
    m_uiScene.addSystem<cro::AudioPlayerSystem>(mb);
}

void BilliardsState::buildScene()
{
    //TODO validate the table data (or should this be done by the menu?)

    std::string path = "assets/golf/tables/" + m_sharedData.mapDirectory + ".table";
    TableData tableData;
    if (tableData.loadFromFile(path))
    {
        cro::ModelDefinition md(m_resources);
        if (md.loadFromFile(tableData.viewModel))
        {
            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>();
            md.createModel(entity);
        }
        m_gameMode = tableData.rules;
    }
    else
    {
        //TODO push error for missing data.
    }

    //update the 3D view
    resizeBuffers();
    
    struct CameraProperties final
    {
        float FOVAdjust = 1.f;
        float farPlane = 5.f;
    };

    auto setPerspective = [&](cro::Camera& cam)
    {
        auto vpSize = glm::vec2(cro::App::getWindow().getSize());

        //fudge to see which camera we're working on 
        auto ent = std::find_if(m_cameras.begin(), m_cameras.end(), [&cam](const cro::Entity& c)
            {
                return c.isValid() && c.getComponent<cro::Camera>() == cam;
            });

        float fovMultiplier = 1.f;
        float farPlane = 5.f;

        if (ent != m_cameras.end() && 
            ent->isValid())
        {
            fovMultiplier = ent->getComponent<CameraProperties>().FOVAdjust;
            farPlane = ent->getComponent<CameraProperties>().farPlane;
        }

        cam.setPerspective(m_sharedData.fov * fovMultiplier * cro::Util::Const::degToRad, vpSize.x / vpSize.y, 0.1f, farPlane);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    auto camEnt = m_gameScene.getActiveCamera();
    camEnt.getComponent<cro::Transform>().setPosition({ -1.5f, 0.8f, 0.f });
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -90.f * cro::Util::Const::degToRad);
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -30.f * cro::Util::Const::degToRad);
    camEnt.addComponent<CameraProperties>();
    m_cameras[CameraID::Side] = camEnt;
    auto& cam = camEnt.getComponent<cro::Camera>();
    cam.resizeCallback = setPerspective;
    setPerspective(cam);

    static constexpr std::uint32_t ShadowMapSize = 2048u;
    cam.shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);

    //front cam
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>().setPosition({ 0.f, 0.8f, 1.8f });
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -30.f * cro::Util::Const::degToRad);
    camEnt.addComponent<cro::Camera>().resizeCallback = setPerspective;
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.addComponent<CameraProperties>().FOVAdjust = 0.8f;
    m_cameras[CameraID::Front] = camEnt;
    setPerspective(camEnt.getComponent<cro::Camera>());

    //overhead cam
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>().setPosition({ 0.f, 2.f, 0.f });
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::Z_AXIS, -90.f * cro::Util::Const::degToRad);
    camEnt.addComponent<cro::Camera>().resizeCallback = setPerspective;
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.addComponent<CameraProperties>().FOVAdjust = 0.75f;
    camEnt.getComponent<CameraProperties>().farPlane = 2.5f;
    m_cameras[CameraID::Overhead] = camEnt;
    setPerspective(camEnt.getComponent<cro::Camera>());


    //player cam is a bit more complicated because it can be rotated
    //by the player, and transformed by the game.
    m_cameraController = m_gameScene.createEntity();
    m_cameraController.addComponent<cro::Transform>();
    m_cameraController.addComponent<ControllerRotation>();

    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>().setPosition({ 0.f, 0.367f, 0.423f });
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -26.7f * cro::Util::Const::degToRad);
    camEnt.addComponent<cro::Camera>().resizeCallback = setPerspective;
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.addComponent<CameraProperties>().FOVAdjust = 0.8f;
    camEnt.getComponent<CameraProperties>().farPlane = 3.f;
    m_cameras[CameraID::Player] = camEnt;
    setPerspective(camEnt.getComponent<cro::Camera>());

    m_cameraController.getComponent<cro::Transform>().addChild(camEnt.getComponent<cro::Transform>());
    m_cameraController.getComponent<ControllerRotation>().activeCamera = &camEnt.getComponent<cro::Camera>().active;

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<ControllerRotation>();

    ControlEntities controlEntities;
    controlEntities.camera = m_cameraController;

    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/golf/models/hole_19/cue.cmt"))
    {
        md.createModel(entity);
        m_cameraController.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }
    controlEntities.cue = entity;

    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();

    if (md.loadFromFile("assets/golf/models/hole_19/preview_ball.cmt"))
    {
        md.createModel(entity);
        entity.getComponent<cro::Model>().setHidden(true);
    }
    controlEntities.previewBall = entity;
    m_inputParser.setControlEntities(controlEntities);



    //lights point straight down in billiards.
    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);

    createUI();


    //TODO this wants to be done after any animation
    m_sharedData.clientConnection.netClient.sendPacket(PacketID::TransitionComplete,
        m_sharedData.clientConnection.connectionID, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
}

void BilliardsState::handleNetEvent(const cro::NetEvent& evt)
{
    if (evt.type == cro::NetEvent::PacketReceived)
    {
        switch (evt.packet.getID())
        {
        default: break;
        case PacketID::TableInfo:
            m_tableInfo = evt.packet.as<TableInfo>();
            break;
        case PacketID::ActorUpdate:
            updateBall(evt.packet.as<BilliardsUpdate>());
            break;
        case PacketID::EntityRemoved:
        {
            auto id = evt.packet.as<std::uint32_t>();

            cro::Command cmd;
            cmd.targetFlags = CommandID::Ball;
            cmd.action = [&, id](cro::Entity e, float)
            {
                if (e.getComponent<InterpolationComponent>().getID() == id)
                {
                    m_gameScene.destroyEntity(e);
                }
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
            break;
        case PacketID::ActorSpawn:
            spawnBall(evt.packet.as<ActorInfo>());
            break;
        case PacketID::SetPlayer:
            m_wantsGameState = false;
            setPlayer(evt.packet.as<BilliardsPlayer>());
            break;
        case PacketID::ClientDisconnected:
            //TODO we should only have 2 clients max
            //so if only one player locally then game is forfeited.
            break;
        case PacketID::AchievementGet:
            LogI << "TODO: Notify Achievement" << std::endl;
            break;
        case PacketID::ServerError:
            switch (evt.packet.as<std::uint8_t>())
            {
            default:
                m_sharedData.errorMessage = "Server Error (Unknown)";
                break;
            case MessageType::MapNotFound:
                m_sharedData.errorMessage = "Server Failed To Load Table Data";
                break;
            }
            requestStackPush(StateID::Error);
            break;
        }
    }
    else if (evt.type == cro::NetEvent::ClientDisconnect)
    {
        m_sharedData.errorMessage = "Disconnected From Server (Host Quit)";
        requestStackPush(StateID::Error);
    }
}

void BilliardsState::spawnBall(const ActorInfo& info)
{
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(info.position);
    entity.getComponent<cro::Transform>().setRotation(cro::Util::Net::decompressQuat(info.rotation));
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Ball;
    entity.addComponent<InterpolationComponent>(InterpolationPoint(info.position, cro::Util::Net::decompressQuat(info.rotation), info.timestamp)).setID(info.serverID);

    m_ballDefinition.createModel(entity);
    
    entity.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto& progress = e.getComponent<cro::Callback>().getUserData<float>();
        progress = std::min(1.f, progress + (dt * 3.f));
        
        float scale = cro::Util::Easing::easeInOutBack(progress);
        e.getComponent<cro::Transform>().setScale(glm::vec3(scale));

        if (progress == 1)
        {
            e.getComponent<cro::Callback>().active = false;
        }
    };

    //set colour/model based on type stored in info.state
    if (m_gameMode == TableData::Eightball)
    {
        switch (info.state)
        {
        default: break;
        case 0: //cueball
            entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", cro::Colour::White);
            m_cueball = entity;
            //m_cameraController.getComponent<cro::Transform>().setPosition(info.position);
            //setActiveCamera(CameraID::Player);
            break;
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7: 
            entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", cro::Colour::Red);
            break;
        case 8:
            entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", cro::Colour::Black);
            break;
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
            entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", cro::Colour::Yellow);
            break;
        }
    }

    /*std::array points =
    {
        InterpolationPoint(glm::vec3(0.f, 0.1f, 0.f), glm::quat(1.f,0.f,0.f,0.f), 1000 + info.timestamp),
        InterpolationPoint(glm::vec3(1.f, 0.1f, 1.f), glm::quat(1.f,0.f,0.f,0.f), 2000 + info.timestamp),
        InterpolationPoint(glm::vec3(1.f, 0.1f, 0.f), glm::quat(1.f,0.f,0.f,0.f), 3000 + info.timestamp),
        InterpolationPoint(glm::vec3(0.f, 0.1f, -1.f), glm::quat(1.f,0.f,0.f,0.f), 4000 + info.timestamp),
        InterpolationPoint(glm::vec3(0.f, 0.1f, 0.f), glm::quat(1.f,0.f,0.f,0.f), 5000 + info.timestamp),
    };
    for (const auto& p : points)
    {
        entity.getComponent<InterpolationComponent>().setTarget(p);
    }*/
}

void BilliardsState::updateBall(const BilliardsUpdate& info)
{
    cro::Command cmd;
    cmd.targetFlags = CommandID::Ball;
    cmd.action = [info](cro::Entity e, float)
    {
        auto& interp = e.getComponent<InterpolationComponent>();
        if (interp.getID() == info.serverID)
        {
            interp.addTarget({ info.position, cro::Util::Net::decompressQuat(info.rotation), info.timestamp });
        }
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
}

void BilliardsState::setPlayer(const BilliardsPlayer& playerInfo)
{
    //TODO hide the cue model and wait for anim to finish to
    //decide which cue model should be shown (local or remote)


    struct TargetData final
    {
        glm::vec3 camStart = glm::vec3(0.f);
        glm::vec3 camTarget = glm::vec3(0.f);

        float rotationStart = 0.f;
        float rotationTarget = 0.f;

        float elapsedTime = 0.f;
    };

    TargetData data;
    data.camStart = m_cameraController.getComponent<cro::Transform>().getPosition();
    data.camTarget = m_cueball.isValid() ? m_cueball.getComponent<cro::Transform>().getPosition() : m_tableInfo.cueballPosition;
    data.rotationStart = m_cameraController.getComponent<ControllerRotation>().rotation;
    data.rotationTarget = std::atan2(-data.camTarget.z, data.camTarget.x) + (cro::Util::Const::PI / 2.f); //TODO get a better target point than 0,0

    //entity performs animation in callback then pops itself when done.
    cro::Entity entity = m_gameScene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<TargetData>(data);
    entity.getComponent<cro::Callback>().function =
        [&, playerInfo](cro::Entity e, float dt) mutable
    {
        const float updateSpeed = dt;

        auto& data = e.getComponent<cro::Callback>().getUserData<TargetData>();
        data.elapsedTime = std::min(1.f, data.elapsedTime + updateSpeed);

        float interpTime = cro::Util::Easing::easeOutQuint(data.elapsedTime);

        //update controller position/rotation
        auto currRotation = data.rotationStart + cro::Util::Maths::shortestRotation(data.rotationStart, data.rotationTarget) * interpTime;
        m_cameraController.getComponent<ControllerRotation>().rotation = currRotation;

        m_cameraController.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, currRotation);
        m_cameraController.getComponent<cro::Transform>().setPosition(interpolate(data.camStart, data.camTarget, interpTime));
        
        if (data.elapsedTime == 1)
        {
            //TODO we want to get the cue entity from somewhere and set rotation to 0...

            m_cameraController.getComponent<ControllerRotation>().rotation = data.rotationTarget;
            m_cameraController.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, data.rotationTarget);
            m_cameraController.getComponent<cro::Transform>().setPosition(data.camTarget);

            //wait for animation to finish first
            if (playerInfo.client == m_sharedData.localConnectionData.connectionID)
            {
                m_inputParser.setActive(true, !m_cueball.isValid());
                m_sharedData.inputBinding.controllerID = m_sharedData.controllerIDs[playerInfo.player];
                m_currentPlayer = playerInfo.player;

                //TODO show / hide cue depending on local player or not

                setActiveCamera(CameraID::Player);
            }

            e.getComponent<cro::Callback>().active = false;
            m_gameScene.destroyEntity(e);
        }
    };
}

void BilliardsState::setActiveCamera(std::int32_t camID)
{
    m_gameScene.getActiveCamera().getComponent<cro::Camera>().active = false;
    m_gameScene.setActiveCamera(m_cameras[camID]);
    m_gameScene.getActiveCamera().getComponent<cro::Camera>().active = true;

    m_activeCamera = camID;

    cro::App::getWindow().setMouseCaptured(camID == CameraID::Player);
}

void BilliardsState::resizeBuffers()
{
    glm::vec2 winSize(cro::App::getWindow().getSize());

    auto vpSize = calcVPSize();
    float maxScale = std::floor(winSize.y / vpSize.y);
    float scale = m_sharedData.pixelScale ? maxScale : 1.f;
    auto texSize = winSize / scale;
    m_gameSceneTexture.create(static_cast<std::uint32_t>(texSize.x), static_cast<std::uint32_t>(texSize.y));

    auto invScale = (maxScale + 1.f) - scale;
    /*glCheck(glPointSize(invScale * BallPointSize));
    glCheck(glLineWidth(invScale));*/

    m_scaleBuffer.setData(&invScale);

    glm::vec2 scaledRes = texSize / invScale;
    m_resolutionBuffer.setData(&scaledRes);
}