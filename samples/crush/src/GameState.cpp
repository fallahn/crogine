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

#include "GameState.hpp"
#include "SharedStateData.hpp"
#include "PlayerSystem.hpp"
#include "PacketIDs.hpp"
#include "ClientCommandIDs.hpp"
#include "InterpolationSystem.hpp"
#include "ClientPacketData.hpp"
#include "DayNightDirector.hpp"
#include "MapData.hpp"
#include "ServerLog.hpp"
#include "GLCheck.hpp"
#include "Collision.hpp"
#include "DebugDraw.hpp"
#include "Messages.hpp"
#include "CrateSystem.hpp"
#include "AvatarScaleSystem.hpp"
#include "ParticleDirector.hpp"
#include "PuntBarSystem.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/ShadowCaster.hpp>
#include <crogine/ecs/components/DynamicTreeComponent.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>

#include <crogine/ecs/components/Drawable2D.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/DynamicTreeSystem.hpp>
#include <crogine/ecs/systems/ParticleSystem.hpp>

#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/graphics/MeshData.hpp>
#include <crogine/graphics/DynamicMeshBuilder.hpp>
#include <crogine/graphics/Image.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Network.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/GlobalConsts.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

#include <cstring>

namespace
{
#include "PortalShader.hpp"

    GLuint portalShader = 0;
    GLint portalUniform = -1;

    //for debug output
    cro::Entity playerEntity;
    std::size_t bitrate = 0;
    std::size_t bitrateCounter = 0;

    constexpr float PortalWidth = 1.6f;
    constexpr float PortalHeight = 1.f;
    constexpr float PortalDepth = -0.5f;

    //render flags for reflection passes
    std::size_t NextPlayerPlane = 0; //index into this array when creating new player
    const std::uint64_t NoPlanes = 0xFFFFFFFFFFFFFF00;
    const std::array<std::uint64_t, 4u> PlayerPlanes =
    {
        0x1, 0x2,0x4,0x8
    };
    const std::uint64_t NoReflect = 0x10;
    const std::uint64_t NoRefract = 0x20;
}

GameState::GameState(cro::StateStack& stack, cro::State::Context context, SharedStateData& sd)
    : cro::State        (stack, context),
    m_sharedData        (sd),
    m_gameScene         (context.appInstance.getMessageBus()),
    m_uiScene           (context.appInstance.getMessageBus()),
    m_requestFlags      (ClientRequestFlags::All), //TODO remember to update this once we actually have stuff to request
    m_dataRequestCount  (0)
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    });

    sd.clientConnection.ready = false;

    //skip requesting these because they're lag-tastic
    m_requestFlags |= (ClientRequestFlags::BushMap | ClientRequestFlags::TreeMap);

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

#ifdef CRO_DEBUG_
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
            }
            ImGui::End();

            if (ImGui::Begin("Textures"))
            {
                ImGui::Image(m_debugViewTexture.getTexture(), { 512.f, 512.f }, { 0.f, 1.f }, { 1.f, 0.f });

                if (ImGui::Button("Perspective"))
                {
                    m_debugCam.getComponent<cro::Camera>().setPerspective(45.f * cro::Util::Const::degToRad, 1.f, 10.f, 60.f);
                }
                ImGui::SameLine();
                if (ImGui::Button("Ortho"))
                {
                    m_debugCam.getComponent<cro::Camera>().setOrthographic(-10.f, 10.f, -10.f, 10.f, 10.f, 60.f);
                }

                ImGui::SameLine();
                if (ImGui::Button("Front"))
                {
                    m_debugCam.getComponent<cro::Transform>().setRotation(glm::vec3(1.f, 0.f, 0.f), 0.f);
                    m_debugCam.getComponent<cro::Transform>().setPosition(glm::vec3(0.f, 6.f, 30.f));
                }
                ImGui::SameLine();
                if (ImGui::Button("Top"))
                {
                    m_debugCam.getComponent<cro::Transform>().setRotation(glm::vec3(1.f, 0.f, 0.f), -90.f * cro::Util::Const::degToRad);
                    m_debugCam.getComponent<cro::Transform>().setPosition(glm::vec3(0.f, 30.f, 0.f));
                }
                ImGui::SameLine();
                if (ImGui::Button("Rear"))
                {
                    m_debugCam.getComponent<cro::Transform>().setRotation(glm::vec3(0.f, 1.f, 0.f), 180.f * cro::Util::Const::degToRad);
                    m_debugCam.getComponent<cro::Transform>().setPosition(glm::vec3(0.f, 6.f, -30.f));
                }

            }
            ImGui::End();
        });
