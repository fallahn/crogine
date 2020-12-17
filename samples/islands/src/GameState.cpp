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
#include "GameConsts.hpp"
#include "SeaSystem.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/graphics/CircleMeshBuilder.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    //for debug output
    cro::Entity playerEntity;
    std::int32_t bitrate = 0;
    std::int32_t bitrateCounter = 0;

    //render flags for reflection passes
    std::size_t NextPlayerPlane = 0; //index into this array when creating new player
    const std::uint64_t NoPlanes = 0xFFFFFFFFFFFFFF00;
    const std::array<std::uint64_t, 4u> PlayerPlanes =
    {
        0x1, 0x2,0x4,0x8
    };
    const std::uint64_t NoReflect = 0x10;
    const std::uint64_t NoRefract = 0x20;

    const std::array Colours =
    {
        cro::Colour::Red(), cro::Colour::Magenta(),
        cro::Colour::Green(), cro::Colour::Yellow()
    };
}

GameState::GameState(cro::StateStack& stack, cro::State::Context context, SharedStateData& sd, std::size_t localPlayers)
    : cro::State        (stack, context),
    m_sharedData        (sd),
    m_gameScene         (context.appInstance.getMessageBus()),
    m_uiScene           (context.appInstance.getMessageBus()),
    m_heightmap         (IslandTileCount * IslandTileCount, 1.f),
    m_localPlayerCount  (localPlayers)
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    });

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
    registerWindow([&]()
        {
            if (playerEntity.isValid())
            {
                ImGui::SetNextWindowSize({ 300.f, 320.f });
                ImGui::Begin("Info");

                ImGui::Text("Player ID: %d", m_sharedData.clientConnection.playerID);

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
#ifdef CRO_DEBUG_
        /*case SDLK_1:
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
        break;*/
#endif //CRO_DEBUG_
        }
    }

    for (auto& [id, parser] : m_inputParsers)
    {
        parser.handleEvent(evt);
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void GameState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
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


    for (auto& [id, parser] : m_inputParsers)
    {
        parser.update();
    }

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void GameState::render()
{
    if (m_cameras.empty())
    {
        return;
    }

    for (auto i = 0u; i < m_cameras.size(); ++i)
    {
        auto ent = m_cameras[i];
        m_gameScene.setActiveCamera(ent);

        auto& cam = ent.getComponent<cro::Camera>();
        cam.renderFlags = NoPlanes | NoRefract;
        auto oldVP = cam.viewport;

        cam.viewport = { 0.f,0.f,1.f,1.f };

        cam.setActivePass(cro::Camera::Pass::Reflection);
        cam.reflectionBuffer.clear(cro::Colour::Red());
        m_gameScene.render(cam.reflectionBuffer);
        cam.reflectionBuffer.display();

        cam.renderFlags = NoPlanes | NoReflect;
        cam.setActivePass(cro::Camera::Pass::Refraction);
        cam.refractionBuffer.clear(cro::Colour::Blue());
        m_gameScene.render(cam.refractionBuffer);
        cam.refractionBuffer.display();

        cam.renderFlags = NoPlanes | PlayerPlanes[i] | NoReflect | NoRefract;
        cam.setActivePass(cro::Camera::Pass::Final);
        cam.viewport = oldVP;
    }

    auto& rt = cro::App::getWindow();
    m_gameScene.render(rt, m_cameras);
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
    m_gameScene.addSystem<SeaSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
}

void GameState::loadAssets()
{
    m_meshIDs[MeshID::SeaPlane] = m_resources.meshes.loadMesh(cro::CircleMeshBuilder(SeaRadius, 30));

    m_islandTexture.create(IslandTileCount, IslandTileCount);
    m_environmentMap.loadFromFile("assets/images/cubemap/beach02.hdr");
    m_gameScene.setCubemap("assets/images/cubemap/sky.ccm");

    m_materialIDs[MaterialID::Sea] = m_gameScene.getSystem<SeaSystem>().loadResources(m_resources);
    m_resources.materials.get(m_materialIDs[MaterialID::Sea]).setProperty("u_depthMap", m_islandTexture);
    //m_resources.materials.get(m_materialIDs[MaterialID::Sea]).setProperty("u_foamMap", m_foamEffect.getTexture());

}

void GameState::createScene()
{

}

void GameState::createUI()
{

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
        //we assume we're receiving local
    {
        auto update = packet.as<PlayerUpdate>();
        if (m_inputParsers.count(update.playerID))
        {
            m_gameScene.getSystem<PlayerSystem>().reconcile(m_inputParsers.at(update.playerID).getEntity(), update);
        }
    }
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
        if (m_inputParsers.count(data.target) != 0)
        {
            auto playerEnt = m_inputParsers.at(data.target).getEntity();
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
    }
    break;
    case PacketID::ClientDisconnected:
        m_sharedData.playerData[packet.as<std::uint8_t>()].name.clear();
        break;
    }
}

void GameState::spawnPlayer(PlayerInfo info)
{
    //used for both local and remote
    auto createActor = [&]()->cro::Entity
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(info.spawnPosition);
        entity.getComponent<cro::Transform>().setRotation(Util::decompressQuat(info.rotation));
            
        entity.addComponent<Actor>().id = info.playerID;
        entity.getComponent<Actor>().serverEntityId = info.serverID;
        return entity;
    };

    //placeholder for player scale
    cro::ModelDefinition md;
    md.loadFromFile("assets/models/player_box.cmt", m_resources, &m_environmentMap);

    auto addSeaplane = [&]()
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -cro::Util::Const::PI / 2.f);
        entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_meshIDs[MeshID::SeaPlane]), m_resources.materials.get(m_materialIDs[MaterialID::Sea]));
        entity.addComponent<SeaComponent>();

        return entity;
    };


    if (info.playerID == m_sharedData.clientConnection.playerID)
    {
        if (m_inputParsers.count(info.playerID) == 0)
        {
            //this is us - the root is controlled by the player and the
            //avatar and sea planes are connected as children
            auto root = createActor();
            
            root.addComponent<Player>().id = info.playerID;
            root.getComponent<Player>().spawnPosition = info.spawnPosition;

            m_inputParsers.insert(std::make_pair(info.playerID, InputParser(m_sharedData.clientConnection.netClient)));
            m_inputParsers.at(info.playerID).setEntity(root);





            m_cameras.emplace_back();
            m_cameras.back() = m_gameScene.createEntity();
            m_cameras.back().addComponent<cro::Transform>().setPosition({ 0.f, CameraHeight, CameraDistance });

            auto rotation = glm::lookAt(glm::vec3(0.f), glm::vec3(0.f, 0.f, -SeaRadius), cro::Transform::Y_AXIS);
            m_cameras.back().getComponent<cro::Transform>().rotate(glm::inverse(rotation));

            auto& cam = m_cameras.back().addComponent<cro::Camera>();
            cam.reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
            cam.reflectionBuffer.setSmooth(true);
            cam.refractionBuffer.create(ReflectionMapSize, ReflectionMapSize);
            cam.refractionBuffer.setSmooth(true);

            auto waterEnt = addSeaplane();
            waterEnt.getComponent<cro::Model>().setRenderFlags(PlayerPlanes[NextPlayerPlane++]);
            waterEnt.addComponent<cro::Callback>().active = true;
            waterEnt.getComponent<cro::Callback>().function =
                [&](cro::Entity e, float)
            {
                e.getComponent<cro::Transform>().setPosition({ 0.f, m_gameScene.getWaterLevel(), -(SeaRadius - CameraDistance) });
            };

            //placeholder for player model
            auto playerEnt = m_gameScene.createEntity();
            playerEnt.addComponent<cro::Transform>().setOrigin({ 0.f, -0.8f, 0.f });
            md.createModel(playerEnt, m_resources);
            playerEnt.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", Colours[NextPlayerPlane - 1]);

            playerEnt.addComponent<cro::Callback>().active = true;
            playerEnt.getComponent<cro::Callback>().function =
                [&](cro::Entity e, float)
            {
                //TODO move this to player system so it's properly sync'd with server
                auto position = e.getComponent<cro::Transform>().getWorldPosition();
                position.x += (IslandSize / 2.f); //puts the position relative to the grid - this should be the origin coords
                position.z += (IslandSize / 2.f);

                auto height = getPlayerHeight(position);
                e.getComponent<cro::Transform>().setPosition({ 0.f, height + IslandWorldHeight, 0.f });
            };







            root.getComponent<cro::Transform>().addChild(m_cameras.back().getComponent<cro::Transform>());
            root.getComponent<cro::Transform>().addChild(waterEnt.getComponent<cro::Transform>());
            root.getComponent<cro::Transform>().addChild(playerEnt.getComponent<cro::Transform>());
        }


        if (m_cameras.size() == 1)
        {
            //this is the first to join
            m_gameScene.setActiveCamera(m_cameras[0]);

            //main camera - updateView() updates all cameras so we only need one callback
            auto camEnt = m_gameScene.getActiveCamera();
            updateView(camEnt.getComponent<cro::Camera>());
            camEnt.getComponent<cro::Camera>().resizeCallback = std::bind(&GameState::updateView, this, std::placeholders::_1);
        }
        else
        {
            //resizes the client views to match number of players
            updateView(m_cameras[0].getComponent<cro::Camera>());
        }
    }
    else
    {
        //spawn an avatar
        //TODO check this avatar doesn't already exist
        auto entity = createActor();
    }
}

