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
#include "Slider.hpp"

#include <crogine/core/App.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/detail/GlobalConsts.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/SceneGraph.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/SpriteRenderer.hpp>
#include <crogine/ecs/systems/TextRenderer.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>

#include <crogine/graphics/SphereBuilder.hpp>
#include <crogine/graphics/QuadBuilder.hpp>
#include <crogine/graphics/StaticMeshBuilder.hpp>

#include <crogine/graphics/Image.hpp>
#include <iomanip>
namespace
{
    void outputIcon(const cro::Image& img)
    {
        CRO_ASSERT(img.getFormat() == cro::ImageFormat::RGBA, "");
        std::stringstream ss;
        ss<< "static const unsigned char icon[] = {" << std::endl;
        ss << std::showbase << std::internal << std::setfill('0');

        auto i = 0;
        for (auto y = 0; y < 16; ++y)
        {
            for (auto x = 0; x < 16; ++x)
            {
                ss << std::hex << (int)img.getPixelData()[i++] << ", ";
                ss << std::hex << (int)img.getPixelData()[i++] << ", ";
                ss << std::hex << (int)img.getPixelData()[i++] << ", ";
                ss << std::hex << (int)img.getPixelData()[i++] << ", ";
            }
            ss << std::endl;
        }

        ss << "};";

        std::ofstream file("icon.hpp");
        file << ss.rdbuf();
        file.close();
    }
}

MainState::MainState(cro::StateStack& stack, cro::State::Context context)
    : cro::State        (stack, context),
    m_backgroundScene   (context.appInstance.getMessageBus()),
    m_menuScene         (context.appInstance.getMessageBus()),
    m_commandSystem     (nullptr),
    m_uiSystem          (nullptr)
{
    context.mainWindow.loadResources([this, &context]()
    {
        addSystems();
        loadAssets();
        createScene();
        createMainMenu();
        createOptionsMenu();
        createScoreMenu();
    });

    //context.mainWindow.setVsyncEnabled(false);
    updateView();
}

//public
bool MainState::handleEvent(const cro::Event& evt)
{
    m_uiSystem->handleEvent(evt);
    return true;
}

void MainState::handleMessage(const cro::Message& msg)
{
    m_backgroundScene.forwardMessage(msg);
    m_menuScene.forwardMessage(msg);

    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            updateView();
        }
    }
}

bool MainState::simulate(cro::Time dt)
{
    m_backgroundScene.simulate(dt);
    m_menuScene.simulate(dt);
    return true;
}

void MainState::render()
{    
    m_backgroundScene.render();
    m_menuScene.render();
}