#endif
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
        case SDLK_F4:
        {
            auto flags = m_gameScene.getSystem<cro::RenderSystem2D>().getFilterFlags();
            if (flags & TwoDeeFlags::Debug)
            {
                flags &= ~TwoDeeFlags::Debug;
            }
            else
            {
                flags |= TwoDeeFlags::Debug;
            }
            m_gameScene.getSystem<cro::RenderSystem2D>().setFilterFlags(flags);
        }
            break;
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
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        switch (evt.cbutton.button)
        {
        default: break;
        case SDL_CONTROLLER_BUTTON_START:
            requestStackPush(States::ID::Pause);
            break;
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
    if (msg.id == MessageID::PlayerMessage)
    {
        const auto& data = msg.getData<PlayerEvent>();
        if (data.type == PlayerEvent::Teleported)
        {
            auto player = data.player;
            player.getComponent<Player>().avatar.getComponent<cro::ParticleEmitter>().start();
        }
        else if(data.type == PlayerEvent::Landed)
        {
            auto player = data.player;
            player.getComponent<Player>().avatar.getComponent<cro::ParticleEmitter>().stop();
        }
    }

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


            //TODO this should have alrady been done in the lobby
            /*if ((m_requestFlags & ClientRequestFlags::MapName) == 0)
            {
                std::uint16_t data = (m_sharedData.clientConnection.connectionID << 8) | ClientRequestFlags::MapName;
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::RequestData, data, cro::NetFlag::Reliable);
            }*/
            //these require the heightmap has already been received
            /*else
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
            }*/
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
    glCheck(glUseProgram(portalShader));
    glCheck(glUniform1f(portalUniform, timeAccum * 0.01f));
    

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

    /*for (auto i = 0u; i < m_cameras.size(); ++i)
    {
        auto ent = m_cameras[i];
        m_gameScene.setActiveCamera(ent);

        auto& cam = ent.getComponent<cro::Camera>();
        cam.renderFlags = NoPlanes | NoRefract;
        auto oldVP = cam.viewport;

        cam.viewport = { 0.f,0.f,1.f,1.f };

        cam.setActivePass(cro::Camera::Pass::Reflection);
        cam.reflectionBuffer.clear(cro::Colour::Red);
        m_gameScene.render(cam.reflectionBuffer);
        cam.reflectionBuffer.display();

        cam.renderFlags = NoPlanes | NoReflect;
        cam.setActivePass(cro::Camera::Pass::Refraction);
        cam.refractionBuffer.clear(cro::Colour::Blue);
        m_gameScene.render(cam.refractionBuffer);
        cam.refractionBuffer.display();

        cam.renderFlags = NoPlanes | PlayerPlanes[i] | NoReflect | NoRefract;
        cam.setActivePass(cro::Camera::Pass::Final);
        cam.viewport = oldVP;
    }*/

    auto& rt = cro::App::getWindow();
    m_gameScene.render(rt, m_cameras);
    m_uiScene.render(rt);


#ifdef CRO_DEBUG_
    //render a far view of the scene in debug to a render texture
    auto oldCam = m_gameScene.setActiveCamera(m_debugCam);
    m_debugViewTexture.clear();
    m_gameScene.render(m_debugViewTexture);
    m_debugViewTexture.display();
    m_gameScene.setActiveCamera(oldCam);
#endif
}

//private
void GameState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CommandSystem>(mb);
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::DynamicTreeSystem>(mb);
    m_gameScene.addSystem<InterpolationSystem>(mb);
    m_gameScene.addSystem<AvatarScaleSystem>(mb);
    m_gameScene.addSystem<PlayerSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
    m_gameScene.addSystem<cro::RenderSystem2D>(mb).setFilterFlags(~TwoDeeFlags::Debug);
    m_gameScene.addSystem<cro::ParticleSystem>(mb);

    m_gameScene.addDirector<DayNightDirector>();
    m_gameScene.addDirector<ParticleDirector>(m_resources.textures);


    m_uiScene.addSystem<PuntBarSystem>(mb);
    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void GameState::loadAssets()
{
    //skyboxes / environment
    m_environmentMap.loadFromFile("assets/images/cubemap/beach02.hdr");
    m_skyMap.loadFromFile("assets/images/cubemap/skybox.hdr");
    m_gameScene.setCubemap(m_skyMap);
    //m_gameScene.setCubemap("assets/images/cubemap/sky.ccm");

    //materials
    auto shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::PBR, cro::ShaderResource::DiffuseColour | cro::ShaderResource::RxShadows);
    m_materialIDs[MaterialID::Default] = m_resources.materials.add(m_resources.shaders.get(shaderID));
    m_resources.materials.get(m_materialIDs[MaterialID::Default]).setProperty("u_colour", cro::Colour(1.f, 1.f, 1.f));
    m_resources.materials.get(m_materialIDs[MaterialID::Default]).setProperty("u_maskColour", cro::Colour(0.f, 1.f, 1.f));
    m_resources.materials.get(m_materialIDs[MaterialID::Default]).setProperty("u_irradianceMap", m_environmentMap.getIrradianceMap());
    m_resources.materials.get(m_materialIDs[MaterialID::Default]).setProperty("u_prefilterMap", m_environmentMap.getPrefilterMap());
    m_resources.materials.get(m_materialIDs[MaterialID::Default]).setProperty("u_brdfMap", m_environmentMap.getBRDFMap());
    //m_resources.materials.get(m_materialIDs[MaterialID::Default]).blendMode = cro::Material::BlendMode::Alpha;

    shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::ShadowMap, cro::ShaderResource::DepthMap);
    m_materialIDs[MaterialID::DefaultShadow] = m_resources.materials.add(m_resources.shaders.get(shaderID));


    m_resources.textures.load(TextureID::Portal, "assets/images/portal.png");
    m_resources.textures.get(TextureID::Portal).setRepeated(true);
    if (m_resources.shaders.loadFromString(ShaderID::Portal, PortalVertex, PortalFragment))
    {
        m_materialIDs[MaterialID::Portal] = m_resources.materials.add(m_resources.shaders.get(ShaderID::Portal));
        m_resources.materials.get(m_materialIDs[MaterialID::Portal]).setProperty("u_diffuseMap", m_resources.textures.get(TextureID::Portal));
        m_resources.materials.get(m_materialIDs[MaterialID::Portal]).blendMode = cro::Material::BlendMode::Alpha;
        portalShader = m_resources.shaders.get(ShaderID::Portal).getGLHandle();
        portalUniform = m_resources.shaders.get(ShaderID::Portal).getUniformMap().at("u_time");
    }

    //model defs - don't forget to pass the env map here!
    m_modelDefs[GameModelID::Crate].loadFromFile("assets/models/box.cmt", m_resources, &m_environmentMap);
    m_modelDefs[GameModelID::Hologram].loadFromFile("assets/models/hologram.cmt", m_resources/*, &m_environmentMap*/);



    //create model geometry for portal field
    m_meshIDs[MeshID::Portal] = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::UV0, 1, GL_TRIANGLE_STRIP));
    auto& mesh = m_resources.meshes.getMesh(m_meshIDs[MeshID::Portal]);

    std::vector<float> verts = 
    {
        0.f,0.f,PortalDepth,                   0.f,0.f,
        PortalWidth,0.f,PortalDepth,           1.f,0.f,
        0.f,PortalHeight,PortalDepth,          0.f,1.f,
        PortalWidth,PortalHeight,PortalDepth,  1.f,1.f
    };
    std::vector<std::uint32_t> indices =
    {
        0,1,2,3
    };
    const std::size_t vertComponentCount = 5;

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexData[0].ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    mesh.boundingBox[0] = { 0.f, 0.f, 0.01f };
    mesh.boundingBox[1] = { PortalWidth, PortalHeight, -0.01f };
    mesh.boundingSphere.radius = std::sqrt((PortalWidth * PortalWidth) + (PortalHeight * PortalHeight));
    mesh.vertexCount = static_cast<std::uint32_t>(verts.size() / vertComponentCount);
    mesh.indexData[0].indexCount = static_cast<std::uint32_t>(indices.size());

