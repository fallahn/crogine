/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2025
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#include "BatcatState.hpp"
#include "ResourceIDs.hpp"
#include "Messages.hpp"
#include "PlayerDirector.hpp"
#include "TerrainChunk.hpp"
#include "TerrainSystem.hpp"

#include <crogine/detail/GlobalConsts.hpp>

#include <crogine/core/App.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/core/Keyboard.hpp>
#include <crogine/core/Mouse.hpp>

#include <crogine/graphics/QuadBuilder.hpp>
#include <crogine/graphics/StaticMeshBuilder.hpp>
#include <crogine/graphics/DynamicMeshBuilder.hpp>
#include <crogine/graphics/IqmBuilder.hpp>
#include <crogine/graphics/SpriteSheet.hpp>

#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/ParticleSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/ProjectionMapSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/AudioSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/BillboardSystem.hpp>
#include <crogine/ecs/systems/UIElementSystem.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Skeleton.hpp>
#include <crogine/ecs/components/ProjectionMap.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/ShadowCaster.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>
#include <crogine/ecs/components/AudioListener.hpp>
#include <crogine/ecs/components/BillboardCollection.hpp>
#include <crogine/ecs/components/UIElement.hpp>

#include <crogine/util/Random.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/util/Constants.hpp>

#include <crogine/detail/glm/gtx/norm.hpp>
#include <crogine/detail/OpenGL.hpp>

#include <crogine/gui/Gui.hpp>

namespace
{
    //cro::UISystem* uiSystem = nullptr;
    cro::CommandSystem* commandSystem = nullptr;

    std::size_t queryCount = 0;

    float fireRate = 0.1f; //rate per second
    glm::vec3 sourcePosition = glm::vec3(-19.f, 10.f, 6.f);
    float sourceRotation = -cro::Util::Const::PI / 2.f;
}

BatcatState::BatcatState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_scene         (context.appInstance.getMessageBus(), 1024),
    m_overlayScene  (context.appInstance.getMessageBus())
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();

        registerWindow([&]() 
            {
                ImGui::SetNextWindowSize({ 300.f, 400.f }, ImGuiCond_FirstUseEver);

                if (ImGui::Begin("Window of funnage"))
                {
                    auto& cam = m_scene.getActiveCamera().getComponent<cro::Camera>();
                    float maxShadow = cam.getMaxShadowDistance();
                    if (ImGui::SliderFloat("Shadow Distance", &maxShadow, 1.f, cam.getFarPlane()))
                    {
                        cam.setMaxShadowDistance(maxShadow);
                    }

                    float exp = cam.getShadowExpansion();
                    if (ImGui::SliderFloat("Shadow Expansion", &exp, 0.f, 50.f))
                    {
                        cam.setShadowExpansion(exp);
                    }

                    ImGui::Image(m_scene.getActiveCamera().getComponent<cro::Camera>().shadowMapBuffer.getTexture(0), { 256.f, 256.f }, { 0.f, 1.f }, { 1.f, 0.f });

                    ImGui::DragFloat("Rate", &fireRate, 0.1f, 0.1f, 10.f);
                    ImGui::DragFloat("Position", &sourcePosition.x, 0.1f, -19.f, 19.f);
                    ImGui::DragFloat("Rotation", &sourceRotation, 0.02f, -cro::Util::Const::PI, cro::Util::Const::PI);
                    
                    auto count = m_scene.getEntityCount();
                    ImGui::Text("Entity Count: %lu", count);

                    ImGui::Text("Query Count %lu", queryCount);
                }
                ImGui::End();
            });

    });

    auto* msg = getContext().appInstance.getMessageBus().post<GameEvent>(MessageID::GameMessage);
    msg->type = GameEvent::RoundStart;

    context.appInstance.resetFrameTime();
}

