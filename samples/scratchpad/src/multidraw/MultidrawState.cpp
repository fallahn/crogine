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
#include <crogine/util/Matrix.hpp>
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

    m_overheadTexture.clear();
    auto oldCam = m_gameScene.setActiveCamera(m_overheadCam);
    m_gameScene.render();
    m_gameScene.setActiveCamera(oldCam);
    m_overheadTexture.display();
}

void MultidrawState::render()
{
    m_gameScene.render();

    m_overheadQuad.draw();
    m_uiScene.render();
}

//private
void MultidrawState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::BillboardSystem>(mb);
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void MultidrawState::loadAssets()
{
    m_environmentMap.loadFromFile("assets/images/hills.hdr");
}

void MultidrawState::createScene()
{
    cro::Billboard board;
    board.size = { 0.4f, 1.f };
    //board.colour = cro::Colour::Green;

    auto bbEnt = m_gameScene.createEntity();
    bbEnt.addComponent<cro::Transform>();
    
    cro::ModelDefinition md(m_resources, &m_environmentMap);
    md.loadFromFile("assets/batcat/models/tree.cmt");
    md.createModel(bbEnt);

    const auto positions = thinks::PoissonDiskSampling(0.5f, std::array<float, 2u>({ -10.f, -10.f }), { 10.f, 10.f });

    auto& bbc = bbEnt.getComponent<cro::BillboardCollection>();
    for (const auto p : positions)
    {
        bbc.addBillboard(board);
        board.position.x = p[0];
        board.position.y = cro::Util::Random::value(-1.f, 1.f);
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


    md.loadFromFile("assets/models/cube2.cmt");
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 2.f, 2.f, 1.f });
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

            e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, -std::atan2(-pos.x, pos.z));
        };
    auto sphereEnt = entity;

    const auto resize = [](cro::Camera& cam)
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


    //use this camera's drawlist to set the billboard collection culling func
    //bbc.setDrawListCallback(camEnt, [this, sphereEnt](cro::Entity camera, cro::Entity parent)
    //    {
    //        const auto inverseTx = glm::inverse(parent.getComponent<cro::Transform>().getWorldTransform());

    //        //TODO this ought to be the camera pos, we just want to visualise the cube as a camera
    //        const auto camTx = inverseTx * m_overheadCam.getComponent<cro::Transform>().getWorldTransform();
    //        const auto testPos = glm::vec3(camTx[3]);

    //        //calc AABB of frustum - use this to only test the chunks which fall into its grid area
    //        glm::vec3 minPos(std::numeric_limits<float>::max());
    //        glm::vec3 maxPos(std::numeric_limits<float>::lowest());
    //        const auto& corners = m_overheadCam.getComponent<cro::Camera>().getFrustumCorners();

    //        for (const auto& c : corners)
    //        {
    //            const auto p = camTx * c;
    //            minPos.x = std::min(minPos.x, p.x);
    //            minPos.y = std::min(minPos.y, p.y);
    //            minPos.z = std::min(minPos.z, p.z);

    //            maxPos.x = std::max(maxPos.x, p.x);
    //            maxPos.y = std::max(maxPos.y, p.y);
    //            maxPos.z = std::max(maxPos.z, p.z);
    //        }

    //        auto& bbc = parent.getComponent<cro::BillboardCollection>();
    //        minPos += glm::vec3(bbc.getChunkSize()*2.f);
    //        maxPos += glm::vec3(bbc.getChunkSize()*2.f);

    //        const auto coordStart = bbc.getChunkCoordStart();
    //        const auto relPos = glm::vec2(minPos.x, minPos.z) - coordStart;
    //        const auto xPos = static_cast<std::int32_t>(std::floor(relPos.x / bbc.getChunkSize()));
    //        const auto yPos = static_cast<std::int32_t>(std::floor(relPos.y / bbc.getChunkSize()));

    //        const auto GridSizeX = static_cast<std::int32_t>(std::floor((maxPos.x - minPos.x) / bbc.getChunkSize()));
    //        const auto startX = std::max(0, xPos);
    //        const auto endX = std::min(bbc.getChunkCount().x, std::uint32_t(xPos + GridSizeX));

    //        const auto GridSizeY = static_cast<std::int32_t>(std::floor((maxPos.z - minPos.z) / bbc.getChunkSize()));
    //        const auto startY = std::max(0, yPos);
    //        const auto endY = std::min(bbc.getChunkCount().y, std::uint32_t(yPos + GridSizeY));

    //        auto& drawList = bbc.getDrawList(camera.getComponent<cro::Camera>().getDrawListIndex());
    //        drawList.clear();

    //        const auto& mesh = parent.getComponent<cro::Model>().getMeshData();

    //        //min / max draw distances
    //        static constexpr float MinDist = 1.f;
    //        static constexpr float MaxDist = 10.f;
    //        static constexpr float MinDist2 = MinDist * MinDist;
    //        static constexpr float MaxDist2 = MaxDist * MaxDist;

    //        for (auto y = startY; y < endY; ++y)
    //        {
    //            for (auto x = startX; x < endX; ++x)
    //            {
    //                const auto idx = (y * bbc.getChunkCount().x) + x;
    //                const auto& chunk = bbc.getChunkIndices()[idx];

    //                const glm::vec3 centre = 
    //                    glm::vec3(coordStart.x + (x * bbc.getChunkSize()) , 
    //                                chunk.meanHeight, 
    //                                coordStart.y + (y * bbc.getChunkSize())) + glm::vec3(bbc.getChunkSize() / 2.f);

    //                const auto dir = centre - testPos;
    //                if (const auto len2 = glm::length2(dir); len2 < MaxDist2 && len2 > MinDist2)
    //                {
    //                    //const float hFOV = std::tan(cam.getFOV() / 2.f) * cam.getAspectRatio();
    //                    const float hFOV = 0.5f; //nice idea but a) assumes horizontal is widest axis b) doesn't account for AABB overlap
    //                    //if (const auto d = glm::dot(glm::normalize(dir), cro::Util::Matrix::getForwardVector(camTx)); d > hFOV)
    //                    {
    //                        drawList.addChunk(mesh, chunk);
    //                    }
    //                }
    //            }
    //        }
    //    });


    const auto tx = glm::lookAt({ 0.f, 4.f, 11.f }, { 0.f,  0.f, 0.f }, cro::Transform::Y_AXIS);
    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setLocalTransform(glm::inverse(tx));

    m_gameScene.getSunlight().getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, -0.439f);
    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.777f);


    const auto resize2 = 
        [](cro::Camera& cam)
        {
            glm::vec2 size(cro::App::getWindow().getSize());
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
            cam.setPerspective(70.f * cro::Util::Const::degToRad, 1.f, 0.5f, 30.f);
        };

    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -cro::Util::Const::PI / 2.f);
    auto& cam2 = entity.addComponent<cro::Camera>();
    cam2.shadowMapBuffer.create(2048, 2048);
    cam2.setMaxShadowDistance(20.f);
    cam2.setShadowExpansion(17.f);
    //cam2.setBlurPassCount(1);
    resize2(cam2);
    cam2.resizeCallback = resize2;

    m_overheadCam = entity;
    sphereEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    bbc.setDrawListCallback(entity, cro::DefaultBillboardCallback(1.f, 50.f));

    m_overheadTexture.create(240, 240);
    m_overheadQuad.setTexture(m_overheadTexture.getTexture());
    m_overheadQuad.setPosition({ 10.f, 10.f });


    registerWindow([this, entity, camEnt]() 
        {
            ImGui::Begin("Cameras");
            const auto& cam1 = camEnt.getComponent<cro::Camera>();
            ImGui::Text("Camera 1 DMap %d", cam1.shadowMapBuffer.getTexture().textureID);

            const auto& cam2 = entity.getComponent<cro::Camera>();
            ImGui::Text("Camera 2 DMap %d", cam2.shadowMapBuffer.getTexture().textureID);
            ImGui::End();
        });
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