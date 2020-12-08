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
#include "BorderMeshBuilder.hpp"
#include "ErrorCheck.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Matrix.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/GlobalConsts.hpp>
#include <crogine/detail/OpenGL.hpp>

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

                auto voxelPos = playerEntity.getComponent<Player>().targetBlockPosition;
                ImGui::Text("Target Position: %d, %d, %d", voxelPos.x, voxelPos.y, voxelPos.z);
                auto voxelType = m_chunkManager.getVoxel(voxelPos);
                ImGui::Text("Target Type: %d", voxelType);

                ImGui::Text("Pitch: %3.3f", playerEntity.getComponent<Player>().cameraPitch);
                ImGui::Text("Yaw: %3.3f", playerEntity.getComponent<Player>().cameraYaw);

                auto forward = m_gameScene.getActiveCamera().getComponent<cro::Transform>().getForwardVector();
                ImGui::Text("Forward: %3.3f, %3.3f, %3.3f", forward.x, forward.y, forward.z);

                auto mouse = playerEntity.getComponent<Player>().inputStack[playerEntity.getComponent<Player>().lastUpdatedInput];
                ImGui::Text("Mouse Movement: %d, %d", mouse.xMove, mouse.yMove);

                ImGui::NewLine();
                ImGui::Separator();
                ImGui::NewLine();
                ImGui::Text("Application average:\n%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
                ImGui::NewLine();
                ImGui::Text("Connection Bitrate: %3.3fkbps", static_cast<float>(bitrate) / 1024.f);

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
            LOG("Switched to naive mesh", cro::Logger::Type::Info);
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
            LOG("Switched to greedy mesh", cro::Logger::Type::Info);
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

    if (msg.id == cro::Message::ConsoleMessage)
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
    m_gameScene.addSystem<InterpolationSystem>(mb);
    m_gameScene.addSystem<PlayerSystem>(mb, m_chunkManager, m_voxelData);
    m_gameScene.addSystem<cro::CallbackSystem>(mb); //currently used to update body model positions so needs to come after player update
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<ChunkSystem>(mb, m_resources, m_chunkManager, m_voxelData);
    m_gameScene.addSystem<cro::SkeletalAnimator>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void GameState::loadAssets()
{
    auto& shader = m_resources.shaders.get(ShaderID::ChunkDebug);
    m_materialIDs[MaterialID::ChunkDebug] = m_resources.materials.add(shader);
    m_meshIDs[MeshID::Border] = m_resources.meshes.loadMesh(BorderMeshBuilder());

    //glCheck(glLineWidth(1.5f));
    //glCheck(glEnable(GL_LINE_SMOOTH));
}

void GameState::createScene()
{
    m_gameScene.setCubemap("assets/images/cubemap/sky.ccm");
    m_gameScene.getSunlight().getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, -cro::Util::Const::PI / 2.f);
}

void GameState::createUI()
{
    m_resources.fonts.load(m_fontID, "assets/fonts/VeraMono.ttf");
    auto& font = m_resources.fonts.get(m_fontID);

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 10.f, 60.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Waiting for server...");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::CommandTarget>().ID = UI::CommandID::WaitMessage;

    //player crosshair
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(cro::DefaultSceneSize.x / 2, cro::DefaultSceneSize.y / 2));
    entity.getComponent<cro::Transform>().setScale({ 2.f, 2.f });
    entity.addComponent<cro::Sprite>().setTexture(m_resources.textures.get("assets/images/hud.png"));
    auto bounds = entity.getComponent<cro::Sprite>().getTextureRect();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::Drawable2D>();

    //camera
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().projectionMatrix = 
        glm::ortho(0.f, static_cast<float>(cro::DefaultSceneSize.x), 0.f, static_cast<float>(cro::DefaultSceneSize.y), -0.1f, 100.f);

    m_uiScene.setActiveCamera(entity);
}

