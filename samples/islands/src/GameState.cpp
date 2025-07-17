/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2022
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
#include "SeaSystem.hpp"
#include "DayNightDirector.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/ShadowCaster.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/graphics/CircleMeshBuilder.hpp>
#include <crogine/graphics/GridMeshBuilder.hpp>
#include <crogine/graphics/MeshBatch.hpp>
#include <crogine/graphics/Image.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Network.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

#include <cstring>

namespace
{
    //for debug output
    cro::Entity playerEntity;
    std::size_t bitrate = 0;
    std::size_t bitrateCounter = 0;

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
        cro::Colour::Red, cro::Colour::Magenta,
        cro::Colour::Green, cro::Colour::Yellow
    };
}

GameState::GameState(cro::StateStack& stack, cro::State::Context context, SharedStateData& sd)
    : cro::State        (stack, context),
    m_sharedData        (sd),
    m_gameScene         (context.appInstance.getMessageBus()),
    m_uiScene           (context.appInstance.getMessageBus()),
    m_requestFlags      (0),
    m_dataRequestCount  (0),
    m_foamEffect        (m_resources),
    m_heightmap         (IslandTileCount * IslandTileCount, 1.f)
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
            ImGui::SetNextWindowSize({ 300.f, 320.f });
            if (ImGui::Begin("Info"))
            {
                if (playerEntity.isValid())
                {
                    ImGui::Text("Player ID: %d", m_sharedData.clientConnection.connectionID);

                    ImGui::NewLine();
                    ImGui::Separator();
                    ImGui::NewLine();
                }

                ImGui::Text("Bitrate: %3.3fkbps", static_cast<float>(bitrate) / 1024.f);


                auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
                float maxDist = cam.getMaxShadowDistance();
                if (ImGui::SliderFloat("Shadow Dist", &maxDist, 1.f, IslandSize))
                {
                    cam.setMaxShadowDistance(maxDist);
                }

                float exp = cam.getShadowExpansion();
                if (ImGui::SliderFloat("Expansion", &exp, 0.f, 50.f))
                {
                    cam.setShadowExpansion(exp);
                }
            }
            ImGui::End();

            if (ImGui::Begin("Textures"))
            {
                if (ImGui::CollapsingHeader("Height Map"))
                {
                    ImGui::Image(m_islandTexture, { 256.f, 256.f }, { 0.f, 1.f }, { 1.f, 0.f });
                }
            }
            ImGui::End();
        });
}

//public
bool GameState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() 
        || cro::ui::wantsKeyboard())
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

    if (m_dataRequestCount == MaxDataRequests)
    {
        m_sharedData.errorMessage = "Failed to download\ndata from the server";
        requestStackPush(States::Error);

        return false;
    }

    //if we haven't had all the data yet ask for it
    if (m_requestFlags != ClientRequestFlags::All)
    {
        if (m_sceneRequestClock.elapsed().asMilliseconds() > 1000)
        {
            m_sceneRequestClock.restart();
            m_dataRequestCount++;

            if ((m_requestFlags & ClientRequestFlags::Heightmap) == 0)
            {
                std::uint16_t data = (m_sharedData.clientConnection.connectionID << 8) | ClientRequestFlags::Heightmap;
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::RequestData, data, cro::NetFlag::Reliable);
            }
            //these require the heightmap has already been received
            else
            {
                if ((m_requestFlags & ClientRequestFlags::TreeMap) == 0)
                {
                    std::uint16_t data = (m_sharedData.clientConnection.connectionID << 8) | ClientRequestFlags::TreeMap;
                    m_sharedData.clientConnection.netClient.sendPacket(PacketID::RequestData, data, cro::NetFlag::Reliable);
                }

                if ((m_requestFlags & ClientRequestFlags::BushMap) == 0)
                {
                    std::uint16_t data = (m_sharedData.clientConnection.connectionID << 8) | ClientRequestFlags::BushMap;
                    m_sharedData.clientConnection.netClient.sendPacket(PacketID::RequestData, data, cro::NetFlag::Reliable);
                }
            }
        }
    }
    else if (!m_sharedData.clientConnection.ready)
    {
        m_sharedData.clientConnection.netClient.sendPacket(PacketID::ClientReady, m_sharedData.clientConnection.connectionID, cro::NetFlag::Reliable);
        m_sharedData.clientConnection.ready = true;
    }

    for (auto& [id, parser] : m_inputParsers)
    {
        parser.update();
    }

    static float timeAccum = 0.f;
    timeAccum += dt;
    m_gameScene.setWaterLevel(std::sin(timeAccum * 0.9f) * 0.08f);

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void GameState::render()
{
    m_foamEffect.update();

    if (m_cameras.empty())
    {
        return;
    }

    for (auto i = 0u; i < m_cameras.size(); ++i)
    {
        auto ent = m_cameras[i];
        m_gameScene.setActiveCamera(ent);

        auto& cam = ent.getComponent<cro::Camera>();
        cam.setRenderFlags(cro::Camera::Pass::Reflection, NoPlanes | NoRefract);
        auto oldVP = cam.viewport;

        cam.viewport = { 0.f,0.f,1.f,1.f };

        cam.setActivePass(cro::Camera::Pass::Reflection);
        cam.reflectionBuffer.clear(cro::Colour::Red);
        m_gameScene.render();
        cam.reflectionBuffer.display();

        cam.setRenderFlags(cro::Camera::Pass::Refraction, NoPlanes | NoReflect);
        cam.setActivePass(cro::Camera::Pass::Refraction);
        cam.refractionBuffer.clear(cro::Colour::Blue);
        m_gameScene.render();
        cam.refractionBuffer.display();

        cam.setRenderFlags(cro::Camera::Pass::Final, NoPlanes | PlayerPlanes[i] | NoReflect | NoRefract);
        cam.setActivePass(cro::Camera::Pass::Final);
        cam.viewport = oldVP;
    }

    auto& rt = cro::App::getWindow();
    m_gameScene.render(m_cameras);
    m_uiScene.render();
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
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_gameScene.addDirector<DayNightDirector>();
}

