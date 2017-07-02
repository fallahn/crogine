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
#include "BackgroundShader.hpp"
#include "BackgroundSystem.hpp"
#include "PostRadial.hpp"
#include "RotateSystem.hpp"
#include "TerrainChunk.hpp"
#include "ChunkBuilder.hpp"
#include "Messages.hpp"
#include "RockFallSystem.hpp"
#include "RandomTranslation.hpp"
#include "VelocitySystem.hpp"
#include "PlayerDirector.hpp"
#include "BackgroundDirector.hpp"
#include "ItemSystem.hpp"
#include "ItemsDirector.hpp"
#include "NPCSystem.hpp"
#include "NpcDirector.hpp"
#include "PlayerSystem.hpp"
#include "PlayerWeaponsSystem.hpp"
#include "NpcWeaponSystem.hpp"
#include "ExplosionSystem.hpp"
#include "HudItems.hpp"

#include <crogine/core/App.hpp>
#include <crogine/core/Clock.hpp>

#include <crogine/graphics/QuadBuilder.hpp>
#include <crogine/graphics/StaticMeshBuilder.hpp>
#include <crogine/graphics/IqmBuilder.hpp>
#include <crogine/graphics/SpriteSheet.hpp>

#include <crogine/ecs/systems/SceneGraph.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/ParticleSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/CollisionSystem.hpp>
#include <crogine/ecs/systems/SpriteRenderer.hpp>
#include <crogine/ecs/systems/SpriteAnimator.hpp>
#include <crogine/ecs/systems/TextRenderer.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>
#include <crogine/ecs/components/CommandID.hpp>
#include <crogine/ecs/components/PhysicsObject.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/Text.hpp>

#include <crogine/util/Random.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/graphics/postprocess/PostChromeAB.hpp>

namespace
{
    const glm::vec2 backgroundSize(21.3f, 7.2f);
    std::size_t rockfallCount = 2;
}

GameState::GameState(cro::StateStack& stack, cro::State::Context context)
    : cro::State        (stack, context),
    m_scene             (context.appInstance.getMessageBus()),
    m_uiScene           (context.appInstance.getMessageBus())
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createHUD();
    });
    //context.appInstance.setClearColour(cro::Colour::White());

    updateView();

    auto* msg = getContext().appInstance.getMessageBus().post<GameEvent>(MessageID::GameMessage);
    msg->type = GameEvent::RoundStart;

    context.appInstance.resetFrameTime();
}

//public
bool GameState::handleEvent(const cro::Event& evt)
{
    if (evt.type == SDL_KEYUP)
    {

    }

    m_scene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void GameState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);

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
    m_uiScene.simulate(dt);
    return true;
}

void GameState::render()
{
    m_scene.render();
    m_uiScene.render();
}

//private
void GameState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_scene.addSystem<BackgroundSystem>(mb);
    m_scene.addSystem<ChunkSystem>(mb);
    m_scene.addSystem<RockFallSystem>(mb);
    m_scene.addSystem<RotateSystem>(mb);   
    m_scene.addSystem<Translator>(mb);   
    m_scene.addSystem<VelocitySystem>(mb);
    m_scene.addSystem<ItemSystem>(mb);
    m_scene.addSystem<NpcSystem>(mb);
    m_scene.addSystem<PlayerSystem>(mb);
    m_scene.addSystem<PlayerWeaponSystem>(mb);
    m_scene.addSystem<NpcWeaponSystem>(mb);
    m_scene.addSystem<ExplosionSystem>(mb);
    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<cro::SkeletalAnimator>(mb);
    m_scene.addSystem<cro::SpriteAnimator>(mb);
    m_scene.addSystem<cro::SceneGraph>(mb);
    m_scene.addSystem<cro::CollisionSystem>(mb);
    m_scene.addSystem<cro::ModelRenderer>(mb);
    m_scene.addSystem<cro::ParticleSystem>(mb);
    m_scene.addSystem<cro::SpriteRenderer>(mb);

    m_scene.addDirector<PlayerDirector>();
    m_scene.addDirector<BackgroundDirector>();
    m_scene.addDirector<ItemDirector>();
    m_scene.addDirector<NpcDirector>();
#ifdef PLATFORM_DESKTOP
    //m_scene.addPostProcess<cro::PostChromeAB>();
    m_scene.addPostProcess<PostRadial>();
#endif

    //UI scene
    m_uiScene.addSystem<HudSystem>(mb);
    m_uiScene.addSystem<cro::SceneGraph>(mb);
    m_uiScene.addSystem<cro::SpriteRenderer>(mb);
    m_uiScene.addSystem<cro::TextRenderer>(mb);
}