#ifdef CRO_DEBUG_
    m_debugViewTexture.create(512, 512);

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(0.f, 6.f, 30.f));
    entity.addComponent<cro::Camera>().setOrthographic(-10.f, 10.f, -10.f, 10.f, 10.f, 60.f);
    entity.getComponent<cro::Camera>().depthBuffer.create(1024, 1024);

    m_debugCam = entity;
#endif

    //set up the particle director
    auto& particleDirector = m_gameScene.getDirector<ParticleDirector>();
    enum ParticleID
    {
        Squish,

        Count
    };
    std::array<std::size_t, ParticleID::Count> ids{};

    ids[ParticleID::Squish] = particleDirector.loadSettings("assets/particles/squish.xyp");

    auto particleHandler = [ids](const cro::Message& msg) -> std::optional<std::pair<std::size_t, glm::vec3>>
    {
        switch (msg.id)
        {
        default: break;
        case MessageID::PlayerMessage:
        {
            const auto& data = msg.getData<PlayerEvent>();
            switch (data.type)
            {
            default: break;
            //case PlayerEvent::Landed:
            //    return std::make_pair(ids[ParticleID::Squish], data.player.getComponent<cro::Transform>().getPosition());
            }
        }
        break;
        }

        return std::nullopt;
    };
    particleDirector.setMessageHandler(particleHandler);
}

void GameState::createScene()
{
    createDayCycle();

    loadMap();

    //ground plane
    cro::ModelDefinition modelDef;
    modelDef.loadFromFile("assets/models/ground_plane.cmt", m_resources, &m_environmentMap);

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    modelDef.createModel(entity, m_resources);
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

    cro::ModelDefinition definition;
    definition.loadFromFile("assets/models/moon.cmt", m_resources);
    definition.createModel(entity, m_resources);
    entity.getComponent<cro::Model>().setRenderFlags(NoRefract);
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //sun
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, SunOffset });
    children.sunNode = entity;

    definition.loadFromFile("assets/models/sun.cmt", m_resources);
    definition.createModel(entity, m_resources);
    entity.getComponent<cro::Model>().setRenderFlags(NoRefract);
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //add the shadow caster node to the day night cycle
    auto sunNode = m_gameScene.getSunlight();
    sunNode.getComponent<cro::Transform>().setPosition({ 0.f, 0.f, 50.f });
    sunNode.addComponent<TargetTransform>();
    rootNode.getComponent<cro::Transform>().addChild(sunNode.getComponent<cro::Transform>());
}

