//Auto-generated source file for Scratchpad Stub 17/11/2026, 09:37:35

#include "MultidrawState.hpp"
#include "../PoissonDisk.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/BillboardCollection.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>

#include <crogine/ecs/systems/BillboardSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Wavetable.hpp>

MultidrawState::MultidrawState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus())
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    });
}

//public
bool MultidrawState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    if (evt.type == SDL_EVENT_KEY_DOWN)
    {
        switch (evt.key.key)
        {
        default: break;
        case SDLK_BACKSPACE:
        case SDLK_ESCAPE:
            requestStackClear();
            requestStackPush(0);
            break;
        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void MultidrawState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool MultidrawState::simulate(float dt)
{
    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void MultidrawState::preRender()
{
    m_gameScene.preRender();
}

void MultidrawState::render()
{
    m_gameScene.render();
    m_uiScene.render();
}

//private
void MultidrawState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::BillboardSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void MultidrawState::loadAssets()
{
}

void MultidrawState::createScene()
{
    cro::Billboard board;
    board.size = { 0.4f, 1.f };
    //board.colour = cro::Colour::Green;

    auto bbEnt = m_gameScene.createEntity();
    bbEnt.addComponent<cro::Transform>();
    
    cro::ModelDefinition md(m_resources);
    md.loadFromFile("assets/batcat/models/tree.cmt");
    md.createModel(bbEnt);

    const auto positions = thinks::PoissonDiskSampling(0.5f, std::array<float, 2u>({ -10.f, -10.f }), { 10.f, 10.f });

    auto& bbc = bbEnt.getComponent<cro::BillboardCollection>();
    for (const auto p : positions)
    {
        bbc.addBillboard(board);
        board.position.x = p[0];
        board.position.z = p[1];
    }
    bbEnt.getComponent<cro::BillboardCollection>().setChunkSize(3);


    md.loadFromFile("assets/models/plane2.cmt");
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, -cro::Util::Const::PI / 2.f);
    entity.getComponent<cro::Transform>().setScale(glm::vec3(2.f));
    md.createModel(entity);


    md.loadFromFile("assets/ssao/cart/cart.cmt");
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    md.createModel(entity);


    md.loadFromFile("assets/models/sphere_1m.cmt");
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    md.createModel(entity);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float)
        {
            static const auto wavetable = cro::Util::Wavetable::sine(0.1f, 8.f);
            static std::size_t is = 0;
            static std::size_t ic = wavetable.size() / 4;

            glm::vec3 pos = { wavetable[is], 2.f, wavetable[ic] };
            e.getComponent<cro::Transform>().setPosition(pos);

            is = (is + 1) % wavetable.size();
            ic = (ic + 1) % wavetable.size();
        };
    auto sphereEnt = entity;

    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        cam.setPerspective(70.f * cro::Util::Const::degToRad, size.x / size.y, 0.1f, 50.f);
    };

    auto camEnt = m_gameScene.getActiveCamera();

    auto& cam = camEnt.getComponent<cro::Camera>();
    cam.shadowMapBuffer.create(2048, 2048);
    cam.setMaxShadowDistance(20.f);
    cam.setShadowExpansion(17.f);
    //cam.setBlurPassCount(1);
    cam.resizeCallback = resize;
    resize(cam);


    //use this camera's drawlist index to set the billboard collection culling func
    //TODO drawlist index is always 0 at this point until the entity as actually added to the CameraSystem
    bbc.setDrawlistCallback(0, [sphereEnt](cro::Entity camera, cro::Entity parent)
        {
            const auto testPos = sphereEnt.getComponent<cro::Transform>().getPosition();
            auto& bbc = parent.getComponent<cro::BillboardCollection>();

            const auto relPos = glm::vec2(testPos.x, testPos.z) - bbc.getChunkCoordStart();
            const auto xPos = std::floor(relPos.x / bbc.getChunkSize());
            const auto yPos = std::floor(relPos.y / bbc.getChunkSize());

            const auto idx = (yPos * bbc.getChunkCount().x) + xPos;
            const auto& chunk = bbc.getChunkIndices()[static_cast<std::int32_t>(idx)];

            const auto& mesh = parent.getComponent<cro::Model>().getMeshData();

            auto& drawList = bbc.getDrawList(camera.getComponent<cro::Camera>().getDrawListIndex());
            drawList.indexOffsetArray.clear();
            drawList.indexSizeArray.clear();
            //TODO hide this horrible cast in a setter
            drawList.indexOffsetArray.push_back(reinterpret_cast<void*>(mesh.indexData[0].iboAllocation.offset + chunk.offset));
            drawList.indexSizeArray.push_back(chunk.count);


        });


    const auto tx = glm::lookAt({ 0.f, 4.f, 11.f }, { 0.f,  0.f, 0.f }, cro::Transform::Y_AXIS);
    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setLocalTransform(glm::inverse(tx));

    m_gameScene.getSunlight().getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, -0.439f);
    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.777f);
}

void MultidrawState::createUI()
{
    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = {0.f, 0.f, 1.f, 1.f};
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -0.1f, 10.f);
    };

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);
}