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

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/util/Constants.hpp>

CollisionState::CollisionState(cro::StateStack& ss, cro::State::Context ctx)
    : cro::State    (ss, ctx),
    m_scene         (ctx.appInstance.getMessageBus()),
    m_ballShape     (0.51f)
{
    buildScene();
}

//public
bool CollisionState::handleEvent(const cro::Event& evt)
{
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

    m_collisionWorld->performDiscreteCollisionDetection();
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

    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::ShadowMapRenderer>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::ModelRenderer>(mb);


    cro::ModelDefinition md(m_resources);
    md.loadFromFile("assets/models/sphere.cmt");

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 12.f, 0.f });
    md.createModel(entity);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        const auto& tx = e.getComponent<cro::Transform>();
        auto rot = tx.getRotation();
        auto pos = tx.getWorldPosition();
        btTransform btXf(btQuaternion(rot.x, rot.y, rot.z, rot.w), btVector3(pos.x, pos.y, pos.z));
        m_ballObject.setWorldTransform(btXf);
    };

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

    //for (const auto& indexData : m_indexData)
    {
        m_groundMesh = std::make_unique<btIndexedMesh>();
        m_groundMesh->m_vertexBase = reinterpret_cast<std::uint8_t*>(m_vertexData.data());
        m_groundMesh->m_numVertices = meshData.vertexCount;
        m_groundMesh->m_vertexStride = meshData.vertexSize;

        m_groundMesh->m_numTriangles = meshData.indexData[0].indexCount / 3;
        m_groundMesh->m_triangleIndexBase = reinterpret_cast<std::uint8_t*>(m_indexData[0].data());
        m_groundMesh->m_triangleIndexStride = 3 * sizeof(std::uint32_t);
    }

    m_groundVertices = std::make_unique<btTriangleIndexVertexArray>();
    m_groundVertices->addIndexedMesh(*m_groundMesh);

    m_groundShape = std::make_unique<btBvhTriangleMeshShape>(m_groundVertices.get(), false);
    m_groundObject.setCollisionShape(m_groundShape.get());

    m_collisionWorld->addCollisionObject(&m_groundObject);

    //we don't need to keep the mesh struct around
    //but we do need to maintain the vertex data.
    m_groundMesh.reset();
}