//private
void MainState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_backgroundScene.addSystem<cro::SceneGraph>(mb);
    m_backgroundScene.addSystem<cro::ModelRenderer>(mb);
    m_backgroundScene.addSystem<RotateSystem>(mb);
    m_backgroundScene.addSystem<DriftSystem>(mb);

    m_menuScene.addSystem<cro::SpriteRenderer>(mb).setDepthAxis(cro::SpriteRenderer::DepthAxis::Z);   
    m_menuScene.addSystem<cro::TextRenderer>(mb);
    m_menuScene.addSystem<cro::SceneGraph>(mb);
    m_uiSystem = &m_menuScene.addSystem<cro::UISystem>(mb);
    m_commandSystem = &m_menuScene.addSystem<cro::CommandSystem>(mb);
    //m_menuScene.addSystem<cro::DebugInfo>(mb);
    m_menuScene.addSystem<SliderSystem>(mb);
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
    
    auto& roidMaterial = m_materialResource.add(MaterialID::Roids, m_shaderResource.get(shaderID));
    roidMaterial.setProperty("u_diffuseMap", m_textureResource.get("assets/materials/roid_diffuse.png"));
    roidMaterial.setProperty("u_normalMap", m_textureResource.get("assets/materials/roid_normal.png"));
    roidMaterial.setProperty("u_maskMap", m_textureResource.get("assets/materials/roid_mask.png"));
    
    /*shaderID = m_shaderResource.preloadBuiltIn(cro::ShaderResource::VertexLit, cro::ShaderResource::DiffuseMap);
    auto& cloudMaterial = m_materialResource.add(MaterialID::PlanetClouds, m_shaderResource.get(shaderID));
    cloudMaterial.setProperty("u_diffuseMap", m_textureResource.get("assets/materials/gas_clouds.png"));
    cloudMaterial.blendMode = cro::Material::BlendMode::Alpha;*/
    
    shaderID = m_shaderResource.preloadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::DiffuseMap);
    auto& starTexture = m_textureResource.get("assets/materials/stars.png");
    starTexture.setRepeated(true);
    auto& skyMaterial = m_materialResource.add(MaterialID::Stars, m_shaderResource.get(shaderID));
    skyMaterial.setProperty("u_diffuseMap", starTexture);
    skyMaterial.blendMode = cro::Material::BlendMode::Additive;


    cro::SphereBuilder sb(2.2f, 8);
    m_meshResource.loadMesh(cro::Mesh::SphereMesh, sb);

    cro::QuadBuilder qb(glm::vec2(16.f, 9.f), glm::vec2(16.f / 9.f, 1.f));
    m_meshResource.loadMesh(cro::Mesh::QuadMesh, qb);

    cro::StaticMeshBuilder smb("assets/models/roid_belt.cmf");
    m_meshResource.loadMesh(MeshID::Roids, smb);

    cro::StaticMeshBuilder msmb("assets/models/moon.cmf");
    m_meshResource.loadMesh(MeshID::Moon, msmb);

    //test sprite sheet
    auto& testFont = m_fontResource.get(FontID::MenuFont);
    testFont.loadFromFile("assets/fonts/VeraMono.ttf");
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
    moonTx.setOrigin({ -5.2f, 0.f, 0.f });
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

    /*auto cloudEntity = m_backgroundScene.createEntity();
    cloudEntity.addComponent<cro::Transform>().setScale({ 1.01f, 1.01f, 1.01f });
    cloudEntity.getComponent<cro::Transform>().setParent(entity);
    cloudEntity.addComponent<cro::Model>(m_meshResource.getMesh(cro::Mesh::SphereMesh), m_materialResource.get(MaterialID::PlanetClouds));
*/
    //create stars
    entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -19.f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(cro::Mesh::QuadMesh), m_materialResource.get(MaterialID::Stars));

    entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -12.f });
    entity.getComponent<cro::Transform>().rotate({ 0.f, 0.f, 1.f }, 3.14f);
    entity.addComponent<cro::Model>(m_meshResource.getMesh(cro::Mesh::QuadMesh), m_materialResource.get(MaterialID::Stars));
    entity.addComponent<Drifter>().amplitude = -0.1f;


    //2D and 3D cameras
    entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>();

    entity.addComponent<Drifter>().amplitude = 0.1f;
    m_backgroundScene.setActiveCamera(entity);

    entity = m_menuScene.createEntity();
    entity.addComponent<cro::Transform>();
    auto& cam2D = entity.addComponent<cro::Camera>();
    cam2D.projection = glm::ortho(0.f, static_cast<float>(cro::DefaultSceneSize.x), 0.f, static_cast<float>(cro::DefaultSceneSize.y), -0.1f, 100.f);
    m_menuScene.setActiveCamera(entity);
}

void MainState::updateView()
{
    glm::vec2 size(cro::App::getWindow().getSize());
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

    auto& cam3D = m_backgroundScene.getActiveCamera().getComponent<cro::Camera>();
    cam3D.projection = glm::perspective(0.6f, 16.f / 9.f, 0.1f, 100.f);
    cam3D.viewport.bottom = (1.f - size.y) / 2.f;
    cam3D.viewport.height = size.y;

    auto& cam2D = m_menuScene.getActiveCamera().getComponent<cro::Camera>();
    cam2D.viewport = cam3D.viewport;
}