void GameState::loadAssets()
{
    m_meshIDs[MeshID::SeaPlane] = m_resources.meshes.loadMesh(cro::CircleMeshBuilder(SeaRadius, 30));

    m_islandTexture.create(IslandTileCount, IslandTileCount);
    m_environmentMap.loadFromFile("assets/images/cubemap/beach01.hdr");
    m_skyMap.loadFromFile("assets/images/cubemap/skybox.hdr");
    m_gameScene.setCubemap(m_skyMap);
    //m_gameScene.setCubemap("assets/images/cubemap/sky.ccm");

    m_materialIDs[MaterialID::Sea] = m_gameScene.getSystem<SeaSystem>()->loadResources(m_resources);
    m_resources.materials.get(m_materialIDs[MaterialID::Sea]).setProperty("u_depthMap", m_islandTexture);
    m_resources.materials.get(m_materialIDs[MaterialID::Sea]).setProperty("u_foamMap", m_foamEffect.getTexture());

    for (auto& md : m_modelDefs)
    {
        md = std::make_unique<cro::ModelDefinition>(m_resources, &m_environmentMap);
    }

    m_modelDefs[GameModelID::GroundBush]->loadFromFile("assets/models/ground_plant01.cmt", true);
    m_modelDefs[GameModelID::Palm01]->loadFromFile("assets/models/palm01.cmt", true);
    m_modelDefs[GameModelID::Palm02]->loadFromFile("assets/models/palm02.cmt", true);
    m_modelDefs[GameModelID::Shrub01]->loadFromFile("assets/models/shrub01.cmt", true);
    m_modelDefs[GameModelID::Shrub02]->loadFromFile("assets/models/shrub02.cmt", true);
    m_modelDefs[GameModelID::Shrub03]->loadFromFile("assets/models/shrub03.cmt", true);
    m_modelDefs[GameModelID::Shrub04]->loadFromFile("assets/models/shrub04.cmt", true);
    m_modelDefs[GameModelID::Shrub05]->loadFromFile("assets/models/shrub05.cmt", true);
    m_modelDefs[GameModelID::Shrub06]->loadFromFile("assets/models/shrub06.cmt", true);
   

    loadIslandAssets();
}

void GameState::createScene()
{
    createDayCycle();
}

void GameState::createUI()
{

}