void GameState::loadMap()
{
    MapData mapData;
    if (mapData.loadFromFile("assets/maps/" + m_sharedData.mapName.toAnsiString()))
    {
        std::array wallColours =
        {
            cro::Colour(0.6f, 1.f, 0.6f),
            cro::Colour(1.f, 0.6f, 0.6f)
        };

        cro::ModelDefinition portalModel;
        portalModel.loadFromFile("assets/models/portal.cmt", m_resources, &m_environmentMap);

        //build the platform geometry
        for (auto i = 0; i < 2; ++i)
        {
            float layerDepth = LayerDepth * Util::direction(i);
            auto meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Normal/* | cro::VertexProperty::UV0*/, 1, GL_TRIANGLES));

            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>();
            entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), m_resources.materials.get(m_materialIDs[MaterialID::Default]));
            entity.getComponent<cro::Model>().setShadowMaterial(0, m_resources.materials.get(m_materialIDs[MaterialID::DefaultShadow]));
            entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", wallColours[i]);
            entity.addComponent<cro::ShadowCaster>();

            std::vector<float> verts;
            std::vector<std::uint32_t> indices; //u16 should be fine but the mesh builder expects 32...

            //0------1
            //     /
            //    /
            //   /
            //  /
            //2------3

            const std::size_t vertComponentCount = 6; //MUST UPDATE THIS WITH VERT COMPONENT COUNT!
            const auto& rects = mapData.getCollisionRects(i);
            for (auto rect : rects)
            {
                auto indexOffset = static_cast<std::uint32_t>(verts.size() / vertComponentCount);

                //front face
                verts.push_back(rect.left); verts.push_back(rect.bottom + rect.height); verts.push_back(layerDepth + LayerThickness); //position
                verts.push_back(0.f); verts.push_back(0.f); verts.push_back(1.f); //normal

                verts.push_back(rect.left + rect.width); verts.push_back(rect.bottom + rect.height); verts.push_back(layerDepth + LayerThickness);
                verts.push_back(0.f); verts.push_back(0.f); verts.push_back(1.f);

                verts.push_back(rect.left); verts.push_back(rect.bottom); verts.push_back(layerDepth + LayerThickness);
                verts.push_back(0.f); verts.push_back(0.f); verts.push_back(1.f);

                verts.push_back(rect.left + rect.width); verts.push_back(rect.bottom); verts.push_back(layerDepth + LayerThickness);
                verts.push_back(0.f); verts.push_back(0.f); verts.push_back(1.f);


                indices.push_back(indexOffset + 0);
                indices.push_back(indexOffset + 2);
                indices.push_back(indexOffset + 1);

                indices.push_back(indexOffset + 2);
                indices.push_back(indexOffset + 3);
                indices.push_back(indexOffset + 1);


                //rear face
                verts.push_back(rect.left); verts.push_back(rect.bottom + rect.height); verts.push_back(layerDepth - LayerThickness); //position
                verts.push_back(0.f); verts.push_back(0.f); verts.push_back(-1.f); //normal

                verts.push_back(rect.left + rect.width); verts.push_back(rect.bottom + rect.height); verts.push_back(layerDepth - LayerThickness);
                verts.push_back(0.f); verts.push_back(0.f); verts.push_back(-1.f);

                verts.push_back(rect.left); verts.push_back(rect.bottom); verts.push_back(layerDepth - LayerThickness);
                verts.push_back(0.f); verts.push_back(0.f); verts.push_back(-1.f);

                verts.push_back(rect.left + rect.width); verts.push_back(rect.bottom); verts.push_back(layerDepth - LayerThickness);
                verts.push_back(0.f); verts.push_back(0.f); verts.push_back(-1.f);


                indexOffset += 4;
                indices.push_back(indexOffset + 1);
                indices.push_back(indexOffset + 2);
                indices.push_back(indexOffset + 0);

                indices.push_back(indexOffset + 1);
                indices.push_back(indexOffset + 3);
                indices.push_back(indexOffset + 2);


                //TODO we could properly calculate the outline of combined shapes
                //although this probably only saves us some small amount of overdraw

                //north face
                verts.push_back(rect.left); verts.push_back(rect.bottom + rect.height); verts.push_back(layerDepth - LayerThickness); //position
                verts.push_back(0.f); verts.push_back(1.f); verts.push_back(0.f); //normal

                verts.push_back(rect.left + rect.width); verts.push_back(rect.bottom + rect.height); verts.push_back(layerDepth - LayerThickness);
                verts.push_back(0.f); verts.push_back(1.f); verts.push_back(0.f);

                verts.push_back(rect.left); verts.push_back(rect.bottom + rect.height); verts.push_back(layerDepth + LayerThickness);
                verts.push_back(0.f); verts.push_back(1.f); verts.push_back(0.f);

                verts.push_back(rect.left + rect.width); verts.push_back(rect.bottom + rect.height); verts.push_back(layerDepth + LayerThickness);
                verts.push_back(0.f); verts.push_back(1.f); verts.push_back(0.f);

                indexOffset += 4;
                indices.push_back(indexOffset + 0);
                indices.push_back(indexOffset + 2);
                indices.push_back(indexOffset + 1);

                indices.push_back(indexOffset + 2);
                indices.push_back(indexOffset + 3);
                indices.push_back(indexOffset + 1);

                //east face
                verts.push_back(rect.left+ rect.width); verts.push_back(rect.bottom + rect.height); verts.push_back(layerDepth + LayerThickness); //position
                verts.push_back(1.f); verts.push_back(0.f); verts.push_back(0.f); //normal

                verts.push_back(rect.left + rect.width); verts.push_back(rect.bottom + rect.height); verts.push_back(layerDepth - LayerThickness);
                verts.push_back(1.f); verts.push_back(0.f); verts.push_back(0.f);

                verts.push_back(rect.left + rect.width); verts.push_back(rect.bottom); verts.push_back(layerDepth + LayerThickness);
                verts.push_back(1.f); verts.push_back(0.f); verts.push_back(0.f);

                verts.push_back(rect.left + rect.width); verts.push_back(rect.bottom); verts.push_back(layerDepth - LayerThickness);
                verts.push_back(1.f); verts.push_back(0.f); verts.push_back(0.f);

                indexOffset += 4;
                indices.push_back(indexOffset + 0);
                indices.push_back(indexOffset + 2);
                indices.push_back(indexOffset + 1);

                indices.push_back(indexOffset + 2);
                indices.push_back(indexOffset + 3);
                indices.push_back(indexOffset + 1);


                //west face
                verts.push_back(rect.left); verts.push_back(rect.bottom + rect.height); verts.push_back(layerDepth - LayerThickness); //position
                verts.push_back(-1.f); verts.push_back(0.f); verts.push_back(0.f); //normal

                verts.push_back(rect.left); verts.push_back(rect.bottom + rect.height); verts.push_back(layerDepth + LayerThickness);
                verts.push_back(-1.f); verts.push_back(0.f); verts.push_back(0.f);

                verts.push_back(rect.left); verts.push_back(rect.bottom); verts.push_back(layerDepth - LayerThickness);
                verts.push_back(-1.f); verts.push_back(0.f); verts.push_back(0.f);

                verts.push_back(rect.left); verts.push_back(rect.bottom); verts.push_back(layerDepth + LayerThickness);
                verts.push_back(-1.f); verts.push_back(0.f); verts.push_back(0.f);

                indexOffset += 4;
                indices.push_back(indexOffset + 0);
                indices.push_back(indexOffset + 2);
                indices.push_back(indexOffset + 1);

                indices.push_back(indexOffset + 2);
                indices.push_back(indexOffset + 3);
                indices.push_back(indexOffset + 1);

                //TODO only add the bottom face if the rect pos > half screen height (ie only when we can see the bottom, potentially)


                //might as well add the dynamic tree components...
                auto collisionEnt = m_gameScene.createEntity();
                collisionEnt.addComponent<cro::Transform>().setPosition({ rect.left, rect.bottom, layerDepth });
                collisionEnt.addComponent<cro::DynamicTreeComponent>().setArea({ glm::vec3(0.f, 0.f, LayerThickness), glm::vec3(rect.width, rect.height, -LayerThickness) });
                collisionEnt.getComponent<cro::DynamicTreeComponent>().setFilterFlags(i + 1);

                collisionEnt.addComponent<CollisionComponent>().rectCount = 1;
                collisionEnt.getComponent<CollisionComponent>().rects[0].material = CollisionMaterial::Solid;
                collisionEnt.getComponent<CollisionComponent>().rects[0].bounds = { 0.f, 0.f, rect.width, rect.height };
                collisionEnt.getComponent<CollisionComponent>().calcSum();

#ifdef CRO_DEBUG_
                //if (i == 0) addBoxDebug(collisionEnt, m_gameScene, cro::Colour::Blue);
#endif
            }

            auto& mesh = entity.getComponent<cro::Model>().getMeshData();

            glCheck(glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo));
            glCheck(glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW));
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexData[0].ibo));
            glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

            mesh.boundingBox[0] = { -10.5f, 0.f, 7.f };
            mesh.boundingBox[1] = { 10.5f, 12.f, 0.5f };
            mesh.boundingSphere.radius = 10.5f;
            mesh.vertexCount = static_cast<std::uint32_t>(verts.size() / vertComponentCount);
            mesh.indexData[0].indexCount = static_cast<std::uint32_t>(indices.size());

            //add in the teleport points
            const auto& teleports = mapData.getTeleportRects(i);
            for (const auto& rect : teleports)
            {
                entity = m_gameScene.createEntity();
                entity.addComponent<cro::Transform>().setPosition({ rect.left, rect.bottom, layerDepth });
                entity.addComponent<cro::DynamicTreeComponent>().setArea({ glm::vec3(0.f, 0.f, LayerThickness), glm::vec3(rect.width, rect.height, -LayerThickness) });
                entity.getComponent<cro::DynamicTreeComponent>().setFilterFlags(i + 1);

                entity.addComponent<CollisionComponent>().rectCount = 1;
                entity.getComponent<CollisionComponent>().rects[0].material = CollisionMaterial::Teleport;
                entity.getComponent<CollisionComponent>().rects[0].bounds = { 0.f, 0.f, rect.width, rect.height };
                entity.getComponent<CollisionComponent>().calcSum();
#ifdef CRO_DEBUG_
                addBoxDebug(entity, m_gameScene, cro::Colour::Magenta);
#endif
                //model
                entity = m_gameScene.createEntity();
                entity.addComponent<cro::Transform>().setPosition({ rect.left, rect.bottom, layerDepth });
                entity.getComponent<cro::Transform>().move({ rect.width / 2.f, 0.f, 0.f });
                entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI* i);
                portalModel.createModel(entity, m_resources);

                //force field
                entity = m_gameScene.createEntity();
                entity.addComponent<cro::Transform>().setPosition({ rect.left - ((PortalWidth - rect.width) / 2.f), rect.bottom, layerDepth });
                entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_meshIDs[MeshID::Portal]), m_resources.materials.get(m_materialIDs[MaterialID::Portal]));
                entity.getComponent<cro::Transform>().move({ PortalWidth * i, 0.f, 0.f });
                entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI* i);
            }
        }
    }
    else
    {
        m_sharedData.errorMessage = "Failed to load map " + m_sharedData.mapName.toAnsiString();
        requestStackPush(States::Error);
    }
}

