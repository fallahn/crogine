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

#include "GolfState.hpp"
#include "GameConsts.hpp"
#include "MenuConsts.hpp"
#include "CommandIDs.hpp"
#include "PacketIDs.hpp"
#include "SharedStateData.hpp"
#include "InterpolationSystem.hpp"
#include "ClientPacketData.hpp"
#include "MessageIDs.hpp"
#include "Clubs.hpp"

#include <crogine/core/ConfigFile.hpp>

#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/SpriteAnimator.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/BillboardSystem.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/BillboardCollection.hpp>

#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/graphics/DynamicMeshBuilder.hpp>
#include <crogine/graphics/CircleMeshBuilder.hpp>

#include <crogine/network/NetClient.hpp>

#include <crogine/gui/Gui.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/util/Matrix.hpp>
#include <crogine/util/Network.hpp>
#include <crogine/util/Random.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include "../ErrorCheck.hpp"

#include <sstream>

namespace
{
#include "WaterShader.inl"
#include "TerrainShader.inl"

    glm::vec3 debugPos = glm::vec3(0.f);
    glm::vec3 ballpos = glm::vec3(0.f);

    const cro::Time ReadyPingFreq = cro::seconds(1.f);

    //used to set the camera target
    struct TargetInfo final
    {
        float targetHeight = CameraStrokeHeight;
        float targetOffset = CameraStrokeOffset;
        float currentHeight = 0.f;
        float currentOffset = 0.f;

        cro::Entity waterPlane;
    };
}

GolfState::GolfState(cro::StateStack& stack, cro::State::Context context, SharedStateData& sd)
    : cro::State    (stack, context),
    m_sharedData    (sd),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus()),
    m_inputParser   (sd.inputBinding, context.appInstance.getMessageBus()),
    m_wantsGameState(true),
    m_currentHole   (0),
    m_terrainBuilder(m_holeData),
    m_camRotation   (0.f),
    m_roundEnded    (false),
    m_viewScale     (1.f)
{
    context.mainWindow.loadResources([this]() {
        loadAssets();
        addSystems();
        buildScene();
        });

#ifdef CRO_DEBUG_
    registerWindow([&]() 
        {
            if (ImGui::Begin("buns"))
            {
                //ImGui::Text("Cam Rotation: %3.3f", m_camRotation);

                //ImGui::Text("ball pos: %3.3f, %3.3f, %3.3f", ballpos.x, ballpos.y, ballpos.z);

                /*auto rot = m_inputParser.getYaw();
                ImGui::Text("Rotation %3.2f", rot);

                auto power = m_inputParser.getPower();
                ImGui::Text("Power: %3.3f", power);

                auto hook = m_inputParser.getHook();
                ImGui::Text("Hook: %3.3f", hook);*/

                float height = m_gameScene.getActiveCamera().getComponent<cro::Transform>().getPosition().y;
                ImGui::Text("Cam height: %3.3f", height);

                auto club = Clubs[getClub()];
                ImGui::Text("Club: %s", club);

                ImGui::Text("Terrain: %s", TerrainStrings[m_currentPlayer.terrain]);

                //ImGui::Image(m_debugTexture.getTexture(), { 320.f, 200.f }, { 0.f, 1.f }, { 1.f, 0.f });
                ImGui::Image(m_gameScene.getActiveCamera().getComponent<cro::Camera>().reflectionBuffer.getTexture(), { 300.f, 300.f }, { 0.f, 1.f }, { 1.f, 0.f });
                //ImGui::Image(m_gameScene.getActiveCamera().getComponent<cro::Camera>().shadowMapBuffer.getTexture(), { 300.f, 300.f }, { 0.f, 1.f }, { 1.f, 0.f });

                if (ImGui::Button("Save Image"))
                {
                    auto path = cro::FileSystem::saveFileDialogue("","png");
                    if (!path.empty())
                    {
                        m_debugTexture.saveToFile(path);
                    }
                }
            }
            ImGui::End();
        });
#endif
}

//public
bool GolfState::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse)
    {
        return true;
    }


    const auto scrollScores = [&](std::int32_t step)
    {
        cro::Command cmd;
        cmd.targetFlags = CommandID::UI::ScoreScroll;
        cmd.action = [step](cro::Entity e, float)
        {
            e.getComponent<cro::Callback>().getUserData<std::int32_t>() = step;
            e.getComponent<cro::Callback>().active = true;
        };
        m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
    };

    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        /*case SDLK_ESCAPE:
        case SDLK_BACKSPACE:
            requestStackClear();
            requestStackPush(States::MainMenu);
            break;
        case SDLK_SPACE:
            hitBall();
            break;*/
        case SDLK_TAB:
            showScoreboard(false);
            break;
#ifdef CRO_DEBUG_
        case SDLK_F2:
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint8_t(ServerCommand::NextHole), cro::NetFlag::Reliable);
            break;
        case SDLK_F3:
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint8_t(ServerCommand::NextPlayer), cro::NetFlag::Reliable);
            break;
        case SDLK_F4:
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint8_t(ServerCommand::GotoGreen), cro::NetFlag::Reliable);
            break;
        case SDLK_F6:
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint8_t(ServerCommand::EndGame), cro::NetFlag::Reliable);
            break;
        case SDLK_F7:
            showCountdown(10);
            break;
