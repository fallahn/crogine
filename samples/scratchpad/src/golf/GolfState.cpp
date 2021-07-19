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
#include "CommandIDs.hpp"
#include "PacketIDs.hpp"
#include "SharedStateData.hpp"
#include "InterpolationSystem.hpp"
#include "ClientPacketData.hpp"
#include "MessageIDs.hpp"

#include <crogine/core/ConfigFile.hpp>

#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/graphics/DynamicMeshBuilder.hpp>

#include <crogine/network/NetClient.hpp>

#include <crogine/gui/Gui.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/util/Matrix.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include "../ErrorCheck.hpp"

namespace
{
    glm::vec3 debugPos = glm::vec3(0.f);
    std::int32_t debugMaterial = -1;
    glm::vec3 ballpos = glm::vec3(0.f);

    glm::vec2 calcVPSize()
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        const float ratio = size.x / size.y;

        const float width = std::min(400.f, std::max(300.f, ViewportHeight * ratio));
        return { width, ViewportHeight };
    }

    const cro::Time ReadyPingFreq = cro::seconds(1.f);
}

GolfState::GolfState(cro::StateStack& stack, cro::State::Context context, SharedStateData& sd)
    : cro::State    (stack, context),
    m_sharedData    (sd),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus()),
    m_inputParser   (sd.inputBinding, context.appInstance.getMessageBus()),
    m_wantsGameState(true),
    m_currentHole   (0)
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
                ImGui::Text("ball pos: %3.3f, %3.3f, %3.3f", ballpos.x, ballpos.y, ballpos.z);

                auto rot = m_inputParser.getYaw();
                ImGui::Text("Rotation %3.2f", rot);

                auto power = m_inputParser.getPower();
                ImGui::Text("Power: %3.3f", power);

                auto hook = m_inputParser.getHook();
                ImGui::Text("Hook: %3.3f", hook);
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
        case SDLK_F2:
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint8_t(ServerCommand::NextHole), cro::NetFlag::Reliable);
            break;
        case SDLK_F3:
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint8_t(ServerCommand::NextPlayer), cro::NetFlag::Reliable);
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
    case MessageID::GameMessage:
    {
        const auto& data = msg.getData<GameEvent>();
        if (data.type == GameEvent::HitBall)
        {
            hitBall();
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

    m_inputParser.update(dt);

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);

    return true;
}

void GolfState::render()
{
    m_renderTexture.clear();
    m_gameScene.render(m_renderTexture);
    m_renderTexture.display();

#ifdef CRO_DEBUG_
    //auto oldCam = m_gameScene.setActiveCamera(m_debugCam);
    //m_debugTexture.clear(cro::Colour::Magenta);
    //m_gameScene.render(m_debugTexture);
    //m_debugTexture.display();
    //m_gameScene.setActiveCamera(oldCam);
#endif

    m_uiScene.render(cro::App::getWindow());
}

//private
void GolfState::loadAssets()
{
    //render buffer
    auto vpSize = calcVPSize();
    m_renderTexture.create(static_cast<std::uint32_t>(vpSize.x) , static_cast<std::uint32_t>(vpSize.y));

    //UI stuffs
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/ui.spt", m_resources.textures);
    m_sprites[SpriteID::Flag01] = spriteSheet.getSprite("flag01");
    m_sprites[SpriteID::Flag02] = spriteSheet.getSprite("flag02");
    m_sprites[SpriteID::Flag03] = spriteSheet.getSprite("flag03");
    m_sprites[SpriteID::Flag04] = spriteSheet.getSprite("flag04");
    m_sprites[SpriteID::PowerBar] = spriteSheet.getSprite("power_bar");
    m_sprites[SpriteID::WindIndicator] = spriteSheet.getSprite("wind_dir");

    spriteSheet.loadFromFile("assets/golf/sprites/player.spt", m_resources.textures);
    m_sprites[SpriteID::Player01] = spriteSheet.getSprite("female");
    m_sprites[SpriteID::Player02] = spriteSheet.getSprite("male");


    m_resources.fonts.load(FontID::UI, "assets/fonts/VeraMono.ttf");

    //ball resources - ball is rendered as a single point
    glCheck(glPointSize(BallPointSize));

    auto shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::VertexColour);
    m_ballResources.materialID = m_resources.materials.add(m_resources.shaders.get(shaderID));
    m_ballResources.ballMeshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_POINTS));
    m_ballResources.shadowMeshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_POINTS));

    auto* meshData = &m_resources.meshes.getMesh(m_ballResources.ballMeshID);
    std::vector<float> verts =
    {
        0.f, 0.f, 0.f,    1.f, 1.f, 1.f, 1.f,
    };
    std::vector<std::uint32_t> indices =
    {
        0
    };
    auto vertStride = (meshData->vertexSize / sizeof(float));
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
                if (!holeData.map.loadFromFile(holeProp.getValue<std::string>()))
                {
                    error = true;
                }
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

#ifdef CRO_DEBUG_
    m_debugTexture.create(800, 100);

    //debug material for wireframes
    shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::VertexColour);
    debugMaterial = m_resources.materials.add(m_resources.shaders.get(shaderID));
    m_resources.materials.get(debugMaterial).blendMode = cro::Material::BlendMode::Alpha;