void GameState::createDayCycle()
{
    auto rootNode = m_gameScene.createEntity();
    rootNode.addComponent<cro::Transform>();
    rootNode.addComponent<cro::CommandTarget>().ID = Client::CommandID::SunMoonNode;
    auto& children = rootNode.addComponent<ChildNode>();

    //moon
    auto entity = m_gameScene.createEntity();
    //we want to always be beyond the 'horizon', but we're fixed relative to the centre of the island
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -SunOffset });
    children.moonNode = entity;

    cro::ModelDefinition definition(m_resources);
    definition.loadFromFile("assets/models/moon.cmt");
    definition.createModel(entity);
    entity.getComponent<cro::Model>().setRenderFlags(NoRefract);
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //sun
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, SunOffset });
    children.sunNode = entity;

    definition.loadFromFile("assets/models/sun.cmt");
    definition.createModel(entity);
    entity.getComponent<cro::Model>().setRenderFlags(NoRefract);
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //add the shadow caster node to the day night cycle
    auto sunNode = m_gameScene.getSunlight();
    sunNode.getComponent<cro::Transform>().setPosition({ 0.f, 0.f, SeaRadius });
    sunNode.addComponent<TargetTransform>();
    rootNode.getComponent<cro::Transform>().addChild(sunNode.getComponent<cro::Transform>());
}

void GameState::handlePacket(const cro::NetEvent::Packet& packet)
{
    switch (packet.getID())
    {
    default: break;
    case PacketID::DayNightUpdate:
        m_gameScene.getDirector<DayNightDirector>()->setTimeOfDay(cro::Util::Net::decompressFloat(packet.as<std::int16_t>(), 8));
        break;
    case PacketID::Heightmap:
        updateHeightmap(packet);
        break;
    case PacketID::Bushmap:
        updateBushmap(packet);
        break;
    case PacketID::Treemap:
        updateTreemap(packet);
        break;
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
        m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
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
            m_gameScene.getSystem<PlayerSystem>()->reconcile(m_inputParsers.at(update.playerID).getEntity(), update);
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
                interp.setTarget({ update.position, cro::Util::Net::decompressQuat(update.rotation), update.timestamp });
            }
        };
        m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
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
        entity.getComponent<cro::Transform>().setRotation(cro::Util::Net::decompressQuat(info.rotation));
            
        entity.addComponent<Actor>().id = info.playerID + info.connectionID;
        entity.getComponent<Actor>().serverEntityId = info.serverID;
        return entity;
    };

    //placeholder for player scale
    cro::ModelDefinition md(m_resources, &m_environmentMap);
    md.loadFromFile("assets/models/pirate.cmt");

    auto addSeaplane = [&]()
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -cro::Util::Const::PI / 2.f);
        entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_meshIDs[MeshID::SeaPlane]), m_resources.materials.get(m_materialIDs[MaterialID::Sea]));
        entity.addComponent<SeaComponent>();

        return entity;
    };


    if (info.connectionID == m_sharedData.clientConnection.connectionID)
    {
        if (m_inputParsers.count(info.playerID) == 0)
        {
            //this is us - the root is controlled by the player and the
            //avatar and sea planes are connected as children
            auto root = createActor();
            
            root.addComponent<Player>().id = info.playerID;
            root.getComponent<Player>().connectionID = info.connectionID;
            root.getComponent<Player>().spawnPosition = info.spawnPosition;

            m_inputParsers.insert(std::make_pair(info.playerID, InputParser(m_sharedData.clientConnection.netClient, m_sharedData.inputBindings[info.playerID])));
            m_inputParsers.at(info.playerID).setEntity(root);


            m_cameras.emplace_back();
            m_cameras.back() = m_gameScene.createEntity();
            m_cameras.back().addComponent<cro::Transform>().setPosition({ 0.f, CameraHeight, CameraDistance });

            auto rotation = glm::lookAt(m_cameras.back().getComponent<cro::Transform>().getPosition(), glm::vec3(0.f, 0.f, -SeaRadius), cro::Transform::Y_AXIS);
            m_cameras.back().getComponent<cro::Transform>().rotate(glm::inverse(rotation));

            auto& cam = m_cameras.back().addComponent<cro::Camera>();
            cam.reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
            cam.reflectionBuffer.setSmooth(true);
            cam.refractionBuffer.create(ReflectionMapSize, ReflectionMapSize);
            cam.refractionBuffer.setSmooth(true);
            cam.shadowMapBuffer.create(1024, 1024);// (4096, 4096);

            auto waterEnt = addSeaplane();
            waterEnt.getComponent<cro::Model>().setRenderFlags(PlayerPlanes[NextPlayerPlane++]);
            waterEnt.addComponent<cro::Callback>().active = true;
            waterEnt.getComponent<cro::Callback>().function =
                [&](cro::Entity e, float)
            {
                e.getComponent<cro::Transform>().setPosition({ 0.f, m_gameScene.getWaterLevel(), -(SeaRadius - CameraDistance) });
            };

            //placeholder for player model
            //don't worry about mis-matching colour IDs... well be setting avatars differently

            auto playerEnt = m_gameScene.createEntity();
            playerEnt.addComponent<cro::Transform>().setScale({ 2.f, 2.f, 2.f });// .setOrigin({ 0.f, -0.8f, 0.f });
            md.createModel(playerEnt);
            //playerEnt.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", Colours[info.playerID + info.connectionID]);

            root.getComponent<Player>().avatar = playerEnt;
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
        md.createModel(entity);

        auto rotation = entity.getComponent<cro::Transform>().getRotation();
        entity.getComponent<cro::Transform>().setOrigin({ 0.f, -0.8f, 0.f });
        //entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", Colours[info.playerID + info.connectionID]);

        entity.addComponent<cro::CommandTarget>().ID = Client::CommandID::Interpolated;
        entity.addComponent<InterpolationComponent>(InterpolationPoint(info.spawnPosition, rotation, info.timestamp));
    }
}

