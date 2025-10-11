/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
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
#include "WavetableAnimator.hpp"
#include "SpawnerAnimationSystem.hpp"
#include "CameraControllerSystem.hpp"
#include "CommonConsts.hpp"
#include "SnailSystem.hpp"

#include <crogine/gui/Gui.hpp>
#include <crogine/core/GameController.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/ShadowCaster.hpp>
#include <crogine/ecs/components/DynamicTreeComponent.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>

#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Text.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/DynamicTreeSystem.hpp>
#include <crogine/ecs/systems/ParticleSystem.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem3D.hpp>
#include <crogine/ecs/systems/SpriteAnimator.hpp>

#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/graphics/MeshData.hpp>
#include <crogine/graphics/DynamicMeshBuilder.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/SpriteSheet.hpp>

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
    float bitrate = 40.f;
    constexpr float BitrateAlpha = 0.9f;
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
    m_gameScene         (context.appInstance.getMessageBus(), 512),
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
            //if (ImGui::Begin("GBuffer 0"))
            //{
            //    auto lightDir = glm::vec4(m_gameScene.getSunlight().getComponent<cro::Sunlight>().getDirection(), 0.f);
            //    ImGui::Text("Light Dir: %3.3f, %3.3f, %3.3f", lightDir.x, lightDir.y, lightDir.z);

            //    for (auto cam : m_cameras)
            //    {
            //        auto dir = cam.getComponent<cro::Camera>().getActivePass().viewMatrix * lightDir;
            //        ImGui::Text("Light view %3.3f, %3.3f, %3.3f", dir.x, dir.y, dir.z);

            //        const auto& buffer = cam.getComponent<cro::GBuffer>().buffer;
            //        auto size = glm::vec2(buffer.getSize());
            //        for (auto i = 0; i < 5; ++i)
            //        {
            //            ImGui::Image(buffer.getTexture(i), { size.x / 3.f, size.y / 3.f }, { 0.f, 1.f }, { 1.f, 0.f });
            //            //if ((i % 3) != 0)
            //            {
            //                ImGui::SameLine();
            //            }
            //        }
            //        //ImGui::NewLine();
            //        ImGui::Separator();
            //    }
            //}
            //ImGui::End();

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
            auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
            /*if (cam.renderFlags & TwoDeeFlags::Debug)
            {
                cam.renderFlags &= ~TwoDeeFlags::Debug;
            }
            else
            {
                cam.renderFlags |= TwoDeeFlags::Debug;
            }*/
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
    else if (msg.id == MessageID::AvatarMessage)
    {
        const auto& data = msg.getData<AvatarEvent>();
        switch (data.type)
        {
        default: break;
        case AvatarEvent::Died:
            cro::GameController::rumbleStart(m_sharedData.inputBindings[data.playerID].controllerID, 60000, 50000, 200);
            break;
        case AvatarEvent::Teleported:
        case AvatarEvent::Spawned:
            cro::GameController::rumbleStart(m_sharedData.inputBindings[data.playerID].controllerID, 55000, 48000, 100);
            break;
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
            m_bitrateClock.restart();
            bitrate = BitrateAlpha * bitrate + ((1.f - BitrateAlpha) * bitrateCounter);
            bitrateCounter = 0;
        }
    }
    else
    {
        //we've been disconnected somewhere - push error state
        m_sharedData.errorMessage = "Lost connection to host.";
        requestStackPush(States::Error);
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

    m_gameScene.render(m_cameras);
    m_uiScene.render();


#ifdef CRO_DEBUG_
    //render a far view of the scene in debug to a render texture
    auto oldCam = m_gameScene.setActiveCamera(m_debugCam);
    m_debugViewTexture.clear();
    m_gameScene.render();
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
    m_gameScene.addSystem<WavetableAnimatorSystem>(mb);
    m_gameScene.addSystem<SpawnerAnimationSystem>(mb);
    m_gameScene.addSystem<CrateSystem>(mb); //local collision to smooth out interpolation
    m_gameScene.addSystem<SnailSystem>(mb); //local collision to smooth out interpolation
    m_gameScene.addSystem<AvatarScaleSystem>(mb);
    m_gameScene.addSystem<PlayerSystem>(mb);
    m_gameScene.addSystem<CameraControllerSystem>(mb, m_avatars);
    m_gameScene.addSystem<cro::SpriteAnimator>(mb);
    m_gameScene.addSystem<cro::SpriteSystem3D>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
    //m_gameScene.addSystem<cro::DeferredRenderSystem>(mb).setEnvironmentMap(m_environmentMap);

    m_gameScene.addSystem<cro::ParticleSystem>(mb);

    m_gameScene.addDirector<DayNightDirector>();
    m_gameScene.addDirector<ParticleDirector>(m_resources.textures);


    m_uiScene.addSystem<PuntBarSystem>(mb);
    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);

    m_uiScene.addDirector<UIDirector>(m_sharedData, m_resources.textures, m_playerUIs);
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
    //m_resources.materials.get(m_materialIDs[MaterialID::Default]).deferred = true;
    //m_resources.materials.get(m_materialIDs[MaterialID::Default]).blendMode = cro::Material::BlendMode::Alpha;

    shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::ShadowMap, cro::ShaderResource::DepthMap);
    m_materialIDs[MaterialID::DefaultShadow] = m_resources.materials.add(m_resources.shaders.get(shaderID));


    m_resources.textures.load(TextureID::Life, "assets/images/life.png");
    m_resources.textures.get(TextureID::Life).setRepeated(true);
    m_resources.textures.load(TextureID::Portal, "assets/images/portal.png");
    m_resources.textures.get(TextureID::Portal).setRepeated(true);
    if (m_resources.shaders.loadFromString(ShaderID::Portal, PortalVertex, PortalFragment))
    {
        m_materialIDs[MaterialID::Portal] = m_resources.materials.add(m_resources.shaders.get(ShaderID::Portal));
        m_resources.materials.get(m_materialIDs[MaterialID::Portal]).setProperty("u_diffuseMap", m_resources.textures.get(TextureID::Portal));
        m_resources.materials.get(m_materialIDs[MaterialID::Portal]).blendMode = cro::Material::BlendMode::Alpha;
        portalShader = m_resources.shaders.get(ShaderID::Portal).getGLHandle();
        portalUniform = m_resources.shaders.get(ShaderID::Portal).getUniformID("u_time");
    }

    //model defs
    for (auto& md : m_modelDefs)
    {
        md = std::make_unique<cro::ModelDefinition>(m_resources, &m_environmentMap);
    }
    m_modelDefs[GameModelID::Crate]->loadFromFile("assets/models/box.cmt");
    m_modelDefs[GameModelID::Spawner]->loadFromFile("assets/models/spawner.cmt");
    m_modelDefs[GameModelID::Balloon]->loadFromFile("assets/models/balloon.cmt");
    m_modelDefs[GameModelID::Hologram]->loadFromFile("assets/models/hologram.cmt");


    //sprites
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/sprites/poopsnail.spt", m_resources.textures);
    m_sprites[SpriteID::PoopSnail] = spriteSheet.getSprite("poopsnail");


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
        0,1,2,3//, 1,0,3,2
    };
    const std::size_t vertComponentCount = 5;

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, mesh.vboAllocation.vboID));
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
    entity.getComponent<cro::Camera>().shadowMapBuffer.create(1024, 1024);

    m_debugCam = entity;