//public
bool BatcatState::handleEvent(const cro::Event& evt)
{    
    if (evt.type == SDL_MOUSEMOTION)
    {
        auto x = static_cast<float>(evt.motion.x);
        auto y = static_cast<float>(evt.motion.y); 

        //convert to world coords
        auto worldPos = m_overlayScene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords({ x, y });

        cro::Command cmd;
        cmd.targetFlags = CommandID::Cursor;
        cmd.action = [worldPos](cro::Entity entity, float)
        {
            entity.getComponent<cro::Transform>().setPosition(worldPos);
        };
        commandSystem->sendCommand(cmd);
    }
    else if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_ESCAPE:
        case SDLK_AC_BACK:
        case SDLK_BACKSPACE:
            requestStackClear();
            requestStackPush(States::ScratchPad::MainMenu);
            break;
        }
    }

    //uiSystem->handleEvent(evt);
    m_scene.forwardEvent(evt);
    return true;
}

void BatcatState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool BatcatState::simulate(float dt)
{
    m_scene.simulate(dt);
    m_overlayScene.simulate(dt);
    return true;
}

void BatcatState::render()
{
    m_sceneTexture.clear();
    m_scene.render();
    m_sceneTexture.display();


    //feeds scene texture through post to output texture, which is in turn drawn by overlay
    m_smaaPost.render(); 
    m_overlayScene.render();
}

//private
void BatcatState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::BillboardSystem>(mb);
    m_scene.addSystem<TerrainSystem>(mb);
    m_scene.addSystem<cro::SkeletalAnimator>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::ShadowMapRenderer>(mb);
    m_scene.addSystem<cro::ModelRenderer>(mb);
    m_scene.addSystem<cro::AudioSystem>(mb);

    m_scene.addDirector<PlayerDirector>();

    //uiSystem = m_overlayScene.addSystem<cro::UISystem>(mb);
    m_overlayScene.addSystem<cro::UIElementSystem>(mb);
    m_overlayScene.addSystem<cro::CameraSystem>(mb);
    m_overlayScene.addSystem<cro::SpriteSystem2D>(mb);
    m_overlayScene.addSystem<cro::TextSystem>(mb);
    m_overlayScene.addSystem<cro::RenderSystem2D>(mb);

    commandSystem = m_overlayScene.addSystem<cro::CommandSystem>(mb);
}

void BatcatState::loadAssets()
{
    for (auto& md : m_modelDefs)
    {
        md = std::make_unique<cro::ModelDefinition>(m_resources);
    }

    m_modelDefs[GameModelID::BatCat]->loadFromFile("assets/batcat/models/batcat02.cmt", true);
    m_modelDefs[GameModelID::TestRoom]->loadFromFile("assets/batcat/models/scene03.cmt");
    m_modelDefs[GameModelID::Moon]->loadFromFile("assets/batcat/models/moon.cmt");
    m_modelDefs[GameModelID::Stars]->loadFromFile("assets/batcat/models/stars.cmt");

    m_modelDefs[GameModelID::Cube]->loadFromFile("assets/batcat/models/cube.cmt");
    m_modelDefs[GameModelID::Arrow]->loadFromFile("assets/batcat/models/arrow.cmt");
    m_modelDefs[GameModelID::Billboards]->loadFromFile("assets/batcat/models/tree.cmt");

    //CRO_ASSERT(m_modelDefs[GameModelID::BatCat].hasSkeleton(), "missing batcat anims");

    m_audioBuffer.loadFromFile("assets/batcat/sound/laser.wav");
}

