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

#include "MainState.hpp"
#include "RotateSystem.hpp"
#include "DriftSystem.hpp"

#include <crogine/core/App.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>

#include <crogine/ecs/systems/SceneRenderer.hpp>
#include <crogine/ecs/systems/MeshSorter.hpp>
#include <crogine/ecs/systems/SceneGraph.hpp>
#include <crogine/ecs/systems/SpriteRenderer.hpp>
#include <crogine/ecs/systems/TextRenderer.hpp>

#include <crogine/graphics/SphereBuilder.hpp>
#include <crogine/graphics/QuadBuilder.hpp>
#include <crogine/graphics/StaticMeshBuilder.hpp>
#include <crogine/graphics/Image.hpp>

namespace
{
    namespace MaterialID
    {
        enum
        {
            Moon,
            Stars,
            Planet,
            Roids
        };
    }

    namespace MeshID
    {
        enum
        {
            Roids = cro::Mesh::ID::Count,
            Moon
        };
    }

    namespace FontID
    {
        enum
        {
            MenuFont
        };
    }
}

MainState::MainState(cro::StateStack& stack, cro::State::Context context)
    : cro::State        (stack, context),
    m_backgroundScene   (context.appInstance.getMessageBus()),
    m_mainMenuScene     (context.appInstance.getMessageBus()),
    m_optionMenuScene   (context.appInstance.getMessageBus()),
    m_highScoreScene    (context.appInstance.getMessageBus()),
    m_currentMenu       (nullptr),
    m_backgroundRenderer(nullptr),
    m_spriteRenderer    (nullptr),
    m_textRenderer      (nullptr),
    m_commandSystem     (nullptr)
{
    addSystems();
    loadAssets();
    createScene();
    //context.appInstance.setClearColour(cro::Colour::Red());
    createMainMenu();
}

//public
bool MainState::handleEvent(const cro::Event& evt)
{
    return true;
}

void MainState::handleMessage(const cro::Message& msg)
{
    m_backgroundScene.forwardMessage(msg);
    m_currentMenu->forwardMessage(msg);
}

bool MainState::simulate(cro::Time dt)
{
    m_backgroundScene.simulate(dt);
    m_currentMenu->simulate(dt);
    return true;
}

void MainState::render() const
{
    m_backgroundRenderer->render();
    m_spriteRenderer->render();
    m_textRenderer->render();
}

//private
void MainState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_backgroundScene.addSystem<cro::SceneGraph>(mb);
    m_backgroundRenderer = &m_backgroundScene.addSystem<cro::SceneRenderer>(mb, m_backgroundScene.getDefaultCamera());
    m_backgroundScene.addSystem<cro::MeshSorter>(mb, *m_backgroundRenderer);
    m_backgroundScene.addSystem<RotateSystem>(mb);
    m_backgroundScene.addSystem<DriftSystem>(mb);
}

void MainState::loadAssets()
{
    cro::int32 shaderID = m_shaderResource.preloadBuiltIn(cro::ShaderResource::VertexLit, cro::ShaderResource::DiffuseMap | cro::ShaderResource::NormalMap);
    auto& moonMaterial = m_materialResource.add(MaterialID::Moon, m_shaderResource.get(shaderID));
    moonMaterial.setProperty("u_diffuseMap", m_textureResource.get("assets/materials/moon_diffuse.png"));
    moonMaterial.setProperty("u_normalMap", m_textureResource.get("assets/materials/moon_normal.png"));
    moonMaterial.setProperty("u_maskMap", m_textureResource.get("assets/materials/moon_mask.png"));

    auto& planetMaterial = m_materialResource.add(MaterialID::Planet, m_shaderResource.get(shaderID));
    planetMaterial.setProperty("u_diffuseMap", m_textureResource.get("assets/materials/gas_diffuse.png"));
    auto& normalTex = m_textureResource.get("assets/materials/gas_normal.png");
    normalTex.setSmooth(true);
    planetMaterial.setProperty("u_normalMap", normalTex);
    planetMaterial.setProperty("u_maskMap", m_textureResource.get("assets/materials/gas_mask.png"));

    //shaderID = m_shaderResource.preloadBuiltIn(cro::ShaderResource::VertexLit, cro::ShaderResource::DiffuseMap);
    auto& roidMaterial = m_materialResource.add(MaterialID::Roids, m_shaderResource.get(shaderID));
    roidMaterial.setProperty("u_diffuseMap", m_textureResource.get("assets/materials/roid_diffuse.png"));
    roidMaterial.setProperty("u_normalMap", m_textureResource.get("assets/materials/roid_normal.png"));
    roidMaterial.setProperty("u_maskMap", m_textureResource.get("assets/materials/roid_mask.png"));
    
    
    shaderID = m_shaderResource.preloadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::DiffuseMap);
    auto& starTexture = m_textureResource.get("assets/materials/stars.png");
    starTexture.setRepeated(true);
    auto& skyMaterial = m_materialResource.add(MaterialID::Stars, m_shaderResource.get(shaderID));
    skyMaterial.setProperty("u_diffuseMap", starTexture);
    skyMaterial.blendMode = cro::Material::BlendMode::Additive;


    cro::SphereBuilder sb(2.2f, 8);
    m_meshResource.loadMesh(sb, cro::Mesh::SphereMesh);

    cro::QuadBuilder qb(glm::vec2(16.f, 9.f), glm::vec2(16.f / 9.f, 1.f));
    m_meshResource.loadMesh(qb, cro::Mesh::QuadMesh);

    cro::StaticMeshBuilder smb("assets/models/roid_belt.cmf");
    m_meshResource.loadMesh(smb, MeshID::Roids);

    cro::StaticMeshBuilder msmb("assets/models/moon.cmf");
    m_meshResource.loadMesh(msmb, MeshID::Moon);
}