#endif

    //set up the particle director
    auto* particleDirector = m_gameScene.getDirector<ParticleDirector>();
    enum ParticleID
    {
        Squish01,
        Squish02,
        Squish03,
        Squish04,
        Squish05,
        Sprockets,
        Spark,
        Fire,

        Count
    };
    std::array<std::size_t, ParticleID::Count> ids{};

    ids[ParticleID::Squish01] = particleDirector->loadSettings("assets/particles/squish.xyp");
    particleDirector->getSettings(ids[ParticleID::Squish01]).colour = PlayerColours[0];

    ids[ParticleID::Squish02] = particleDirector->loadSettings("assets/particles/squish.xyp");
    particleDirector->getSettings(ids[ParticleID::Squish02]).colour = PlayerColours[1];

    ids[ParticleID::Squish03] = particleDirector->loadSettings("assets/particles/squish.xyp");
    particleDirector->getSettings(ids[ParticleID::Squish03]).colour = PlayerColours[2];

    ids[ParticleID::Squish04] = particleDirector->loadSettings("assets/particles/squish.xyp");
    particleDirector->getSettings(ids[ParticleID::Squish04]).colour = PlayerColours[3];

    ids[ParticleID::Squish05] = particleDirector->loadSettings("assets/particles/squish.xyp");
    particleDirector->getSettings(ids[ParticleID::Squish05]).colour = cro::Colour(std::uint8_t(230), 188, 20);

    ids[ParticleID::Sprockets] = particleDirector->loadSettings("assets/particles/box.xyp");
    ids[ParticleID::Spark] = particleDirector->loadSettings("assets/particles/spark.xyp");
    ids[ParticleID::Fire] = particleDirector->loadSettings("assets/particles/fire.xyp");

    auto particleHandler = [ids](const cro::Message& msg) -> std::optional<ParticleEvent>
    {
        switch (msg.id)
        {
        default: break;
        case MessageID::AvatarMessage:
        {
            const auto& data = msg.getData<AvatarEvent>();
            ParticleEvent evt;
            switch (data.type)
            {
            default: break;
            case AvatarEvent::Died:
                switch (data.playerID)
                {
                default: break;
                case 0:
                    evt.id = ids[ParticleID::Squish01];
                    break;
                case 1:
                    evt.id = ids[ParticleID::Squish02];
                    break;
                case 2:
                    evt.id = ids[ParticleID::Squish03];
                    break;
                case 3:
                    evt.id = ids[ParticleID::Squish04];
                    break;
                }

                evt.position = data.position;
                return evt;
            }
        }
        break;
        case MessageID::ActorMessage:
        {
            const auto& data = msg.getData<ActorEvent>();
            ParticleEvent evt;
            evt.position = data.position;

            switch (data.type)
            {
            default: break;
            case ActorEvent::Added:
                switch (data.id)
                {
                default: break;
                case ActorID::Explosion:
                    evt.id = ids[ParticleID::Fire];
                    return evt;
                case ActorID::Crate:
                    evt.id = ids[ParticleID::Spark];
                    return evt;
                }
                break;
            case ActorEvent::Removed:
                switch (data.id)
                {
                default: break;
                case ActorID::Explosion:
                    evt.id = ids[ParticleID::Fire];
                    return evt;
                case ActorID::Crate:
                    evt.id = ids[ParticleID::Sprockets];
                    evt.velocity = data.velocity;
                    return evt;
                case ActorID::PoopSnail:
                    evt.id = ids[ParticleID::Squish05];
                    return evt;
                }
                break;
            }
        }
            break;
        }
        
        return std::nullopt;
    };
    particleDirector->setMessageHandler(particleHandler);
}

