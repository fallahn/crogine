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
#include "PlayerWeaponSystem.hpp"

#include <crogine/detail/GlobalConsts.hpp>

#include <crogine/core/App.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/ConfigFile.hpp>

#include <crogine/graphics/QuadBuilder.hpp>
#include <crogine/graphics/StaticMeshBuilder.hpp>
#include <crogine/graphics/IqmBuilder.hpp>
#include <crogine/graphics/SpriteSheet.hpp>

#include <crogine/ecs/systems/SceneGraph.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/ParticleSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/TextRenderer.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/ProjectionMapSystem.hpp>
#include <crogine/ecs/systems/SpriteRenderer.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/CollisionSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>
#include <crogine/ecs/components/CommandID.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Skeleton.hpp>
#include <crogine/ecs/components/ProjectionMap.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/PhysicsObject.hpp>
#include <crogine/ecs/components/ShadowCaster.hpp>

#include <crogine/util/Random.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/util/Constants.hpp>

#include <glm/gtx/norm.hpp>

namespace
{
    cro::UISystem* uiSystem = nullptr;
    cro::CommandSystem* commandSystem = nullptr;
}

GameState::GameState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_scene         (context.appInstance.getMessageBus()),
    m_overlayScene  (context.appInstance.getMessageBus())
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    });
    context.appInstance.setClearColour(cro::Colour(0.4f, 0.58f, 0.92f));
    //context.mainWindow.setVsyncEnabled(false);

    updateView();

    auto* msg = getContext().appInstance.getMessageBus().post<GameEvent>(MessageID::GameMessage);
    msg->type = GameEvent::RoundStart;

    context.appInstance.resetFrameTime();
}

//public
bool GameState::handleEvent(const cro::Event& evt)
{    
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

bool GameState::simulate(cro::Time dt)
{
    m_scene.simulate(dt);
    m_overlayScene.simulate(dt);
    return true;
}

void GameState::render()
{
    m_scene.render();
    m_overlayScene.render();
}

//private
void GameState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    //m_scene.addSystem<cro::ProjectionMapSystem>(mb);
    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<PlayerWeaponSystem>(mb);
    m_scene.addSystem<cro::SkeletalAnimator>(mb);
    m_scene.addSystem<cro::CollisionSystem>(mb);
    m_scene.addSystem<cro::SceneGraph>(mb);
    m_scene.addSystem<cro::ShadowMapRenderer>(mb, m_shadowMap);
    m_scene.addSystem<cro::ModelRenderer>(mb);
    m_scene.addSystem<cro::SpriteRenderer>(mb);

    m_scene.addDirector<PlayerDirector>();

    uiSystem = &m_overlayScene.addSystem<cro::UISystem>(mb);
    m_overlayScene.addSystem<cro::SceneGraph>(mb);
    m_overlayScene.addSystem<cro::TextRenderer>(mb);
    m_overlayScene.addSystem<cro::SpriteRenderer>(mb);
    commandSystem = &m_overlayScene.addSystem<cro::CommandSystem>(mb);
}

void GameState::loadAssets()
{
    m_modelDefs[GameModelID::BatCat].loadFromFile("assets/models/batcat.cmt", m_resources);
    m_modelDefs[GameModelID::TestRoom].loadFromFile("assets/models/test_room.cmt", m_resources);
    m_modelDefs[GameModelID::Grenade].loadFromFile("assets/models/grenade.cmt", m_resources);

    CRO_ASSERT(m_modelDefs[GameModelID::BatCat].skeleton, "missing batcat anims");
    m_modelDefs[GameModelID::BatCat].skeleton->play(AnimationID::BatCat::Idle);

    m_shadowMap.create(512, 512); //TODO adjust size based on platform
}