void GameState::updateView(cro::Camera&)
{
    CRO_ASSERT(!m_cameras.empty(), "Need at least one camera!");

    const float fov = 42.f * cro::Util::Const::degToRad;
    const float nearPlane = 0.1f;
    const float farPlane = IslandSize * 1.8f;
    float aspect = 16.f / 9.f;

    glm::vec2 size(cro::App::getWindow().getSize());
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;
    cro::FloatRect viewport(0.f, (1.f - size.y) / 2.f, size.x, size.y);

    //update the UI camera to match the new screen size
    auto& cam2D = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam2D.viewport = viewport;

    //set up the viewports
    switch (m_cameras.size())
    {
    case 1:
        m_cameras[0].getComponent<cro::Camera>().viewport = viewport;
        break;
    case 2:
        aspect = 8.f / 9.f;
        viewport.width /= 2.f;
        m_cameras[0].getComponent<cro::Camera>().viewport = viewport;
        viewport.left = viewport.width;
        m_cameras[1].getComponent<cro::Camera>().viewport = viewport;
        break;

    case 3:
    case 4:
        viewport.width /= 2.f;
        viewport.height /= 2.f;
        viewport.bottom += viewport.height;
        m_cameras[0].getComponent<cro::Camera>().viewport = viewport;

        viewport.left = viewport.width;
        m_cameras[1].getComponent<cro::Camera>().viewport = viewport;

        viewport.left = 0.f;
        viewport.bottom -= viewport.height;
        m_cameras[2].getComponent<cro::Camera>().viewport = viewport;

        if (m_cameras.size() == 4)
        {
            viewport.left = viewport.width;
            m_cameras[3].getComponent<cro::Camera>().viewport = viewport;
        }
        break;

    default:
        break;
    }

    //set up projection
    for (auto cam : m_cameras)
    {
        cam.getComponent<cro::Camera>().projectionMatrix = glm::perspective(fov, aspect, nearPlane, farPlane);
    }
}

float GameState::getPlayerHeight(glm::vec3 position)
{
    auto lerp = [](float a, float b, float t) constexpr
    {
        return a + t * (b - a);
    };

    const auto getHeightAt = [&](std::int32_t x, std::int32_t y)
    {
        //heightmap is flipped relative to the world innit
        x = std::min(static_cast<std::int32_t>(IslandTileCount), std::max(0, x));
        y = std::min(static_cast<std::int32_t>(IslandTileCount), std::max(0, y));
        return m_heightmap[(IslandTileCount - y) * IslandTileCount + x];
    };

    float posX = position.x / TileSize;
    float posY = position.z / TileSize;

    float intpart = 0.f;
    auto modX = std::modf(posX, &intpart) / TileSize;
    auto modY = std::modf(posY, &intpart) / TileSize; //normalise this for lerpitude

    std::int32_t x = static_cast<std::int32_t>(posX);
    std::int32_t y = static_cast<std::int32_t>(posY);

    float topLine = lerp(getHeightAt(x, y), getHeightAt(x + 1, y), modX);
    float botLine = lerp(getHeightAt(x, y + 1), getHeightAt(x + 1, y + 1), modX);
    return lerp(topLine, botLine, modY) * IslandHeight;
};