void GameState::handlePacket(const cro::NetEvent::Packet& packet)
{
    switch (packet.getID())
    {
    default: break;
    case PacketID::PlayerDisconnect:
        removePlayer(packet.as<std::uint8_t>());
        break;
    case PacketID::Ping:
        m_sharedData.clientConnection.netClient.sendPacket(PacketID::Ping, packet.as<std::int32_t>(), cro::NetFlag::Unreliable);
        break;
    case PacketID::ConnectionRefused:
        if (packet.as<std::uint8_t>() == MessageType::ServerQuit)
        {
            m_sharedData.errorMessage = "Server Closed The Connection";
            requestStackPush(States::Error);
        }
        break;
    case PacketID::StateChange:
        if (packet.as<std::uint8_t>() == sv::StateID::Lobby)
        {
            //TODO push some sort of round summary state

            requestStackPop();
            requestStackPush(States::MainMenu);
        }
        break;
    case PacketID::LogMessage:
        LogW << "Server: " << sv::LogStrings[packet.as<std::int32_t>()] << std::endl;
        break;
    case PacketID::DayNightUpdate:
        m_gameScene.getDirector<DayNightDirector>().setTimeOfDay(cro::Util::Net::decompressFloat(packet.as<std::int16_t>(), 8));
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
                //check if this is a remote player and remove the
                //box icon from their avatar first
                if (e.hasComponent<PlayerAvatar>())
                {
                    m_gameScene.destroyEntity(e.getComponent<PlayerAvatar>().holoEnt);
                }
                m_gameScene.destroyEntity(e);
            }
        };
        m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
    }
        break;
    case PacketID::ActorSpawn:
        spawnActor(packet.as<ActorSpawn>());
        break;
    case PacketID::PlayerSpawn:
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
                interp.setTarget({ cro::Util::Net::decompressVec3(update.position), cro::Util::Net::decompressQuat(update.rotation), update.timestamp });
            }
        };
        m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
    }
        break;
    case PacketID::PlayerState:
        //use this for things like teleport effects
        avatarUpdate(packet.as<PlayerStateChange>());
        break;
    case PacketID::CrateUpdate:
    {
        auto data = packet.as<CrateState>();
        crateUpdate(data);
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
    case PacketID::GameMessage:
        switch (packet.as<std::uint8_t>())
        {
        default: break;
        case GameEvent::GameBegin:
            startGame();
            break;
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
            
        //player ID is relative to client, so will always be 0 on a net game
        //adding the client ID aligns it correctly with what the server has
        entity.addComponent<Actor>().id = info.playerID + info.connectionID; 
        entity.getComponent<Actor>().serverEntityId = info.serverID;

        return entity;
    };

    //placeholder for player scale
    cro::ModelDefinition md;
    md.loadFromFile("assets/models/player_box.cmt", m_resources, &m_environmentMap);

    cro::EmitterSettings particles;
    particles.loadFromFile("assets/particles/portal.xyp", m_resources.textures);

    if (info.connectionID == m_sharedData.clientConnection.connectionID)
    {
        if (m_inputParsers.count(info.playerID) == 0)
        {
            //this is us - the root is controlled by the player and the
            //avatar is connected as a child
            auto root = createActor();
            
            root.addComponent<Player>().id = info.playerID;
            root.getComponent<Player>().connectionID = info.connectionID;
            root.getComponent<Player>().spawnPosition = info.spawnPosition;
            root.getComponent<Player>().collisionLayer = info.spawnPosition.z > 0 ? 0 : 1;
            root.getComponent<Player>().local = true;
            root.getComponent<Player>().direction = info.spawnPosition.x > 0 ? Player::Left : Player::Right;

            root.addComponent<cro::DynamicTreeComponent>().setArea(PlayerBounds);
            root.getComponent<cro::DynamicTreeComponent>().setFilterFlags(root.getComponent<Player>().collisionLayer + 1);

            root.addComponent<CollisionComponent>().rectCount = 3;
            root.getComponent<CollisionComponent>().rects[0].material = CollisionMaterial::Body;
            root.getComponent<CollisionComponent>().rects[0].bounds = { -PlayerSize.x / 2.f, 0.f, PlayerSize.x, PlayerSize.y };
            root.getComponent<CollisionComponent>().rects[1].material = CollisionMaterial::Foot;
            root.getComponent<CollisionComponent>().rects[1].bounds = FootBounds;
            root.getComponent<CollisionComponent>().rects[2].material = CollisionMaterial::Sensor;
            auto crateArea = Util::expand(CrateArea, 0.1f);
            crateArea.left += CrateCarryOffset.x;
            crateArea.bottom += CrateCarryOffset.y;
            root.getComponent<CollisionComponent>().rects[2].bounds = crateArea;
            root.getComponent<CollisionComponent>().calcSum();

            //adjust the sum so it includes the crate whichever side it's on
            auto& sumRect = root.getComponent<CollisionComponent>().sumRect;
            auto diff =((CrateCarryOffset.x + (CrateArea.width / 2.f)) + (PlayerBounds[0].x / 2.f)) + 0.2f;
            sumRect.left -= diff;
            sumRect.width += diff;


            auto playerNumber = info.playerID + info.connectionID;
            /*
            Urrrgh so this is getting confusing. Each player should have a unique number regardless
            if they are playing on the same machine or remotely - however input indexing is done
            relative to the current connection

            Ideally the playerID should be refactored so that it properly represents the player number
            and the existing ID, as an offset from the connection ID, should be the input index.

            This is actually most of the way there now so it's just a case of making the naming less confusing
            SO DO THIS AND GET OFF YOUR LAZY 
            
            */

            m_inputParsers.insert(std::make_pair(playerNumber, InputParser(m_sharedData.clientConnection.netClient, m_sharedData.inputBindings[info.playerID])));
            m_inputParsers.at(playerNumber).setEntity(root); //remember this is initially disabled!

            m_cameras.emplace_back();
            m_cameras.back() = m_gameScene.createEntity();
            m_cameras.back().addComponent<cro::Transform>().setPosition({ 0.f, CameraHeight, CameraDistance });

            auto rotation = glm::lookAt(m_cameras.back().getComponent<cro::Transform>().getPosition(), glm::vec3(0.f, 0.f, -50.f), cro::Transform::Y_AXIS);
            m_cameras.back().getComponent<cro::Transform>().rotate(glm::inverse(rotation));
            m_cameras.back().addComponent<std::uint8_t>() = playerNumber; //so we can tell which player this cam follows

            auto& cam = m_cameras.back().addComponent<cro::Camera>();
            cam.reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
            cam.reflectionBuffer.setSmooth(true);
            cam.refractionBuffer.create(ReflectionMapSize, ReflectionMapSize);
            cam.refractionBuffer.setSmooth(true);
            cam.depthBuffer.create(4096, 4096);

            auto camController = m_gameScene.createEntity();
            camController.addComponent<cro::Transform>().addChild(m_cameras.back().getComponent<cro::Transform>());
            camController.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI * root.getComponent<Player>().collisionLayer);
            camController.addComponent<cro::Callback>().active = true;
            camController.getComponent<cro::Callback>().function =
                [root](cro::Entity e, float dt)
            {
                static const float TotalDistance = LayerDepth * 2.f;
                auto position = root.getComponent<cro::Transform>().getPosition().z;
                float currentDistance = LayerDepth - position;
                currentDistance /= TotalDistance;

                e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI * currentDistance);
            };


            //displays above player when carrying a box
            auto holoEnt = m_gameScene.createEntity();
            holoEnt.addComponent<cro::Transform>().setPosition(glm::vec3(0.f, PlayerBounds[1].y * 1.5f, 0.f));
            holoEnt.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
            m_modelDefs[GameModelID::Hologram].createModel(holoEnt, m_resources);
            holoEnt.addComponent<AvatarScale>().rotationSpeed = HoloRotationSpeed;
            

            //placeholder for player model
            auto playerEnt = m_gameScene.createEntity();
            playerEnt.addComponent<cro::Transform>().setOrigin({ 0.f, -0.4f, 0.f });
            md.createModel(playerEnt, m_resources);
            playerEnt.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", PlayerColours[info.playerID + info.connectionID]);
            playerEnt.addComponent<PlayerAvatar>().holoEnt = holoEnt;
            playerEnt.addComponent<cro::ParticleEmitter>().settings = particles;


            root.getComponent<Player>().avatar = playerEnt;
            root.getComponent<cro::Transform>().addChild(camController.getComponent<cro::Transform>());
            root.getComponent<cro::Transform>().addChild(playerEnt.getComponent<cro::Transform>());
            root.getComponent<cro::Transform>().addChild(holoEnt.getComponent<cro::Transform>());

            m_avatars[root.getComponent<Actor>().id] = playerEnt;

#ifdef CRO_DEBUG_
            addBoxDebug(root, m_gameScene);
            root.addComponent<DebugInfo>();

            registerWindow([root, playerNumber]()
                {
                    const auto& player = root.getComponent<Player>();
                    std::string label = "Player " + std::to_string(playerNumber);
                    if (ImGui::Begin(label.c_str()))
                    {
                        const auto& debug = root.getComponent<DebugInfo>();
                        ImGui::Text("Nearby: %d", debug.nearbyEnts);
                        ImGui::Text("Colliding: %d", debug.collidingEnts);
                        /*ImGui::Text("Bounds: %3.3f, %3.3f, %3.3f, - %3.3f, %3.3f, %3.3f",
                            debug.bounds[0].x, debug.bounds[0].y, debug.bounds[0].z,
                            debug.bounds[1].x, debug.bounds[1].y, debug.bounds[1].z);*/

                        switch (player.state)
                        {
                        default:
                            ImGui::Text("State: unknown");
                            break;
                        case Player::State::Falling:
                            ImGui::Text("State: Falling");
                            break;
                        case Player::State::Walking:
                            ImGui::Text("State: Walking");
                            break;
                        case Player::State::Teleport:
                            ImGui::Text("Teleporting");
                            break;
                        }

                        ImGui::Text("Vel X: %3.3f", player.velocity.x);
                        auto pos = root.getComponent<cro::Transform>().getPosition();
                        ImGui::Text("Pos: %3.3f, %3.3f, %3.3f", pos.x, pos.y, pos.z);
                        if (player.direction == 0)
                        {
                            ImGui::Text("Direction : Left");
                        }
                        else
                        {
                            ImGui::Text("Direction : Right");
                        }

                        auto foot = player.collisionFlags & (1 << CollisionMaterial::Foot);
                        ImGui::Text("Foot: %d", foot);

                        auto sensor = player.collisionFlags & (1 << CollisionMaterial::Sensor);
                        ImGui::Text("Sensor: %d", sensor);

                        ImGui::Text("Punt Level: %3.3f", player.puntLevel);
                    }
                    ImGui::End();
                });

            //create a 'shadow' so we can see the networked version of the player
            /*auto entity = createActor();
            md.createModel(entity, m_resources);

            auto rot = entity.getComponent<cro::Transform>().getRotation();
            entity.getComponent<cro::Transform>().setOrigin({ 0.f, -0.4f, 0.f });
            entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", cro::Colour(0.1f,0.1f,0.1f));

            entity.addComponent<cro::CommandTarget>().ID = Client::CommandID::Interpolated;
            entity.addComponent<InterpolationComponent>(InterpolationPoint(info.spawnPosition, rot, info.timestamp));*/
#endif
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
        md.createModel(entity, m_resources);

        auto rotation = entity.getComponent<cro::Transform>().getRotation();
        entity.getComponent<cro::Transform>().setOrigin({ 0.f, -0.4f, 0.f });
        entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", PlayerColours[entity.getComponent<Actor>().id]);

        entity.addComponent<cro::CommandTarget>().ID = Client::CommandID::Interpolated;
        entity.addComponent<InterpolationComponent>(InterpolationPoint(info.spawnPosition, rotation, info.timestamp));

        entity.addComponent<cro::DynamicTreeComponent>().setArea(PlayerBounds);
        entity.getComponent<cro::DynamicTreeComponent>().setFilterFlags((info.playerID / 2) + 1);

        entity.addComponent<CollisionComponent>().rectCount = 1;
        entity.getComponent<CollisionComponent>().rects[0].material = CollisionMaterial::Body;
        entity.getComponent<CollisionComponent>().rects[0].bounds = { -PlayerSize.x / 2.f, 0.f, PlayerSize.x, PlayerSize.y };
        entity.getComponent<CollisionComponent>().calcSum();

        entity.addComponent<cro::ParticleEmitter>().settings = particles; //teleport effect

        //displays above player when carrying a box
        auto holoEnt = m_gameScene.createEntity();
        holoEnt.addComponent<cro::Transform>().setPosition(glm::vec3(0.f, PlayerBounds[1].y * 1.5f, 0.f));
        holoEnt.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
        m_modelDefs[GameModelID::Hologram].createModel(holoEnt, m_resources);
        holoEnt.addComponent<AvatarScale>().rotationSpeed = HoloRotationSpeed;

        entity.addComponent<PlayerAvatar>().holoEnt = holoEnt; //to track joined crates
        entity.getComponent<cro::Transform>().addChild(holoEnt.getComponent<cro::Transform>());
        m_avatars[entity.getComponent<Actor>().id] = entity;

#ifdef CRO_DEBUG_
        addBoxDebug(entity, m_gameScene, cro::Colour::Magenta);
#endif
    }
}