void GameState::createScene()
{
    //rooms
    static const std::size_t roomCount = 6;
    glm::vec3 houseScale(1.3f);
    const auto& bb = m_resources.meshes.getMesh(m_modelDefs[GameModelID::TestRoom].meshID).boundingBox;
    const float stride = ((bb[1].x - bb[0].x) * houseScale.x) - 0.01f;

    for (auto i = 0u; i < roomCount; ++i)
    {
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[GameModelID::TestRoom].meshID),
                                        m_resources.materials.get(m_modelDefs[GameModelID::TestRoom].materialIDs[0]));
        for (auto i = 0u; i < m_modelDefs[GameModelID::TestRoom].materialCount; ++i)
        {
            entity.getComponent<cro::Model>().setMaterial(i, m_resources.materials.get(m_modelDefs[GameModelID::TestRoom].materialIDs[i]));
        }
        entity.addComponent<cro::Transform>().scale(houseScale);
        entity.getComponent<cro::Transform>().setPosition({ i * stride, 0.6f, -0.5f });

        cro::PhysicsShape ps;
        ps.type = cro::PhysicsShape::Type::Box;
        ps.extent = { 0.01f, 0.2f, 0.25f };
        ps.extent *= houseScale;
        ps.position = { -0.67f, 0.25f, 0.4f };
        ps.position *= houseScale;
        entity.addComponent<cro::PhysicsObject>().addShape(ps);
        ps.position.x = -ps.position.x;
        entity.getComponent<cro::PhysicsObject>().addShape(ps);
        entity.getComponent<cro::PhysicsObject>().setCollisionFlags(CollisionID::Player |CollisionID::Weapon);
        entity.getComponent<cro::PhysicsObject>().setCollisionGroups(CollisionID::Wall);

        entity.getComponent<cro::Model>().setShadowMaterial(0, m_resources.materials.get(m_modelDefs[GameModelID::TestRoom].shadowIDs[0]));
        entity.addComponent<cro::ShadowCaster>();
    }

    //dat cat man
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[GameModelID::BatCat].meshID),
                                    m_resources.materials.get(m_modelDefs[GameModelID::BatCat].materialIDs[0]));
    //shadow caster for cat
    if (m_modelDefs[GameModelID::BatCat].castShadows)
    {
        entity.getComponent<cro::Model>().setShadowMaterial(0, m_resources.materials.get(m_modelDefs[GameModelID::BatCat].shadowIDs[0]));
        entity.addComponent<cro::ShadowCaster>();
        if (m_modelDefs[GameModelID::BatCat].skeleton)
        {
            entity.getComponent<cro::ShadowCaster>().skinned = true;
        }
    }

    entity.addComponent<cro::Transform>().setScale({ 0.002f, 0.002f, 0.002f });
    entity.getComponent<cro::Transform>().setRotation({ -cro::Util::Const::PI / 2.f, cro::Util::Const::PI / 2.f, 0.f });
    entity.addComponent<cro::Skeleton>() = *m_modelDefs[GameModelID::BatCat].skeleton;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Player;
    entity.addComponent<Player>();


    auto& phys = entity.addComponent<cro::PhysicsObject>();
    cro::PhysicsShape ps;
    ps.type = cro::PhysicsShape::Type::Sphere;
    ps.radius = 0.1f;
    ps.position.z = 0.4f;
    phys.addShape(ps);

    ps.type = cro::PhysicsShape::Type::Capsule;
    ps.length = 0.1f;
    ps.position.z = 0.14f;
    ps.orientation = cro::PhysicsShape::Orientation::Z;
    phys.addShape(ps);

    phys.setCollisionFlags(CollisionID::Wall);
    phys.setCollisionGroups(CollisionID::Player);


    //weapon entities
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/sprites/lasers.spt", m_resources.textures);
    auto laserSize = spriteSheet.getSprite("player_pulse").getSize();

    const glm::vec3 laserScale(0.002f);
    laserSize *= laserScale.x;
    laserSize /= 2.f;

    cro::PhysicsShape laserShape;
    laserShape.type = cro::PhysicsShape::Type::Box;
    laserShape.extent = { laserSize.x, laserSize.y, 0.001f };

    static const std::size_t laserCount = 12;
    for (auto i = 0u; i < laserCount; ++i)
    {
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setScale(laserScale);
        entity.getComponent<cro::Transform>().setPosition({ -100.f, 0.f, 0.f });
        entity.getComponent<cro::Transform>().setOrigin({ laserSize.x / laserScale.x, laserSize.y / laserScale.x, 0.f });
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("player_pulse");
        entity.addComponent<PlayerWeapon>().type = PlayerWeapon::Laser;
        entity.addComponent<cro::PhysicsObject>().addShape(laserShape);
        entity.getComponent<cro::PhysicsObject>().setCollisionGroups(CollisionID::Weapon);
        entity.getComponent<cro::PhysicsObject>().setCollisionFlags(CollisionID::Bounds | CollisionID::Wall | CollisionID::Npc);
    }

    static const std::size_t grenadeCount = 4;
    glm::vec3 grenadeScale(0.05f);
    cro::PhysicsShape grenadeShape;
    grenadeShape.type = cro::PhysicsShape::Type::Box;
    grenadeShape.extent = grenadeScale / 2.f;

    for (auto i = 0u; i < grenadeCount; ++i)
    {
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setScale(grenadeScale);
        entity.getComponent<cro::Transform>().setPosition({ -100.f, 0.f, 0.f });

        entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[GameModelID::Grenade].meshID),
                                        m_resources.materials.get(m_modelDefs[GameModelID::Grenade].materialIDs[0]));

        entity.addComponent<PlayerWeapon>().type = PlayerWeapon::Grenade;
        entity.addComponent<cro::PhysicsObject>().addShape(grenadeShape);
        entity.getComponent<cro::PhysicsObject>().setCollisionGroups(CollisionID::Weapon);
        entity.getComponent<cro::PhysicsObject>().setCollisionFlags(CollisionID::Bounds | CollisionID::Wall | CollisionID::Npc);
    }


    //3D camera
    auto ent = m_scene.createEntity();
    ent.addComponent<cro::Transform>().setPosition({ 0.f, 0.6f, 2.3f });
    ent.addComponent<cro::Camera>();
    ent.addComponent<cro::CommandTarget>().ID = CommandID::Camera;
    m_scene.getSystem<cro::ShadowMapRenderer>().setProjectionOffset({ 0.f, 1.4f, -2.3f });
    m_scene.getSunlight().setDirection({ -0.1f, -1.f, 0.f });

    cro::PhysicsShape boundsShape;
    boundsShape.type = cro::PhysicsShape::Type::Box;
    boundsShape.extent = { 0.01f, 0.5f, 0.5f };
    boundsShape.position.x = 1.2f;
    boundsShape.position.z = -ent.getComponent<cro::Transform>().getPosition().z;
    ent.addComponent<cro::PhysicsObject>().addShape(boundsShape);
    boundsShape.position.x = -boundsShape.position.x;
    ent.getComponent<cro::PhysicsObject>().addShape(boundsShape);
    boundsShape.extent = { 1.2f, 0.01f, 0.5f };
    boundsShape.position.x = 0.f;
    boundsShape.position.y = -0.63f;
    ent.getComponent<cro::PhysicsObject>().addShape(boundsShape);
    ent.getComponent<cro::PhysicsObject>().setCollisionGroups(CollisionID::Bounds);
    ent.getComponent<cro::PhysicsObject>().setCollisionFlags(CollisionID::Weapon);

    m_scene.setActiveCamera(ent);



    //auto& catText = entity.addComponent<cro::Text>(font);
    //catText.setColour(cro::Colour::Red());
    //catText.setString("Hello, my name is Charles");
}