void GameState::createScene()
{
    createDayCycle();

    loadMap();

    //ground plane
    cro::ModelDefinition modelDef(m_resources, &m_environmentMap);
    modelDef.loadFromFile("assets/models/ground_plane.cmt");

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    modelDef.createModel(entity);


    //hedz
    modelDef.loadFromFile("assets/models/head01.cmt");
    
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 23.f, -20.f });
    entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI);
    modelDef.createModel(entity);

    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        if (m_avatars[0].isValid())
        {
            auto target = glm::inverse(glm::lookAt(e.getComponent<cro::Transform>().getPosition(), m_avatars[0].getComponent<cro::Transform>().getWorldPosition(), cro::Transform::Y_AXIS));
            auto current = e.getComponent<cro::Transform>().getRotation();
            auto rot = glm::slerp(current, glm::quat_cast(target), dt);

            e.getComponent<cro::Transform>().setRotation(rot);
        }
    };


    modelDef.loadFromFile("assets/models/head02.cmt");

    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 23.f, 20.f });
    entity.getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, -0.154f);
    modelDef.createModel(entity);

    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        if (m_avatars[1].isValid())
        {
            auto target = glm::inverse(glm::lookAt(e.getComponent<cro::Transform>().getPosition(), m_avatars[1].getComponent<cro::Transform>().getWorldPosition(), cro::Transform::Y_AXIS));
            auto current = e.getComponent<cro::Transform>().getRotation();
            auto rot = glm::slerp(current, glm::quat_cast(target), dt);
            e.getComponent<cro::Transform>().setRotation(rot);
        }
    };

    //hillz
    modelDef.loadFromFile("assets/models/hills.cmt");

    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    modelDef.createModel(entity);


    //disable the default camera so we don't need a gbuffer on it
    m_gameScene.getActiveCamera().getComponent<cro::Camera>().active = false;
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

    cro::ModelDefinition definition(m_resources, &m_environmentMap);
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
            cro::Colour(0.6f, 0.6f, 1.f),
            cro::Colour(0.8f, 0.8f, 1.f)
        };

        cro::ModelDefinition portalModel(m_resources, &m_environmentMap);
        portalModel.loadFromFile("assets/models/portal.cmt");

        cro::EmitterSettings smokeParticles;
        smokeParticles.loadFromFile("assets/particles/smoke.xyp", m_resources.textures);

        cro::SpriteSheet spriteSheet;
        spriteSheet.loadFromFile("assets/sprites/electric.spt", m_resources.textures);

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

            glCheck(glBindBuffer(GL_ARRAY_BUFFER, mesh.vboAllocation.vboID));
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
                portalModel.createModel(entity);

                //force field
                entity = m_gameScene.createEntity();
                entity.addComponent<cro::Transform>().setPosition({ rect.left - ((PortalWidth - rect.width) / 2.f), rect.bottom, layerDepth });
                entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_meshIDs[MeshID::Portal]), m_resources.materials.get(m_materialIDs[MaterialID::Portal]));
                entity.getComponent<cro::Transform>().move({ PortalWidth * i, 0.f, 0.f });
                entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI* i);
            }

            const auto& cratePoints = mapData.getCratePositions(i);
            for (auto p : cratePoints)
            {
                entity = m_gameScene.createEntity();
                entity.addComponent<cro::Transform>().setPosition(glm::vec3(p, layerDepth ));
                entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, i* cro::Util::Const::PI);
                entity.addComponent<WavetableAnimator>();
                m_modelDefs[GameModelID::Spawner]->createModel(entity);

                auto particleEnt = m_gameScene.createEntity();
                particleEnt.addComponent<cro::Transform>().setPosition(glm::vec3(0.f, 0.f, -0.5f));
                particleEnt.addComponent<cro::ParticleEmitter>().settings = smokeParticles;
                particleEnt.getComponent<cro::ParticleEmitter>().start();

                entity.getComponent<cro::Transform>().addChild(particleEnt.getComponent<cro::Transform>());

                //attach electricity sprite
                auto elecEnt = m_gameScene.createEntity();
                elecEnt.addComponent<cro::Transform>().setScale(glm::vec3(1.f) / ConstVal::MapUnits);
                elecEnt.addComponent<cro::Model>();
                elecEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("electric");
                auto bounds = elecEnt.getComponent<cro::Sprite>().getTextureBounds();
                elecEnt.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
                elecEnt.addComponent<cro::SpriteAnimation>().play(0);

                entity.getComponent<cro::Transform>().addChild(elecEnt.getComponent<cro::Transform>());
            }
        }


        cro::ModelDefinition spawnModel(m_resources, &m_environmentMap);
        spawnModel.loadFromFile("assets/models/player_spawn.cmt");

        cro::ModelDefinition spinModel(m_resources, &m_environmentMap);
        spinModel.loadFromFile("assets/models/player_spinner.cmt");

        cro::EmitterSettings spawnParticles;
        spawnParticles.loadFromFile("assets/particles/spawn.xyp", m_resources.textures);

        const auto& spawnPoints = mapData.getSpawnPositions();
        for (auto i = 0u; i < spawnPoints.size(); ++i)
        {
            auto layerDepth = LayerDepth - (2 * (LayerDepth * (i % 2)));
            auto rect = SpawnBase;

            auto spawnEnt = m_gameScene.createEntity();
            spawnEnt.addComponent<cro::Transform>().setPosition(glm::vec3(spawnPoints[i].x, spawnPoints[i].y, layerDepth));
            spawnModel.createModel(spawnEnt);

            spawnEnt.addComponent<cro::DynamicTreeComponent>().setArea({ glm::vec3(rect.left, rect.bottom, LayerThickness), glm::vec3(rect.width, rect.height, -LayerThickness) });
            spawnEnt.getComponent<cro::DynamicTreeComponent>().setFilterFlags(layerDepth > 0 ? 1 : 2);

            spawnEnt.addComponent<CollisionComponent>().rectCount = 1;
            spawnEnt.getComponent<CollisionComponent>().rects[0].material = CollisionMaterial::Solid;
            spawnEnt.getComponent<CollisionComponent>().rects[0].bounds = rect;
            spawnEnt.getComponent<CollisionComponent>().calcSum();


            auto spinEnt = m_gameScene.createEntity();
            spinEnt.addComponent<cro::Transform>();
            spinEnt.addComponent<cro::ParticleEmitter>().settings = spawnParticles;
            spinEnt.getComponent<cro::ParticleEmitter>().start();

            spinModel.createModel(spinEnt);
            spawnEnt.getComponent<cro::Transform>().addChild(spinEnt.getComponent<cro::Transform>());
#ifdef CRO_DEBUG_
            addBoxDebug(spawnEnt, m_gameScene, cro::Colour::Red);
#endif
            spawnEnt.addComponent<SpawnerAnimation>().spinnerEnt = spinEnt;
            m_spawnerEntities[i] = spawnEnt;
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
            requestStackPop();
            requestStackPush(States::MainMenu);
        }
        break;
    case PacketID::LogMessage:
        LogW << "Server: " << sv::LogStrings[packet.as<std::int32_t>()] << std::endl;
        break;
    case PacketID::DayNightUpdate:
        m_gameScene.getDirector<DayNightDirector>()->setTimeOfDay(cro::Util::Net::decompressFloat(packet.as<std::int16_t>(), 8));
        break;
    case PacketID::EntityRemoved:
    {
        removeEntity(packet.as<ActorRemoved>());
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
        if (m_inputParsers.count(update.getPlayerID()))
        {
            m_gameScene.getSystem<PlayerSystem>()->reconcile(m_inputParsers.at(update.getPlayerID()).getEntity(), update);
        }
    }
        break;
    case PacketID::ActorUpdate:
        updateActor(packet.as<ActorUpdate>());
        break;
    case PacketID::ActorIdleUpdate:
        updateIdleActor(packet.as<ActorIdleUpdate>());
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
    case PacketID::SnailUpdate:
        snailUpdate(packet.as<SnailState>());
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
    {
        auto evt = packet.as<std::uint8_t>();
        auto* msg = m_uiScene.postMessage<GameEvent>(MessageID::GameMessage);
        msg->type = static_cast<GameEvent::Type>(evt);

        switch (evt)
        {
        default: break;
        case GameEvent::GameBegin:
            startGame();
            break;
        case GameEvent::RoundWarn:
        
        break;
        case GameEvent::SuddenDeath:

        break;
        case GameEvent::GameEnd:
        //block input this should time out from the server
        for (auto& ip : m_inputParsers)
        {
            ip.second.setEnabled(false);
        }
        break;
        }
    }
        break;
    case PacketID::ClientDisconnected:
        m_sharedData.playerData[packet.as<std::uint8_t>()].name.clear();
        break;
    case PacketID::RoundStats:
        updateRoundStats(packet.as<RoundStats>());
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
    cro::ModelDefinition md(m_resources, &m_environmentMap);
    md.loadFromFile("assets/models/player_box.cmt");

    cro::EmitterSettings particles;
    particles.loadFromFile("assets/particles/portal.xyp", m_resources.textures);

    if (info.connectionID == m_sharedData.clientConnection.connectionID)
    {
        std::uint8_t playerNumber = info.playerID + info.connectionID;
        if (m_inputParsers.count(playerNumber) == 0)
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
            root.getComponent<Player>().cameraTargetIndex = playerNumber;

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


            m_sharedData.localPlayer.playerID = playerNumber; //this is irrelevant in split screen
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

            auto camEnt = m_cameras.emplace_back(m_gameScene.createEntity());
            camEnt.addComponent<cro::Transform>().setPosition({ 0.f, CameraHeight, CameraDistance });
            //camEnt.addComponent<cro::GBuffer>();

            auto rotation = glm::lookAt(camEnt.getComponent<cro::Transform>().getPosition(), glm::vec3(0.f, 0.f, -50.f), cro::Transform::Y_AXIS);
            camEnt.getComponent<cro::Transform>().rotate(glm::inverse(rotation));
            camEnt.addComponent<std::uint8_t>() = playerNumber; //so we can tell which player this cam follows

            auto& cam = m_cameras.back().addComponent<cro::Camera>();
            /*cam.reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
            cam.reflectionBuffer.setSmooth(true);
            cam.refractionBuffer.create(ReflectionMapSize, ReflectionMapSize);
            cam.refractionBuffer.setSmooth(true);*/
            cam.shadowMapBuffer.create(2048, 2048);

            auto camController = m_gameScene.createEntity();
            camController.addComponent<cro::Transform>().addChild(camEnt.getComponent<cro::Transform>());
            camController.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI * root.getComponent<Player>().collisionLayer);
            camController.addComponent<CameraController>().targetPlayer = root;


            //displays above player when carrying a box
            auto holoEnt = m_gameScene.createEntity();
            holoEnt.addComponent<cro::Transform>().setPosition(glm::vec3(0.f, PlayerBounds[1].y * 1.5f, 0.f));
            holoEnt.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
            m_modelDefs[GameModelID::Hologram]->createModel(holoEnt);
            holoEnt.addComponent<AvatarScale>().rotationSpeed = HoloRotationSpeed;
            

            //placeholder for player model
            auto playerEnt = m_gameScene.createEntity();
            playerEnt.addComponent<cro::Transform>().setOrigin({ 0.f, -0.4f, 0.f });
            md.createModel(playerEnt);
            playerEnt.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", PlayerColours[info.playerID + info.connectionID]);
            playerEnt.addComponent<PlayerAvatar>().holoEnt = holoEnt;
            playerEnt.addComponent<cro::ParticleEmitter>().settings = particles;


            root.getComponent<Player>().avatar = playerEnt;
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
                        case Player::State::Dead:
                            ImGui::Text("Dead");
                            break;
                        case Player::State::Reset:
                            ImGui::Text("Resetting");
                            break;
                        case ::Player::State::Spectate:
                            ImGui::Text("Spectating");
                            break;
                        }

                        ImGui::Text("Vel X: %3.3f", player.velocity.x);
                        auto pos = root.getComponent<cro::Transform>().getPosition();
                        ImGui::Text("Pos: %3.3f, %3.3f, %3.3f", pos.x, pos.y, pos.z);
                        if (player.direction == -1)
                        {
                            ImGui::Text("Direction : Left");
                        }
                        else
                        {
                            ImGui::Text("Direction : Right");
                        }
                        ImGui::Text("Layer: %d", player.collisionLayer);

                        auto foot = player.collisionFlags & (1 << CollisionMaterial::Foot);
                        ImGui::Text("Foot: %d", foot);

                        auto sensor = player.collisionFlags & (1 << CollisionMaterial::Sensor);
                        ImGui::Text("Sensor: %d", sensor);

                        ImGui::Text("Punt Level: %3.3f", player.puntLevel);
                        ImGui::Text("Lives: %d", player.lives);

                        //ImGui::Text("Box %3.3f", root.getComponent<CollisionComponent>().rects[2].bounds.left);
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
        md.createModel(entity);

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
        holoEnt.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());
        holoEnt.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
        m_modelDefs[GameModelID::Hologram]->createModel(holoEnt);
        holoEnt.addComponent<AvatarScale>().rotationSpeed = HoloRotationSpeed;

        entity.addComponent<PlayerAvatar>().holoEnt = holoEnt; //to track joined crates
        entity.getComponent<cro::Transform>().addChild(holoEnt.getComponent<cro::Transform>());
        m_avatars[entity.getComponent<Actor>().id] = entity;

        if (m_sharedData.localPlayerCount == 1)
        {
            //display remote player names
            //TODO - something with billboards?
        }

