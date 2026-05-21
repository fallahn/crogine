//Auto-generated source file for Scratchpad Stub 25/11/2025, 09:58:27

#include "DeckIntroState.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Random.hpp>

#include <crogine/detail/glm/gtc/type_ptr.hpp>

namespace
{
    constexpr glm::vec2 ScreenArea = glm::vec2(1.44f, 0.9f);
    constexpr float BallRadius = 0.08f;
    constexpr float LogoRadius = 0.4f;
}

DeckIntroState::DeckIntroState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus(),1024),
    m_uiScene       (context.appInstance.getMessageBus()),
    m_ballDefinition(m_resources, &m_envMap)
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    });
}

DeckIntroState::~DeckIntroState()
{
    if (m_logoBody)
    {
        m_collisionWorld->removeRigidBody(m_logoBody.get());
    }

    for (auto& body : m_ballBodies)
    {
        m_collisionWorld->removeRigidBody(body.get());
    }

    for (auto& body : m_planeBodies)
    {
        if (body)
        {
            m_collisionWorld->removeRigidBody(body.get());
        }
    }
}

//public
bool DeckIntroState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_BACKSPACE:
        case SDLK_ESCAPE:
            requestStackClear();
            requestStackPush(0);
            break;
        case SDLK_l:
            spawnBall({cro::Util::Random::value(-0.7f, 0.7f), ScreenArea.y / 2.f, 0.f});
            break;
        case SDLK_k:
            spawnLogo();
            break;
        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void DeckIntroState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool DeckIntroState::simulate(float dt)
{
    m_collisionWorld->stepSimulation(dt, 10);

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void DeckIntroState::render()
{
    m_gameScene.render();
    m_uiScene.render();
}

//private
void DeckIntroState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void DeckIntroState::loadAssets()
{
    m_envMap.loadFromFile("assets/images/hills.hdr");
    m_ballDefinition.loadFromFile("assets/arcade/sportsball/models/golf_ball.cmt");
}

void DeckIntroState::createScene()
{
    //note these have to be created in the right order so that destruction
    //is properly done in reverse...
    m_collisionConfiguration = std::make_unique<btDefaultCollisionConfiguration>();
    m_collisionDispatcher = std::make_unique<btCollisionDispatcher>(m_collisionConfiguration.get());
    m_broadphaseInterface = std::make_unique<btDbvtBroadphase>();
    m_constraintSolver = std::make_unique<btSequentialImpulseConstraintSolver>();
    m_collisionWorld = std::make_unique<btDiscreteDynamicsWorld>(
        m_collisionDispatcher.get(),
        m_broadphaseInterface.get(),
        m_constraintSolver.get(),
        m_collisionConfiguration.get());


    //balls are 0.05 radius
    m_ballShape = std::make_unique<btSphereShape>(BallRadius + 0.005f);
    m_logoShape = std::make_unique<btSphereShape>(LogoRadius);

    //screen area is 1.44m x 0.9m
    m_planeShapes[PlaneShape::Ground] = std::make_unique<btStaticPlaneShape>(btVector3(0.f, 1.f, 0.f), -ScreenArea.y / 2.f);
    m_planeShapes[PlaneShape::Left] = std::make_unique<btStaticPlaneShape>(btVector3(1.f, 0.f, 0.f), -ScreenArea.x / 2.f);
    m_planeShapes[PlaneShape::Right] = std::make_unique<btStaticPlaneShape>(btVector3(-1.f, 0.f, 0.f), -ScreenArea.x / 2.f);
    m_planeShapes[PlaneShape::Rear] = std::make_unique<btStaticPlaneShape>(btVector3(0.f, 0.f, 1.f), -BallRadius / 1.8f);
    m_planeShapes[PlaneShape::Front] = std::make_unique<btStaticPlaneShape>(btVector3(0.f, 0.f, -1.f), -BallRadius / 1.8f);


    for (auto i = 0; i < PlaneShape::Count; ++i)
    {
        btRigidBody::btRigidBodyConstructionInfo info(0.f, nullptr, m_planeShapes[i].get());
        info.m_restitution = 0.8f;
        info.m_friction = 0.01f;
        m_planeBodies[i] = std::make_unique<btRigidBody>(info);
        m_collisionWorld->addRigidBody(m_planeBodies[i].get());
    }

    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        cam.setOrthographic(-ScreenArea.x / 2.f, ScreenArea.x / 2.f, -ScreenArea.y / 2.f, ScreenArea.y / 2.f, 0.01f, 10.f);
    };

    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);

    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 0.f, 0.f, 2.f });
}

void DeckIntroState::createUI()
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

void DeckIntroState::spawnBall(glm::vec3 pos)
{
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(pos);
    entity.getComponent<cro::Transform>().setScale(glm::vec3(BallRadius));
    //entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, cro::Util::Random::value(-1.f, 1.f));
    //entity.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, cro::Util::Random::value(-1.f, 1.f));
    m_ballDefinition.createModel(entity);


    auto& ballMotion = entity.addComponent<BallMotion>();
    ballMotion.parent = entity;

    static constexpr float Mass = 0.05f;
    btVector3 inertia = { 0.f, 0.f, 0.f };
    m_ballShape->calculateLocalInertia(Mass, inertia);

    btRigidBody::btRigidBodyConstructionInfo info(0.046f, &ballMotion, m_ballShape.get(), inertia);
    info.m_restitution = 0.7f;
    info.m_friction = 1.f;

    btTransform transform;
    transform.setFromOpenGLMatrix(&entity.getComponent<cro::Transform>().getWorldTransform()[0][0]);

    auto& body = m_ballBodies.emplace_back(std::make_unique<btRigidBody>(info));
    body->setWorldTransform(transform);
    body->setCcdMotionThreshold(BallRadius);
    body->setCcdSweptSphereRadius(BallRadius * 0.5f);

    m_collisionWorld->addRigidBody(body.get());
}

void DeckIntroState::spawnLogo()
{
    if (!m_logoBody)
    {
        btRigidBody::btRigidBodyConstructionInfo info(0.f, nullptr, m_logoShape.get());
        info.m_restitution = 0.7f;

        m_logoBody = std::make_unique<btRigidBody>(info);
        m_collisionWorld->addRigidBody(m_logoBody.get());
    }
}

//ball motion state
void BallMotion::getWorldTransform(btTransform& dest) const
{
    const auto& tx = parent.getComponent<cro::Transform>();
    dest.setFromOpenGLMatrix(&tx.getWorldTransform()[0][0]);
}

void BallMotion::setWorldTransform(const btTransform& src)
{
    static std::array<float, 16> matrixBuffer = {};

    src.getOpenGLMatrix(matrixBuffer.data());
    auto mat = glm::make_mat4(matrixBuffer.data());

    auto& tx = parent.getComponent<cro::Transform>();
    tx.setPosition(glm::vec3(mat[3]));
    tx.setRotation(glm::quat_cast(mat));
}