void GameState::updateView(cro::Camera& cam3D)
{
    glm::vec2 size(cro::App::getWindow().getSize());
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

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
    auto createActor = [&](bool hideBody)->cro::Entity
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(info.spawnPosition);
        entity.getComponent<cro::Transform>().setRotation(Util::decompressQuat(info.rotation));
            
        entity.addComponent<Actor>().id = info.playerID;
        entity.getComponent<Actor>().serverEntityId = info.serverID;

        cro::ModelDefinition modelDef;
        modelDef.loadFromFile("assets/models/head.cmt", m_resources);
        modelDef.createModel(entity, m_resources);

        auto headEnt = entity;

        //body model
        modelDef.loadFromFile("assets/models/body_animated.cmt", m_resources);
        entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        entity.addComponent<cro::CommandTarget>().ID = Client::CommandID::BodyMesh;
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().userData = std::make_any<std::uint8_t>(info.playerID); //this is used to ID the body model when hiding it
        entity.getComponent<cro::Callback>().function =
            [&, headEnt](cro::Entity e, float dt)
        {
            //remove this entity if the head entity was removed
            if (headEnt.destroyed())
            {
                e.getComponent<cro::Callback>().active = false;
                m_gameScene.destroyEntity(e);
            }
            else
            {
                const auto& headTx = headEnt.getComponent<cro::Transform>();
                const auto& mat = headTx.getLocalTransform();
                float y = -std::atan2(mat[0][2], mat[0][0]);

                auto& tx = e.getComponent<cro::Transform>();
                //auto currentY = tx.getRotation().z;
                //auto rotation = cro::Util::Maths::shortestRotation(currentY, y);

                //if (std::abs(rotation) < 0.5f)
                {
                    tx.setRotation(cro::Transform::Y_AXIS, y);
                }
                /*else
                {
                    tx.rotate(glm::vec3(0.f, 1.f, 0.f), rotation * dt * 4.f);
                }*/

                tx.setPosition(headTx.getPosition());

                //TODO interpolate rotation to delay slightly
            }
        };
        modelDef.createModel(entity, m_resources);
        entity.getComponent<cro::Model>().setHidden(hideBody);
        entity.getComponent<cro::Skeleton>().play(0);

        //used to draw AABB
        entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();// .setOrigin(-Player::aabb[0]);
        entity.getComponent<cro::Transform>().setScale(Player::aabb.getSize());
        auto material = m_resources.materials.get(m_materialIDs[MaterialID::ChunkDebug]);
        material.setProperty("u_colour", cro::Colour::White());
        entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_meshIDs[MeshID::Border]), material);
        entity.getComponent<cro::Model>().setHidden(true);
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&, headEnt](cro::Entity e, float)
        {
            if (headEnt.destroyed())
            {
                m_gameScene.destroyEntity(e);
            }
            else
            {
                e.getComponent<cro::Transform>().setPosition(
                    headEnt.getComponent<cro::Transform>().getPosition() + Player::aabb[0]);
            }
        };
        entity.addComponent<cro::CommandTarget>().ID = Client::CommandID::DebugMesh;

        return headEnt;
    };

    if (info.playerID == m_sharedData.clientConnection.playerID)
    {
        if (!m_inputParser.getEntity().isValid())
        {
            //this is us
            auto entity = createActor(true);

            auto rotation = entity.getComponent<cro::Transform>().getRotation();
            auto pitch = rotation.x;
            if (pitch > (cro::Util::Const::PI / 2.f))
            {
                pitch -= cro::Util::Const::PI;
            }
            
            entity.addComponent<Player>().id = info.playerID;
            entity.getComponent<Player>().spawnPosition = info.spawnPosition;
            entity.getComponent<Player>().cameraPitch = pitch;
            entity.getComponent<Player>().cameraYaw = rotation.y;


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

            auto camEnt = entity;
            updateView(camEnt.getComponent<cro::Camera>());
            camEnt.getComponent<cro::Camera>().resizeCallback = std::bind(&GameState::updateView, this, std::placeholders::_1);

            //create a wireframe to highlight the block we look at
            //scaling by a small amount fixes the z-fighting
            entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>().setScale(glm::vec3(1.01f));


            auto material = m_resources.materials.get(m_materialIDs[MaterialID::ChunkDebug]);
            material.setProperty("u_colour", cro::Colour::Black());

            entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_meshIDs[MeshID::Border]), material);
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [&](cro::Entity e, float)
            {
                const auto& player = playerEntity.getComponent<Player>();
                e.getComponent<cro::Model>().setHidden(m_voxelData.getVoxel(m_chunkManager.getVoxel(player.targetBlockPosition)).type != vx::Type::Solid);
                e.getComponent<cro::Transform>().setPosition(glm::vec3(player.targetBlockPosition) - glm::vec3(0.005f));
            };


            //remove the please wait message
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
        
        auto entity = createActor(false);
        auto rotation = entity.getComponent<cro::Transform>().getRotation();

        entity.addComponent<cro::CommandTarget>().ID = Client::CommandID::Interpolated;
        entity.addComponent<InterpolationComponent>(InterpolationPoint(info.spawnPosition, rotation, info.timestamp));
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

        {
            cro::Command cmd;
            cmd.targetFlags = Client::CommandID::BodyMesh;
            cmd.action = [&](cro::Entity e, float)
            {
                auto id = std::any_cast<std::uint8_t>(e.getComponent<cro::Callback>().userData);
                if (id == m_sharedData.clientConnection.playerID)
                {
                    e.getComponent<cro::Model>().setHidden(true);
                }
            };
            m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
        }

        break;
    case 1:
        tx.setOrigin(glm::vec3(0.f, 0.f, -3.f)); //TODO update this once we settle on a scale (need smaller heads!)

        {
            cro::Command cmd;
            cmd.targetFlags = Client::CommandID::BodyMesh;
            cmd.action = [&](cro::Entity e, float)
            {
                auto id = std::any_cast<std::uint8_t>(e.getComponent<cro::Callback>().userData);
                if (id == m_sharedData.clientConnection.playerID)
                {
                    e.getComponent<cro::Model>().setHidden(false);
                }
            };
            m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
        }

        break;
    case 2:
        tx.rotate(glm::vec3(0.f, 1.f, 0.f), cro::Util::Const::PI);
        break;
    }
}