#endif
        }
    }
    else if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_TAB:
            showScoreboard(true);
            break;
        case SDLK_UP:
            scrollScores(1);
            break;
        case SDLK_DOWN:
            scrollScores(-1);
            break;
        case SDLK_RETURN:
            showScoreboard(false);
            break;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONDOWN)
    {
        switch (evt.cbutton.button)
        {
        default: break;
        case SDL_CONTROLLER_BUTTON_BACK:
            showScoreboard(true);
            break;
        case SDL_CONTROLLER_BUTTON_B:
            showScoreboard(false);
            break;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        switch (evt.cbutton.button)
        {
        default: break;
        case SDL_CONTROLLER_BUTTON_BACK:
            showScoreboard(false);
            break;
        }
    }



    m_inputParser.handleEvent(evt);

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);

    return true;
}

void GolfState::handleMessage(const cro::Message& msg)
{
    switch (msg.id)
    {
    default: break;
    case MessageID::GolfMessage:
    {
        const auto& data = msg.getData<GolfEvent>();
        if (data.type == GolfEvent::HitBall)
        {
            hitBall();
        }
        else if (data.type == GolfEvent::ClubChanged)
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::StrokeIndicator;
            cmd.action = [&](cro::Entity e, float) 
            {
                float scale = Clubs[getClub()].power / Clubs[ClubID::Driver].power;
                e.getComponent<cro::Transform>().setScale({ scale, 1.f });
            };
            m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
        }
    }
    break;
    }

    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool GolfState::simulate(float dt)
{
    //glm::vec3 move(0.f);
    //const float speed = 20.f * dt;
    //if (cro::Keyboard::isKeyPressed(SDL_SCANCODE_W))
    //{
    //    move.z -= speed;
    //}
    //if (cro::Keyboard::isKeyPressed(SDL_SCANCODE_S))
    //{
    //    move.z += speed;
    //}
    //if (cro::Keyboard::isKeyPressed(SDL_SCANCODE_A))
    //{
    //    move.x -= speed;
    //}
    //if (cro::Keyboard::isKeyPressed(SDL_SCANCODE_D))
    //{
    //    move.x += speed;
    //}
    //debugPos += move;
    //setCameraPosition(debugPos);


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
        requestStackPush(States::Golf::Error);
    }

    //update time uniforms
    static float elapsed = dt;
    elapsed += dt;

    glUseProgram(m_waterShader.shaderID);
    glUniform1f(m_waterShader.timeUniform, elapsed * 15.f);
    //glUseProgram(0);



    m_inputParser.update(dt);

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);

    return true;
}

void GolfState::render()
{
    //render reflections first
    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    //cam.renderFlags = NoPlanes | NoRefract;
    auto oldVP = cam.viewport;

    cam.viewport = { 0.f,0.f,1.f,1.f };

    cam.setActivePass(cro::Camera::Pass::Reflection);
    cam.reflectionBuffer.clear(cro::Colour::Red);
    m_gameScene.render(cam.reflectionBuffer);
    cam.reflectionBuffer.display();

    //cam.renderFlags = NoPlanes | PlayerPlanes[i] | NoReflect | NoRefract;
    cam.setActivePass(cro::Camera::Pass::Final);
    cam.viewport = oldVP;


    //then render scene
    m_renderTexture.clear();
    m_gameScene.render(m_renderTexture);
    m_renderTexture.display();

#ifdef CRO_DEBUG_
    auto oldCam = m_gameScene.setActiveCamera(m_debugCam);
    m_debugTexture.clear(cro::Colour::Magenta);
    m_gameScene.render(m_debugTexture);
    m_debugTexture.display();
    m_gameScene.setActiveCamera(oldCam);
#endif

    m_uiScene.render(cro::App::getWindow());
}