void BatcatState::createScene()
{
    std::vector<glm::mat4> tx;
    for (auto i = 0; i < 7; ++i)
    {
        float x = 2.f * i;
        for (auto j = 0; j < 10; ++j)
        {
            float z = 3.f * j;
            tx.push_back(glm::translate(glm::mat4(1.f), glm::vec3(-x, 0.f, z)));
        }
    }
    std::reverse(tx.begin(), tx.end());

    //dat cat man
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec3(2.f));
    entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI / 2.f);
    entity.getComponent<cro::Transform>().setPosition({ -21.f, 0.f, -6.f });
    m_modelDefs[GameModelID::BatCat]->createModel(entity);
    entity.getComponent<cro::Skeleton>().play(AnimationID::BatCat::Run);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Player;
    entity.addComponent<Player>();
    entity.getComponent<cro::Model>().setInstanceTransforms(tx);

    /*for (auto p : tx)
    {
        auto pos = glm::vec3(p[3]);
        auto z = -pos.x;
        pos.x = pos.z;
        pos.z = z;

        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setScale(glm::vec3(0.03f));
        entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, cro::Util::Const::PI / 2.f);
        entity.getComponent<cro::Transform>().setPosition({ -21.f, 0.f, -6.f });
        entity.getComponent<cro::Transform>().move(pos * 0.03f);
        m_modelDefs[GameModelID::BatCat]->createModel(entity);
        entity.getComponent<cro::Skeleton>().play(AnimationID::BatCat::Run);
        entity.addComponent<cro::CommandTarget>().ID = CommandID::Player;
        entity.addComponent<Player>();
        entity.getComponent<cro::Model>().setInstanceTransforms(tx);
    }*/

    //load terrain chunks
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setScale({ 200.f / 175.f, 1.f, 1.f });
    m_modelDefs[GameModelID::TestRoom]->createModel(entity);
    //auto bb = entity.getComponent<cro::Model>().getMeshData().boundingBox;
    entity.addComponent<TerrainChunk>().inUse = true;
    entity.getComponent<TerrainChunk>().width = 200.f;// bb[1].x - bb[0].x; //TODO fix this

    auto chunkEnt = entity;

    //billboard ent
    cro::Billboard board;
    board.size = { 2.f, 5.f };
    //board.colour = cro::Colour::Green;

    auto bbEnt = m_scene.createEntity();
    bbEnt.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 6.f });
    m_modelDefs[GameModelID::Billboards]->createModel(bbEnt);

    for (auto i = 0u; i < 46u; ++i)
    {
        bbEnt.getComponent<cro::BillboardCollection>().addBillboard(board);
        board.position.x += 3.3f;
        board.position.z = static_cast<float>(cro::Util::Random::value(-2, 2));
    }

    entity.getComponent<cro::Transform>().addChild(bbEnt.getComponent<cro::Transform>());

    //TODO these will be different types of chunk
    const int count = 3;
    for (auto i = 0; i < count; ++i)
    {
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 400.f, 0.f, 0.f });
        entity.getComponent<cro::Transform>().setScale({ 200.f / 175.f, 1.f, 1.f });
        //auto bb = entity.getComponent<cro::Model>().getMeshData().boundingBox;
        m_modelDefs[GameModelID::TestRoom]->createModel(entity);
        entity.addComponent<TerrainChunk>().width = 200.f;

        bbEnt = m_scene.createEntity();
        bbEnt.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 3.f + (3.f * i) });
        m_modelDefs[GameModelID::Billboards]->createModel(bbEnt);

        board.position.x = 0.f;
        for (auto i = 0u; i < 46u; ++i)
        {
            bbEnt.getComponent<cro::BillboardCollection>().addBillboard(board);
            board.position.x += 3.3f;
            board.position.z = static_cast<float>(cro::Util::Random::value(-2, 2));
        }
        entity.getComponent<cro::Transform>().addChild(bbEnt.getComponent<cro::Transform>());
    }


    //moon and stars background
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -60.f, 70.f, -180.f });
    entity.getComponent<cro::Transform>().setScale(glm::vec3(6.f));
    m_modelDefs[GameModelID::Moon]->createModel(entity);

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.5, -200.f });
    entity.getComponent<cro::Transform>().setScale(glm::vec3(3.f));
    m_modelDefs[GameModelID::Stars]->createModel(entity);

    //3D camera
    auto ent = m_scene.createEntity();
    ent.addComponent<cro::Transform>().setPosition({ 0.f, 10.f, 50.f });
    //projection is set in updateView()
    ent.addComponent<cro::Camera>().shadowMapBuffer.create(2048, 2048);
    ent.getComponent<cro::Camera>().resizeCallback = std::bind(&BatcatState::updateView, this, std::placeholders::_1);
    ent.addComponent<cro::CommandTarget>().ID = CommandID::Camera;
    updateView(ent.getComponent<cro::Camera>());