void GameState::removePlayer(std::uint8_t id)
{
    //player ents ought to have been removed by the server
    //this is more for notification porpoises


}

void GameState::spawnActor(ActorSpawn as)
{
    if (as.id <= ActorID::PlayerFour)
    {
        return;
    }
    
    auto position = cro::Util::Net::decompressVec3(as.position);

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(position);
    entity.addComponent<Actor>().id = as.id;
    entity.getComponent<Actor>().serverEntityId = as.serverEntityId;

    entity.addComponent<cro::CommandTarget>().ID = Client::CommandID::Interpolated;
    entity.addComponent<InterpolationComponent>(InterpolationPoint(position, glm::quat(1.f, 0.f, 0.f, 0.f), as.timestamp));

    switch (as.id)
    {
    default: 
        LogW << "Spawned actor with unrecognised id " << as.id << std::endl;
        break;
    case ActorID::Crate:
        m_modelDefs[GameModelID::Crate].createModel(entity, m_resources);

        //add some collision properties which are updated from the server
        //rather than simulated locally, so the player gets proper collision
        //when the crate is idle
        entity.addComponent<CollisionComponent>().rectCount = 2;
        entity.getComponent<CollisionComponent>().rects[0].material = CollisionMaterial::Crate;
        entity.getComponent<CollisionComponent>().rects[0].bounds = CrateArea;
        entity.getComponent<CollisionComponent>().rects[1].material = CollisionMaterial::Foot;
        entity.getComponent<CollisionComponent>().rects[1].bounds = CrateFoot;
        entity.getComponent<CollisionComponent>().calcSum();

        entity.addComponent<cro::DynamicTreeComponent>().setArea(CrateBounds);
        entity.getComponent<cro::DynamicTreeComponent>().setFilterFlags((position.z > 0 ? 1 : 2) | CollisionID::Crate);

        entity.addComponent<Crate>();

        entity.addComponent<AvatarScale>().current = 1.f;
        entity.getComponent<AvatarScale>().target = 1.f;

#ifdef CRO_DEBUG_
        addBoxDebug(entity, m_gameScene, cro::Colour::Red);        
#endif
        break;
    }
}

