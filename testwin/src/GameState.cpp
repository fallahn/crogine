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
#include "BackgroundController.hpp"
#include "PostRadial.hpp"
#include "RotateSystem.hpp"
#include "TerrainChunk.hpp"
#include "ChunkBuilder.hpp"
#include "Messages.hpp"
#include "RockFallSystem.hpp"

#include <crogine/core/App.hpp>
#include <crogine/core/Clock.hpp>

#include <crogine/graphics/QuadBuilder.hpp>
#include <crogine/graphics/StaticMeshBuilder.hpp>

#include <crogine/ecs/systems/MeshSorter.hpp>
#include <crogine/ecs/systems/SceneGraph.hpp>
#include <crogine/ecs/systems/SceneRenderer.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Camera.hpp>

namespace
{
    BackgroundController* cnt = nullptr;
    const glm::vec2 backgroundSize(21.3f, 7.2f);
    std::size_t rockfallCount = 2;
}

GameState::GameState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_scene         (context.appInstance.getMessageBus()),
    m_commandSystem (nullptr)
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
    });
    //context.appInstance.setClearColour(cro::Colour::White());

    updateView();

    auto* msg = getContext().appInstance.getMessageBus().post<GameEvent>(MessageID::GameMessage);
    msg->type = GameEvent::RoundStart;
}

//public
bool GameState::handleEvent(const cro::Event& evt)
{
    if (evt.type == SDL_KEYUP)
    {
        if (evt.key.keysym.sym == SDLK_SPACE)
        {
            cnt->setMode(BackgroundController::Mode::Shake);
        }
        else if (evt.key.keysym.sym == SDLK_RETURN)
        {
            cnt->setMode(BackgroundController::Mode::Scroll);
            cnt->setScrollSpeed(0.2f);
        }
    }
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
    return true;
}

void GameState::render()
{
    m_scene.render();
}

//private
void GameState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::SceneGraph>(mb);
    auto& sceneRenderer = m_scene.addSystem<cro::SceneRenderer>(mb);
    m_scene.addSystem<cro::MeshSorter>(mb, sceneRenderer);
    cnt = &m_scene.addSystem<BackgroundController>(mb);
    cnt->setScrollSpeed(0.2f);
    m_scene.addSystem<ChunkSystem>(mb);
    m_scene.addSystem<RockFallSystem>(mb);
    m_scene.addSystem<RotateSystem>(mb);

    m_scene.addPostProcess<PostRadial>();
}