void MainState::createScene()
{
    //-----background-----//
    //create planet / moon
    auto entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 2.3f, -0.7f, -6.f });
    entity.getComponent<cro::Transform>().setRotation({ -0.5f, 0.f, 0.4f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(cro::Mesh::SphereMesh), m_materialResource.get(MaterialID::Planet));
    auto& planetRotator = entity.addComponent<Rotator>();
    planetRotator.speed = 0.02f;
    planetRotator.axis.y = 0.2f;

    //auto moonAxis = m_backgroundScene.createEntity();
    //auto& moonAxisTx = moonAxis.addComponent<cro::Transform>();
    //moonAxisTx.setOrigin({ -5.6f, 0.f, 0.f });
    //moonAxisTx.setParent(entity);
    
    auto moonEntity = m_backgroundScene.createEntity();
    auto& moonTx = moonEntity.addComponent<cro::Transform>();
    moonTx.setScale({ 0.22f, 0.22f, 0.22f });
    moonTx.setOrigin({ -5.6f, 0.f, 0.f });
    moonTx.setParent(entity);
    moonEntity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::Moon), m_materialResource.get(MaterialID::Moon));
    //auto& moonRotator = moonEntity.addComponent<Rotator>();
    //moonRotator.axis.y = 1.f;
    //moonRotator.speed = 0.4f;

    auto roidEntity = m_backgroundScene.createEntity();  
    roidEntity.addComponent<cro::Transform>().setScale({ 0.7f, 0.7f, 0.7f });
    roidEntity.getComponent<cro::Transform>().setParent(entity);
    roidEntity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::Roids), m_materialResource.get(MaterialID::Roids));
    auto& roidRotator = roidEntity.addComponent<Rotator>();
    roidRotator.speed = -0.03f;
    roidRotator.axis.y = 1.f;

    //create stars
    entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -19.f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(cro::Mesh::QuadMesh), m_materialResource.get(MaterialID::Stars));

    entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -15.f });
    entity.getComponent<cro::Transform>().rotate({ 0.f, 0.f, 1.f }, 3.14f);
    entity.addComponent<cro::Model>(m_meshResource.getMesh(cro::Mesh::QuadMesh), m_materialResource.get(MaterialID::Stars));
    entity.addComponent<Drifter>().amplitude = -0.1f;

    entity = m_backgroundScene.createEntity();
    /*auto& tx4 = */entity.addComponent<cro::Transform>();
    /*tx4.move({ 0.f, 0.4f, 1.f });
    tx4.rotate({ 1.f, 0.f, 0.f }, -0.1f);*/
    entity.addComponent<cro::Camera>();
    entity.addComponent<Drifter>().amplitude = 0.05f;
    m_backgroundRenderer->setActiveCamera(entity);

    //-----menus-----//
}