//private
void GolfState::loadAssets()
{
    //render buffer
    auto vpSize = calcVPSize();
    m_renderTexture.create(static_cast<std::uint32_t>(vpSize.x) , static_cast<std::uint32_t>(vpSize.y));

    //model definitions
    for (auto& md : m_modelDefs)
    {
        md = std::make_unique<cro::ModelDefinition>(m_resources);
    }
    m_modelDefs[ModelID::Ball]->loadFromFile("assets/golf/models/ball.cmt");
    m_modelDefs[ModelID::BallShadow]->loadFromFile("assets/golf/models/ball_shadow.cmt");

    //UI stuffs
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/ui.spt", m_resources.textures);
    m_sprites[SpriteID::PowerBar] = spriteSheet.getSprite("power_bar");
    m_sprites[SpriteID::PowerBarInner] = spriteSheet.getSprite("power_bar_inner");
    m_sprites[SpriteID::HookBar] = spriteSheet.getSprite("hook_bar");
    m_sprites[SpriteID::WindIndicator] = spriteSheet.getSprite("wind_dir");

    spriteSheet.loadFromFile("assets/golf/sprites/player.spt", m_resources.textures);
    m_sprites[SpriteID::Player01] = spriteSheet.getSprite("female");
    m_sprites[SpriteID::Player02] = spriteSheet.getSprite("male");


    m_resources.fonts.load(FontID::UI, "assets/golf/fonts/IBM_CGA.ttf");


    //ball resources - ball is rendered as a single point
    //at a distance, and as a model when closer
    glCheck(glPointSize(BallPointSize));

    auto shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::VertexColour);
    m_ballResources.materialID = m_resources.materials.add(m_resources.shaders.get(shaderID));
    m_ballResources.ballMeshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_POINTS));
    m_ballResources.shadowMeshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_POINTS));

    auto* meshData = &m_resources.meshes.getMesh(m_ballResources.ballMeshID);
    std::vector<float> verts =
    {
        0.f, 0.f, 0.f,    LeaderboardTextLight.getRed(), LeaderboardTextLight.getGreen(), LeaderboardTextLight.getBlue(), 1.f
    };
    std::vector<std::uint32_t> indices =
    {
        0
    };

    meshData->vertexCount = 1;
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    auto* submesh = &meshData->indexData[0];
    submesh->indexCount = 1;
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    meshData = &m_resources.meshes.getMesh(m_ballResources.shadowMeshID);
    verts =
    {
        0.f, 0.f, 0.f,    0.5f, 0.5f, 0.5f, 1.f,
    };
    meshData->vertexCount = 1;
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    submesh = &meshData->indexData[0];
    submesh->indexCount = 1;
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));



    //load the map data
    bool error = false;
    auto mapDir = m_sharedData.mapDirectory.toAnsiString();
    auto mapPath = ConstVal::MapPath + mapDir + "/course.data";

    if (!cro::FileSystem::fileExists(mapPath))
    {
        LOG("Course file doesn't exist", cro::Logger::Type::Error);
        error = true;
    }

    cro::ConfigFile courseFile;
    if (!courseFile.loadFromFile(mapPath))
    {
        error = true;
    }

    std::vector<std::string> holeStrings;
    const auto& props = courseFile.getProperties();
    for (const auto& prop : props)
    {
        const auto& name = prop.getName();
        if (name == "hole")
        {
            holeStrings.push_back(prop.getValue<std::string>());
        }
        else if (name == "skybox")
        {
            m_gameScene.setCubemap(prop.getValue<std::string>());

            //disable smoothing for super-pixels
            glCheck(glBindTexture(GL_TEXTURE_CUBE_MAP, m_gameScene.getCubemap().textureID));
            glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
            glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        }
    }

    if (holeStrings.empty())
    {
        LOG("No hole files in course data", cro::Logger::Type::Error);
        error = true;
    }

    cro::ConfigFile holeCfg;
    cro::ModelDefinition modelDef(m_resources);
    for (const auto& hole : holeStrings)
    {
        if (!cro::FileSystem::fileExists(hole))
        {
            LOG("Hole file is missing", cro::Logger::Type::Error);
            error = true;
        }

        if (!holeCfg.loadFromFile(hole))
        {
            LOG("Failed opening hole file", cro::Logger::Type::Error);
            error = true;
        }

        static constexpr std::int32_t MaxProps = 5;
        std::int32_t propCount = 0;
        auto& holeData = m_holeData.emplace_back();

        const auto& holeProps = holeCfg.getProperties();
        for (const auto& holeProp : holeProps)
        {
            const auto& name = holeProp.getName();
            if (name == "map")
            {
                if (!m_currentMap.loadFromFile(holeProp.getValue<std::string>()))
                {
                    error = true;
                }
                holeData.mapPath = holeProp.getValue<std::string>();
                propCount++;
            }
            else if (name == "pin")
            {
                //TODO not sure how we ensure these are sane values?
                auto pin = holeProp.getValue<glm::vec2>();
                holeData.pin = { pin.x, 0.f, -pin.y };
                propCount++;
            }
            else if (name == "tee")
            {
                auto tee = holeProp.getValue<glm::vec2>();
                holeData.tee = { tee.x, 0.f, -tee.y };
                propCount++;
            }
            else if (name == "par")
            {
                holeData.par = holeProp.getValue<std::int32_t>();
                if (holeData.par < 1 || holeData.par > 10)
                {
                    LOG("Invalid PAR value", cro::Logger::Type::Error);
                    error = true;
                }
                propCount++;
            }
            else if (name == "model")
            {
                if (modelDef.loadFromFile(holeProp.getValue<std::string>()))
                {
                    holeData.modelEntity = m_gameScene.createEntity();
                    holeData.modelEntity.addComponent<cro::Transform>();
                    modelDef.createModel(holeData.modelEntity);
                    holeData.modelEntity.getComponent<cro::Model>().setHidden(true);
                    propCount++;
                }
                else
                {
                    LOG("Failed loading model file", cro::Logger::Type::Error);
                    error = true;
                }
            }
        }

        if (propCount != MaxProps)
        {
            LOG("Missing hole property", cro::Logger::Type::Error);
            error = true;
        }
    }


    if (error)
    {
        m_sharedData.errorMessage = "Failed to load course data";
        requestStackPush(States::Golf::Error);
    }


    //reserve the slots for each hole score
    for (auto& client : m_sharedData.connectionData)
    {
        for (auto& player : client.playerData)
        {
            player.score = 0;
            player.holeScores.clear();
            player.holeScores.resize(holeStrings.size());
            std::fill(player.holeScores.begin(), player.holeScores.end(), 0);
        }
    }


    //load materials
    std::fill(m_materialIDs.begin(), m_materialIDs.end(), -1);

    //cel shaded material
    m_resources.shaders.loadFromString(ShaderID::Cel, CelVertex, CelFragment);
    auto& shader = m_resources.shaders.get(ShaderID::Cel);
    m_materialIDs[MaterialID::Cel] = m_resources.materials.add(shader);

    shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::VertexColour);
    m_materialIDs[MaterialID::WireFrame] = m_resources.materials.add(m_resources.shaders.get(shaderID));
    m_resources.materials.get(m_materialIDs[MaterialID::WireFrame]).blendMode = cro::Material::BlendMode::Alpha;

    m_resources.shaders.loadFromString(ShaderID::Water, WaterVertex, WaterFragment);
    m_materialIDs[MaterialID::Water] = m_resources.materials.add(m_resources.shaders.get(ShaderID::Water));


    m_waterShader.shaderID = m_resources.shaders.get(ShaderID::Water).getGLHandle();
    m_waterShader.timeUniform = m_resources.shaders.get(ShaderID::Water).getUniformMap().at("u_time");