void GameState::updateView(cro::Camera&)
{
    CRO_ASSERT(!m_cameras.empty(), "Need at least one camera!");

    const float fov = 36.f * cro::Util::Const::degToRad;
    const float nearPlane = 0.1f;
    const float farPlane = IslandSize * 1.7f;
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
        cam.getComponent<cro::Camera>().setPerspective(fov, aspect, nearPlane, farPlane, 3);
        cam.getComponent<cro::Camera>().setShadowExpansion(20.f);
    }
}

void GameState::updateHeightmap(const cro::NetEvent::Packet& packet)
{
    auto size = packet.getSize();
    if (size % sizeof(float) != 0)
    {
        //something is wrong, but we'll automatically request again
        //so we should count the number of times failed as an exit strat
        m_dataRequestCount++;
    }
    else
    {
        std::memcpy(m_heightmap.data(), packet.getData(), size);

        m_gameScene.getSystem<PlayerSystem>()->setHeightmap(m_heightmap);

        //preview texture / height map
        cro::Image img;
        img.create(IslandTileCount, IslandTileCount, cro::Colour::Black);
        for (auto i = 0u; i < m_heightmap.size(); ++i)
        {
            auto level = m_heightmap[i] * 255.f;
            auto channel = static_cast<std::uint8_t>(level);
            cro::Colour c(channel, channel, channel);

            auto x = i % IslandTileCount;
            auto y = i / IslandTileCount;
            img.setPixel(x, y, c);
        }

        m_islandTexture.update(img.getPixelData());
        m_islandTexture.setSmooth(true);

        updateIslandVerts(m_heightmap);

        m_requestFlags |= ClientRequestFlags::Heightmap;

        //spawn boats now that we have a heightmap
        cro::ModelDefinition modelDef(m_resources, &m_environmentMap);
        modelDef.loadFromFile("assets/models/boat.cmt");

        for (auto i = 0u; i < BoatSpawns.size(); ++i)
        {
            auto spawn = BoatSpawns[i];
            auto testPoint = spawn;
            testPoint.x += (IslandSize / 2.f); //puts the position relative to the grid - this should be the origin coords
            testPoint.z += (IslandSize / 2.f);

            spawn.y = readHeightmap(testPoint, m_heightmap) + IslandWorldHeight;
            spawn.y -= 0.2f;

            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(spawn);
            entity.getComponent<cro::Transform>().setScale({ 1.6f,1.6f,1.6f });
            entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, ((cro::Util::Const::PI / 2.f) * i) - (cro::Util::Const::PI / 4.f));
            modelDef.createModel(entity);
        }
    }
}