namespace
{
    const cro::FloatRect buttonArea(0.f, 0.f, 64.f, 64.f);
}

void GameState::createUI()
{
    //2D camera
    auto ent = m_overlayScene.createEntity();
    ent.addComponent<cro::Transform>();
    auto& cam2D = ent.addComponent<cro::Camera>();
    cam2D.projection = glm::ortho(0.f, /*static_cast<float>(cro::DefaultSceneSize.x)*/1280.f, 0.f, /*static_cast<float>(cro::DefaultSceneSize.y)*/720.f, -0.1f, 100.f);
    m_overlayScene.setActiveCamera(ent);

    //preview shadow map
    ent = m_overlayScene.createEntity();
    ent.addComponent<cro::Transform>().setPosition({ 20.f, 20.f, 0.f });
    ent.addComponent<cro::Sprite>().setTexture(m_shadowMap.getTexture());

#ifdef PLATFORM_MOBILE
    m_resources.textures.get("assets/ui/ui_buttons.png", false).setSmooth(true);
    auto mouseEnter = uiSystem->addCallback(
        [](cro::Entity entity, cro::uint64)
    {
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Blue());
    });

    auto mouseExit = uiSystem->addCallback(
        [](cro::Entity entity, cro::uint64)
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
#else
    ent = m_overlayScene.createEntity();
    auto& font = m_resources.fonts.get(0);
    font.loadFromFile("assets/VeraMono.ttf");
    auto& text = ent.addComponent<cro::Text>(font);
    text.setColour(cro::Colour::Blue());
    text.setString("WASD to move");
    text.setCharSize(60);
    ent.addComponent<cro::Transform>().setPosition({ 50.f, 700.f, 0.f });

#endif //PLATFORM_MOBILE
}

void GameState::updateView()
{
    glm::vec2 size(cro::App::getWindow().getSize());
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

    auto& cam3D = m_scene.getActiveCamera().getComponent<cro::Camera>();
    cam3D.projection = glm::perspective(0.6f, 16.f / 9.f, 0.1f, 100.f);
    cam3D.viewport.bottom = (1.f - size.y) / 2.f;
    cam3D.viewport.height = size.y;

    auto& cam2D = m_overlayScene.getActiveCamera().getComponent<cro::Camera>();
    cam2D.viewport = cam3D.viewport;
}