void GameState::updateView(cro::Camera&)
{
    CRO_ASSERT(!m_cameras.empty(), "Need at least one camera!");

    const float fov = 56.f * cro::Util::Const::degToRad;
    const float nearPlane = 0.1f;
    const float farPlane = 170.f;
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

        //add the screen split
        {
            auto& verts = m_splitScreenNode.getComponent<cro::Drawable2D>().getVertexData();
            verts =
            {
                cro::Vertex2D(glm::vec2(cro::DefaultSceneSize.x / 2.f, 0.f), cro::Colour::Black),
                cro::Vertex2D(glm::vec2(cro::DefaultSceneSize.x / 2.f, cro::DefaultSceneSize.y), cro::Colour::Black),
            };
            m_splitScreenNode.getComponent<cro::Drawable2D>().updateLocalBounds();
        }

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

        //add the screen split
        {
            auto& verts = m_splitScreenNode.getComponent<cro::Drawable2D>().getVertexData();
            verts =
            {
                cro::Vertex2D(glm::vec2(cro::DefaultSceneSize.x / 2.f, 0.f), cro::Colour::Black),
                cro::Vertex2D(glm::vec2(cro::DefaultSceneSize.x / 2.f, cro::DefaultSceneSize.y), cro::Colour::Black),

                cro::Vertex2D(glm::vec2(cro::DefaultSceneSize.x / 2.f, cro::DefaultSceneSize.y), cro::Colour::Transparent),
                cro::Vertex2D(glm::vec2(0.f, cro::DefaultSceneSize.y / 2.f), cro::Colour::Transparent),

                cro::Vertex2D(glm::vec2(0.f, cro::DefaultSceneSize.y / 2.f), cro::Colour::Black),
                cro::Vertex2D(glm::vec2(cro::DefaultSceneSize.x, cro::DefaultSceneSize.y / 2.f), cro::Colour::Black),
            };
            m_splitScreenNode.getComponent<cro::Drawable2D>().updateLocalBounds();
        }

        break;

    default:
        break;
    }

    //set up projection
    for (auto cam : m_cameras)
    {
        cam.getComponent<cro::Camera>().setPerspective(fov, aspect, nearPlane, farPlane);
    }



    //update the UI camera
    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();

    cam.setOrthographic(0.f, static_cast<float>(cro::DefaultSceneSize.x), 0.f, static_cast<float>(cro::DefaultSceneSize.y), -2.f, 100.f);
    cam.viewport.bottom = (1.f - size.y) / 2.f;
    cam.viewport.height = size.y;

    updatePlayerUI();
}

