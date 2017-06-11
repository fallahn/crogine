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
#include <crogine/util/Constants.hpp>
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
    m_modelDefs[MenuModelID::LookoutBase].loadFromFile("assets/models/lookout_base.cmt", m_resources);
    m_modelDefs[MenuModelID::ArcticPost].loadFromFile("assets/models/arctic_outpost.cmt", m_resources);
    m_modelDefs[MenuModelID::GasPlanet].loadFromFile("assets/models/planet.cmt", m_resources);
    m_modelDefs[MenuModelID::Moon].loadFromFile("assets/models/moon.cmt", m_resources);
    m_modelDefs[MenuModelID::Roids].loadFromFile("assets/models/roid_belt.cmt", m_resources);
    m_modelDefs[MenuModelID::Stars].loadFromFile("assets/models/stars.cmt", m_resources);
    m_modelDefs[MenuModelID::Sun].loadFromFile("assets/models/sun.cmt", m_resources);

    //test sprite sheet
    auto& testFont = m_resources.fonts.get(FontID::MenuFont);
    testFont.loadFromFile("assets/fonts/VeraMono.ttf");
}

void MainState::createScene()
{
    //-----background-----//
    //create planet / moon
    auto entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 4.f, -0.7f, -8.f });
    entity.getComponent<cro::Transform>().setRotation({ -0.5f, 0.f, 0.4f });
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[MenuModelID::GasPlanet].meshID),
                                    m_resources.materials.get(m_modelDefs[MenuModelID::GasPlanet].materialIDs[0]));
    auto& planetRotator = entity.addComponent<Rotator>();
    planetRotator.speed = 0.02f;
    planetRotator.axis.y = 0.2f;

    //auto moonAxis = m_backgroundScene.createEntity();
    //auto& moonAxisTx = moonAxis.addComponent<cro::Transform>();
    //moonAxisTx.setOrigin({ -5.6f, 0.f, 0.f });
    //moonAxisTx.setParent(entity);
    
    auto moonEntity = m_backgroundScene.createEntity();
    auto& moonTx = moonEntity.addComponent<cro::Transform>();
    moonTx.setScale(glm::vec3(0.44f));
    moonTx.setOrigin({ 0.f, 0.f, -8.2f });
    moonTx.setParent(entity);
    moonEntity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[MenuModelID::Moon].meshID),
                                        m_resources.materials.get(m_modelDefs[MenuModelID::Moon].materialIDs[0]));
    //auto& moonRotator = moonEntity.addComponent<Rotator>();
    //moonRotator.axis.y = 1.f;
    //moonRotator.speed = 0.4f;

    auto arcticEntity = m_backgroundScene.createEntity();
    auto& arcticTx = arcticEntity.addComponent<cro::Transform>();
    arcticTx.setScale(glm::vec3(0.8f));
    arcticTx.setOrigin({ -30.f, 0.f, -2.f });
    arcticTx.setParent(entity);
    auto& arcticModel = arcticEntity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[MenuModelID::ArcticPost].meshID),
                                                                m_resources.materials.get(m_modelDefs[MenuModelID::ArcticPost].materialIDs[0]));
    for (auto i = 0u; i < m_modelDefs[MenuModelID::ArcticPost].materialCount; ++i)
    {
        arcticModel.setMaterial(i, m_resources.materials.get(m_modelDefs[MenuModelID::ArcticPost].materialIDs[i]));
    }

    auto lookoutEntity = m_backgroundScene.createEntity();
    auto& lookoutTx = lookoutEntity.addComponent<cro::Transform>();
    lookoutTx.setScale(glm::vec3(0.7f));
    lookoutTx.setOrigin({ 18.f, 0.f, 2.f });
    lookoutTx.setParent(entity);
    auto& lookoutModel = lookoutEntity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[MenuModelID::LookoutBase].meshID),
        m_resources.materials.get(m_modelDefs[MenuModelID::LookoutBase].materialIDs[0]));
    for (auto i = 0u; i < m_modelDefs[MenuModelID::LookoutBase].materialCount; ++i)
    {
        lookoutModel.setMaterial(i, m_resources.materials.get(m_modelDefs[MenuModelID::LookoutBase].materialIDs[i]));
    }

    auto roidEntity = m_backgroundScene.createEntity();  
    roidEntity.addComponent<cro::Transform>().setScale({ 0.7f, 0.7f, 0.7f });
    roidEntity.getComponent<cro::Transform>().setParent(entity);
    roidEntity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[MenuModelID::Roids].meshID),
                                        m_resources.materials.get(m_modelDefs[MenuModelID::Roids].materialIDs[0]));
    auto& roidRotator = roidEntity.addComponent<Rotator>();
    roidRotator.speed = -0.03f;
    roidRotator.axis.y = 1.f;

    /*auto cloudEntity = m_backgroundScene.createEntity();
    cloudEntity.addComponent<cro::Transform>().setScale({ 1.01f, 1.01f, 1.01f });
    cloudEntity.getComponent<cro::Transform>().setParent(entity);
    cloudEntity.addComponent<cro::Model>(m_meshResource.getMesh(cro::Mesh::SphereMesh), m_materialResource.get(MaterialID::PlanetClouds));
*/
    //create stars / sun
    entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -39.f });
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[MenuModelID::Stars].meshID),
                                    m_resources.materials.get(m_modelDefs[MenuModelID::Stars].materialIDs[0]));

    entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -28.f });
    entity.getComponent<cro::Transform>().rotate({ 0.f, 0.f, 1.f }, 3.14f);
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[MenuModelID::Stars].meshID),
                                    m_resources.materials.get(m_modelDefs[MenuModelID::Stars].materialIDs[0]));
    entity.addComponent<Drifter>().amplitude = -0.1f;

    entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -4.f, 2.6f, -11.9f });
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_modelDefs[MenuModelID::Sun].meshID),
                                    m_resources.materials.get(m_modelDefs[MenuModelID::Sun].materialIDs[0]));

    //2D and 3D cameras
    entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>();

    /*auto &camRot = entity.addComponent<Rotator>();
    camRot.axis.y = 1.f; camRot.speed = 0.7f;*/

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