void GameState::loadAssets()
{
    m_resources.shaders.preloadFromString(Shaders::Background::Vertex, Shaders::Background::Fragment, ShaderID::Background);
    auto& farTexture = m_resources.textures.get("assets/materials/background_far.png");
    farTexture.setRepeated(true);
    farTexture.setSmooth(true);
    auto& farMaterial = m_resources.materials.add(MaterialID::GameBackgroundFar, m_resources.shaders.get(ShaderID::Background));
    farMaterial.setProperty("u_diffuseMap", farTexture);
    farMaterial.blendMode = cro::Material::BlendMode::Alpha;

    auto& midTexture = m_resources.textures.get("assets/materials/background_mid.png");
    midTexture.setRepeated(true);
    midTexture.setSmooth(true);
    auto& midMaterial = m_resources.materials.add(MaterialID::GameBackgroundMid, m_resources.shaders.get(ShaderID::Background));
    midMaterial.setProperty("u_diffuseMap", midTexture);
    midMaterial.blendMode = cro::Material::BlendMode::Alpha;

    auto& nearTexture = m_resources.textures.get("assets/materials/background_near.png");
    nearTexture.setRepeated(true);
    nearTexture.setSmooth(true);
    auto& nearMaterial = m_resources.materials.add(MaterialID::GameBackgroundNear, m_resources.shaders.get(ShaderID::Background));
    nearMaterial.setProperty("u_diffuseMap", nearTexture);
    nearMaterial.blendMode = cro::Material::BlendMode::Alpha;

    cro::QuadBuilder qb(backgroundSize);
    m_resources.meshes.loadMesh(MeshID::GameBackground, qb);


    m_modelDefs[GameModelID::Player].loadFromFile("assets/models/player.cmt", m_resources);
    m_modelDefs[GameModelID::PlayerShield].loadFromFile("assets/models/player_shield.cmt", m_resources);
    m_modelDefs[GameModelID::CollectableBatt].loadFromFile("assets/models/collectable_batt.cmt", m_resources);
    m_modelDefs[GameModelID::CollectableBomb].loadFromFile("assets/models/collectable_bomb.cmt", m_resources);
    m_modelDefs[GameModelID::CollectableBot].loadFromFile("assets/models/collectable_bot.cmt", m_resources);
    m_modelDefs[GameModelID::CollectableHeart].loadFromFile("assets/models/collectable_heart.cmt", m_resources);
    m_modelDefs[GameModelID::CollectableShield].loadFromFile("assets/models/collectable_shield.cmt", m_resources);
    m_modelDefs[GameModelID::CollectableWeaponUpgrade].loadFromFile("assets/models/weapon_upgrade.cmt", m_resources);
    m_modelDefs[GameModelID::Elite].loadFromFile("assets/models/elite.cmt", m_resources);
    m_modelDefs[GameModelID::TurretBase].loadFromFile("assets/models/turret_base.cmt", m_resources);
    m_modelDefs[GameModelID::TurretCannon].loadFromFile("assets/models/turret_cannon.cmt", m_resources);
    m_modelDefs[GameModelID::Choppa].loadFromFile("assets/models/choppa.cmt", m_resources);
    m_modelDefs[GameModelID::Speedray].loadFromFile("assets/models/speed.cmt", m_resources);
    m_modelDefs[GameModelID::WeaverHead].loadFromFile("assets/models/weaver_head.cmt", m_resources);
    m_modelDefs[GameModelID::WeaverBody].loadFromFile("assets/models/weaver_body.cmt", m_resources);


    auto shaderID = m_resources.shaders.preloadBuiltIn(cro::ShaderResource::BuiltIn::Unlit, cro::ShaderResource::VertexColour);
    m_resources.materials.add(MaterialID::TerrainChunk, m_resources.shaders.get(shaderID));

    ChunkBuilder chunkBuilder;
    m_resources.meshes.loadMesh(MeshID::TerrainChunkA, chunkBuilder);
    m_resources.meshes.loadMesh(MeshID::TerrainChunkB, chunkBuilder);


    shaderID = m_resources.shaders.preloadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::DiffuseMap | cro::ShaderResource::Subrects);
    for (auto i = 0u; i < rockfallCount; ++i)
    {
        auto& rockMat = m_resources.materials.add(MaterialID::Rockfall + i, m_resources.shaders.get(shaderID));
        rockMat.setProperty("u_diffuseMap", m_resources.textures.get("assets/materials/npc/tites.png"));
        rockMat.setProperty("u_subrect", glm::vec4(0.f, 0.f, 0.25f, 1.f));
        rockMat.blendMode = cro::Material::BlendMode::Alpha;
    }
    cro::QuadBuilder rockQuad({ 1.f, 1.f });
    m_resources.meshes.loadMesh(MeshID::RockQuad, rockQuad);
}

namespace
{
    //temporarily hold values shared between load functions
    cro::Entity chunkEntityA(0, 0);
    cro::Entity chunkEntityB(0, 0);
    cro::Entity playerEntity(0, 0);
    cro::Entity weaponEntity(0, 0);
}

void GameState::createScene()
{
    loadTerrain();
    loadModels();
    loadParticles();
    loadWeapons();

    //3D camera
    auto ent = m_scene.createEntity();
    ent.addComponent<cro::Transform>();
    ent.addComponent<cro::Camera>();
    m_scene.setActiveCamera(ent);
}

namespace
{
    const float UIPadding = 20.f;
    const float UISpacing = 6.f;

#ifdef PLATFORM_DESKTOP
    const glm::vec2 uiRes(1920.f, 1080.f);
    //const glm::vec2 uiRes(1280.f, 720.f);
#else
    const glm::vec2 uiRes(1280.f, 720.f);
#endif
}