#endif
}

void GolfState::addSystems()
{
    auto& mb = m_gameScene.getMessageBus();

    m_gameScene.addSystem<InterpolationSystem>(mb);
    m_gameScene.addSystem<cro::CommandSystem>(mb);
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void GolfState::buildScene()
{
    if (m_holeData.empty())
    {
        return;
    }

    cro::ModelDefinition md(m_resources);
    md.loadFromFile("assets/golf/models/water_plane.cmt");

    cro::Entity entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 150.f, -0.1f, -100.f });
    md.createModel(entity);




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
    auto material = m_resources.materials.get(debugMaterial);
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    auto& meshData = entity.getComponent<cro::Model>().getMeshData();

    std::vector<float> verts =
    {
        0.f, 0.f, 0.f,    1.f, 1.f, 1.f, 1.f,
        5.f, 0.f, 0.f,    1.f, 1.f, 1.f, 0.2f
    };
    std::vector<std::uint32_t> indices =
    {
        0,1
    };

    auto vertStride = (meshData.vertexSize / sizeof(float));
    meshData.vertexCount = verts.size() / vertStride;
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData.vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData.vertexSize * meshData.vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    auto& sub = meshData.indexData[0];
    sub.indexCount = static_cast<std::uint32_t>(indices.size());
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sub.ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sub.indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    entity.getComponent<cro::Model>().setHidden(true);





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

    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = updateView;
    updateView(cam);

    setCurrentHole(0);
    buildUI(); //put this here because we don't want to do this if the map data didn't load


#ifdef CRO_DEBUG_
    debugPos = m_holeData[0].tee;

    m_debugCam = m_gameScene.createEntity();
    m_debugCam.addComponent<cro::Transform>().setPosition({ 20.f, -10.f, 100.f });
    auto& dCam = m_debugCam.addComponent<cro::Camera>();
    dCam.setOrthographic(0.f, 250.f, 0.f, 31.25f, -0.1f, 200.f);
#endif
}

void GolfState::buildUI()
{
    if (m_holeData.empty())
    {
        return;
    }

    //draws the background using the render texture
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_renderTexture.getTexture());
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    auto courseEnt = entity;


    auto& camera = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    camera.updateMatrices(m_gameScene.getActiveCamera().getComponent<cro::Transform>());
    auto pos = camera.coordsToPixel(m_holeData[0].tee, m_renderTexture.getSize());
    
    //player sprite - TODO apply avatar customisation
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(pos);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::Player01];
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width * 0.78f, 0.f));
    courseEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //flag sprite. TODO make this less crap
    //this is updated by a command sent when the 3D camera is positioned
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::Flag01];
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::FlagSprite;
    courseEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //root used to show/hide input UI
    auto rootNode = m_uiScene.createEntity();
    rootNode.addComponent<cro::Transform>().setPosition(UIHiddenPosition);
    rootNode.addComponent<cro::CommandTarget>().ID = CommandID::UI::Root;

    auto windowSize = glm::vec2(cro::App::getWindow().getSize());
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(PowerbarPosition * windowSize);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::PowerBar];
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().position = PowerbarPosition;
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    entity.getComponent<cro::Transform>().rotate(cro::Util::Const::PI / 2.f);
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(WindPosition * windowSize);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::WindIndicator];
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().position = WindPosition;
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    //TODO wind callback to rotate with wind direction



    auto& font = m_resources.fonts.get(FontID::UI);

    //player's name
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(PlayerNamePosition * windowSize);
    entity.addComponent<UIElement>().position = PlayerNamePosition;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::PlayerName | CommandID::UI::UIElement;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(8);



    auto updateView = [&, courseEnt, rootNode](cro::Camera& cam) mutable
    {
        auto size = glm::vec2(cro::App::getWindow().getSize());
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -0.1f, 1.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        auto vpSize = calcVPSize();

        float viewScale = std::floor(size.x / vpSize.x);
        courseEnt.getComponent<cro::Transform>().setScale(glm::vec2(viewScale));
        courseEnt.getComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, -0.1f));
        courseEnt.getComponent<cro::Transform>().setOrigin(vpSize / 2.f);
        courseEnt.getComponent<cro::Sprite>().setTextureRect({ 0.f, 0.f, vpSize.x, vpSize.y });

        //send command to UIElements and reposition (and potentially rescale)
        cro::Command cmd;
        cmd.targetFlags = CommandID::UI::UIElement;
        cmd.action = [size, viewScale](cro::Entity e, float)
        {
            auto pos = size * e.getComponent<UIElement>().position;
            e.getComponent<cro::Transform>().setPosition(pos);
            e.getComponent<cro::Transform>().setScale(glm::vec2(viewScale));
        };
        m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
    };

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = updateView;
    updateView(cam);

#ifdef CRO_DEBUG_
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 500.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_debugTexture.getTexture());
#endif
}

