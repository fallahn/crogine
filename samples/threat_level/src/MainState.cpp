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
#include "MyApp.hpp"

#include <crogine/core/App.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/detail/GlobalConsts.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>
#include <crogine/ecs/components/AudioListener.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/ShadowCaster.hpp>

#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/AudioSystem.hpp>
//#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>

#include <crogine/graphics/SphereBuilder.hpp>
#include <crogine/graphics/QuadBuilder.hpp>
#include <crogine/graphics/StaticMeshBuilder.hpp>

#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/postprocess/PostChromeAB.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/audio/AudioMixer.hpp>

#include <iomanip>
#include <fstream>

namespace
{
#include "MenuConsts.inl"

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

    //cro::CommandSystem* buns = nullptr;
}

MainState::MainState(cro::StateStack& stack, cro::State::Context context, ResourcePtr& sharedResources)
    : cro::State        (stack, context),
    m_backgroundScene   (context.appInstance.getMessageBus()),
    m_menuScene         (context.appInstance.getMessageBus()),
    m_sharedResources   (*sharedResources),
    m_commandSystem     (nullptr),
    m_uiSystem          (nullptr)
{   
    context.mainWindow.loadResources([this, &context]()
    {
        addSystems();
        loadAssets();
        createScene();
        createMenus();
    });

    updateView();
}

//public
bool MainState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsKeyboard() || cro::ui::wantsMouse())
    {
        return false;
    }

    m_uiSystem->handleEvent(evt);
    return true;
}

void MainState::handleMessage(const cro::Message& msg)
{
    m_backgroundScene.forwardMessage(msg);
    m_menuScene.forwardMessage(msg);


    //TODO this should be a camera callback
    //see Camera::resizeCallback
    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            updateView();
        }
    }
}

bool MainState::simulate(float dt)
{    
    m_backgroundScene.simulate(dt);
    m_menuScene.simulate(dt);

    //auto size = getContext().mainWindow.getSize();
    //DPRINT("window size", std::to_string(size.x) + ", " + std::to_string(size.y));

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
    //buns = &m_backgroundScene.addSystem<cro::CommandSystem>(mb);
    m_backgroundScene.addSystem<RotateSystem>(mb);
    m_backgroundScene.addSystem<DriftSystem>(mb);
    m_backgroundScene.addSystem<cro::AudioSystem>(mb);
    m_backgroundScene.addSystem<cro::CameraSystem>(mb);
    //m_backgroundScene.addSystem<cro::ShadowMapRenderer>(mb);
    m_backgroundScene.addSystem<cro::ModelRenderer>(mb);

#ifdef PLATFORM_DESKTOP
    m_backgroundScene.addPostProcess<cro::PostChromeAB>();
#endif
    
    m_commandSystem = m_menuScene.addSystem<cro::CommandSystem>(mb);
    m_menuScene.addSystem<SliderSystem>(mb);
    m_menuScene.addSystem<cro::CallbackSystem>(mb);
    m_menuScene.addSystem<cro::SpriteSystem2D>(mb);
    m_menuScene.addSystem<cro::TextSystem>(mb);
    m_menuScene.addSystem<cro::RenderSystem2D>(mb);
    m_uiSystem = m_menuScene.addSystem<cro::UISystem>(mb);

    m_menuScene.addSystem<cro::CameraSystem>(mb);
}

void MainState::loadAssets()
{
    for (auto& md : m_modelDefs)
    {
        md = std::make_unique<cro::ModelDefinition>(m_resources);
    }

    m_modelDefs[MenuModelID::LookoutBase]->loadFromFile("assets/models/lookout_base.cmt");
    m_modelDefs[MenuModelID::ArcticPost]->loadFromFile("assets/models/arctic_outpost.cmt");
    m_modelDefs[MenuModelID::GasPlanet]->loadFromFile("assets/models/planet.cmt");
    m_modelDefs[MenuModelID::Moon]->loadFromFile("assets/models/moon.cmt");
    m_modelDefs[MenuModelID::Roids]->loadFromFile("assets/models/roid_belt.cmt");
    m_modelDefs[MenuModelID::Stars]->loadFromFile("assets/models/stars.cmt");
    m_modelDefs[MenuModelID::Sun]->loadFromFile("assets/models/sun.cmt");

    m_sharedResources.fonts.load(FontID::MenuFont, "assets/fonts/Audiowide-Regular.ttf");
    m_sharedResources.fonts.load(FontID::ScoreboardFont, "assets/fonts/Now-Bold.otf");

    //audio
    m_resources.audio.load(AudioID::TestStream, "assets/audio/music/background.ogg", true);
}