void GameState::createHUD()
{
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/sprites/hud.spt", m_resources.textures);

    //health bar
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("bar_outside");
    entity.addComponent<cro::Transform>().setPosition({ uiRes.x - (entity.getComponent<cro::Sprite>().getSize().x + UIPadding), UIPadding * 1.5f, 0.f });
    
    auto innerEntity = m_uiScene.createEntity();
    innerEntity.addComponent<cro::Transform>().setParent(entity);
    innerEntity.addComponent<cro::Sprite>() = spriteSheet.getSprite("bar_inside");
    innerEntity.addComponent<HudItem>().type = HudItem::Type::HealthBar;

    //lives
    glm::vec3 startPoint(UIPadding, UIPadding, 0.f);
    for (auto i = 0u; i < 5; ++i)
    {
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(startPoint);
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("life");
        startPoint.x += entity.getComponent<cro::Sprite>().getSize().x + UISpacing;
        entity.addComponent<HudItem>().type = HudItem::Type::Life;
        entity.getComponent<HudItem>().value = static_cast<float>(i);
        if (i > 2)
        {
            entity.getComponent<cro::Sprite>().setColour(cro::Colour::White());
        }
        else
        {
            entity.getComponent<HudItem>().active = true;
        }
    }

    //bonus weapons
    startPoint.x = uiRes.x - (((spriteSheet.getSprite("bomb").getSize().x + UISpacing) * 2.f) + UIPadding);
    startPoint.y = uiRes.y - (spriteSheet.getSprite("bomb").getSize().y + UIPadding);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(startPoint);
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("bomb");
    entity.addComponent<HudItem>().type = HudItem::Type::Bomb;

    startPoint.x += entity.getComponent<cro::Sprite>().getSize().x + UISpacing;
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(startPoint);
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("emp");
    entity.addComponent<HudItem>().type = HudItem::Type::Emp;

#ifdef PLATFORM_MOBILE
    //on mobile add a pause icon
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("paws");
    entity.addComponent<cro::Transform>().setPosition({ (uiRes.x - entity.getComponent<cro::Sprite>().getSize()).x / 2.f, UIPadding, 0.f });
    entity.addComponent<HudItem>().type = HudItem::Type::Paws;
#endif

    //score text
    auto& scoreFont = m_resources.fonts.get(0);
    scoreFont.loadFromFile("assets/fonts/Audiowide-Regular.ttf");
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ UIPadding, uiRes.y - UIPadding, 0.f });
    entity.addComponent<cro::Text>(scoreFont);
    entity.getComponent<cro::Text>().setString("0000000000");
    entity.getComponent<cro::Text>().setCharSize(50);
    entity.getComponent<cro::Text>().setColour(cro::Colour::Cyan());
    entity.addComponent<HudItem>().type = HudItem::Type::Score;
    entity.getComponent<HudItem>().value = 0.f;

    //camera
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    auto& cam2D = entity.addComponent<cro::Camera>();
    cam2D.projection = glm::ortho(0.f, uiRes.x, 0.f, uiRes.y, -0.1f, 100.f);
    m_uiScene.setActiveCamera(entity);
}

void GameState::loadTerrain()
{
    //background layers
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -18.f });
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(MeshID::GameBackground), m_resources.materials.get(MaterialID::GameBackgroundFar));

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -14.f });
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(MeshID::GameBackground), m_resources.materials.get(MaterialID::GameBackgroundMid));

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -11.f });
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(MeshID::GameBackground), m_resources.materials.get(MaterialID::GameBackgroundNear));
    entity.addComponent<BackgroundComponent>();

    //terrain chunks
    cro::PhysicsShape boundsShape;
    boundsShape.type = cro::PhysicsShape::Type::Box;
    boundsShape.extent = { 10.65f, 0.6f, 1.6f };
    boundsShape.position = { 0.f, 2.85f, 0.f };

    entity = m_scene.createEntity();
    auto& chunkTxA = entity.addComponent<cro::Transform>();
    chunkTxA.setPosition({ 0.f, 0.f, -8.8f });
    //chunkTxA.setScale({ 0.5f, 0.5f, 1.f });
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(MeshID::TerrainChunkA), m_resources.materials.get(MaterialID::TerrainChunk));
    entity.addComponent<TerrainChunk>();
    entity.addComponent<cro::PhysicsObject>().addShape(boundsShape);
    boundsShape.position.y = -2.85f;
    entity.getComponent<cro::PhysicsObject>().addShape(boundsShape);
    entity.getComponent<cro::PhysicsObject>().setCollisionGroups(CollisionID::Environment);
    entity.getComponent<cro::PhysicsObject>().setCollisionFlags(CollisionID::Player | CollisionID::NpcLaser);

    chunkEntityA = entity; //keep this so we can attach turrets to it

    entity = m_scene.createEntity();
    auto& chunkTxB = entity.addComponent<cro::Transform>();
    chunkTxB.setPosition({ backgroundSize.x, 0.f, -8.8f });
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(MeshID::TerrainChunkB), m_resources.materials.get(MaterialID::TerrainChunk));
    entity.addComponent<TerrainChunk>();
    entity.addComponent<cro::PhysicsObject>().addShape(boundsShape);
    boundsShape.position.y = 2.85f;
    entity.getComponent<cro::PhysicsObject>().addShape(boundsShape);
    entity.getComponent<cro::PhysicsObject>().setCollisionGroups(CollisionID::Environment);
    entity.getComponent<cro::PhysicsObject>().setCollisionFlags(CollisionID::Player | CollisionID::NpcLaser);

    chunkEntityB = entity;

    //left/right bounds
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -9.3f });
    entity.addComponent<cro::PhysicsObject>().setCollisionGroups(CollisionID::Bounds);
    entity.getComponent<cro::PhysicsObject>().setCollisionFlags(CollisionID::Player | CollisionID::PlayerLaser | CollisionID::NpcLaser);
    boundsShape.extent = { 1.25f, 4.f, 1.5f };
    boundsShape.position.y = 0.f;
    boundsShape.position.x = -6.25f;
    entity.getComponent<cro::PhysicsObject>().addShape(boundsShape);
    boundsShape.position.x = 6.25f;
    entity.getComponent<cro::PhysicsObject>().addShape(boundsShape);

    //some rockfall parts
    glm::vec3 rockscale(0.6f, 1.2f, 1.f);
    auto bb = m_resources.meshes.getMesh(m_modelDefs[MeshID::RockQuad].meshID).boundingBox;
    cro::PhysicsShape ps;
    ps.type = cro::PhysicsShape::Type::Box;
    ps.extent = (bb[1] - bb[0]) * rockscale / 2.f;
    ps.extent.z = 0.1f;

    for (auto i = 0u; i < rockfallCount; ++i)
    {
        entity = m_scene.createEntity();
        entity.addComponent<RockFall>();
        auto& tx = entity.addComponent<cro::Transform>();
        tx.setScale(rockscale);
        tx.setPosition({ 0.f, 3.4f, -9.1f });
        entity.addComponent<cro::PhysicsObject>().addShape(ps);
        entity.getComponent<cro::PhysicsObject>().setCollisionGroups(CollisionID::Environment);
        entity.getComponent<cro::PhysicsObject>().setCollisionFlags(CollisionID::Player | CollisionID::PlayerLaser);
        entity.addComponent<cro::Model>(m_resources.meshes.getMesh(MeshID::RockQuad), m_resources.materials.get(MaterialID::Rockfall + i));
    }
}