#ifdef CRO_DEBUG_
    m_debugTexture.create(320, 200);
#endif
}

void GolfState::addSystems()
{
    auto& mb = m_gameScene.getMessageBus();

    m_gameScene.addSystem<InterpolationSystem>(mb);
    m_gameScene.addSystem<cro::CommandSystem>(mb);
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::SkeletalAnimator>(mb);
    m_gameScene.addSystem<cro::BillboardSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::SpriteAnimator>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void GolfState::buildScene()
{
    if (m_holeData.empty())
    {
        return;
    }


    //quality holing
    cro::ModelDefinition md(m_resources);
    md.loadFromFile("assets/golf/models/cup.cmt");
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    md.createModel(entity);

    auto holeEntity = entity;

    md.loadFromFile("assets/golf/models/flag.cmt");
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Flag;
    md.createModel(entity);
    if (md.hasSkeleton())
    {
        entity.getComponent<cro::Skeleton>().play(0);
    }

    auto flagEntity = entity;

    //displays the stroke direction
    auto pos = m_holeData[0].tee;
    pos.y += 0.01f;
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(pos);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, m_inputParser.getYaw());
    };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::StrokeIndicator;

    auto meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_LINE_STRIP));
    auto material = m_resources.materials.get(m_materialIDs[MaterialID::WireFrame]);
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    auto* meshData = &entity.getComponent<cro::Model>().getMeshData();

    std::vector<float> verts =
    {
        0.f, 0.f, 0.f,    1.f, 1.f, 1.f, 1.f,
        5.f, 0.f, 0.f,    1.f, 1.f, 1.f, 0.2f
    };
    std::vector<std::uint32_t> indices =
    {
        0,1
    };

    auto vertStride = (meshData->vertexSize / sizeof(float));
    meshData->vertexCount = verts.size() / vertStride;
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    auto* submesh = &meshData->indexData[0];
    submesh->indexCount = static_cast<std::uint32_t>(indices.size());
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    entity.getComponent<cro::Model>().setHidden(true);


    //draw the flag pole as a single line which can be
    //see from a distance - hole and model are also attached to this
    meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_LINE_STRIP));
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Hole;
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    entity.addComponent<cro::Transform>().setPosition(m_holeData[0].pin);
    entity.getComponent<cro::Transform>().addChild(holeEntity.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().addChild(flagEntity.getComponent<cro::Transform>());

    meshData = &entity.getComponent<cro::Model>().getMeshData();
    verts =
    {
        0.f, 2.f, 0.f,    LeaderboardTextLight.getRed(), LeaderboardTextLight.getGreen(), LeaderboardTextLight.getBlue(), 1.f,
        0.f, 0.f, 0.f,    LeaderboardTextLight.getRed(), LeaderboardTextLight.getGreen(), LeaderboardTextLight.getBlue(), 1.f,
        0.f,  0.001f,  0.f,    0.f, 0.f, 0.f, 0.5f,
        1.4f, 0.001f, 1.4f,    0.f, 0.f, 0.f, 0.01f,
    };
    indices =
    {
        0,1,2,3
    };
    meshData->vertexCount = verts.size() / vertStride;
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    submesh = &meshData->indexData[0];
    submesh->indexCount = static_cast<std::uint32_t>(indices.size());
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));



    //attach a water plane to the camera
    //this is updated when positioning the camera
    meshID = m_resources.meshes.loadMesh(cro::CircleMeshBuilder(150.f, 30));
    auto waterEnt = m_gameScene.createEntity();
    waterEnt.addComponent<cro::Transform>().setPosition(m_holeData[0].pin);
    waterEnt.getComponent<cro::Transform>().move({ 0.f, 0.f, -30.f });
    waterEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -cro::Util::Const::PI / 2.f);
    waterEnt.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), m_resources.materials.get(m_materialIDs[MaterialID::Water]));
    m_gameScene.setWaterLevel(WaterLevel);


    //tee marker
    md.loadFromFile("assets/golf/models/tee_balls.cmt");
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(m_holeData[0].tee);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Tee;
    md.createModel(entity);
    entity.getComponent<cro::Model>().setMaterial(0, m_resources.materials.get(m_materialIDs[MaterialID::Cel]));

    auto pinDir = m_holeData[m_currentHole].pin - m_holeData[0].tee;
    m_camRotation = std::atan2(-pinDir.z, pinDir.x);
    entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, m_camRotation);
    //TODO add callback for hole transition
    


    //update the 3D view
    auto updateView = [&](cro::Camera& cam)
    {
        auto vpSize = calcVPSize();

        cam.setPerspective(FOV, vpSize.x / vpSize.y, 0.1f, vpSize.x);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        if (static_cast<std::uint32_t>(vpSize.x) != m_renderTexture.getSize().x)
        {
            m_renderTexture.create(static_cast<std::uint32_t>(vpSize.x), static_cast<std::uint32_t>(vpSize.y));
        }
    };

    auto camEnt = m_gameScene.getActiveCamera();
    auto& cam = camEnt.getComponent<cro::Camera>();
    cam.resizeCallback = updateView;
    updateView(cam);

    //used by transition callback to interp camera
    camEnt.addComponent<TargetInfo>().waterPlane = waterEnt;
    cam.reflectionBuffer.create(1024, 1024);

    m_terrainBuilder.create(m_resources, m_gameScene);

    m_currentPlayer.position = m_holeData[m_currentHole].tee; //prevents the initial camera movement

    setCurrentHole(0);
    buildUI(); //put this here because we don't want to do this if the map data didn't load


    auto sunEnt = m_gameScene.getSunlight();
    sunEnt.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, -0.967f);
    sunEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -1.5f);