void GameState::updateBushmap(const cro::NetEvent::Packet& packet)
{
    auto size = packet.getSize();
    if (size % sizeof(glm::vec2) != 0)
    {
        //count the failure and wait for next try
        m_dataRequestCount++;
    }
    else
    {       
        std::vector<std::vector<glm::mat4>> transforms(6);
        std::vector<glm::mat4> shrubTransforms;

        std::vector<glm::vec2> positions(size / sizeof(glm::vec2));
        std::memcpy(positions.data(), packet.getData(), size);

        bool shrub = false;
        for(auto i = 0u; i < positions.size(); ++i)
        {
            auto p = positions[i];
            float y = readHeightmap({ p.x, 0.f, p.y  }, m_heightmap) + IslandWorldHeight;
            p.x -= (IslandSize / 2.f); //offset from the centre
            p.y -= (IslandSize / 2.f);

            glm::mat4 tx = glm::translate(glm::mat4(1.f), glm::vec3(p.x, y, p.y));
            tx = glm::rotate(tx, cro::Util::Const::TAU / cro::Util::Random::value(1, 8), cro::Transform::Y_AXIS);
            tx = glm::scale(tx, glm::vec3(2.f + static_cast<float>(cro::Util::Random::value(-23, 24)) / 100.f));

            if (!shrub)
            {
                transforms[i % 6].push_back(tx);
            }
            else
            {
                //ground shrub
                shrubTransforms.push_back(tx);
            }

            if ((i % 6) == 5)
            {
                shrub = !shrub;
            }
        }

        auto i = 0;
        for (auto& tx : transforms)
        {
            std::sort(tx.begin(), tx.end(),
                [](const glm::mat4& a, const glm::mat4& b)
                {
                    return a[3].z > b[3].z;
                });


            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>();
            m_modelDefs[GameModelID::Shrub01 + i]->createModel(entity);
            entity.getComponent<cro::Model>().setInstanceTransforms(tx);

            i++;
        }

        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        m_modelDefs[GameModelID::GroundBush]->createModel(entity);
        entity.getComponent<cro::Model>().setInstanceTransforms(shrubTransforms);

        m_requestFlags |= ClientRequestFlags::BushMap;
    }
}

void GameState::updateTreemap(const cro::NetEvent::Packet& packet)
{
    auto size = packet.getSize();
    if (size % sizeof(glm::vec2) != 0)
    {
        //count the failure and wait for next try
        m_dataRequestCount++;
    }
    else
    {
        std::vector<glm::vec2> positions(size / sizeof(glm::vec2));
        std::memcpy(positions.data(), packet.getData(), size);

        std::vector<std::vector<glm::mat4>> transforms(2);
        for (auto i = 0u; i < positions.size(); ++i)
        {
            auto p = positions[i];
            float y = readHeightmap({ p.x, 0.f, p.y }, m_heightmap) + IslandWorldHeight;

            p.x -= (IslandSize / 2.f); //offset from the centre
            p.y -= (IslandSize / 2.f);

            glm::mat4 tx = glm::translate(glm::mat4(1.f), glm::vec3(p.x, y, p.y));
            tx = glm::rotate(tx, cro::Util::Const::TAU / cro::Util::Random::value(1, 8), cro::Transform::Y_AXIS);
            tx = glm::scale(tx, glm::vec3(2.f + static_cast<float>(cro::Util::Random::value(-23, 24)) / 100.f));
            transforms[i%2].push_back(tx);
        }

        auto i = 0;
        for (auto& tx : transforms)
        {
            //sort from front to back
            std::sort(tx.begin(), tx.end(),
                [](const glm::mat4& a, const glm::mat4& b)
                {
                    return a[3].z > b[3].z;
                });

            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>();
            m_modelDefs[GameModelID::Palm01 + i]->createModel(entity);
            entity.getComponent<cro::Model>().setInstanceTransforms(tx);
            i++;
        }

        m_requestFlags |= ClientRequestFlags::TreeMap;
    }
}