void GameState::loadAssets()
{
    m_shaderResource.preloadFromString(Shaders::Background::Vertex, Shaders::Background::Fragment, ShaderID::Background);
    auto& farTexture = m_textureResource.get("assets/materials/background_far.png");
    farTexture.setRepeated(true);
    farTexture.setSmooth(true);
    auto& farMaterial = m_materialResource.add(MaterialID::GameBackgroundFar, m_shaderResource.get(ShaderID::Background));
    farMaterial.setProperty("u_diffuseMap", farTexture);

    auto& midTexture = m_textureResource.get("assets/materials/background_mid.png");
    midTexture.setRepeated(true);
    midTexture.setSmooth(true);
    auto& midMaterial = m_materialResource.add(MaterialID::GameBackgroundMid, m_shaderResource.get(ShaderID::Background));
    midMaterial.setProperty("u_diffuseMap", midTexture);
    midMaterial.blendMode = cro::Material::BlendMode::Alpha;

    auto& nearTexture = m_textureResource.get("assets/materials/background_near.png");
    nearTexture.setRepeated(true);
    nearTexture.setSmooth(true);
    auto& nearMaterial = m_materialResource.add(MaterialID::GameBackgroundNear, m_shaderResource.get(ShaderID::Background));
    nearMaterial.setProperty("u_diffuseMap", nearTexture);
    nearMaterial.blendMode = cro::Material::BlendMode::Alpha;

    cro::QuadBuilder qb(backgroundSize);
    m_meshResource.loadMesh(qb, MeshID::GameBackground);

    auto shaderID = m_shaderResource.preloadBuiltIn(cro::ShaderResource::BuiltIn::VertexLit, cro::ShaderResource::DiffuseMap | cro::ShaderResource::NormalMap);
    auto& playerMaterial = m_materialResource.add(MaterialID::PlayerShip, m_shaderResource.get(shaderID));
    playerMaterial.setProperty("u_diffuseMap", m_textureResource.get("assets/materials/player_diffuse.png"));
    playerMaterial.setProperty("u_maskMap", m_textureResource.get("assets/materials/player_mask.png"));
    playerMaterial.setProperty("u_normalMap", m_textureResource.get("assets/materials/player_normal.png"));

    cro::StaticMeshBuilder playerMesh("assets/models/player_ship.cmf");
    m_meshResource.loadMesh(playerMesh, MeshID::PlayerShip);

    shaderID = m_shaderResource.preloadBuiltIn(cro::ShaderResource::BuiltIn::Unlit, cro::ShaderResource::VertexColour);
    m_materialResource.add(MaterialID::TerrainChunk, m_shaderResource.get(shaderID));

    ChunkBuilder chunkBuilder;
    m_meshResource.loadMesh(chunkBuilder, MeshID::TerrainChunkA);
    m_meshResource.loadMesh(chunkBuilder, MeshID::TerrainChunkB);


    shaderID = m_shaderResource.preloadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::DiffuseMap | cro::ShaderResource::Subrects);
    for (auto i = 0u; i < rockfallCount; ++i)
    {
        auto& rockMat = m_materialResource.add(MaterialID::Rockfall + i, m_shaderResource.get(shaderID));
        rockMat.setProperty("u_diffuseMap", m_textureResource.get("assets/materials/npc/tites.png"));
        rockMat.setProperty("u_subrect", glm::vec4(0.f, 0.f, 0.25f, 1.f));
        rockMat.blendMode = cro::Material::BlendMode::Alpha;
    }
    cro::QuadBuilder rockQuad({ 1.f, 1.f });
    m_meshResource.loadMesh(rockQuad, MeshID::RockQuad);
}

void GameState::createScene()
{
    //background layers
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -18.f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::GameBackground), m_materialResource.get(MaterialID::GameBackgroundFar));

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -14.f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::GameBackground), m_materialResource.get(MaterialID::GameBackgroundMid));

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -11.f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::GameBackground), m_materialResource.get(MaterialID::GameBackgroundNear));
    entity.addComponent<BackgroundComponent>();

    //terrain chunks
    entity = m_scene.createEntity();
    auto& chunkTxA = entity.addComponent<cro::Transform>();
    chunkTxA.setPosition({ 0.f, 0.f, -9.f });
    //chunkTxA.setScale({ 0.5f, 0.5f, 1.f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::TerrainChunkA), m_materialResource.get(MaterialID::TerrainChunk));
    entity.addComponent<TerrainChunk>();

    entity = m_scene.createEntity();
    auto& chunkTxB = entity.addComponent<cro::Transform>();
    chunkTxB.setPosition({ backgroundSize.x, 0.f, -9.f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::TerrainChunkB), m_materialResource.get(MaterialID::TerrainChunk));
    entity.addComponent<TerrainChunk>();

    //some rockfall parts
    for (auto i = 0u; i < rockfallCount; ++i)
    {
        entity = m_scene.createEntity();
        entity.addComponent<RockFall>();
        auto& tx = entity.addComponent<cro::Transform>();
        tx.setScale({ 0.6f, 1.2f, 1.f });
        tx.setPosition({ 0.f, 3.4f, -9.1f });

        entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::RockQuad), m_materialResource.get(MaterialID::Rockfall + i));
    }

    //player ship
    entity = m_scene.createEntity();
    auto& playerTx = entity.addComponent<cro::Transform>();
    playerTx.setPosition({ -3.4f, 0.f, -9.25f });
    playerTx.setScale({ 0.5f, 0.5f, 0.5f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::PlayerShip), m_materialResource.get(MaterialID::PlayerShip));

    auto& rotator = entity.addComponent<Rotator>();
    rotator.axis.x = 1.f;
    rotator.speed = 1.f;

    //3D camera
    auto ent = m_scene.createEntity();
    ent.addComponent<cro::Transform>();
    ent.addComponent<cro::Camera>();
    m_scene.setActiveCamera(ent);
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

    /*auto& cam2D = m_menuScene.getActiveCamera().getComponent<cro::Camera>();
    cam2D.viewport = cam3D.viewport;*/
}