#ifdef CRO_DEBUG_
        addBoxDebug(entity, m_gameScene, cro::Colour::Magenta);
#endif
    }

    

    m_uiScene.getDirector<UIDirector>()->addPlayer(); //just tracks the player count
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
        m_modelDefs[GameModelID::Crate]->createModel(entity);

        //add some collision properties which are updated from the server
        //rather than simulated locally, so the player gets proper collision
        //when the crate is idle
        entity.addComponent<CollisionComponent>().rectCount = 1;
        entity.getComponent<CollisionComponent>().rects[0].material = CollisionMaterial::Crate;
        entity.getComponent<CollisionComponent>().rects[0].bounds = CrateArea;
        entity.getComponent<CollisionComponent>().calcSum();

        entity.addComponent<cro::DynamicTreeComponent>().setArea(CrateBounds);
        entity.getComponent<cro::DynamicTreeComponent>().setFilterFlags((position.z > 0 ? 1 : 2) | CollisionID::Crate);

        entity.addComponent<Crate>().local = true;

        entity.addComponent<AvatarScale>().current = 1.f;
        entity.getComponent<AvatarScale>().target = 1.f;

#ifdef CRO_DEBUG_
        addBoxDebug(entity, m_gameScene, cro::Colour::Red);        
#endif
        break;
    case ActorID::Explosion:

        break;
    case ActorID::Balloon:
        m_modelDefs[GameModelID::Balloon]->createModel(entity);
        break;
    case ActorID::PoopSnail:
        entity.addComponent<cro::Model>();
        entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::PoopSnail];
        entity.getComponent<cro::Transform>().setScale(glm::vec3(1.f) / 32.f);
        entity.addComponent<cro::SpriteAnimation>().play(0);
        {
            auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
            entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, 0.f, 0.f });
        }

        entity.addComponent<CollisionComponent>().rectCount = 1;
        entity.getComponent<CollisionComponent>().rects[0].material = CollisionMaterial::Snail;
        entity.getComponent<CollisionComponent>().rects[0].bounds = SnailArea;
        entity.getComponent<CollisionComponent>().calcSum();

        entity.addComponent<cro::DynamicTreeComponent>().setArea(SnailBounds);
        entity.getComponent<cro::DynamicTreeComponent>().setFilterFlags((position.z > 0 ? 1 : 2));

        entity.addComponent<Snail>().local = true;
        break;
    }

    //let the world know
    auto* msg = m_gameScene.postMessage<ActorEvent>(MessageID::ActorMessage);
    msg->id = as.id;
    msg->position = entity.getComponent<cro::Transform>().getPosition();
    msg->type = ActorEvent::Added;
}

