/*-----------------------------------------------------------------------

Matt Marchant 2020
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

#include "GameState.hpp"
#include "SharedStateData.hpp"
#include "PlayerSystem.hpp"
#include "PacketIDs.hpp"
#include "ActorIDs.hpp"
#include "ClientCommandIDs.hpp"
#include "InterpolationSystem.hpp"
#include "ClientPacketData.hpp"
#include "MenuConsts.hpp"
#include "Chunk.hpp"
#include "ChunkSystem.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Sprite.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/TextRenderer.hpp>
#include <crogine/ecs/systems/SpriteRenderer.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/GlobalConsts.hpp>

namespace
{
    //for debug output
    cro::Entity playerEntity;
    std::int32_t bitrate = 0;
    std::int32_t bitrateCounter = 0;
}

GameState::GameState(cro::StateStack& stack, cro::State::Context context, SharedStateData& sd)
    : cro::State    (stack, context),
    m_sharedData    (sd),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus()),
    m_fontID        (0),
    m_inputParser   (sd.clientConnection.netClient),
    m_cameraPosIndex(0)
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    });

    updateView();
    context.mainWindow.setMouseCaptured(true);
    sd.clientConnection.ready = false;

    //console commands
    registerCommand("sv_playermode", 
        [&](const std::string& params)
        {
            if (!params.empty())
            {
                ServerCommand cmd;
                cmd.target = 0;

                if (params == "fly")
                {
                    cmd.commandID = CommandPacket::SetModeFly;
                }
                else if (params == "walk")
                {
                    cmd.commandID = CommandPacket::SetModeWalk;
                }
                else
                {
                    cro::Console::print(params + ": unknown parameter.");
                    return;
                }
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, cmd, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
            }
            else
            {
                cro::Console::print("Missing parameter. Usage: sv_playermode <mode>");
            }
        });

    //debug output
    playerEntity = {};
    registerWindow([&]()
        {
            if (playerEntity.isValid())
            {
                ImGui::SetNextWindowSize({ 300.f, 320.f });
                ImGui::Begin("Info");

                ImGui::Text("Player ID: %d", m_sharedData.clientConnection.playerID);

                auto pos = playerEntity.getComponent<cro::Transform>().getPosition();
                auto rotation = playerEntity.getComponent<cro::Transform>().getRotation();// *cro::Util::Const::radToDeg;
                ImGui::Text("Position: %3.3f, %3.3f, %3.3f", pos.x, pos.y, pos.z);
                ImGui::Text("Rotation: %3.3f, %3.3f, %3.3f", rotation.x, rotation.y, rotation.z);

                ImGui::Text("Pitch: %3.3f", playerEntity.getComponent<Player>().cameraPitch);
                ImGui::Text("Yaw: %3.3f", playerEntity.getComponent<Player>().cameraYaw);

                auto mouse = playerEntity.getComponent<Player>().inputStack[playerEntity.getComponent<Player>().lastUpdatedInput];
                ImGui::Text("Mouse Movement: %d, %d", mouse.xMove, mouse.yMove);

                ImGui::NewLine();
                ImGui::Separator();
                ImGui::NewLine();
                ImGui::Text("Bitrate: %3.3fkbps", static_cast<float>(bitrate) / 1024.f);

                ImGui::End();
            }
        });
}

//public
bool GameState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() 
        || cro::ui::wantsKeyboard()
        || cro::Console::isVisible())
    {
        return true;
    }

    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_PAUSE:
        case SDLK_ESCAPE:
        case SDLK_p:
            requestStackPush(States::ID::Pause);
            break;
        case SDLK_F3:
            //first/third person camera
            updateCameraPosition();
            break;
#ifdef CRO_DEBUG_
        case SDLK_1:
        {
            ServerCommand cmd;
            cmd.target = 0;
            cmd.commandID = CommandPacket::SetModeWalk;
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, cmd, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
            break;
        case SDLK_2:
        {
            ServerCommand cmd;
            cmd.target = 0;
            cmd.commandID = CommandPacket::SetModeFly;
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, cmd, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
        break;
        case SDLK_4:
        {
            cro::Command cmd;
            cmd.targetFlags = Client::CommandID::ChunkMesh;
            cmd.action = [](cro::Entity e, float)
            {
                auto& chunkComponent = e.getComponent<ChunkComponent>();
                if (chunkComponent.meshType != ChunkComponent::Naive)
                {
                    chunkComponent.meshType = ChunkComponent::Naive;
                    chunkComponent.needsUpdate = true;
                }
            };
            m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
        }
            break;
        case SDLK_5:
        {
            cro::Command cmd;
            cmd.targetFlags = Client::CommandID::ChunkMesh;
            cmd.action = [](cro::Entity e, float)
            {
                auto& chunkComponent = e.getComponent<ChunkComponent>();
                if (chunkComponent.meshType != ChunkComponent::Greedy)
                {
                    chunkComponent.meshType = ChunkComponent::Greedy;
                    chunkComponent.needsUpdate = true;
                }
            };
            m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
            std::cout << "put back correct operator == !!!\n";
        }
        break;
#endif //CRO_DEBUG_
        case SDLK_3:
        {
            cro::Command cmd;
            cmd.targetFlags = Client::CommandID::DebugMesh;
            cmd.action = [](cro::Entity e, float)
            {
                e.getComponent<cro::Model>().setHidden(!e.getComponent<cro::Model>().isHidden());
            };
            m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
        }
        break;

        }
    }

    m_inputParser.handleEvent(evt);

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void GameState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);

    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            updateView();
        }
    }
    else if (msg.id == cro::Message::ConsoleMessage)
    {
        const auto& data = msg.getData<cro::Message::ConsoleEvent>();
        if (getStateCount() == 1) //ignore this if pause menu is open
        {
            getContext().mainWindow.setMouseCaptured(data.type == cro::Message::ConsoleEvent::Closed);
        }
    }
}

bool GameState::simulate(float dt)
{
    if (m_sharedData.clientConnection.connected)
    {
        cro::NetEvent evt;
        while (m_sharedData.clientConnection.netClient.pollEvent(evt))
        {
            if (evt.type == cro::NetEvent::PacketReceived)
            {
                bitrateCounter += evt.packet.getSize() * 8;
                handlePacket(evt.packet);
            }
            else if (evt.type == cro::NetEvent::ClientDisconnect)
            {
                m_sharedData.errorMessage = "Disconnected from server.";
                requestStackPush(States::Error);
            }
        }

        if (m_bitrateClock.elapsed().asMilliseconds() > 1000)
        {
            //TODO convert this to a moving average
            m_bitrateClock.restart();
            bitrate = bitrateCounter;
            bitrateCounter = 0;
        }
    }
    else
    {
        //we've been disconnected somewhere - push error state
    }

    //if we haven't had the server reply yet, tell it we're ready
    if (!m_sharedData.clientConnection.ready
        && m_sceneRequestClock.elapsed().asMilliseconds() > 1000)
    {
        m_sceneRequestClock.restart();
        m_sharedData.clientConnection.netClient.sendPacket(PacketID::ClientReady, m_sharedData.clientConnection.playerID, cro::NetFlag::Unreliable);
    }


    m_inputParser.update();

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void GameState::render()
{
    auto& rt = cro::App::getWindow();
    m_gameScene.render(rt);
    m_uiScene.render(rt);
}

//private
void GameState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CommandSystem>(mb);
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<InterpolationSystem>(mb);
    m_gameScene.addSystem<PlayerSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<ChunkSystem>(mb, m_resources);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::SpriteRenderer>(mb);
    m_uiScene.addSystem<cro::TextRenderer>(mb);
}

void GameState::loadAssets()
{

}

void GameState::createScene()
{
    m_gameScene.setCubemap("assets/images/cubemap/sky.ccm");
}

void GameState::createUI()
{
    auto& font = m_resources.fonts.get(m_fontID);
    font.loadFromFile("assets/fonts/VeraMono.ttf");

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 10.f, 60.f });
    entity.addComponent<cro::Text>(font).setString("Waiting for server...");
    entity.getComponent<cro::Text>().setColour(TextNormalColour);
    entity.addComponent<cro::CommandTarget>().ID = UI::CommandID::WaitMessage;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().projectionMatrix = 
        glm::ortho(0.f, static_cast<float>(cro::DefaultSceneSize.x), 0.f, static_cast<float>(cro::DefaultSceneSize.y), -0.1f, 100.f);

    m_uiScene.setActiveCamera(entity);

    updateView();
}

void GameState::updateView()
{
    glm::vec2 size(cro::App::getWindow().getSize());
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

    auto& cam3D = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam3D.projectionMatrix = glm::perspective(75.f * cro::Util::Const::degToRad, 16.f / 9.f, 0.1f, 1024.f);
    cam3D.viewport.bottom = (1.f - size.y) / 2.f;
    cam3D.viewport.height = size.y;

    auto& cam2D = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam2D.viewport = cam3D.viewport;
}

void GameState::handlePacket(const cro::NetEvent::Packet& packet)
{
    switch (packet.getID())
    {
    default: break;
    case PacketID::EntityRemoved:
    {
        auto entityID = packet.as<std::uint32_t>();
        cro::Command cmd;
        cmd.targetFlags = Client::CommandID::Interpolated;
        cmd.action = [&,entityID](cro::Entity e, float)
        {
            if (e.getComponent<Actor>().serverEntityId == entityID)
            {
                m_gameScene.destroyEntity(e);
            }
        };
        m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
    }
        break;
    case PacketID::PlayerSpawn:
        //TODO we want to flag all these + world data
        //so we know to stop requesting world data
        m_sharedData.clientConnection.ready = true;
        spawnPlayer(packet.as<PlayerInfo>());
        break;
    case PacketID::PlayerUpdate:
        //we assume we're only receiving our own
        m_gameScene.getSystem<PlayerSystem>().reconcile(m_inputParser.getEntity(), packet.as<PlayerUpdate>());
        break;
    case PacketID::ActorUpdate:
    {
        auto update = packet.as<ActorUpdate>();
        cro::Command cmd;
        cmd.targetFlags = Client::CommandID::Interpolated;
        cmd.action = [update](cro::Entity e, float)
        {
            if (e.isValid() &&
                e.getComponent<Actor>().serverEntityId == update.serverID)
            {
                auto& interp = e.getComponent<InterpolationComponent>();
                interp.setTarget({ update.position, Util::decompressQuat(update.rotation), update.timestamp });
            }
        };
        m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
    }
        break;
    case PacketID::ServerCommand:
    {
        auto data = packet.as<ServerCommand>();
        auto playerEnt = m_inputParser.getEntity();
        if (playerEnt.isValid() && playerEnt.getComponent<Player>().id == data.target)
        {
            switch (data.commandID)
            {
            default: break;
            case CommandPacket::SetModeFly:
                playerEnt.getComponent<Player>().flyMode = true;
                cro::Logger::log("Server set mode to fly", cro::Logger::Type::Info);
                break;
            case CommandPacket::SetModeWalk:
                playerEnt.getComponent<Player>().flyMode = false;
                cro::Logger::log("Server set mode to walk", cro::Logger::Type::Info);
                break;
            }
        }
    }
    break;
    case PacketID::ClientDisconnected:
        m_sharedData.playerData[packet.as<std::uint8_t>()].name.clear();
        break;
    case PacketID::ChunkData:
        m_gameScene.getSystem<ChunkSystem>().parseChunkData(packet);
        break;
    }
}

void GameState::spawnPlayer(PlayerInfo info)
{
    auto createActor = [&]()->cro::Entity
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(info.spawnPosition);
        entity.getComponent<cro::Transform>().setRotation(Util::decompressQuat(info.rotation));
            
        entity.addComponent<Actor>().id = info.playerID;
        entity.getComponent<Actor>().serverEntityId = info.serverID;
        return entity;
    };


    if (info.playerID == m_sharedData.clientConnection.playerID)
    {
        if (!m_inputParser.getEntity().isValid())
        {
            //this is us
            auto entity = createActor();

            auto rotation = entity.getComponent<cro::Transform>().getRotation();
            auto pitch = rotation.x;
            if (pitch > (cro::Util::Const::PI / 2.f))
            {
                pitch -= cro::Util::Const::PI;
            }
            
            entity.addComponent<Player>().id = info.playerID;
            entity.getComponent<Player>().spawnPosition = info.spawnPosition;
            //entity.getComponent<Player>().cameraPitch = pitch;
            //entity.getComponent<Player>().cameraYaw = rotation.y;


            playerEntity = entity;
            m_inputParser.setEntity(entity);

            auto& tx = entity.getComponent<cro::Transform>();

            //add the camera as a child so we can change
            //the perspective as necessary
            entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>().setOrigin(glm::vec3(0.f, -0.2f, 0.f));;
            tx.addChild(entity.getComponent<cro::Transform>());

            entity.addComponent<cro::Camera>();
            m_gameScene.setActiveCamera(entity);
            updateView();

            //TODO create a head/body that only gets drawn in third person


            //reove the plase wait message
            cro::Command cmd;
            cmd.targetFlags = UI::CommandID::WaitMessage;
            cmd.action = [&](cro::Entity e, float)
            {
                m_uiScene.destroyEntity(e);
                //cro::Logger::log("Fix deleting texts!", cro::Logger::Type::Warning);
            };
            m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
        }
    }
    else
    {
        //spawn an avatar
        //TODO check this avatar doesn't already exist
        
        auto entity = createActor();
        auto rotation = entity.getComponent<cro::Transform>().getRotationQuat();

        //TODO do we want to cache this model def?
        cro::ModelDefinition modelDef;
        modelDef.loadFromFile("assets/models/head.cmt", m_resources);

        entity.addComponent<cro::CommandTarget>().ID = Client::CommandID::Interpolated;
        entity.addComponent<InterpolationComponent>(InterpolationPoint(info.spawnPosition, rotation, info.timestamp));
        modelDef.createModel(entity, m_resources);

        auto headEnt = entity;

        //body model
        modelDef.loadFromFile("assets/models/body.cmt", m_resources);
        entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setOrigin({ 0.f, 0.55f, 0.f }); //TODO we need to get some sizes from the mesh - will AABB do?
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&, headEnt](cro::Entity e, float)
        {
            //remove this entity if the head entity was removed
            if (headEnt.destroyed())
            {
                e.getComponent<cro::Callback>().active = false;
                m_gameScene.destroyEntity(e);
            }
            else
            {
                e.getComponent<cro::Transform>().setPosition(headEnt.getComponent<cro::Transform>().getPosition());
            }
        };
        modelDef.createModel(entity, m_resources);
    }
}

void GameState::updateCameraPosition()
{
    m_cameraPosIndex = (m_cameraPosIndex + 1) % 3;
    auto& tx = m_gameScene.getActiveCamera().getComponent<cro::Transform>();
    switch (m_cameraPosIndex)
    {
    default: break;
    case 0:
        tx.setRotation(glm::quat(1.f, 0.f, 0.f, 0.f));
        tx.setOrigin(glm::vec3(0.f, -0.2f, 0.f));
        break;
    case 1:
        tx.setOrigin(glm::vec3(0.f, 0.f, -10.f)); //TODO update this once we settle on a scale (need smaller heads!)
        break;
    case 2:
        tx.rotate(glm::vec3(0.f, 1.f, 0.f), cro::Util::Const::PI);
        break;
    }
}