#ifdef CRO_DEBUG_
    setupDebug();
#endif
}

void GolfState::spawnBall(const ActorInfo& info)
{
    //render the ball as a point so no perspective is applied to the scale
    auto material = m_resources.materials.get(m_ballResources.materialID);

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(info.position);
    entity.getComponent<cro::Transform>().setOrigin({ 0.f, -0.003f, 0.f }); //pushes the ent above the ground a bit to stop Z fighting
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Ball;
    entity.addComponent<InterpolationComponent>().setID(info.serverID);
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_ballResources.ballMeshID), material);

    //ball shadow
    auto ballEnt = entity;
    material.blendMode = cro::Material::BlendMode::Multiply;

    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(info.position);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [ballEnt](cro::Entity e, float)
    {
        auto ballPos = ballEnt.getComponent<cro::Transform>().getPosition();
        ballPos.y = 0.003f; //just to prevent z-fighting
        e.getComponent<cro::Transform>().setPosition(ballPos);
    };
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_ballResources.shadowMeshID), material);

    auto shadowEnt = entity;
    entity = m_gameScene.createEntity();
    shadowEnt.getComponent<cro::Transform>().addChild(entity.addComponent<cro::Transform>());
    m_modelDefs[ModelID::BallShadow]->createModel(entity);
    entity.getComponent<cro::Transform>().setScale(glm::vec3(1.3f));

    //adding a ball model means we see something a bit more reasonable when close up
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    m_modelDefs[ModelID::Ball]->createModel(entity);
    entity.getComponent<cro::Model>().setMaterial(0, m_resources.materials.get(m_materialIDs[MaterialID::Cel]));
    ballEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void GolfState::handleNetEvent(const cro::NetEvent& evt)
{
    switch (evt.type)
    {
    case cro::NetEvent::PacketReceived:
        switch (evt.packet.getID())
        {
        default: break;
        case PacketID::ClientDisconnected:
        {
            removeClient(evt.packet.as<std::uint8_t>());
        }
            break;
        case PacketID::ServerError:
            switch (evt.packet.as<std::uint8_t>())
            {
            default:
                m_sharedData.errorMessage = "Server Error (Unknown)";
                break;
            case MessageType::MapNotFound:
                m_sharedData.errorMessage = "Server Failed To Load Map";
                break;
            }
            requestStackPush(States::Golf::Error);
            break;
        case PacketID::SetPlayer:
            m_wantsGameState = false;
            {
                auto playerData = evt.packet.as<ActivePlayer>();
                createTransition(playerData);
            }
            break;
        case PacketID::ActorSpawn:
            spawnBall(evt.packet.as<ActorInfo>());
            break;
        case PacketID::ActorUpdate:
        {
            auto update = evt.packet.as<ActorInfo>();

            cro::Command cmd;
            cmd.targetFlags = CommandID::Ball;
            cmd.action = [update](cro::Entity e, float)
            {
                if (e.isValid())
                {
                    auto& interp = e.getComponent<InterpolationComponent>();
                    if (interp.getID() == update.serverID)
                    {
                        interp.setTarget({ update.position, {1.f,0.f,0.f,0.f}, update.timestamp });
                    }
                }
                ballpos = update.position;
            };
            m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
        }
            break;
        case PacketID::ActorAnimation:
        {
            auto animID = evt.packet.as<std::uint8_t>();
            cro::Command cmd;
            cmd.targetFlags = CommandID::UI::PlayerSprite;
            cmd.action = [animID](cro::Entity e, float)
            {
                e.getComponent<cro::SpriteAnimation>().play(animID);
            };
            m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
        }
            break;
        case PacketID::WindDirection:
            updateWindDisplay(cro::Util::Net::decompressVec3(evt.packet.as<std::array<std::int16_t, 3u>>()));
            break;
        case PacketID::SetHole:
            setCurrentHole(evt.packet.as<std::uint8_t>());
            break;
        case PacketID::ScoreUpdate:
        {
            auto su = evt.packet.as<ScoreUpdate>();
            auto& player = m_sharedData.connectionData[su.client].playerData[su.player];
            
            if (su.hole < player.holeScores.size())
            {
                player.score = su.score;
                player.holeScores[su.hole] = su.stroke;
            }
        }
            break;
        case PacketID::GameEnd:
            showCountdown(evt.packet.as<std::uint8_t>());
            break;
        case PacketID::StateChange:
            if (evt.packet.as<std::uint8_t>() == sv::StateID::Lobby)
            {
                requestStackClear();
                requestStackPush(States::Golf::Menu);
            }
            break;
        }
        break;
    case cro::NetEvent::ClientDisconnect:
        m_sharedData.errorMessage = "Disconnected From Server (Host Quit)";
        requestStackPush(States::Golf::Error);
        break;
    default: break;
    }
}

void GolfState::removeClient(std::uint8_t clientID)
{
    for (auto i = 0u; i < m_sharedData.connectionData[clientID].playerCount; ++i)
    {
        LogI << m_sharedData.connectionData[clientID].playerData[i].name.toAnsiString() << " left the game" << std::endl;
    }
    m_sharedData.connectionData[clientID].playerCount = 0;
}

