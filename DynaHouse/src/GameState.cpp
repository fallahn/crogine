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

#include <crogine/detail/GlobalConsts.hpp>

#include <crogine/core/App.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/ConfigFile.hpp>

#include <crogine/graphics/QuadBuilder.hpp>
#include <crogine/graphics/StaticMeshBuilder.hpp>
#include <crogine/graphics/IqmBuilder.hpp>

#include <crogine/ecs/systems/SceneGraph.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/ParticleSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/TextRenderer.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/ProjectionMapSystem.hpp>
#include <crogine/ecs/systems/SpriteRenderer.hpp>
#include <crogine/ecs/systems/UISystem.hpp>

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

#include <crogine/util/Random.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/util/Constants.hpp>

#include <glm/gtx/norm.hpp>

namespace
{
    cro::int32 rowCount = 3;
    cro::int32 rowWidth = 6;

    cro::Skeleton batCatSkeleton;

    cro::UISystem* uiSystem = nullptr;
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
    context.appInstance.setClearColour(cro::Colour::White());
    //context.mainWindow.setVsyncEnabled(false);

    updateView();

    auto* msg = getContext().appInstance.getMessageBus().post<GameEvent>(MessageID::GameMessage);
    msg->type = GameEvent::RoundStart;
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
    m_scene.addSystem<cro::ProjectionMapSystem>(mb);
    m_scene.addSystem<cro::ParticleSystem>(mb);
    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<cro::SkeletalAnimator>(mb);
    m_scene.addSystem<cro::TextRenderer>(mb);
    m_scene.addSystem<cro::SceneGraph>(mb);
    m_scene.addSystem<cro::ModelRenderer>(mb);

    m_scene.addDirector<PlayerDirector>();

    uiSystem = &m_overlayScene.addSystem<cro::UISystem>(mb);
    m_overlayScene.addSystem<cro::SceneGraph>(mb);
    m_overlayScene.addSystem<cro::TextRenderer>(mb);
    m_overlayScene.addSystem<cro::SpriteRenderer>(mb);
}

void GameState::loadAssets()
{
    cro::ConfigFile cfg;
    cfg.loadFromFile("assets/models/batcat.cmt");


    auto shaderID = m_shaderResource.preloadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::DiffuseMap | cro::ShaderResource::ReceiveProjection);

    auto& roomOne = m_materialResource.add(MaterialID::RoomOne, m_shaderResource.get(shaderID));
    roomOne.setProperty("u_diffuseMap", m_textureResource.get("assets/textures/room1x1.png"));

    auto& roomTwo = m_materialResource.add(MaterialID::RoomTwo, m_shaderResource.get(shaderID));
    roomTwo.setProperty("u_diffuseMap", m_textureResource.get("assets/textures/room2x1.png"));
    roomTwo.setProperty("u_projectionMap", m_textureResource.get("assets/textures/shadow.png", false));
    m_textureResource.get("assets/textures/shadow.png").setSmooth(true);

    cro::StaticMeshBuilder r1("assets/models/room1x1.cmf");
    m_meshResource.loadMesh(MeshID::RoomOne, r1);

    cro::StaticMeshBuilder r2("assets/models/room2x1.cmf");
    m_meshResource.loadMesh(MeshID::RoomTwo, r2);

    shaderID = m_shaderResource.preloadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::DiffuseMap /*| cro::ShaderResource::NormalMap*/ | cro::ShaderResource::Skinning);
    auto& batMat = m_materialResource.add(MaterialID::BatCat, m_shaderResource.get(shaderID));
    auto& batDiff = m_textureResource.get("assets/textures/batcat_diffuse.png");
    batDiff.setRepeated(true);
    batMat.setProperty("u_diffuseMap", batDiff);
    /*auto& batMask = m_textureResource.get("assets/textures/batcat_mask.png");
    batMask.setRepeated(true);
    batMat.setProperty("u_maskMap", batMask);
    auto& batNormal = m_textureResource.get("assets/textures/batcat_normal.png");
    batNormal.setRepeated(true);
    batMat.setProperty("u_normalMap", batNormal);*/

    cro::IqmBuilder catBuilder("assets/models/batcat.iqm");
    m_meshResource.loadMesh(MeshID::BatCat, catBuilder);
    batCatSkeleton = catBuilder.getSkeleton();
    batCatSkeleton.currentAnimation = AnimationID::BatCat::Idle;
    batCatSkeleton.animations[AnimationID::BatCat::Idle].playing = true;
}