void GameState::loadModels()
{
    //player ship
    glm::vec3 playerScale(0.4f);
    auto entity = m_scene.createEntity();
    auto& playerTx = entity.addComponent<cro::Transform>();
    playerTx.setPosition({ -35.4f, 0.f, -9.3f });
    playerTx.setScale(playerScale);
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[GameModelID::Player].meshID),
        m_resources.materials.get(m_modelDefs[GameModelID::Player].materialIDs[0]));
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Player;
    entity.addComponent<Velocity>().friction = 2.5f;
    auto bb = m_resources.meshes.getMesh(m_modelDefs[GameModelID::Player].meshID).boundingBox;
    cro::PhysicsShape shipShape;
    shipShape.type = cro::PhysicsShape::Type::Box;
    shipShape.extent = (bb[1] - bb[0]) * playerScale / 2.f;
    entity.addComponent<cro::PhysicsObject>().addShape(shipShape);
    entity.getComponent<cro::PhysicsObject>().setCollisionGroups(CollisionID::Player);
    entity.getComponent<cro::PhysicsObject>().setCollisionFlags(CollisionID::Collectable | CollisionID::Environment | CollisionID::NPC | CollisionID::Bounds);
    entity.addComponent<PlayerInfo>();
    
    cro::EmitterSettings smokeEmitter;
    smokeEmitter.loadFromFile("assets/particles/smoke.cps", m_resources.textures);
    entity.addComponent<cro::ParticleEmitter>().emitterSettings = smokeEmitter;
    //entity.getComponent<cro::ParticleEmitter>().start();
    
    playerEntity = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[GameModelID::PlayerShield].meshID),
        m_resources.materials.get(m_modelDefs[GameModelID::PlayerShield].materialIDs[0]));
    entity.addComponent<cro::Transform>().setParent(playerEntity);
    entity.addComponent<Rotator>().speed = 1.f;
    entity.getComponent<Rotator>().axis.z = 1.f;
    playerEntity.getComponent<PlayerInfo>().shieldEntity = entity.getIndex();

    //collectables
    static const glm::vec3 coinScale(0.12f);
    cro::PhysicsShape coinShape;
    coinShape.type = cro::PhysicsShape::Type::Box;
    bb = m_resources.meshes.getMesh(m_modelDefs[GameModelID::CollectableBatt].meshID).boundingBox;
    coinShape.extent = (bb[1] - bb[0]) * coinScale / 2.f;
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 5.41f, 1.4f, -9.3f });
    entity.getComponent<cro::Transform>().setScale(coinScale);
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[GameModelID::CollectableBatt].meshID),
        m_resources.materials.get(m_modelDefs[GameModelID::CollectableBatt].materialIDs[0]));
    auto& battSpin = entity.addComponent<Rotator>();
    battSpin.axis.y = 1.f;
    battSpin.speed = 3.2f;
    entity.addComponent<CollectableItem>().type = CollectableItem::EMP;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Collectable;
    entity.addComponent<cro::PhysicsObject>().addShape(coinShape);
    entity.getComponent<cro::PhysicsObject>().setCollisionGroups(CollisionID::Collectable);
    entity.getComponent<cro::PhysicsObject>().setCollisionFlags(CollisionID::Player | CollisionID::PlayerLaser);

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 5.42f, 0.6f, -9.3f });
    entity.getComponent<cro::Transform>().setScale(coinScale);
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[GameModelID::CollectableBomb].meshID),
        m_resources.materials.get(m_modelDefs[GameModelID::CollectableBomb].materialIDs[0]));
    auto& bombSpin = entity.addComponent<Rotator>();
    bombSpin.axis.y = 1.f;
    bombSpin.speed = 2.9f;
    entity.addComponent<CollectableItem>().type = CollectableItem::Bomb;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Collectable;
    entity.addComponent<cro::PhysicsObject>().addShape(coinShape);
    entity.getComponent<cro::PhysicsObject>().setCollisionGroups(CollisionID::Collectable);
    entity.getComponent<cro::PhysicsObject>().setCollisionFlags(CollisionID::Player | CollisionID::PlayerLaser);

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 5.6f, -0.2f, -9.3f });
    entity.getComponent<cro::Transform>().setScale(coinScale);
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[GameModelID::CollectableBot].meshID),
        m_resources.materials.get(m_modelDefs[GameModelID::CollectableBot].materialIDs[0]));
    auto& botSpin = entity.addComponent<Rotator>();
    botSpin.axis.y = 1.f;
    botSpin.speed = 2.994f;
    entity.addComponent<CollectableItem>().type = CollectableItem::Buddy;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Collectable;
    entity.addComponent<cro::PhysicsObject>().addShape(coinShape);
    entity.getComponent<cro::PhysicsObject>().setCollisionGroups(CollisionID::Collectable);
    entity.getComponent<cro::PhysicsObject>().setCollisionFlags(CollisionID::Player | CollisionID::PlayerLaser);

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 5.38f, -1.f, -9.3f });
    entity.getComponent<cro::Transform>().setScale(coinScale);
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[GameModelID::CollectableHeart].meshID),
        m_resources.materials.get(m_modelDefs[GameModelID::CollectableHeart].materialIDs[0]));
    auto& heartSpin = entity.addComponent<Rotator>();
    heartSpin.axis.y = 1.f;
    heartSpin.speed = 2.873f;
    entity.addComponent<CollectableItem>().type = CollectableItem::Life;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Collectable;
    entity.addComponent<cro::PhysicsObject>().addShape(coinShape);
    entity.getComponent<cro::PhysicsObject>().setCollisionGroups(CollisionID::Collectable);
    entity.getComponent<cro::PhysicsObject>().setCollisionFlags(CollisionID::Player | CollisionID::PlayerLaser);

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 5.4f, -1.7f, -9.3f });
    entity.getComponent<cro::Transform>().setScale(coinScale);
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[GameModelID::CollectableShield].meshID),
        m_resources.materials.get(m_modelDefs[GameModelID::CollectableShield].materialIDs[0]));
    auto& shieldSpin = entity.addComponent<Rotator>();
    shieldSpin.axis.y = 1.f;
    shieldSpin.speed = 3.028f;
    entity.addComponent<CollectableItem>().type = CollectableItem::Shield;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Collectable;
    entity.addComponent<cro::PhysicsObject>().addShape(coinShape);
    entity.getComponent<cro::PhysicsObject>().setCollisionGroups(CollisionID::Collectable);
    entity.getComponent<cro::PhysicsObject>().setCollisionFlags(CollisionID::Player | CollisionID::PlayerLaser);

    entity = m_scene.createEntity();
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[GameModelID::CollectableWeaponUpgrade].meshID),
        m_resources.materials.get(m_modelDefs[GameModelID::CollectableWeaponUpgrade].materialIDs[0]));

    entity.addComponent<cro::Transform>().setPosition({ 7.f, 0.f, -9.3f });
    entity.addComponent<CollectableItem>().type = CollectableItem::WeaponUpgrade;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Collectable;
    //auto bounds = m_resources.meshes.getMesh(m_modelDefs[GameModelID::CollectableWeaponUpgrade].meshID).boundingBox;
    coinShape.extent = { 0.16f, 0.16f, 0.16f };// (bounds[1] - bounds[0]) / 2.f;
    entity.addComponent<cro::PhysicsObject>().addShape(coinShape);
    entity.getComponent<cro::PhysicsObject>().setCollisionGroups(CollisionID::Collectable);
    entity.getComponent<cro::PhysicsObject>().setCollisionFlags(CollisionID::Player);

    //----NPCs----//
    //elite
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 5.9f, 1.5f, -9.3f });
    entity.getComponent<cro::Transform>().setScale(glm::vec3(0.6f));
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[GameModelID::Elite].meshID),
        m_resources.materials.get(m_modelDefs[GameModelID::Elite].materialIDs[0]));
    entity.addComponent<Npc>().type = Npc::Elite;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Elite;
    bb = m_resources.meshes.getMesh(m_modelDefs[GameModelID::Elite].meshID).boundingBox;
    
    cro::PhysicsShape eliteShape;
    eliteShape.type = cro::PhysicsShape::Type::Box;
    eliteShape.extent = (bb[1] - bb[0]) * entity.getComponent<cro::Transform>().getScale() / 2.f;
    entity.addComponent<cro::PhysicsObject>().addShape(eliteShape);
    entity.getComponent<cro::PhysicsObject>().setCollisionGroups(CollisionID::NPC);
    entity.getComponent<cro::PhysicsObject>().setCollisionFlags(CollisionID::Player | CollisionID::PlayerLaser);
    entity.addComponent<cro::ParticleEmitter>().emitterSettings = smokeEmitter;

    weaponEntity = m_scene.createEntity(); //weapon sprites are added below
    weaponEntity.addComponent<cro::Transform>().setParent(entity);
    weaponEntity.getComponent<cro::Transform>().move({ -0.6f, -0.1f, 0.f });
    weaponEntity.addComponent<NpcWeapon>().type = NpcWeapon::Laser;
    weaponEntity.getComponent<NpcWeapon>().damage = 1.f;

    //choppa
    cro::EmitterSettings flames;
    flames.loadFromFile("assets/particles/flames.cps", m_resources.textures);
    const float choppaSpacing = ChoppaNavigator::spacing;
    glm::vec3 choppaScale(0.6f);
    bb = m_resources.meshes.getMesh(m_modelDefs[GameModelID::Choppa].meshID).boundingBox;
    cro::PhysicsShape choppaShape;
    choppaShape.type = cro::PhysicsShape::Type::Box;
    choppaShape.extent = (bb[1] - bb[0]) * choppaScale / 2.f;
    for (auto i = 0; i < 3; ++i)
    {
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 5.9f, -choppaSpacing + (i * choppaSpacing), -9.3f });
        entity.getComponent<cro::Transform>().setRotation({ -cro::Util::Const::PI / 2.f, 0.f, 0.f });
        entity.getComponent<cro::Transform>().setScale(choppaScale);
        entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[GameModelID::Choppa].meshID),
            m_resources.materials.get(m_modelDefs[GameModelID::Choppa].materialIDs[0]));
        CRO_ASSERT(m_modelDefs[GameModelID::Choppa].skeleton, "Skeleton missing from choppa!");
        entity.addComponent<cro::Skeleton>() = *m_modelDefs[GameModelID::Choppa].skeleton;
        entity.getComponent<cro::Skeleton>().play(0);
        entity.addComponent<Npc>().type = Npc::Choppa;
        entity.getComponent<Npc>().choppa.ident = i;
        entity.getComponent<Npc>().choppa.shootTime = cro::Util::Random::value(0.1f, 1.f);
        entity.addComponent<cro::CommandTarget>().ID = CommandID::Choppa;
        entity.addComponent<cro::PhysicsObject>().addShape(choppaShape);
        entity.getComponent<cro::PhysicsObject>().setCollisionGroups(CollisionID::NPC);
        entity.getComponent<cro::PhysicsObject>().setCollisionFlags(CollisionID::Player | CollisionID::PlayerLaser);
        entity.addComponent<cro::ParticleEmitter>().emitterSettings = flames;
    }

    //speedray
    bb = m_resources.meshes.getMesh(m_modelDefs[GameModelID::Speedray].meshID).boundingBox;
    glm::vec3 speedrayScale(0.8f);
    cro::PhysicsShape speedrayShape;
    speedrayShape.type = cro::PhysicsShape::Type::Box;
    speedrayShape.extent = (bb[1] - bb[0]) * speedrayScale / 2.f;
    for (auto i = 0u; i < SpeedrayNavigator::count; ++i)
    {
        const float rotateOffset = (cro::Util::Const::PI / SpeedrayNavigator::count) * i;

        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 6.f, 0.f, -9.3f });
        entity.getComponent<cro::Transform>().rotate(glm::vec3(1.f, 0.f, 0.f), cro::Util::Const::PI - rotateOffset);
        entity.getComponent<cro::Transform>().setScale(speedrayScale);
        entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[GameModelID::Speedray].meshID),
            m_resources.materials.get(m_modelDefs[GameModelID::Speedray].materialIDs[0]));
        entity.getComponent<cro::Model>().setMaterial(1, m_resources.materials.get(m_modelDefs[GameModelID::Speedray].materialIDs[1]));
        auto& speedRot = entity.addComponent<Rotator>();
        speedRot.axis.x = 1.f;
        speedRot.speed = -cro::Util::Const::TAU / 2.f;
        entity.addComponent<Npc>().type = Npc::Speedray;
        entity.getComponent<Npc>().speedray.ident = i;
        entity.addComponent<cro::CommandTarget>().ID = CommandID::Speedray;
        entity.addComponent<cro::PhysicsObject>().addShape(speedrayShape);
        entity.getComponent<cro::PhysicsObject>().setCollisionGroups(CollisionID::NPC);
        entity.getComponent<cro::PhysicsObject>().setCollisionFlags(CollisionID::Player | CollisionID::PlayerLaser);
    }

    //attach turret to each of the terrain chunks
    cro::PhysicsShape turrShape;
    turrShape.type = cro::PhysicsShape::Type::Sphere;
    turrShape.radius = 0.01f;
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 10.f, 0.f, 0.f }); //places off screen to start
    entity.getComponent<cro::Transform>().setScale({ 0.3f, 0.3f, 0.3f });
    entity.getComponent<cro::Transform>().setParent(chunkEntityA); //attach to scenery
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[GameModelID::TurretBase].meshID),
        m_resources.materials.get(m_modelDefs[GameModelID::TurretBase].materialIDs[0]));
    entity.addComponent<cro::CommandTarget>().ID = CommandID::TurretA;

    auto canEnt = m_scene.createEntity();
    canEnt.addComponent<cro::Transform>().setParent(entity);
    canEnt.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[GameModelID::TurretCannon].meshID),
        m_resources.materials.get(m_modelDefs[GameModelID::TurretCannon].materialIDs[0]));
    canEnt.addComponent<Npc>().type = Npc::Turret;
    canEnt.addComponent<cro::PhysicsObject>().addShape(turrShape);

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 10.f, 0.f, 0.f });
    entity.getComponent<cro::Transform>().setScale({ 0.3f, 0.3f, 0.3f });
    entity.getComponent<cro::Transform>().setParent(chunkEntityB); //attach to scenery
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[GameModelID::TurretBase].meshID),
        m_resources.materials.get(m_modelDefs[GameModelID::TurretBase].materialIDs[0]));
    entity.addComponent<cro::CommandTarget>().ID = CommandID::TurretB;

    canEnt = m_scene.createEntity();
    canEnt.addComponent<cro::Transform>().setParent(entity);
    canEnt.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[GameModelID::TurretCannon].meshID),
        m_resources.materials.get(m_modelDefs[GameModelID::TurretCannon].materialIDs[0]));
    canEnt.addComponent<Npc>().type = Npc::Turret;
    canEnt.addComponent<cro::PhysicsObject>().addShape(turrShape);

    //weaver
    glm::vec3 weaverScale(0.35f);
    bb = m_resources.meshes.getMesh(m_modelDefs[GameModelID::WeaverBody].meshID).boundingBox;
    cro::PhysicsShape weaverShape;
    weaverShape.type = cro::PhysicsShape::Type::Box;
    for (auto i = 0u; i < WeaverNavigator::count; ++i)
    {
        cro::int32 modelID = (i == 0) ? GameModelID::WeaverHead : GameModelID::WeaverBody;

        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ (static_cast<float>(i) * (WeaverNavigator::spacing * weaverScale.x)) + 10.f, 0.f, -9.3f });
        entity.getComponent<cro::Transform>().setScale(weaverScale);
        entity.getComponent<cro::Transform>().rotate({ 1.f, 0.f, 0.f }, static_cast<float>(i) * 0.3f);
        entity.getComponent<cro::Transform>().rotate({ 0.f, 1.f, 0.f }, cro::Util::Const::PI / 2.f);
        entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[modelID].meshID),
            m_resources.materials.get(m_modelDefs[modelID].materialIDs[0]));
        entity.addComponent<Npc>().type = Npc::Weaver;
        entity.getComponent<Npc>().weaver.ident = WeaverNavigator::count - i;
        entity.addComponent<cro::CommandTarget>().ID = CommandID::Weaver;
        auto& rot = entity.addComponent<Rotator>();
        rot.axis.z = 1.f;
        rot.speed = 2.f;

        weaverShape.extent = ((bb[1] - bb[0]) / 2.f) * weaverScale;
        entity.addComponent<cro::PhysicsObject>().addShape(weaverShape);
        entity.getComponent<cro::PhysicsObject>().setCollisionGroups(CollisionID::NPC);
        entity.getComponent<cro::PhysicsObject>().setCollisionFlags(CollisionID::Player | CollisionID::PlayerLaser);

        weaverScale *= 0.92f;
    }
}