void GolfState::setCurrentHole(std::uint32_t hole)
{
    updateScoreboard();
    showScoreboard(true);

    //CRO_ASSERT(hole < m_holeData.size(), "");
    if (hole >= m_holeData.size())
    {
        m_sharedData.errorMessage = "Server requested hole\nnot found";
        requestStackPush(States::Golf::Error);
        return;
    }

    m_terrainBuilder.update(hole);

    m_holeData[m_currentHole].modelEntity.getComponent<cro::Model>().setHidden(true);
    m_currentHole = hole;

    m_holeData[m_currentHole].modelEntity.getComponent<cro::Model>().setHidden(false);
    m_currentMap.loadFromFile(m_holeData[m_currentHole].mapPath);
    glm::vec2 size(m_currentMap.getSize());
    m_holeData[m_currentHole].modelEntity.getComponent<cro::Transform>().setOrigin({ -size.x / 2.f, 0.f, size.y / 2.f });


    //creates an entity which calls setCamPosition() in an
    //interpolated manner until we reach the dest,
    //at which point the ent destroys itself
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(m_currentPlayer.position);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto currPos = e.getComponent<cro::Transform>().getPosition();
        auto travel = m_holeData[m_currentHole].tee - currPos;

        //TODO we may also have to interp this from the prev pin to the new one
        auto pinDir = m_holeData[m_currentHole].pin - currPos;
        m_camRotation = std::atan2(-pinDir.z, pinDir.x);

        auto targetInfo = m_gameScene.getActiveCamera().getComponent<TargetInfo>();

        if (glm::length2(travel) < 0.005f)
        {
            //we're there
            setCameraPosition(m_holeData[m_currentHole].tee, targetInfo.targetHeight, targetInfo.targetOffset);

            e.getComponent<cro::Callback>().active = false;
            m_gameScene.destroyEntity(e);
        }
        else
        {
            auto height = targetInfo.targetHeight - targetInfo.currentHeight;
            auto offset = targetInfo.targetOffset - targetInfo.currentOffset;

            static constexpr float Speed = 4.f;
            e.getComponent<cro::Transform>().move(travel * Speed * dt);
            setCameraPosition(e.getComponent<cro::Transform>().getPosition(),
                targetInfo.currentHeight + (height * Speed * dt),
                targetInfo.currentOffset + (offset * Speed * dt));
        }
    };

    //make sure we have the correct target position
    m_gameScene.getActiveCamera().getComponent<TargetInfo>().targetHeight = CameraStrokeHeight;
    m_gameScene.getActiveCamera().getComponent<TargetInfo>().targetOffset = CameraStrokeOffset;


    m_currentPlayer.position = m_holeData[m_currentHole].tee;

    cro::Command cmd;
    cmd.targetFlags = CommandID::Hole;
    cmd.action = [&](cro::Entity e, float)
    {
        //TODO trigger a transition callback instead
        e.getComponent<cro::Transform>().setPosition(m_holeData[m_currentHole].pin);
    };
    m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

    cmd.targetFlags = CommandID::Tee;
    cmd.action = [&](cro::Entity e, float)
    {
        //TODO trigger a transition callback instead
        e.getComponent<cro::Transform>().setPosition(m_holeData[m_currentHole].tee);
    };
    m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

    //TODO some sort of 'loading' effect of the terrain - maybe a shader on the buffer sprite?


}

void GolfState::setCameraPosition(glm::vec3 position, float height, float viewOffset)
{
    static constexpr float MinDist = 6.f;
    static constexpr float MaxDist = 270.f;
    static constexpr float DistDiff = MaxDist - MinDist;
    float heightMultiplier = 1.f; //goes to -1.f at max dist

    auto target = m_holeData[m_currentHole].pin - position;

    auto dist = glm::length(target);
    float distNorm = std::min(1.f, (dist - MinDist) / DistDiff);
    heightMultiplier -= (2.f * distNorm);

    target *= 1.f - ((1.f - 0.08f) * distNorm);
    target += position;


    auto camEnt = m_gameScene.getActiveCamera();
    camEnt.getComponent<cro::Transform>().setPosition({ position.x, height, position.z });

    auto lookat = glm::lookAt(camEnt.getComponent<cro::Transform>().getPosition(), glm::vec3(target.x, height * heightMultiplier, target.z), cro::Transform::Y_AXIS);
    camEnt.getComponent<cro::Transform>().setRotation(glm::inverse(lookat));

    auto offset = -camEnt.getComponent<cro::Transform>().getForwardVector();
    camEnt.getComponent<cro::Transform>().move(offset * viewOffset);

    camEnt.getComponent<TargetInfo>().currentHeight = height;
    camEnt.getComponent<TargetInfo>().currentOffset = viewOffset;
    camEnt.getComponent<TargetInfo>().waterPlane.getComponent<cro::Transform>().setPosition({ target.x, WaterLevel, target.z });
}