#ifdef CRO_DEBUG_
    auto shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::VertexColour);
    auto materialID = m_resources.materials.add(m_resources.shaders.get(shaderID));
    auto meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_LINES));

    m_resources.materials.get(materialID).blendMode = cro::Material::BlendMode::Alpha;
    //m_resources.materials.get(materialID).enableDepthTest = false;

    /*auto debugEnt = m_scene.createEntity();
    debugEnt.addComponent<cro::Transform>();
    debugEnt.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), m_resources.materials.get(materialID));*/
    //debugEnt.addComponent<cro::Callback>().active = true;
    //debugEnt.getComponent<cro::Callback>().function =
    //    [&,ent](cro::Entity e, float)
    //{
    //    const auto& cam = ent.getComponent<cro::Camera>();
    //    //e.getComponent<cro::Transform>().setPosition(ent.getComponent<cro::Transform>().getWorldPosition());
    //    e.getComponent<cro::Transform>().setPosition(cam.depthPosition);
    //    e.getComponent<cro::Transform>().setRotation(m_scene.getSunlight().getComponent<cro::Transform>().getRotation());

    //    std::vector<float> verts =
    //    {
    //        cam.depthDebug[0], cam.depthDebug[2], cam.depthDebug[4],
    //        1.f,0.f,1.f,1.f,

    //        cam.depthDebug[1], cam.depthDebug[2], cam.depthDebug[4],
    //        1.f,0.f,1.f,1.f,

    //        cam.depthDebug[0], cam.depthDebug[3], cam.depthDebug[4],
    //        1.f,0.f,1.f,1.f,

    //        cam.depthDebug[1], cam.depthDebug[3], cam.depthDebug[4],
    //        1.f,0.f,1.f,1.f,

    //        cam.depthDebug[0], cam.depthDebug[2], -cam.depthDebug[5],
    //        0.f,1.f,1.f,1.f,

    //        cam.depthDebug[1], cam.depthDebug[2], -cam.depthDebug[5],
    //        0.f,1.f,1.f,1.f,

    //        cam.depthDebug[0], cam.depthDebug[3], -cam.depthDebug[5],
    //        0.f,1.f,1.f,1.f,

    //        cam.depthDebug[1], cam.depthDebug[3], -cam.depthDebug[5],
    //        0.f,1.f,1.f,1.f,
    //    };

    //    std::vector<std::uint32_t> indices =
    //    {
    //        0,1, 1,3, 3,2, 2,0,
    //        4,5, 5,7, 7,6, 6,4,
    //        0,4, 1,5, 3,7, 2,6
    //    };

    //    auto& meshData = e.getComponent<cro::Model>().getMeshData();
    //    glBindBuffer(GL_ARRAY_BUFFER, meshData.vbo);
    //    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
    //    glBindBuffer(GL_ARRAY_BUFFER, 0);

    //    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[0].ibo);
    //    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(std::uint32_t), indices.data(), GL_DYNAMIC_DRAW);
    //    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    //    meshData.boundingBox[0] = { cam.depthDebug[0], cam.depthDebug[2], cam.depthDebug[4] };
    //    meshData.boundingBox[1] = { cam.depthDebug[1], cam.depthDebug[3], -cam.depthDebug[5] };
    //    meshData.boundingSphere.centre = meshData.boundingBox[0] + ((meshData.boundingBox[1] - meshData.boundingBox[0]) / 2.f);
    //    meshData.boundingSphere.radius = glm::length(meshData.boundingSphere.centre);
    //};

    /*auto& meshData = debugEnt.getComponent<cro::Model>().getMeshData();
    meshData.vertexCount = 8;
    meshData.vertexSize = 7 * meshData.vertexCount * sizeof(float);
    meshData.indexData[0].indexCount = 24;*/