void MainState::createMainMenu()
{
    //test sprite sheet
    auto& testFont = m_fontResource.get(FontID::MenuFont);
    testFont.loadFromFile("assets/fonts/VeraMono.ttf");
    //cro::Image img;
    //img.loadFromFile("assets/fonts/sdf.png");
    //testFont.loadFromImage(img, { 32.f, 32.f }, cro::Font::Type::SDF);
    
    auto& mb = getContext().appInstance.getMessageBus();
    m_currentMenu = &m_mainMenuScene;
    m_spriteRenderer = &m_mainMenuScene.addSystem<cro::SpriteRenderer>(mb);
    m_textRenderer = &m_mainMenuScene.addSystem<cro::TextRenderer>(mb);
    m_mainMenuScene.addSystem<cro::SceneGraph>(mb);

    auto& uiTexture = m_textureResource.get("assets/sprites/menu.png");
    auto entity = m_mainMenuScene.createEntity();
    auto& titleSprite = entity.addComponent<cro::Sprite>();
    titleSprite.setTexture(uiTexture);
    titleSprite.setTextureRect({ 0.f, 64.f, 1024.f, 320.f });
    auto& titleTx = entity.addComponent<cro::Transform>();
    titleTx.setOrigin({ 512.f, 160.f, 0.f });
    titleTx.setPosition({ 960.f - 512.f, 900.f, 0.f });
    titleTx.setScale({ 1.5f, 1.5f, 1.5f });

    entity = m_mainMenuScene.createEntity();
    auto& gameSprite = entity.addComponent<cro::Sprite>();
    gameSprite.setTexture(uiTexture);
    gameSprite.setTextureRect({ 0.f, 0.f, 256.f, 64.f });
    auto& gameTx = entity.addComponent<cro::Transform>();
    gameTx.setPosition({ 960.f - 256.f, 620.f, 0.f });
    gameTx.setScale({ 2.f, 2.f, 2.f });

    auto textEnt = m_mainMenuScene.createEntity();
    auto& gameText = textEnt.addComponent<cro::Text>(testFont);
    gameText.setString("Play");
    gameText.setColour(cro::Colour::Blue());
    auto& gameTextTx = textEnt.addComponent<cro::Transform>();
    gameTextTx.setPosition({ 90.f, 50.f, 0.f });
    gameTextTx.setParent(entity);

    entity = m_mainMenuScene.createEntity();
    auto& optionSprite = entity.addComponent<cro::Sprite>();
    optionSprite.setTexture(uiTexture);
    optionSprite.setTextureRect({ 0.f, 0.f, 256.f, 64.f });
    auto& optionTx = entity.addComponent<cro::Transform>();
    optionTx.setPosition({ 960.f - 256.f, 460.f, 0.f });
    optionTx.setScale({ 2.f, 2.f, 2.f });

    textEnt = m_mainMenuScene.createEntity();
    auto& optionText = textEnt.addComponent<cro::Text>(testFont);
    optionText.setString("Options");
    optionText.setColour(cro::Colour::Magenta());
    auto& texTx = textEnt.addComponent<cro::Transform>();
    texTx.setParent(entity);
    texTx.move({ 64.f, 50.f, 0.f });

    entity = m_mainMenuScene.createEntity();
    auto& scoreSprite = entity.addComponent<cro::Sprite>();
    scoreSprite.setTexture(uiTexture);
    scoreSprite.setTextureRect({ 0.f, 0.f, 256.f, 64.f });
    auto& scoreTx = entity.addComponent<cro::Transform>();
    scoreTx.setPosition({ 960.f - 256.f, 300.f, 0.f });
    scoreTx.setScale({ 2.f, 2.f, 2.f });

    textEnt = m_mainMenuScene.createEntity();
    auto& scoreText = textEnt.addComponent<cro::Text>(testFont);
    scoreText.setString("Scores");
    scoreText.setColour(cro::Colour::Green());
    auto& scoreTexTx = textEnt.addComponent<cro::Transform>();
    scoreTexTx.setParent(entity);
    scoreTexTx.move({ 74.f, 50.f, 0.f });

    entity = m_mainMenuScene.createEntity();
    auto& quitSprite = entity.addComponent<cro::Sprite>();
    quitSprite.setTexture(uiTexture);
    quitSprite.setTextureRect({ 0.f, 0.f, 256.f, 64.f });
    auto& quitTx = entity.addComponent<cro::Transform>();
    quitTx.setPosition({ 960.f - 256.f, 140.f, 0.f });
    quitTx.setScale({ 2.f, 2.f, 2.f });

    textEnt = m_mainMenuScene.createEntity();
    auto& quitText = textEnt.addComponent<cro::Text>(testFont);
    quitText.setString("Quit");
    quitText.setColour(cro::Colour::Red());
    auto& quitTexTx = textEnt.addComponent<cro::Transform>();
    quitTexTx.setParent(entity);
    quitTexTx.move({ 88.f, 50.f, 0.f });


    //entity = m_mainMenuScene.createEntity();
    //auto& buns = entity.addComponent<cro::Sprite>();
    //buns.setTexture(testFont.getTexture());
    ////buns.setTextureRect(testFont.getGlyph('@'));
    //entity.addComponent<cro::Transform>().move({ 30.f, 0.f, 0.f });
}