void GameState::updateIslandVerts(const std::vector<float>& heightmap)
{
    const glm::vec4 WetSand = { 0.44f, 0.29f, 0.11f, 1.f };
    const glm::vec4 Grass = { 0.184f, 0.29f, 0.03f, 1.f };

    struct Vertex final
    {
        glm::vec3 pos = glm::vec3(0.f);
        glm::vec4 colour = glm::vec4(1.f);
        glm::vec3 normal = glm::vec3(0.f);
        glm::vec3 tan = glm::vec3(0.f);
        glm::vec3 bitan = glm::vec3(0.f);
        glm::vec2 uv = glm::vec2(0.f);
    };
    std::vector<Vertex> verts(IslandTileCount * IslandTileCount);

    auto heightAt = [&](std::int32_t x, std::int32_t y)
    {
        x = std::min(static_cast<std::int32_t>(IslandTileCount) - 1, std::max(0, x));
        y = std::min(static_cast<std::int32_t>(IslandTileCount) - 1, std::max(0, y));

        return heightmap[y * IslandTileCount + x];
    };

    for (auto i = 0u; i < heightmap.size(); ++i)
    {
        std::int32_t x = i % IslandTileCount;
        std::int32_t y = i / IslandTileCount;

        verts[i].pos = { x * TileSize, y * TileSize, heightmap[i] * IslandHeight };

        if (verts[i].pos.z < /*IslandHeight + IslandWorldHeight*/2.2f)
        {
            //sand should be wet
            verts[i].colour = WetSand;
        }
        else if (verts[i].pos.z > GrassHeight)
        {
            verts[i].colour = Grass;
        }

        verts[i].uv = { verts[i].pos.x / IslandSize, verts[i].pos.y / IslandSize };

        verts[i].uv *= 8.f;

        //normal calc
        auto l = heightAt(x - 1, y);
        auto r = heightAt(x + 1, y);
        auto u = heightAt(x, y + 1);
        auto d = heightAt(x, y - 1);

        verts[i].normal = { l - r, d - u, 2.f };
        verts[i].normal = glm::normalize(verts[i].normal);

        //tan/bitan
        //if we assume the UV go parallel with the grid we can just use
        //a 90 deg rotation and the cross product of the result
        verts[i].tan = { verts[i].normal.z, verts[i].normal.y, -verts[i].normal.x };
        verts[i].bitan = glm::cross(verts[i].tan, verts[i].normal);
    }

    auto& meshData = m_islandEntity.getComponent<cro::Model>().getMeshData();

    CRO_ASSERT(verts.size() == meshData.vertexCount, "Incorrect vertex count");
    CRO_ASSERT(sizeof(Vertex) == meshData.vertexSize, "Incorrect vertex size");

    glBindBuffer(GL_ARRAY_BUFFER, meshData.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GameState::loadIslandAssets()
{
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().rotate(-cro::Transform::X_AXIS, cro::Util::Const::PI / 2.f);
    entity.getComponent<cro::Transform>().move({ 0.f, IslandWorldHeight, 0.f }); //small extra amount to stop z-fighting
    entity.getComponent<cro::Transform>().setOrigin({ IslandSize / 2.f, IslandSize / 2.f, 0.f });

    auto meshID = m_resources.meshes.loadMesh(cro::GridMeshBuilder({ IslandSize, IslandSize }, IslandTileCount - 1, glm::vec2(1.f), true)); //this accounts for extra row of vertices
    auto shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::PBR, cro::ShaderResource::DiffuseMap |
        cro::ShaderResource::NormalMap |
        cro::ShaderResource::MaskMap |
        cro::ShaderResource::VertexColour |
        cro::ShaderResource::RxShadows);

    m_materialIDs[MaterialID::Island] = m_resources.materials.add(m_resources.shaders.get(shaderID));
    //m_resources.materials.get(m_materialIDs[MaterialID::Island]).setProperty("u_diffuseMap", m_islandTexture);
    //m_resources.materials.get(m_materialIDs[MaterialID::Island]).setProperty("u_colour", cro::Colour::Green());

    m_resources.textures.load(TextureID::SandAlbedo, "assets/materials/sand01/albedo.png");
    m_resources.textures.load(TextureID::SandNormal, "assets/materials/sand01/normal.png");
    m_resources.textures.load(TextureID::SandMask, "assets/materials/sand01/mask.png");

    auto& albedo = m_resources.textures.get(TextureID::SandAlbedo);
    albedo.setSmooth(true);
    albedo.setRepeated(true);
    auto& normal = m_resources.textures.get(TextureID::SandNormal);
    normal.setSmooth(true);
    normal.setRepeated(true);
    auto& mask = m_resources.textures.get(TextureID::SandMask);
    mask.setSmooth(true);
    mask.setRepeated(true);

    auto& material = m_resources.materials.get(m_materialIDs[MaterialID::Island]);
    material.setProperty("u_diffuseMap", albedo);
    material.setProperty("u_normalMap", normal);
    material.setProperty("u_maskMap", mask);
    material.setProperty("u_colour", cro::Colour::White);

    material.setProperty("u_irradianceMap", m_environmentMap.getIrradianceMap());
    material.setProperty("u_prefilterMap", m_environmentMap.getPrefilterMap());
    material.setProperty("u_brdfMap", m_environmentMap.getBRDFMap());

    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    m_islandEntity = entity;
}