void GolfState::spawnBall(const ActorInfo& info)
{
    //render the ball as a point so no perspective is applied to the scale
    auto material = m_resources.materials.get(m_ballResources.materialID);

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(info.position);
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
        ballPos.y = 0.f;
        e.getComponent<cro::Transform>().setPosition(ballPos);
    };
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_ballResources.shadowMeshID), material);
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
                setCameraPosition(playerData.position);
                setCurrentPlayer(playerData);
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
    CRO_ASSERT(hole < m_holeData.size(), "");

    m_holeData[m_currentHole].modelEntity.getComponent<cro::Model>().setHidden(true);
    m_currentHole = hole;

    m_holeData[m_currentHole].modelEntity.getComponent<cro::Model>().setHidden(false);
    glm::vec2 size(m_holeData[m_currentHole].map.getSize());
    m_holeData[m_currentHole].modelEntity.getComponent<cro::Transform>().setOrigin({ -size.x / 2.f, 0.f, size.y / 2.f });

    setCameraPosition(m_holeData[m_currentHole].tee); //TODO is this not overriden by setting the active player?
}

void GolfState::setCameraPosition(glm::vec3 position)
{
    auto camEnt = m_gameScene.getActiveCamera();
    camEnt.getComponent<cro::Transform>().setPosition({ position.x, CameraHeight, position.z });
    auto target = m_holeData[m_currentHole].pin;
    auto lookat = glm::lookAt(camEnt.getComponent<cro::Transform>().getPosition(), glm::vec3(target.x, CameraHeight, target.z), cro::Transform::Y_AXIS);
    camEnt.getComponent<cro::Transform>().setRotation(glm::inverse(lookat));

    auto offset = -camEnt.getComponent<cro::Transform>().getForwardVector();
    camEnt.getComponent<cro::Transform>().move(offset * CameraOffset);

    //calc which one of the flag sprites to use based on distance
    const auto maxLength = glm::length(m_holeData[m_currentHole].pin - m_holeData[m_currentHole].tee) * 0.75f;
    const auto currLength = glm::length(m_holeData[m_currentHole].pin - position);

    const std::int32_t size = static_cast<std::int32_t>(maxLength / currLength);
    std::int32_t flag = SpriteID::Flag01; //largest
    switch (size)
    {
    case 0:
        flag = SpriteID::Flag04;
        break;
    case 1:
        flag = SpriteID::Flag03;
        break;
    case 2:
        flag = SpriteID::Flag02;
        break;
    case 3:
    default: break;
        flag = SpriteID::Flag01;
        break;
    }

    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::FlagSprite;
    cmd.action = [&, flag](cro::Entity e, float)
    {
        e.getComponent<cro::Sprite>() = m_sprites[flag];
        //TODO scale the sprite based on the distance to its cutoff?
        //rememberthe view will actually be static so we won't see the size animating
        auto pos = m_gameScene.getActiveCamera().getComponent<cro::Camera>().coordsToPixel(m_holeData[m_currentHole].pin, m_renderTexture.getSize());
        e.getComponent<cro::Transform>().setPosition(pos);
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
}

void GolfState::setCurrentPlayer(const ActivePlayer& player)
{
    //LogI << "player set to " << (int)player.client << ", " << (int)player.player << ", at: " << player.position << std::endl;
    
    m_currentPlayer = player;

    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::PlayerName;
    cmd.action =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::Text>().setString(m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].name);
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);


    //show ui if this is our client
    auto localPlayer = (player.client == m_sharedData.clientConnection.connectionID);
    
    cmd.targetFlags = CommandID::UI::Root;
    cmd.action = [localPlayer](cro::Entity e, float)
    {
        auto uiPos = localPlayer ? glm::vec2(0.f) : UIHiddenPosition;
        e.getComponent<cro::Transform>().setPosition(uiPos);
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);


    //stroke indicator is in model scene...
    cmd.targetFlags = CommandID::StrokeIndicator;
    cmd.action = [localPlayer](cro::Entity e, float)
    {
        e.getComponent<cro::Model>().setHidden(!localPlayer);
    };
    m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

    //if client is ours activate input/set initial stroke direction
    m_inputParser.setActive(localPlayer);
    m_inputParser.setHoleDirection(m_holeData[m_currentHole].pin - player.position);
}

void GolfState::hitBall()
{
    //TODO adjust pitch on player input?
    //should be 0 in putting mode, so perhaps adjust for club
    auto pitch = cro::Util::Const::PI / 4.f;

    auto yaw = m_inputParser.getYaw();

    //add hook/slice to yaw
    yaw += MaxHook * m_inputParser.getHook();

    glm::vec3 impulse(1.f, 0.f, 0.f);
    auto rotation = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), yaw, cro::Transform::Y_AXIS);
    rotation = glm::rotate(rotation, pitch, cro::Transform::Z_AXIS);
    impulse = glm::toMat3(rotation) * impulse;

    impulse *= 20.f * m_inputParser.getPower(); //TODO get this magnitude from input/current club

    InputUpdate update;
    update.clientID = m_sharedData.localPlayer.connectionID;
    update.playerID = m_currentPlayer.player;
    update.impulse = impulse;

    m_sharedData.clientConnection.netClient.sendPacket(PacketID::InputUpdate, update, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);

    m_inputParser.setActive(false);
}