void GameState::updateActor(ActorUpdate update)
{
    cro::Command cmd;
    cmd.targetFlags = Client::CommandID::Interpolated;
    cmd.action = [update](cro::Entity e, float)
    {
        if (e.isValid() &&
            e.getComponent<Actor>().serverEntityId == update.serverID)
        {
            auto& interp = e.getComponent<InterpolationComponent>();
            interp.setTarget({ cro::Util::Net::decompressVec3(update.position), 
                cro::Util::Net::decompressQuat(update.rotation), 
                update.timestamp });
        }
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
}

void GameState::updateIdleActor(ActorIdleUpdate update)
{
    cro::Command cmd;
    cmd.targetFlags = Client::CommandID::Interpolated;
    cmd.action = [update](cro::Entity e, float)
    {
        if (e.isValid() &&
            e.getComponent<Actor>().serverEntityId == update.serverID)
        {
            const auto& tx = e.getComponent<cro::Transform>();

            auto& interp = e.getComponent<InterpolationComponent>();
            interp.setTarget({ tx.getPosition(), tx.getRotation(), update.timestamp });
        }
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
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
#ifdef CRO_DEBUG_
    for (auto& ip : m_inputParsers)
    {
        ip.second.setEnabled(true);
    }
#else
    //TODO wrangle this into the UI director
    auto& font = m_sharedData.fonts.get(m_sharedData.defaultFontID);
    
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(cro::DefaultSceneSize) / 2.f);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("READY");
    entity.getComponent<cro::Text>().setFillColour(cro::Colour::Red);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setCharacterSize(120);

    entity.addComponent<cro::Callback>().setUserData<std::pair<float, std::uint32_t>>(2.f, 0);
    entity.getComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& [currTime, state] = e.getComponent<cro::Callback>().getUserData<std::pair<float, std::uint32_t>>();
        currTime -= dt;
        if (currTime < 0)
        {
            switch (state)
            {
            default:
            case 0:
                e.getComponent<cro::Text>().setString("GO!");
                state++;

                for (auto& ip : m_inputParsers)
                {
                    ip.second.setEnabled(true);
                }
                currTime = 1.f;

                break;
            case 1:
                e.getComponent<cro::Callback>().active = false;
                m_uiScene.destroyEntity(e);
                break;
            }
        }
    };
#endif
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
            //LogI << "Set crate to " << state << ", with owner " << e.getComponent<Crate>().owner << std::endl;
#endif //DEBUG
        }
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
}