void MainState::createScene()
{
    //-----background-----//

    const glm::vec3 planetPos(4.f, -0.7f, -8.f);
    //create planet / moon
    auto entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(planetPos);
    entity.getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, -0.25f);
    entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 0.8f);
    m_modelDefs[MenuModelID::GasPlanet]->createModel(entity);
    auto& planetRotator = entity.addComponent<Rotator>();
    planetRotator.speed = 0.02f;
    planetRotator.axis.y = 0.2f;

    auto moonAxis = m_backgroundScene.createEntity();
    auto& moonAxisTx = moonAxis.addComponent<cro::Transform>();
    moonAxisTx.setOrigin({ 0.f, 0.f, -5.6f });
    entity.getComponent<cro::Transform>().addChild(moonAxisTx);
    
    auto moonEntity = m_backgroundScene.createEntity();
    auto& moonTx = moonEntity.addComponent<cro::Transform>();
    moonTx.setScale(glm::vec3(0.34f));
    //moonTx.setOrigin({ 11.f, 0.f, 0.f });
    moonAxisTx.addChild(moonTx);
    m_modelDefs[MenuModelID::Moon]->createModel(moonEntity);
    auto& moonRotator = moonEntity.addComponent<Rotator>();
    moonRotator.axis.y = 1.f;
    moonRotator.speed = 0.1f;   

    auto arcticEntity = m_backgroundScene.createEntity();
    auto& arcticTx = arcticEntity.addComponent<cro::Transform>();
    arcticTx.setScale(glm::vec3(0.8f));
    arcticTx.setOrigin({ 5.8f, 0.f, 5.f });
    entity.getComponent<cro::Transform>().addChild(arcticTx);
    m_modelDefs[MenuModelID::ArcticPost]->createModel(arcticEntity);
    arcticEntity.addComponent<cro::AudioEmitter>(m_resources.audio.get(AudioID::TestStream)).setLooped(true);
    arcticEntity.getComponent<cro::AudioEmitter>().play();
    //arcticEntity.getComponent<cro::AudioEmitter>().setRolloff(20.f);
    arcticEntity.getComponent<cro::AudioEmitter>().setVolume(0.5f);
    arcticEntity.addComponent<cro::CommandTarget>().ID = (1 << 30);
       
    auto lookoutEntity = m_backgroundScene.createEntity();
    auto& lookoutTx = lookoutEntity.addComponent<cro::Transform>();
    lookoutTx.setScale(glm::vec3(0.7f));
    lookoutTx.setOrigin({ -8.f, 0.f, 2.f });
    entity.getComponent<cro::Transform>().addChild(lookoutTx);
    m_modelDefs[MenuModelID::LookoutBase]->createModel(lookoutEntity);
    //lookoutEntity.addComponent<cro::AudioEmitter>(m_resources.audio.get(AudioID::Test)).play(true);
    
    auto roidEntity = m_backgroundScene.createEntity();  
    roidEntity.addComponent<cro::Transform>().setScale({ 0.7f, 0.7f, 0.7f });
    entity.getComponent<cro::Transform>().addChild(roidEntity.getComponent<cro::Transform>());
    m_modelDefs[MenuModelID::Roids]->createModel(roidEntity);
    auto& roidRotator = roidEntity.addComponent<Rotator>();
    roidRotator.speed = -0.03f;
    roidRotator.axis.y = 1.f;

    //create stars / sun
    entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -39.f });
    m_modelDefs[MenuModelID::Stars]->createModel(entity);

    entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -28.f });
    entity.getComponent<cro::Transform>().rotate({ 0.f, 0.f, 1.f }, 3.14f);
    m_modelDefs[MenuModelID::Stars]->createModel(entity);
    entity.addComponent<Drifter>().amplitude = -0.1f;

    entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -4.f, 2.6f, -11.9f });
    m_modelDefs[MenuModelID::Sun]->createModel(entity);


    //set up lighting
    //m_backgroundScene.getSunlight().getComponent<cro::Sunlight>().setColour(cro::Colour(0.48f, 0.48f, 0.48f));
    m_backgroundScene.getSunlight().getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, cro::Util::Const::PI - 0.79f);
    m_backgroundScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 0.79f);

    //2D and 3D cameras
    entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>();// .setPosition({ 0.f, 0.f, 4.f });
    entity.addComponent<cro::Camera>();
    entity.addComponent<cro::AudioListener>();
    entity.addComponent<Drifter>().amplitude = 0.1f;

    m_backgroundScene.setActiveCamera(entity);
    m_backgroundScene.setActiveListener(entity);

    //used for menu scene
    entity = m_menuScene.createEntity();
    entity.addComponent<cro::Transform>();
    auto& cam2D = entity.addComponent<cro::Camera>();
    cam2D.setOrthographic(0.f, static_cast<float>(cro::DefaultSceneSize.x), 0.f, static_cast<float>(cro::DefaultSceneSize.y), -2.f, 100.f);
    m_menuScene.setActiveCamera(entity);

    updateView();    
}