void GameState::loadParticles()
{
    //particle systems
    auto entity = m_scene.createEntity();
    auto& snowEmitter = entity.addComponent<cro::ParticleEmitter>();
    snowEmitter.emitterSettings.loadFromFile("assets/particles/snow.cps", m_resources.textures);
    snowEmitter.start();
    entity.addComponent<cro::Transform>();
    auto& translator = entity.addComponent<RandomTranslation>();
    for (auto& p : translator.translations)
    {
        p.x = cro::Util::Random::value(-3.5f, 12.1f);
        p.y = 3.1f;
        p.z = -9.2f;
    }
    entity.addComponent<cro::CommandTarget>().ID = CommandID::SnowParticles;

    //rock fragments from ceiling
    entity = m_scene.createEntity();
    auto& rockEmitter = entity.addComponent<cro::ParticleEmitter>();
    rockEmitter.emitterSettings.loadFromFile("assets/particles/rocks.cps", m_resources.textures);

    entity.addComponent<cro::Transform>();
    auto& rockTrans = entity.addComponent<RandomTranslation>();
    for (auto& p : rockTrans.translations)
    {
        p.x = cro::Util::Random::value(-4.2f, 4.2f);
        p.y = 3.f;
        p.z = -8.6f;
    }
    entity.addComponent<cro::CommandTarget>().ID = CommandID::RockParticles;

    //impact explosions
    cro::SpriteSheet explosion;
    auto loadExplosion = [&]()
    {
        for (auto i = 0u; i < 3u; ++i)
        {
            auto entity = m_scene.createEntity();
            entity.addComponent<cro::Sprite>() = explosion.getSprite("explosion");
            auto size = entity.getComponent<cro::Sprite>().getSize();
            entity.addComponent<cro::Transform>().setPosition({ 0.f, 160.f, -9.3f });
            entity.getComponent<cro::Transform>().setScale(glm::vec3(0.04f));
            entity.getComponent<cro::Transform>().setOrigin({ size.x / 2.f, size.y / 2.f, 0.f });
            entity.addComponent<cro::SpriteAnimaton>();
            entity.addComponent<Explosion>().ident = i;
        }
    };

    explosion.loadFromFile("assets/sprites/explosion01.spt", m_resources.textures);
    loadExplosion();
    explosion.loadFromFile("assets/sprites/explosion02.spt", m_resources.textures);
    loadExplosion();
    explosion.loadFromFile("assets/sprites/explosion03.spt", m_resources.textures);
    loadExplosion();
}