void GameState::startGame()
{
    for (auto& ip : m_inputParsers)
    {
        ip.second.setEnabled(true);
    }
}

void GameState::crateUpdate(const CrateState& data)
{
    //TODO raise a message here for things like audio events

    cro::Command cmd;
    cmd.targetFlags = Client::CommandID::Interpolated;
    cmd.action = [&, data](cro::Entity e, float)
    {
        if (e.getComponent<Actor>().serverEntityId == data.serverEntityID)
        {
            e.getComponent<Crate>().state = static_cast<Crate::State>(data.state);
            e.getComponent<Crate>().owner = (data.state == Crate::Idle) ? -1 : data.owner;
            e.getComponent<Crate>().collisionLayer = data.collisionLayer;
            e.getComponent<cro::DynamicTreeComponent>().setFilterFlags((data.collisionLayer + 1) | CollisionID::Crate);

            std::string state;
            switch (data.state)
            {
            default: break;
            case Crate::Ballistic:
                state = "Ballistic";
                break;
            case Crate::Falling:
                state = "falling";
                if (data.owner > -1)
                {
                    auto& as = m_avatars[data.owner].getComponent<PlayerAvatar>().holoEnt.getComponent<AvatarScale>();
                    as.target = 0.f;
                }
                {
                    auto& as = e.getComponent<AvatarScale>();
                    as.target = 1.f;
                }
                break;
            case Crate::Idle:
                state = "idle";
                break;
            case Crate::Carried:
                state = "carried";
                {
                    auto& as = m_avatars[e.getComponent<Crate>().owner].getComponent<PlayerAvatar>().holoEnt.getComponent<AvatarScale>();
                    as.target = 1.f;
                }
                {
                    auto& as = e.getComponent<AvatarScale>();
                    as.target = 0.f;
                }
                break;
            }
#ifdef CRO_DEBUG_
            LogI << "Set crate to " << state << ", with owner " << e.getComponent<Crate>().owner << std::endl;
#endif //DEBUG
        }
    };
    m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
}

void GameState::avatarUpdate(const PlayerStateChange& data)
{
    //TODO raise a message for this for things like sound effects
    //if not a local player


    std::string state;
    switch (data.playerState)
    {
    default: break;
    case PlayerEvent::Jumped:
        state = "jumped";
        break;
    case PlayerEvent::Landed:
        state = "landed";
        if (m_avatars[data.playerID].isValid())
        {
            m_avatars[data.playerID].getComponent<cro::ParticleEmitter>().stop();
            m_avatars[data.playerID].getComponent<cro::Transform>().setScale(glm::vec3(1.f));
        }
        break;
    case PlayerEvent::DroppedCrate:
        state = "dropped crate";
        break;
    case PlayerEvent::Teleported:
        state = "teleported";
        if (m_avatars[data.playerID].isValid())
        {
            m_avatars[data.playerID].getComponent<cro::ParticleEmitter>().start();
        }
        break;
    case PlayerEvent::Died:
        state = "died";
        if (m_avatars[data.playerID].isValid())
        {
            m_avatars[data.playerID].getComponent<cro::Transform>().setScale(glm::vec3(0.f));
            m_avatars[data.playerID].getComponent<PlayerAvatar>().holoEnt.getComponent<AvatarScale>().target = 0.f;
        }
        break;
    case PlayerEvent::None:
        state = "none";
        if (m_avatars[data.playerID].isValid())
        {
            
        }
        break;
    }
#ifdef CRO_DEBUG_
    LogI << "Player " << state << std::endl;
#endif //DEBUG
}