void GolfState::setCurrentPlayer(const ActivePlayer& player)
{
    updateScoreboard();
    showScoreboard(false);

    m_currentPlayer = player;

    //TODO we could move much of the UI stuff to its
    //own class or function to encapsulate betterererer
    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::PlayerName;
    cmd.action =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::Text>().setString(m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].name);
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);


    cmd.targetFlags = CommandID::UI::PinDistance;
    cmd.action =
        [&](cro::Entity e, float)
    {
        //if we're on the green convert to cm
        float ballDist = glm::length(player.position - m_holeData[m_currentHole].pin);
        std::int32_t distance = 0;

        if (ballDist > 5)
        {
            distance = static_cast<std::int32_t>(ballDist);
            e.getComponent<cro::Text>().setString("Distance: " + std::to_string(distance) + "m");
        }
        else
        {
            distance = static_cast<std::int32_t>(ballDist * 100.f);
            e.getComponent<cro::Text>().setString("Distance: " + std::to_string(distance) + "cm");
        }
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);


    cmd.targetFlags = CommandID::UI::HoleNumber;
    cmd.action =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::Text>().setString("Hole: " + std::to_string(m_currentHole + 1));
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);


    //show ui if this is our client
    auto localPlayer = (player.client == m_sharedData.clientConnection.connectionID);
    
    cmd.targetFlags = CommandID::UI::Root;
    cmd.action = [&,localPlayer](cro::Entity e, float)
    {
        float sizeX = static_cast<float>(cro::App::getWindow().getSize().x);
        sizeX /= m_viewScale.x;

        auto uiPos = localPlayer ? glm::vec2(sizeX / 2.f, UIBarHeight / 2.f) : UIHiddenPosition;
        e.getComponent<cro::Transform>().setPosition(uiPos);
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);


    //stroke indicator is in model scene...
    cmd.targetFlags = CommandID::StrokeIndicator;
    cmd.action = [localPlayer, player](cro::Entity e, float)
    {
        auto position = player.position;
        position.y = 0.01f;
        e.getComponent<cro::Transform>().setPosition(position);
        e.getComponent<cro::Model>().setHidden(!localPlayer);
    };
    m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

    //if client is ours activate input/set initial stroke direction
    m_inputParser.setActive(localPlayer);
    m_inputParser.setHoleDirection(m_holeData[m_currentHole].pin - player.position);

    //TODO apply the correct sprite to the player entity
    cmd.targetFlags = CommandID::UI::PlayerSprite;
    cmd.action = [&,player](cro::Entity e, float)
    {
        if (player.terrain != TerrainID::Green)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);

            const auto& camera = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
            auto pos = camera.coordsToPixel(player.position, m_renderTexture.getSize());
            e.getComponent<cro::Transform>().setPosition(pos);
        }
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
}

void GolfState::hitBall()
{
    auto pitch = Clubs[getClub()].angle;// cro::Util::Const::PI / 4.f;

    auto yaw = m_inputParser.getYaw();

    //add hook/slice to yaw
    yaw += MaxHook * m_inputParser.getHook();

    glm::vec3 impulse(1.f, 0.f, 0.f);
    auto rotation = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), yaw, cro::Transform::Y_AXIS);
    rotation = glm::rotate(rotation, pitch, cro::Transform::Z_AXIS);
    impulse = glm::toMat3(rotation) * impulse;

    impulse *= Clubs[getClub()].power * m_inputParser.getPower();

    InputUpdate update;
    update.clientID = m_sharedData.localPlayer.connectionID;
    update.playerID = m_currentPlayer.player;
    update.impulse = impulse;

    m_sharedData.clientConnection.netClient.sendPacket(PacketID::InputUpdate, update, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);

    m_inputParser.setActive(false);


    //increase the local stroke count so that the UI is updated
    //the server will set the actual value
    m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeScores[m_currentHole]++;

    //hide the indicator
    cro::Command cmd;
    cmd.targetFlags = CommandID::StrokeIndicator;
    cmd.action = [](cro::Entity e, float)
    {
        e.getComponent<cro::Model>().setHidden(true);
    };
    m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
}