#endif

    auto sunEnt = m_scene.getSunlight();
    sunEnt.getComponent<cro::Transform>().setPosition({ -19.f, 11.f, 12.f });
    sunEnt.getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, -(0.797f/* + (cro::Util::Const::PI / 2.f)*/));
    sunEnt.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -0.72f);

    ent.addComponent<cro::AudioListener>();
    m_scene.setActiveCamera(ent);
    m_scene.setActiveListener(ent);


    //function for creating sound ents
    auto launchEnt = [&]()
    {
        auto e = m_scene.createEntity();
        e.addComponent<cro::Transform>().setPosition(sourcePosition);
        e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, sourceRotation);
        m_modelDefs[GameModelID::Cube]->createModel(e);

        static const float Speed = 35.f;

        auto velocity = e.getComponent<cro::Transform>().getForwardVector() * Speed;
        e.addComponent<cro::Callback>().active = true;
        e.getComponent<cro::Callback>().setUserData<std::pair<float, glm::vec3>>(0.f, velocity);
        e.getComponent<cro::Callback>().function =
            [&](cro::Entity ett, float dt)
        {
            static const float MaxLifetime = 6.f;

            auto& [lifetime, vel] = ett.getComponent<cro::Callback>().getUserData<std::pair<float, glm::vec3>>();
            auto& tx = ett.getComponent<cro::Transform>();
            tx.move(vel * dt);

            lifetime += dt;
            if (lifetime > MaxLifetime)
            {
                ett.getComponent<cro::Callback>().active = false;
                m_scene.destroyEntity(ett);
            }
        };

        e.addComponent<cro::AudioEmitter>(m_audioBuffer);
        e.getComponent<cro::AudioEmitter>().play();
        e.getComponent<cro::AudioEmitter>().setVolume(1.f);
        e.getComponent<cro::AudioEmitter>().setRolloff(0.8f);
    };

    //this ent spawns our sound entities
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    m_modelDefs[GameModelID::Arrow]->createModel(entity);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(10.f);
    entity.getComponent<cro::Callback>().function =
        [launchEnt](cro::Entity e, float dt)
    {
        auto& tx = e.getComponent<cro::Transform>();
        tx.setPosition(sourcePosition);
        tx.setRotation(cro::Transform::Y_AXIS, sourceRotation);

        auto& timer = e.getComponent<cro::Callback>().getUserData<float>();
        timer += dt;
        if (timer > 1.f / fireRate)
        {
            launchEnt();

            timer = 0.f;
        }
    };
}

namespace
{
    const cro::FloatRect buttonArea(0.f, 0.f, 64.f, 64.f);
}

