/*-----------------------------------------------------------------------

Matt Marchant 2021
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include "CollisionState.hpp"
#include "BallSystem.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/util/Constants.hpp>

namespace
{
    bool showDebug = false;
}

CollisionState::CollisionState(cro::StateStack& ss, cro::State::Context ctx)
    : cro::State    (ss, ctx),
    m_scene         (ctx.appInstance.getMessageBus()),
    m_ballShape     (0.22f)
{
    buildScene();
}

//public
bool CollisionState::handleEvent(const cro::Event& evt)
{
    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_l:
        {
            auto& ball = m_ballEntity.getComponent<Ball>();
            ball.state = Ball::State::Awake;
            ball.velocity += glm::vec3(0.01f, 25.f, -0.02f);
        }
            break;
        case SDLK_p:
            showDebug = !showDebug;
            m_debugDrawer.setDebugMode(showDebug ? std::numeric_limits<std::int32_t>::max() : 0);
        }
    }

    m_scene.forwardEvent(evt);

    return false;
}

void CollisionState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool CollisionState::simulate(float dt)
{
    m_scene.simulate(dt);

    m_collisionWorld->debugDrawWorld();

    return false;
}

void CollisionState::render()
{
    auto& rw = cro::App::getWindow();
    m_scene.render(rw);

    //render the debug stuff
    auto viewProj = m_scene.getActiveCamera().getComponent<cro::Camera>().getActivePass().viewProjectionMatrix;
    m_debugDrawer.render(viewProj);
}

//private
void CollisionState::buildScene()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_scene.addSystem<BallSystem>(mb, m_collisionWorld);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::ShadowMapRenderer>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::ModelRenderer>(mb);


    cro::ModelDefinition md(m_resources);
    md.loadFromFile("assets/models/sphere.cmt");

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 12.f, 0.f });
    md.createModel(entity);
    entity.addComponent<Ball>().collisionObject = &m_ballObject;
    entity.getComponent<Ball>().entity = entity;
    m_ballObject.setUserPointer(&entity.getComponent<Ball>());
    m_ballEntity = entity;

    md.loadFromFile("assets/collision/models/physics_test.cmt");
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    md.createModel(entity);
    setupCollisionWorld(entity.getComponent<cro::Model>().getMeshData());

    auto callback = [](cro::Camera& cam)
    {
        glm::vec2 winSize(cro::App::getWindow().getSize());
        cam.setPerspective(60.f * cro::Util::Const::degToRad, winSize.x / winSize.y, 0.1f, 80.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };
    auto& camera = m_scene.getActiveCamera().getComponent<cro::Camera>();
    camera.resizeCallback = callback;
    camera.shadowMapBuffer.create(1024, 1024);
    callback(camera);

    m_scene.getActiveCamera().getComponent<cro::Transform>().move({ 0.f, 10.f, 11.f });
    m_scene.getActiveCamera().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -20.f * cro::Util::Const::degToRad);

    m_scene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -25.f * cro::Util::Const::degToRad);
    m_scene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -25.f * cro::Util::Const::degToRad);
}

void CollisionState::setupCollisionWorld(const cro::Mesh::Data& meshData)
{
    m_collisionConfiguration = std::make_unique<btDefaultCollisionConfiguration>();
    m_collisionDispatcher = std::make_unique<btCollisionDispatcher>(m_collisionConfiguration.get());

    m_broadphaseInterface = std::make_unique<btDbvtBroadphase>();
    m_collisionWorld = std::make_unique<btCollisionWorld>(m_collisionDispatcher.get(), m_broadphaseInterface.get(), m_collisionConfiguration.get());

    m_collisionWorld->setDebugDrawer(&m_debugDrawer);

    m_ballObject.setCollisionShape(&m_ballShape);
    m_collisionWorld->addCollisionObject(&m_ballObject);


    cro::Mesh::readVertexData(meshData, m_vertexData, m_indexData);
    m_groundVertices = std::make_unique<btTriangleIndexVertexArray>();


    //for (const auto& indexData : m_indexData)
    {
        btIndexedMesh groundMesh;
        groundMesh.m_vertexBase = reinterpret_cast<std::uint8_t*>(m_vertexData.data());
        groundMesh.m_numVertices = meshData.vertexCount;
        groundMesh.m_vertexStride = meshData.vertexSize;

        groundMesh.m_numTriangles = meshData.indexData[0].indexCount / 3;
        groundMesh.m_triangleIndexBase = reinterpret_cast<std::uint8_t*>(m_indexData[0].data());
        groundMesh.m_triangleIndexStride = 3 * sizeof(std::uint32_t);

        m_groundVertices->addIndexedMesh(groundMesh);
    }

    m_groundShape = std::make_unique<btBvhTriangleMeshShape>(m_groundVertices.get(), false);
    m_groundObject.setCollisionShape(m_groundShape.get());

    m_collisionWorld->addCollisionObject(&m_groundObject);
}