void GolfState::createTransition(const ActivePlayer& playerData)
{
    //hide player sprite
    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::PlayerSprite;
    cmd.action = [](cro::Entity e, float)
    {
        e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

    //hide hud
    cmd.targetFlags = CommandID::UI::Root;
    cmd.action = [](cro::Entity e, float) 
    {
        e.getComponent<cro::Transform>().setPosition(UIHiddenPosition);
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

    if (playerData.terrain == TerrainID::Green)
    {
        m_gameScene.getActiveCamera().getComponent<TargetInfo>().targetHeight = CameraPuttHeight;
        m_gameScene.getActiveCamera().getComponent<TargetInfo>().targetOffset = CameraPuttOffset;
    }
    else
    {
        m_gameScene.getActiveCamera().getComponent<TargetInfo>().targetHeight = CameraStrokeHeight;
        m_gameScene.getActiveCamera().getComponent<TargetInfo>().targetOffset = CameraStrokeOffset;
    }

    //creates an entity which calls setCamPosition() in an
    //interpolated manner until we reach the dest,
    //at which point we update the active player and
    //the ent destroys itself
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(m_currentPlayer.position);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<ActivePlayer>(playerData);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        const auto& playerData = e.getComponent<cro::Callback>().getUserData<ActivePlayer>();

        auto currPos = e.getComponent<cro::Transform>().getPosition();
        auto travel = playerData.position - currPos;

        auto pinDir = m_holeData[m_currentHole].pin - currPos;
        m_camRotation = std::atan2(-pinDir.z, pinDir.x);

        auto targetInfo = m_gameScene.getActiveCamera().getComponent<TargetInfo>();

        if (glm::length2(travel) < 0.005f)
        {
            //we're there
            setCameraPosition(playerData.position, targetInfo.targetHeight, targetInfo.targetOffset);
            setCurrentPlayer(playerData);

            e.getComponent<cro::Callback>().active = false;
            m_gameScene.destroyEntity(e);
        }
        else
        {
            auto height = targetInfo.targetHeight - targetInfo.currentHeight;
            auto offset = targetInfo.targetOffset - targetInfo.currentOffset;

            static constexpr float Speed = 4.f;
            e.getComponent<cro::Transform>().move(travel * Speed * dt);
            setCameraPosition(e.getComponent<cro::Transform>().getPosition(), 
                targetInfo.currentHeight + (height * Speed * dt), 
                targetInfo.currentOffset + (offset * Speed * dt));
        }
    };
}

void GolfState::updateWindDisplay(glm::vec3 direction)
{
    float rotation = std::atan2(-direction.z, direction.x);

    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::WindSock;
    cmd.action = [&, rotation](cro::Entity e, float dt)
    {
        auto r = rotation - m_camRotation;
        //e.getComponent<cro::Transform>().setRotation(r);

        //this is smoother but uses a horrible static var
        static float currRotation = 0.f;
        currRotation += (r - currRotation) * dt;
        e.getComponent<cro::Transform>().setRotation(currRotation);
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

    cmd.targetFlags = CommandID::UI::WindString;
    cmd.action = [direction](cro::Entity e, float dt)
    {
        float knots = direction.y * KnotsPerMetre;
        std::stringstream ss;
        ss.precision(2);
        ss << std::fixed << knots << " knots";
        e.getComponent<cro::Text>().setString(ss.str());
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

    cmd.targetFlags = CommandID::Flag;
    cmd.action = [rotation](cro::Entity e, float)
    {
        e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation);
    };
    m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
}

std::int32_t GolfState::getClub() const
{
    switch (m_currentPlayer.terrain)
    {
    default: return m_inputParser.getClub();
    case TerrainID::Bunker: return ClubID::PitchWedge;
    case TerrainID::Green: return ClubID::Putter;
    }
}

#ifdef CRO_DEBUG_

void GolfState::setupDebug()
{
    debugPos = m_holeData[0].tee;

    m_debugCam = m_gameScene.createEntity();
    m_debugCam.addComponent<cro::Transform>().setPosition({ 0.f, 10.f, 0.f });
    m_debugCam.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
    auto& dCam = m_debugCam.addComponent<cro::Camera>();
    dCam.setOrthographic(0.f, 320.f, 0.f, 200.f, -0.1f, 20.f);
    dCam.viewport = { 0.f, 0.f, 1.f, 1.f };

    return;


    //shadow map frustum
    auto shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::VertexColour);
    auto materialID = m_resources.materials.add(m_resources.shaders.get(shaderID));
    auto meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_LINES));

    m_resources.materials.get(materialID).blendMode = cro::Material::BlendMode::Alpha;
    //m_resources.materials.get(materialID).enableDepthTest = false;

    auto ent = m_gameScene.getActiveCamera();

    auto debugEnt = m_gameScene.createEntity();
    debugEnt.addComponent<cro::Transform>();
    debugEnt.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), m_resources.materials.get(materialID));
    debugEnt.addComponent<cro::Callback>().active = true;
    debugEnt.getComponent<cro::Callback>().function =
        [&, ent](cro::Entity e, float)
    {
        const auto& cam = ent.getComponent<cro::Camera>();
        //e.getComponent<cro::Transform>().setPosition(ent.getComponent<cro::Transform>().getWorldPosition());
        e.getComponent<cro::Transform>().setPosition(cam.depthPosition);
        e.getComponent<cro::Transform>().setRotation(m_gameScene.getSunlight().getComponent<cro::Transform>().getRotation());

        std::vector<float> verts =
        {
            cam.depthDebug[0], cam.depthDebug[2], cam.depthDebug[4],
            1.f,0.f,1.f,1.f,

            cam.depthDebug[1], cam.depthDebug[2], cam.depthDebug[4],
            1.f,0.f,1.f,1.f,

            cam.depthDebug[0], cam.depthDebug[3], cam.depthDebug[4],
            1.f,0.f,1.f,1.f,

            cam.depthDebug[1], cam.depthDebug[3], cam.depthDebug[4],
            1.f,0.f,1.f,1.f,

            cam.depthDebug[0], cam.depthDebug[2], -cam.depthDebug[5],
            0.f,1.f,1.f,1.f,

            cam.depthDebug[1], cam.depthDebug[2], -cam.depthDebug[5],
            0.f,1.f,1.f,1.f,

            cam.depthDebug[0], cam.depthDebug[3], -cam.depthDebug[5],
            0.f,1.f,1.f,1.f,

            cam.depthDebug[1], cam.depthDebug[3], -cam.depthDebug[5],
            0.f,1.f,1.f,1.f,
        };

        std::vector<std::uint32_t> indices =
        {
            0,1, 1,3, 3,2, 2,0,
            4,5, 5,7, 7,6, 6,4,
            0,4, 1,5, 3,7, 2,6
        };

        auto& meshData = e.getComponent<cro::Model>().getMeshData();
        glBindBuffer(GL_ARRAY_BUFFER, meshData.vbo);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[0].ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(std::uint32_t), indices.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        meshData.boundingBox[0] = { cam.depthDebug[0], cam.depthDebug[2], cam.depthDebug[4] };
        meshData.boundingBox[1] = { cam.depthDebug[1], cam.depthDebug[3], -cam.depthDebug[5] };
        meshData.boundingSphere.centre = meshData.boundingBox[0] + ((meshData.boundingBox[1] - meshData.boundingBox[0]) / 2.f);
        meshData.boundingSphere.radius = glm::length(meshData.boundingSphere.centre);
    };

    auto& meshData = debugEnt.getComponent<cro::Model>().getMeshData();
    meshData.vertexCount = 8;
    meshData.vertexSize = 7 * meshData.vertexCount * sizeof(float);
    meshData.indexData[0].indexCount = 24;
}

#endif