void GameState::loadWeapons()
{
    //weapon sprites
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/sprites/lasers.spt", m_resources.textures);
    glm::vec3 pulseScale(0.006f);
    const std::size_t maxPulses = 6;
    for (auto i = 0u; i < maxPulses; ++i)
    {
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("player_pulse");
        auto size = entity.getComponent<cro::Sprite>().getSize();

        entity.addComponent<cro::Transform>().setScale(pulseScale);
        entity.getComponent<cro::Transform>().setPosition(glm::vec3(-10.f));
        entity.getComponent<cro::Transform>().setOrigin({ size.x / 2.f, size.y / 2.f, 0.f });

        cro::PhysicsShape ps;
        ps.type = cro::PhysicsShape::Type::Box;
        ps.extent = { size.x * pulseScale.x, size.y * pulseScale.y, 0.2f };
        ps.extent /= 2.f;
        ps.extent *= glm::vec3(0.85f);

        entity.addComponent<cro::PhysicsObject>().setCollisionGroups(CollisionID::PlayerLaser);
        entity.getComponent<cro::PhysicsObject>().setCollisionFlags(CollisionID::Bounds | CollisionID::Collectable | CollisionID::NPC);
        entity.getComponent<cro::PhysicsObject>().addShape(ps);

        entity.addComponent<PlayerWeapon>();
    }

    //use a single laser we'll scale it to make it look bigger or smaller
    glm::vec3 laserScale(0.15f, 0.006f, 1.f);
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("player_laser");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent());
    auto size = entity.getComponent<cro::Sprite>().getSize();

    entity.addComponent<cro::Transform>().setScale(laserScale);
    entity.getComponent<cro::Transform>().setPosition(glm::vec3(-9.3f));
    //entity.getComponent<cro::Transform>().setOrigin({ 0.f, size.y / 2.f, 0.f });
    entity.getComponent<cro::Transform>().setParent(playerEntity);

    cro::PhysicsShape laserPhys;
    laserPhys.type = cro::PhysicsShape::Type::Box;
    laserPhys.extent = { size.x * laserScale.x, size.y * laserScale.y, 0.2f };
    laserPhys.extent /= 2.f;
    laserPhys.extent *= glm::vec3(0.85f);

    entity.addComponent<cro::PhysicsObject>().setCollisionGroups(CollisionID::PlayerLaser);
    entity.getComponent<cro::PhysicsObject>().setCollisionFlags(CollisionID::Collectable | CollisionID::NPC);
    entity.getComponent<cro::PhysicsObject>().addShape(laserPhys);

    entity.addComponent<PlayerWeapon>().type = PlayerWeapon::Type::Laser;


    //npc weapons - orbs
    cro::PhysicsShape ps;
    ps.type = cro::PhysicsShape::Type::Box;

    auto orbScale = glm::vec3(0.005f);
    static const std::size_t orbCount = 20;
    for (auto i = 0u; i < orbCount; ++i)
    {
        entity = m_scene.createEntity();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("npc_orb");
        size = entity.getComponent<cro::Sprite>().getSize();

        entity.addComponent<cro::Transform>().setPosition({ 0.f, 10.f, -8.f });
        entity.getComponent<cro::Transform>().setOrigin({ size.x / 2.f, size.y / 2.f, 0.f });
        entity.getComponent<cro::Transform>().setScale(orbScale);

        ps.extent = glm::vec3(size.x / 2.f, size.y / 2.f, 1.f) * orbScale;
        entity.addComponent<cro::PhysicsObject>().addShape(ps);
        entity.getComponent<cro::PhysicsObject>().setCollisionGroups(CollisionID::NpcLaser);
        entity.getComponent<cro::PhysicsObject>().setCollisionFlags(CollisionID::Bounds | CollisionID::Environment | CollisionID::Player);

        entity.addComponent<NpcWeapon>().type = NpcWeapon::Orb;
    }

    //elite laser is an orb and a beam attached to the weapon ent (see elite entity above)
    laserScale.y = 0.004f;
    auto laserEnt = m_scene.createEntity();
    laserEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("npc_laser");
    laserEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent());
    size = laserEnt.getComponent<cro::Sprite>().getSize();
    laserEnt.addComponent<cro::Transform>().setScale(laserScale);
    laserEnt.getComponent<cro::Transform>().setParent(weaponEntity);
    laserEnt.getComponent<cro::Transform>().rotate({ 0.f, 0.f, 1.f }, cro::Util::Const::PI);
    laserEnt.getComponent<cro::Transform>().setOrigin({ 0.f, size.y / 2.f, 0.f });
    laserEnt.getComponent<cro::Transform>().setPosition({ 0.f, -50.f, 0.f });
    //laserEnt.getComponent<cro::Transform>().setPosition({ 0.f, 0.f, -9.3f });

    //laserPhys.extent.x = -laserPhys.extent.x;
    cro::PhysicsShape laserShape;
    laserShape.type = cro::PhysicsShape::Type::Box;
    laserShape.extent = { size.x * laserScale.x, size.y * laserScale.y, 0.2f };
    laserShape.extent /= 2.f;
    laserShape.extent *= glm::vec3(0.85f);
    //laserShape.position.x = -laserShape.extent.x;
    laserEnt.addComponent<cro::PhysicsObject>().setCollisionGroups(CollisionID::NpcLaser);
    laserEnt.getComponent<cro::PhysicsObject>().setCollisionFlags(CollisionID::Player);
    laserEnt.getComponent<cro::PhysicsObject>().addShape(laserShape);
    
    auto orbEnt = m_scene.createEntity();
    orbEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("npc_orb");
    orbEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent());
    size = orbEnt.getComponent<cro::Sprite>().getSize();
    orbEnt.addComponent<cro::Transform>().setParent(weaponEntity);
    orbEnt.getComponent<cro::Transform>().setScale(glm::vec3(0.01f)); //approx orbScale * 1/eliteScale. So many magic numbers!!!
    orbEnt.getComponent<cro::Transform>().setOrigin({ size.x / 2.f, size.y / 2.f, 0.f });
    orbEnt.getComponent<cro::Transform>().setPosition({ 0.f, -50.f, 0.f });


    //choppa pulses
    static const std::size_t choppaPulseCount = 18;
    pulseScale *= 0.5f;
    for (auto i = 0u; i < choppaPulseCount; ++i)
    {
        entity = m_scene.createEntity();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("npc_pulse");
        auto size = entity.getComponent<cro::Sprite>().getSize();

        entity.addComponent<cro::Transform>().setScale(pulseScale);
        entity.getComponent<cro::Transform>().setPosition(glm::vec3(10.f));
        entity.getComponent<cro::Transform>().setOrigin({ size.x / 2.f, size.y / 2.f, 0.f });

        //cro::PhysicsShape cs;
        ps.type = cro::PhysicsShape::Type::Box;
        ps.extent = { size.x * pulseScale.x, size.y * pulseScale.y, 0.2f };
        ps.extent /= 2.f;
        ps.extent *= glm::vec3(0.85f);

        entity.addComponent<cro::PhysicsObject>().setCollisionGroups(CollisionID::NpcLaser);
        entity.getComponent<cro::PhysicsObject>().setCollisionFlags(CollisionID::Bounds | CollisionID::Player);
        entity.getComponent<cro::PhysicsObject>().addShape(ps);

        entity.addComponent<NpcWeapon>().type = NpcWeapon::Pulse;
    }
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

    auto& cam2D = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam2D.viewport = cam3D.viewport;
}