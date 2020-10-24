/*-----------------------------------------------------------------------

Matt Marchant 2017
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

#include "GameState.hpp"
#include "ResourceIDs.hpp"
#include "Messages.hpp"
#include "PlayerDirector.hpp"
#include "TerrainChunk.hpp"
#include "TerrainSystem.hpp"

#include <crogine/detail/GlobalConsts.hpp>

#include <crogine/core/App.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/ConfigFile.hpp>

#include <crogine/graphics/QuadBuilder.hpp>
#include <crogine/graphics/StaticMeshBuilder.hpp>
#include <crogine/graphics/IqmBuilder.hpp>
#include <crogine/graphics/SpriteSheet.hpp>

#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/ParticleSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/ProjectionMapSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/AudioSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/BillboardSystem.hpp>

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
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/BillboardCollection.hpp>

#include <crogine/util/Random.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/util/Constants.hpp>

#include <crogine/detail/glm/gtx/norm.hpp>

#include <crogine/gui/Gui.hpp>

namespace
{
    cro::UISystem* uiSystem = nullptr;
    cro::CommandSystem* commandSystem = nullptr;

    std::size_t queryCount = 0;

    float fireRate = 0.1f; //rate per second
    glm::vec3 sourcePosition = glm::vec3(-19.f, 10.f, 6.f);
    float sourceRotation = -cro::Util::Const::PI / 2.f;
}

GameState::GameState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_scene         (context.appInstance.getMessageBus(), 124),
    m_overlayScene  (context.appInstance.getMessageBus())
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();


        registerWindow([&]() 
            {
                ImGui::SetNextWindowSize({ 200.f, 400.f }, ImGuiCond_FirstUseEver);

                if (ImGui::Begin("Window of funnage"))
                {
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

    updateView();

    auto* msg = getContext().appInstance.getMessageBus().post<GameEvent>(MessageID::GameMessage);
    msg->type = GameEvent::RoundStart;

    context.appInstance.resetFrameTime();
}

//public
bool GameState::handleEvent(const cro::Event& evt)
{    
    if (evt.type == SDL_MOUSEMOTION)
    {
        auto x = static_cast<float>(evt.motion.x);
        auto y = static_cast<float>(evt.motion.y); 

        //convert to world coords
        //bit of a kludge using fixed view size
        auto windowSize = cro::App::getWindow().getSize();
        x = (x / windowSize.x) * 1280.f;
        y = (1.f - (y / windowSize.y)) * 720.f;

        cro::Command cmd;
        cmd.targetFlags = CommandID::Cursor;
        cmd.action = [x, y](cro::Entity entity, float)
        {
            entity.getComponent<cro::Transform>().setPosition({ x, y, 0 });
        };
        commandSystem->sendCommand(cmd);
    }
    
    uiSystem->handleEvent(evt);
    m_scene.forwardEvent(evt);
    return true;
}

void GameState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);

    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            updateView();
        }
    }
}

bool GameState::simulate(float dt)
{
    m_scene.simulate(dt);
    m_overlayScene.simulate(dt);
    return true;
}

void GameState::render()
{
    auto& rt = cro::App::getWindow();
    m_scene.render(rt);
    m_overlayScene.render(rt);
}

//private
void GameState::addSystems()
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

    uiSystem = &m_overlayScene.addSystem<cro::UISystem>(mb);
    m_overlayScene.addSystem<cro::CameraSystem>(mb);
    m_overlayScene.addSystem<cro::SpriteSystem>(mb);
    m_overlayScene.addSystem<cro::TextSystem>(mb);
    m_overlayScene.addSystem<cro::RenderSystem2D>(mb);

    commandSystem = &m_overlayScene.addSystem<cro::CommandSystem>(mb);
}

void GameState::loadAssets()
{
    m_modelDefs[GameModelID::BatCat].loadFromFile("assets/models/batcat.cmt", m_resources);
    m_modelDefs[GameModelID::TestRoom].loadFromFile("assets/models/scene03.cmt", m_resources);
    m_modelDefs[GameModelID::Moon].loadFromFile("assets/models/moon.cmt", m_resources);
    m_modelDefs[GameModelID::Stars].loadFromFile("assets/models/stars.cmt", m_resources);

    m_modelDefs[GameModelID::Cube].loadFromFile("assets/models/cube.cmt", m_resources);
    m_modelDefs[GameModelID::Arrow].loadFromFile("assets/models/arrow.cmt", m_resources);
    m_modelDefs[GameModelID::Billboards].loadFromFile("assets/models/tree.cmt", m_resources);

    //CRO_ASSERT(m_modelDefs[GameModelID::BatCat].hasSkeleton(), "missing batcat anims");

    m_audioBuffer.loadFromFile("assets/sound/laser.wav");
}

void GameState::createScene()
{
    //dat cat man
    auto entity = m_scene.createEntity();
    m_modelDefs[GameModelID::BatCat].createModel(entity, m_resources);

    entity.addComponent<cro::Transform>().setScale(glm::vec3(0.03f));
    entity.getComponent<cro::Transform>().setRotation({ 0.f, 0.f, cro::Util::Const::PI / 2.f });
    entity.getComponent<cro::Transform>().setPosition({ -19.f, 0.f, 6.f });
    entity.getComponent<cro::Skeleton>().play(AnimationID::BatCat::Run);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Player;
    entity.addComponent<Player>();


    //load terrain chunks
    entity = m_scene.createEntity();
    m_modelDefs[GameModelID::TestRoom].createModel(entity, m_resources);
    entity.addComponent<cro::Transform>().setScale({ 200.f / 175.f, 1.f, 1.f });
    //auto bb = entity.getComponent<cro::Model>().getMeshData().boundingBox;
    entity.addComponent<TerrainChunk>().inUse = true;
    entity.getComponent<TerrainChunk>().width = 200.f;// bb[1].x - bb[0].x; //TODO fix this

    auto chunkEnt = entity;

    //billboard ent
    cro::Billboard board;
    board.size = { 2.f, 5.f };
    board.colour = cro::Colour::Green();

    auto bbEnt = m_scene.createEntity();
    m_modelDefs[GameModelID::Billboards].createModel(bbEnt, m_resources);
    bbEnt.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 6.f });

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
        m_modelDefs[GameModelID::TestRoom].createModel(entity, m_resources);
        entity.addComponent<cro::Transform>().setPosition({ 400.f, 0.f, 0.f });
        entity.getComponent<cro::Transform>().setScale({ 200.f / 175.f, 1.f, 1.f });
        //auto bb = entity.getComponent<cro::Model>().getMeshData().boundingBox;
        entity.addComponent<TerrainChunk>().width = 200.f;

        bbEnt = m_scene.createEntity();
        m_modelDefs[GameModelID::Billboards].createModel(bbEnt, m_resources);
        bbEnt.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 3.f + (3.f * i) });

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
    m_modelDefs[GameModelID::Moon].createModel(entity, m_resources);
    entity.addComponent<cro::Transform>().setPosition({ -60.f, 70.f, -180.f });
    entity.getComponent<cro::Transform>().setScale(glm::vec3(6.f));

    entity = m_scene.createEntity();
    m_modelDefs[GameModelID::Stars].createModel(entity, m_resources);
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.5, -200.f });
    entity.getComponent<cro::Transform>().setScale(glm::vec3(3.f));

    //3D camera
    auto ent = m_scene.createEntity();
    ent.addComponent<cro::Transform>().setPosition({ 0.f, 10.f, 50.f });
    //projection is set in updateView()
    ent.addComponent<cro::Camera>();// .projection = glm::perspective(45.f, 16.f / 9.f, 0.1f, 20.f);
    ent.addComponent<cro::CommandTarget>().ID = CommandID::Camera;
    m_scene.getSystem<cro::ShadowMapRenderer>().setProjectionOffset({ 19.f, 16.4f, -10.3f });
    m_scene.getSunlight().setDirection({ -0.f, -1.f, -0.4f });
    m_scene.getSunlight().setProjectionMatrix(glm::ortho(-5.6f, 5.6f, -5.6f, 5.6f, 0.1f, 80.f));

    ent.addComponent<cro::AudioListener>();
    m_scene.setActiveCamera(ent);
    m_scene.setActiveListener(ent);


    //function for creating sound ents
    auto launchEnt = [&]()
    {
        auto e = m_scene.createEntity();
        e.addComponent<cro::Transform>().setPosition(sourcePosition);
        e.getComponent<cro::Transform>().setRotation(glm::vec3(0.f, 0.f, sourceRotation));
        m_modelDefs[GameModelID::Cube].createModel(e, m_resources);

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
    m_modelDefs[GameModelID::Arrow].createModel(entity, m_resources);
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(10.f);
    entity.getComponent<cro::Callback>().function =
        [launchEnt](cro::Entity e, float dt)
    {
        auto& tx = e.getComponent<cro::Transform>();
        tx.setPosition(sourcePosition);
        tx.setRotation(glm::vec3(0.f, 0.f, sourceRotation));

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

#include <crogine/graphics/BoundingBox.hpp>
void GameState::createUI()
{
    //2D camera
    auto ent = m_overlayScene.createEntity();
    ent.addComponent<cro::Transform>();
    auto& cam2D = ent.addComponent<cro::Camera>();
    cam2D.projectionMatrix = glm::ortho(0.f, /*static_cast<float>(cro::DefaultSceneSize.x)*/1280.f, 0.f, /*static_cast<float>(cro::DefaultSceneSize.y)*/720.f, -0.1f, 100.f);
    m_overlayScene.setActiveCamera(ent);

    //preview shadow map
    /*ent = m_overlayScene.createEntity();
    ent.addComponent<cro::Transform>().setPosition({ 20.f, 20.f, 0.f });
    ent.getComponent<cro::Transform>().setScale(glm::vec3(0.5f));
    ent.addComponent<cro::Sprite>(m_scene.getSystem<cro::ShadowMapRenderer>().getDepthMapTexture());
    ent.addComponent<cro::Drawable2D>();*/


    cro::SpriteSheet targetSheet;
    targetSheet.loadFromFile("assets/sprites/target.spt", m_resources.textures);
    ent = m_overlayScene.createEntity();
    ent.addComponent<cro::Sprite>() = targetSheet.getSprite("target");
    ent.addComponent<cro::Drawable2D>();
    auto size = ent.getComponent<cro::Sprite>().getSize();
    ent.addComponent<cro::Transform>().setOrigin({ size.x / 2.f, size.y / 2.f, 0.f });
    ent.getComponent<cro::Transform>().setScale(glm::vec3(0.5f));
    ent.addComponent<cro::CommandTarget>().ID = CommandID::Cursor;

    m_resources.fonts.load(1, "assets/VeraMono.ttf");
    
    ent = m_overlayScene.createEntity();
    ent.addComponent<cro::Transform>().setPosition({ 200.f, 100.f });
    ent.addComponent<cro::Drawable2D>();
    ent.addComponent<cro::Text>(m_resources.fonts.get(1)).setString("dslkdfgklfd");

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

void GameState::updateView()
{
    glm::vec2 size(cro::App::getWindow().getSize());
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

    auto& cam3D = m_scene.getActiveCamera().getComponent<cro::Camera>();
    cam3D.projectionMatrix = glm::perspective(35.f * cro::Util::Const::degToRad, 16.f / 9.f, 0.1f, 280.f);
    cam3D.viewport.bottom = (1.f - size.y) / 2.f;
    cam3D.viewport.height = size.y;

    auto& cam2D = m_overlayScene.getActiveCamera().getComponent<cro::Camera>();
    cam2D.viewport = cam3D.viewport;
}