void GameState::createScene()
{
    //starting room
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::RoomTwo), m_materialResource.get(MaterialID::RoomTwo));
    entity.addComponent<cro::Transform>().move({ 0.f, 0.5f, 0.5f });
        
    //dat cat man
    entity = m_scene.createEntity();
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::BatCat), m_materialResource.get(MaterialID::BatCat));
    entity.addComponent<cro::Transform>().setScale({ 0.002f, 0.002f, 0.002f });
    entity.getComponent<cro::Transform>().setRotation({ -cro::Util::Const::PI / 2.f, cro::Util::Const::PI / 2.f, 0.f });
    entity.addComponent<cro::Skeleton>() = batCatSkeleton;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Player;
    entity.addComponent<Player>();

    auto mapEntity = m_scene.createEntity();
    mapEntity.addComponent<cro::ProjectionMap>();
    mapEntity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 300.f });
    mapEntity.getComponent<cro::Transform>().setParent(entity);
    //mapEntity.getComponent<cro::Transform>().rotate({ 1.f, 0.f, 0.f }, -cro::Util::Const::PI/* / 2.f*/);

    //3D camera
    auto ent = m_scene.createEntity();
    ent.addComponent<cro::Transform>().setPosition({ 0.f, 0.5f, 2.3f });
    ent.addComponent<cro::Camera>();
    ent.addComponent<cro::CommandTarget>().ID = CommandID::Camera;
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

#ifdef PLATFORM_MOBILE
    m_textureResource.get("assets/ui/ui_buttons.png", false).setSmooth(true);
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
    button1.setTexture(m_textureResource.get("assets/ui/ui_buttons.png"));
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
    button3.setTexture(m_textureResource.get("assets/ui/ui_buttons.png"));
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
    button4.setTexture(m_textureResource.get("assets/ui/ui_buttons.png"));
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
    auto& font = m_fontResource.get(0);
    font.loadFromFile("assets/VeraMono.ttf");
    auto& text = ent.addComponent<cro::Text>(font);
    text.setColour(cro::Colour::Blue());
    text.setString("WASD to move");
    ent.addComponent<cro::Transform>().setPosition({ 50.f, 680.f, 0.f });
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

void GameState::buildRow(std::size_t floor)
{
    addStairs(floor);

    cro::int32 position = 1;
    while (position < rowWidth)
    {
        cro::int32 step = (1 + (cro::Util::Random::value(2, 3) % 2));
        if (step == 1)
        {
            addSingle(floor, position);
        }
        else
        {
            addDouble(floor, position);
        }
        position += step;
    }

    cro::int32 endSize = (position > rowWidth) ? 1 : 2;
    addEnd(floor, position, endSize);

    position = 0;
    while (position > -(rowWidth - 1))
    {
        cro::int32 step = (1 + (cro::Util::Random::value(2, 3) % 2));
        position -= step;        
        if (step == 1)
        {
            addSingle(floor, position);
        }
        else
        {
            addDouble(floor, position);
        }
    }

    endSize = (position <= -rowWidth) ? 1 : 2;
    addEnd(floor, position - endSize, endSize);
}

void GameState::addStairs(std::size_t floor)
{
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.5f, static_cast<float>(floor), 0.f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(cro::Mesh::QuadMesh), m_materialResource.get(floor == 0 ? MaterialID::Blue : MaterialID::Red));
    entity.addComponent<cro::CommandTarget>().ID = (1 << (rowCount * rowWidth));
}

void GameState::addSingle(std::size_t floor, cro::int32 position)
{
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ static_cast<float>(position) + 0.5f, static_cast<float>(floor), 0.f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::RoomOne), m_materialResource.get(MaterialID::RoomOne));
    entity.addComponent<cro::CommandTarget>().ID = (1 << (rowCount * rowWidth));
}

void GameState::addDouble(std::size_t floor, cro::int32 position)
{
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ static_cast<float>(position) + 1.f, static_cast<float>(floor), 0.f });
    //entity.getComponent<cro::Transform>().setScale({ 2.f, 1.f, 1.f });
    entity.addComponent<cro::Model>(m_meshResource.getMesh(MeshID::RoomTwo), m_materialResource.get(MaterialID::RoomTwo));
    entity.addComponent<cro::CommandTarget>().ID = (1 << (rowCount * rowWidth));
}

void GameState::addEnd(std::size_t floor, cro::int32 position, std::size_t width)
{
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ static_cast<float>(position) + (static_cast<float>(width) / 2.f), static_cast<float>(floor), 0.f });

    if (width > 1)
    {
        entity.getComponent<cro::Transform>().setScale({ static_cast<float>(width), 1.f, 1.f });
    }

    entity.addComponent<cro::Model>(m_meshResource.getMesh(cro::Mesh::QuadMesh), m_materialResource.get(MaterialID::Brown));
    entity.addComponent<cro::CommandTarget>().ID = (1 << (rowCount * rowWidth));
}