void GameState::snailUpdate(const SnailState& data)
{
    cro::Command cmd;
    cmd.targetFlags = Client::CommandID::Interpolated;
    cmd.action = [data](cro::Entity e, float)
    {
        if (e.getComponent<Actor>().serverEntityId == data.serverEntityID)
        {
            switch (data.state)
            {
            default: break;
            case Snail::Falling:

                break;
            case Snail::Walking:
                e.getComponent<cro::SpriteAnimation>().play(0);
                break;
            case Snail::Digging:
                e.getComponent<cro::SpriteAnimation>().play(1);
                break;
            }
        }
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
}

void GameState::avatarUpdate(const PlayerStateChange& data)
{
    //raise a message for this for things like sound effects
    auto* msg = getContext().appInstance.getMessageBus().post<AvatarEvent>(MessageID::AvatarMessage);
    msg->playerID = data.playerID;
    msg->lives = data.lives;

    if (m_avatars[data.playerID].isValid())
    {
        msg->position = m_avatars[data.playerID].getComponent<cro::Transform>().getWorldPosition();
    }

    switch (data.playerState)
    {
    default: break;
    case PlayerEvent::Scored:
        msg->type = AvatarEvent::Scored;
        break;
    case PlayerEvent::Spawned:
        msg->type = AvatarEvent::Spawned;
        m_spawnerEntities[data.playerID].getComponent<SpawnerAnimation>().start();
        break;
    case PlayerEvent::Reset:
        msg->type = AvatarEvent::Reset;
        break;
    case PlayerEvent::Jumped:
        msg->type = AvatarEvent::Jumped;
        break;
    case PlayerEvent::Landed:
        msg->type = AvatarEvent::Landed;
        if (m_avatars[data.playerID].isValid())
        {
            m_avatars[data.playerID].getComponent<cro::ParticleEmitter>().stop();
            m_avatars[data.playerID].getComponent<cro::Transform>().setScale(glm::vec3(1.f));
        }
        break;
    case PlayerEvent::DroppedCrate:
        msg->type = AvatarEvent::DroppedCrate;
        break;
    case PlayerEvent::Teleported:
        msg->type = AvatarEvent::Teleported;
        if (m_avatars[data.playerID].isValid())
        {
            m_avatars[data.playerID].getComponent<cro::ParticleEmitter>().start();
        }
        break;
    case PlayerEvent::Died:
        msg->type = AvatarEvent::Died;
        if (m_avatars[data.playerID].isValid())
        {
            m_avatars[data.playerID].getComponent<cro::Transform>().setScale(glm::vec3(0.f));
            m_avatars[data.playerID].getComponent<PlayerAvatar>().holoEnt.getComponent<AvatarScale>().target = 0.f;

            msg->position = m_avatars[data.playerID].getComponent<cro::Transform>().getWorldPosition();
        }
        break;
    case PlayerEvent::None:
        msg->type = AvatarEvent::None;
        if (m_avatars[data.playerID].isValid())
        {
            
        }
        break;
    case PlayerEvent::Retired:
        msg->type = AvatarEvent::Retired;
        LogI << (int)data.playerID << " was eliminated" << std::endl;
        break;
    }
}

void GameState::removeEntity(ActorRemoved actor)
{
    cro::Command cmd;
    cmd.targetFlags = Client::CommandID::Interpolated;
    cmd.action = [&,actor](cro::Entity e, float)
    {
        if (!e.destroyed() && //this might be raised multiple times in split screen
            e.getComponent<Actor>().serverEntityId == actor.serverID)
        {
            e.getComponent<InterpolationComponent>().setRemoved(actor.timestamp);

            if (e.hasComponent<PlayerAvatar>())
            {
                m_gameScene.destroyEntity(e.getComponent<PlayerAvatar>().holoEnt);
            }
        }
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
}