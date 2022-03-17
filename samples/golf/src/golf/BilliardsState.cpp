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
#include "GameConsts.hpp"
#include "BilliardsSystem.hpp"
#include "InterpolationSystem.hpp"
#include "server/ServerPacketData.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>

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
    m_scaleBuffer       ("PixelScale", sizeof(float)),
    m_resolutionBuffer  ("ScaledResolution", sizeof(glm::vec2)),
    m_viewScale         (2.f),
    m_ballDefinition    (m_resources),
    m_wantsGameState    (true)
{
    ctx.mainWindow.loadResources([&]() 
        {
            loadAssets();
            addSystems();
            buildScene();
        });
}

//public
bool BilliardsState::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse)
    {
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
                //m_sharedData.inputBinding.controllerID = m_sharedData.controllerIDs[m_currentPlayer.player];
                break;
            }
        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return false;
}

void BilliardsState::handleMessage(const cro::Message& msg)
{
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

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return false;
}

void BilliardsState::render()
{
    m_gameSceneTexture.clear(cro::Colour::AliceBlue);
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
    }
    else
    {
        //TODO push error for missing data.
    }

    //update the 3D view
    auto updateView = [&](cro::Camera& cam)
    {
        auto vpSize = calcVPSize();

        auto winSize = glm::vec2(cro::App::getWindow().getSize());
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

        cam.setPerspective(m_sharedData.fov * cro::Util::Const::degToRad, texSize.x / texSize.y, 0.1f, 5.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    auto camEnt = m_gameScene.getActiveCamera();
    camEnt.getComponent<cro::Transform>().setPosition({-1.5f, 0.8f, 0.f});
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -90.f * cro::Util::Const::degToRad);
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -30.f * cro::Util::Const::degToRad);
    //m_cameras[CameraID::Player] = camEnt;
    auto& cam = camEnt.getComponent<cro::Camera>();
    cam.resizeCallback = updateView;
    updateView(cam);

    static constexpr std::uint32_t ShadowMapSize = 2048u;
    cam.shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);

    //TODO overhead cam etc


    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);

    createUI();
}

void BilliardsState::handleNetEvent(const cro::NetEvent& evt)
{
    if (evt.type == cro::NetEvent::PacketReceived)
    {
        switch (evt.packet.getID())
        {
        default: break;
        case PacketID::ActorUpdate:
            updateBall(evt.packet.as<ActorInfo>());
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

            //TODO this wants to be done after any animation
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::TransitionComplete,
                m_sharedData.clientConnection.connectionID, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);

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

    //TODO set colour/model based on type stored in info.state

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

void BilliardsState::updateBall(const ActorInfo& info)
{
    cro::Command cmd;
    cmd.targetFlags = CommandID::Ball;
    cmd.action = [info](cro::Entity e, float)
    {
        auto& interp = e.getComponent<InterpolationComponent>();
        if (interp.getID() == info.serverID)
        {
            interp.setTarget({ info.position, cro::Util::Net::decompressQuat(info.rotation), info.timestamp });
        }
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
}