void MainState::createMenus()
{
    cro::SpriteSheet spriteSheetButtons;
    spriteSheetButtons.loadFromFile("assets/sprites/ui_menu.spt", m_sharedResources.textures);
    const auto buttonNormalArea = spriteSheetButtons.getSprite("button_inactive").getTextureRect();
    const auto buttonHighlightArea = spriteSheetButtons.getSprite("button_active").getTextureRect();

    cro::SpriteSheet spriteSheetIcons;
    spriteSheetIcons.loadFromFile("assets/sprites/ui_icons.spt", m_sharedResources.textures);

    auto mouseEnterCallback = m_uiSystem->addCallback([&, buttonHighlightArea](cro::Entity e)
    {
        e.getComponent<cro::Sprite>().setTextureRect(buttonHighlightArea);
    });
    auto mouseExitCallback = m_uiSystem->addCallback([&, buttonNormalArea](cro::Entity e)
    {
        e.getComponent<cro::Sprite>().setTextureRect(buttonNormalArea);
    });

    createMainMenu(mouseEnterCallback, mouseExitCallback, spriteSheetButtons, spriteSheetIcons);
    createOptionsMenu(mouseEnterCallback, mouseExitCallback, spriteSheetButtons, spriteSheetIcons);
    createScoreMenu(mouseEnterCallback, mouseExitCallback, spriteSheetButtons, spriteSheetIcons);

    //preview shadow map
    /*auto entity = m_menuScene.createEntity();
    entity.addComponent<cro::Sprite>().setTexture(m_backgroundScene.getSystem<cro::ShadowMapRenderer>().getDepthMapTexture());
    entity.addComponent<cro::Transform>().setPosition({ 10.f, 10.f, 0.f });
    entity.getComponent<cro::Transform>().setScale(glm::vec3(0.5f));*/
}

void MainState::updateView()
{
    glm::vec2 size(cro::App::getWindow().getSize());
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

    //cro::Logger::log("resized to: " + std::to_string(size.x) + ", " + std::to_string(size.y));
    //m_backgroundScene.getActiveCamera().getComponent<cro::Transform>().setScale({ -1.f, -1.f, 1.f });

    auto& cam3D = m_backgroundScene.getActiveCamera().getComponent<cro::Camera>();
    cam3D.setPerspective(35.f * cro::Util::Const::degToRad, 16.f / 9.f, 0.1f, 100.f);
    cam3D.viewport.bottom = (1.f - size.y) / 2.f;
    cam3D.viewport.height = size.y;

    auto& cam2D = m_menuScene.getActiveCamera().getComponent<cro::Camera>();
    cam2D.viewport = cam3D.viewport;
}