void BatcatState::createUI()
{
    //2D camera
    auto ent = m_overlayScene.createEntity();
    ent.addComponent<cro::Transform>();
    auto& cam2D = ent.addComponent<cro::Camera>();
    /*cam2D.setOrthographic(0.f, 1280.f, 0.f, 720.f, -10.f, 10.f);
    cam2D.resizeCallback = std::bind(&BatcatState::calcViewport, this, std::placeholders::_1);
    calcViewport(cam2D);*/

    cam2D.resizeCallback = [](cro::Camera& cam)
        {
            glm::vec2 size(cro::App::getWindow().getSize());
            cam.setOrthographic(0.f, size.x, 0.f, size.y, -10.f, 10.f);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        };
    cam2D.resizeCallback(cam2D);

    m_overlayScene.setActiveCamera(ent);

    /*cro::SpriteSheet targetSheet;
    targetSheet.loadFromFile("assets/batcat/sprites/target.spt", m_resources.textures);
    ent = m_overlayScene.createEntity();
    ent.addComponent<cro::Sprite>() = targetSheet.getSprite("target");
    ent.addComponent<cro::Drawable2D>();
    auto size = ent.getComponent<cro::Sprite>().getSize();
    ent.addComponent<cro::Transform>().setOrigin({ size.x / 2.f, size.y / 2.f, 0.f });
    ent.getComponent<cro::Transform>().setScale(glm::vec3(0.5f));
    ent.addComponent<cro::CommandTarget>().ID = CommandID::Cursor;*/

    m_resources.fonts.load(1, "assets/fonts/VeraMono.ttf");
    m_resources.fonts.get(1).setSmooth(true);
    
    ent = m_overlayScene.createEntity();
    ent.addComponent<cro::Transform>().setPosition({ 200.f, 100.f, 1.f });
    ent.addComponent<cro::Drawable2D>();
    ent.addComponent<cro::Text>(m_resources.fonts.get(1)).setString("Hallo! I am text.");




    const auto& tex = m_resources.textures.get("assets/batcat/Unigine01.png");



    //SMAA output
    ent = m_overlayScene.createEntity();
    ent.addComponent<cro::Transform>();
    ent.addComponent<cro::UIElement>().depth = -0.1f;
    ent.getComponent<cro::UIElement>().resizeCallback =
        [](cro::Entity e)
        {
            glm::vec2 size(cro::App::getWindow().getSize());
            const auto y = (size.y - ((size.x / 16.f) * 9.f)) / 2.f;
            e.getComponent<cro::UIElement>().relativePosition = { 0.f, y / size.y };
        };
    m_smaaRoot = ent;

    ent = m_overlayScene.createEntity();
    ent.addComponent<cro::Transform>();
    ent.addComponent<cro::Drawable2D>().setBlendMode(cro::Material::BlendMode::None);
    ent.addComponent<cro::Sprite>(m_outputTexture.getTexture());
    m_smaaRoot.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
    auto outputEnt = ent;

//    const std::string f = R"(
//uniform sampler2D u_texture;
//VARYING_IN vec2 v_texCoord;
//VARYING_IN vec4 v_colour;
//OUTPUT
//
//void main(){FRAG_OUT = vec4(texture(u_texture, v_texCoord).rgb * v_colour.rgb, 1.0);}
//
//)";
//
//    m_resources.shaders.loadFromString(ShaderID::SMAAPreview, cro::RenderSystem2D::getDefaultVertexShader(), f, "#define TEXTURED\n");


    ent = m_overlayScene.createEntity();
    ent.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    ent.addComponent<cro::Drawable2D>();// .setShader(&m_resources.shaders.get(ShaderID::SMAAPreview));
    ent.addComponent<cro::Sprite>(m_outputTexture.getTexture(), cro::Material::BlendMode::None);
    ent.addComponent<cro::UIElement>().resizeCallback =
        [&](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setTexture(m_smaaPost.getEdgeTexture());
        };
    m_smaaRoot.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
    auto edgeEnt = ent;



    ent = m_overlayScene.createEntity();
    ent.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    ent.addComponent<cro::Drawable2D>();// .setShader(&m_resources.shaders.get(ShaderID::SMAAPreview));
    ent.addComponent<cro::Sprite>(m_outputTexture.getTexture(), cro::Material::BlendMode::None);
    ent.addComponent<cro::UIElement>().resizeCallback =
        [&](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setTexture(m_smaaPost.getWeightTexture());
        };
    m_smaaRoot.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
    auto weightEnt = ent;

    //non-SMAA
    ent = m_overlayScene.createEntity();
    ent.addComponent<cro::Transform>();
    ent.addComponent<cro::Drawable2D>();
    ent.addComponent<cro::Sprite>(/*m_sceneTexture.getTexture()*/tex);
    ent.addComponent<cro::UIElement>().depth = -0.2f;
    ent.getComponent<cro::UIElement>().resizeCallback =
        [](cro::Entity e)
        {
            glm::vec2 size(cro::App::getWindow().getSize());
            const auto y = (size.y - ((size.x / 16.f) * 9.f)) / 2.f;
            e.getComponent<cro::UIElement>().relativePosition = { 0.f, y / size.y };
        };
    
    registerWindow([&, outputEnt, edgeEnt, weightEnt]() mutable
        {
            if (ImGui::Begin("SMAA"))
            {
                static bool showSMAA = true;
                if (ImGui::Checkbox("SMAA", &showSMAA))
                {
                    const auto scale = showSMAA ? 1.f : 0.f;
                    m_smaaRoot.getComponent<cro::Transform>().setScale(glm::vec2(scale));
                }

                static int output = 0;
                static std::array t =
                {
                    std::string("Colour"),
                    std::string("Edges"),
                    std::string("Weight"),
                };

                if (ImGui::InputInt("SMAA Output", &output))
                {
                    output %= t.size();
                    switch (output)
                    {
                    default:
                    case 0:
                        outputEnt.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                        edgeEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                        weightEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                        break;
                    case 1:
                        outputEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                        edgeEnt.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                        weightEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                        break;
                    case 2:
                        outputEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                        edgeEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                        weightEnt.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                        break;
                    }
                }
                ImGui::Text("%s", t[output].c_str());
            }
            ImGui::End();        
        });


#ifdef PLATFORM_MOBILE
    m_resources.textures.get("assets/ui/ui_buttons.png", false).setSmooth(true);
    auto mouseEnter = uiSystem->addCallback(
        [](cro::Entity entity, glm::vec2)
    {
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Blue());
    });

    auto mouseExit = uiSystem->addCallback(
        [](cro::Entity entity, glm::vec2)
    {
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::White());
    });

    auto mouseDown = uiSystem->addCallback(
        [this](cro::Entity entity, cro::uint64 flags)
    {
        if ((flags & cro::UISystem::LeftMouse)
            || (flags & cro::UISystem::Finger))
        {
            auto msg = getContext().appInstance.getMessageBus().post<UIEvent>(MessageID::UIMessage);
            msg->type = UIEvent::ButtonPressed;
            msg->button = static_cast<UIEvent::Button>(entity.getComponent<cro::UIInput>().ID);
        }
    });

    auto mouseUp = uiSystem->addCallback(
        [this](cro::Entity entity, cro::uint64 flags)
    {
        if ((flags & cro::UISystem::LeftMouse)
            || (flags & cro::UISystem::Finger))
        {
            auto msg = getContext().appInstance.getMessageBus().post<UIEvent>(MessageID::UIMessage);
            msg->type = UIEvent::ButtonReleased;
            msg->button = static_cast<UIEvent::Button>(entity.getComponent<cro::UIInput>().ID);
        }
    });


    ent = m_overlayScene.createEntity();
    ent.addComponent<cro::Transform>().setPosition({ 40.f, 40.f, -0.1f });
    ent.getComponent<cro::Transform>().setScale({ 2.f, 2.f, 2.f });
    ent.addComponent<cro::Drawable2D>();
    auto& button1 = ent.addComponent<cro::Sprite>();
    button1.setTexture(m_resources.textures.get("assets/ui/ui_buttons.png"));
    button1.setTextureRect(buttonArea);
    auto& ui1 = ent.addComponent<cro::UIInput>();
    ui1.area = buttonArea;
    ui1.ID = UIEvent::Left;
    ui1.callbacks[cro::UIInput::MouseEnter] = mouseEnter;
    ui1.callbacks[cro::UIInput::MouseExit] = mouseExit;
    ui1.callbacks[cro::UIInput::MouseDown] = mouseDown;
    ui1.callbacks[cro::UIInput::MouseUp] = mouseUp;


    ent = m_overlayScene.createEntity();
    ent.addComponent<cro::Transform>().setPosition({ 320.f, 168.f, -0.1f });
    ent.getComponent<cro::Transform>().setScale({ 2.f, 2.f, 2.f });
    ent.getComponent<cro::Transform>().rotate({ 0.f, 0.f, 1.f }, cro::Util::Const::PI);
    ent.addComponent<cro::Drawable2D>();
    auto& button2 = ent.addComponent<cro::Sprite>();
    button2 = button1;
    auto& ui2 = ent.addComponent<cro::UIInput>();
    ui2.area = buttonArea;
    ui2.ID = UIEvent::Right;
    ui2.callbacks[cro::UIInput::MouseEnter] = mouseEnter;
    ui2.callbacks[cro::UIInput::MouseExit] = mouseExit;
    ui2.callbacks[cro::UIInput::MouseDown] = mouseDown;
    ui2.callbacks[cro::UIInput::MouseUp] = mouseUp;

    ent = m_overlayScene.createEntity();
    ent.addComponent<cro::Transform>().setPosition({ 960.f, 40.f, -0.1f });
    ent.getComponent<cro::Transform>().setScale({ 2.f, 2.f, 2.f });
    ent.addComponent<cro::Drawable2D>();
    auto& button3 = ent.addComponent<cro::Sprite>();
    button3.setTexture(m_resources.textures.get("assets/ui/ui_buttons.png"));
    button3.setTextureRect({ 64.f, 0.f, 64.f, 64.f });
    auto& ui3 = ent.addComponent<cro::UIInput>();
    ui3.area = buttonArea;
    ui3.ID = UIEvent::Jump;
    ui3.callbacks[cro::UIInput::MouseEnter] = mouseEnter;
    ui3.callbacks[cro::UIInput::MouseExit] = mouseExit;
    ui3.callbacks[cro::UIInput::MouseDown] = mouseDown;
    ui3.callbacks[cro::UIInput::MouseUp] = mouseUp;

    ent = m_overlayScene.createEntity();
    ent.addComponent<cro::Transform>().setPosition({ 1112.f, 40.f, -0.1f });
    ent.getComponent<cro::Transform>().setScale({ 2.f, 2.f, 2.f });
    ent.addComponent<cro::Drawable2D>();
    auto& button4 = ent.addComponent<cro::Sprite>();
    button4.setTexture(m_resources.textures.get("assets/ui/ui_buttons.png"));
    button4.setTextureRect({ 128.f, 0.f, 64.f, 64.f });
    auto& ui4 = ent.addComponent<cro::UIInput>();
    ui4.area = buttonArea;
    ui4.ID = UIEvent::Fire;
    ui4.callbacks[cro::UIInput::MouseEnter] = mouseEnter;
    ui4.callbacks[cro::UIInput::MouseExit] = mouseExit;
    ui4.callbacks[cro::UIInput::MouseDown] = mouseDown;
    ui4.callbacks[cro::UIInput::MouseUp] = mouseUp;
#endif //PLATFORM_MOBILE
}

void BatcatState::calcViewport(cro::Camera& cam)
{
    auto windowSize = cro::App::getWindow().getSize();
    glm::vec2 size(windowSize);
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

    cam.viewport.bottom = (1.f - size.y) / 2.f;
    cam.viewport.height = size.y;
}

void BatcatState::updateView(cro::Camera& cam3D)
{
    glm::uvec2 size = cro::App::getWindow().getSize();
    size.y = (size.x / 16) * 9;

    m_sceneTexture.create(size.x, size.y);
    m_outputTexture.create(size.x, size.y, false);
    m_smaaPost.create(/*m_sceneTexture.getTexture()*/m_resources.textures.get("assets/batcat/Unigine01.png"), m_outputTexture);

    cam3D.setPerspective(35.f * cro::Util::Const::degToRad, 16.f/9.f, 6.f, 280.f);
    cam3D.setMaxShadowDistance(90.f);
    cam3D.viewport = { 0.f, 0.f, 1.f, 1.f };
}