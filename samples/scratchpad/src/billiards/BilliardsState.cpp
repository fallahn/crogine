/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include "BilliardsState.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>

#include <crogine/util/Constants.hpp>

namespace
{
    bool showDebug = false;
}

BilliardsState::BilliardsState(cro::StateStack& ss, cro::State::Context ctx)
    : cro::State(ss, ctx),
    m_scene(ctx.appInstance.getMessageBus())
{
    addSystems();
    buildScene();
}

BilliardsState::~BilliardsState()
{
    for (auto& o : m_tableObjects)
    {
        m_collisionWorld->removeCollisionObject(o.get());
    }
}

//public
bool BilliardsState::handleEvent(const cro::Event& evt)
{
    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_p:
            showDebug = !showDebug;
            m_debugDrawer.setDebugMode(showDebug ? /*std::numeric_limits<std::int32_t>::max()*/3 : 0);
            break;
        case SDLK_BACKSPACE:
            requestStackPop();
            requestStackPush(States::ScratchPad::MainMenu);
            break;
        }
    }

    m_scene.forwardEvent(evt);
    return false;
}

void BilliardsState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool BilliardsState::simulate(float dt)
{
    m_collisionWorld->stepSimulation(dt, 10);
    m_collisionWorld->debugDrawWorld();

    m_scene.simulate(dt);
    return false;
}

void BilliardsState::render()
{
    m_scene.render();

    auto viewProj = m_scene.getActiveCamera().getComponent<cro::Camera>().getActivePass().viewProjectionMatrix;
    m_debugDrawer.render(viewProj);
}

//private
void BilliardsState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::ModelRenderer>(mb);
}

void BilliardsState::buildScene()
{
    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/billiards/pool_collision.cmt"))
    {
        //table
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);

        initCollision(entity.getComponent<cro::Model>().getMeshData());
    }

    //ball
    if (md.loadFromFile("assets/billiards/ball.cmt"))
    {
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.5f, 0.f });
        md.createModel(entity);
    }


    //camera / sunlight
    auto callback = [](cro::Camera& cam)
    {
        glm::vec2 winSize(cro::App::getWindow().getSize());
        cam.setPerspective(60.f * cro::Util::Const::degToRad, winSize.x / winSize.y, 0.1f, 80.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };
    auto& camera = m_scene.getActiveCamera().getComponent<cro::Camera>();
    camera.resizeCallback = callback;
    //camera.shadowMapBuffer.create(2048, 2048);
    callback(camera);

    m_scene.getActiveCamera().getComponent<cro::Transform>().move({ 0.f, 1.f, 2.f });
    m_scene.getActiveCamera().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -28.f * cro::Util::Const::degToRad);

    m_scene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -25.f * cro::Util::Const::degToRad);
    m_scene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -25.f * cro::Util::Const::degToRad);
}

void BilliardsState::initCollision(const cro::Mesh::Data& meshData)
{
    m_collisionConfiguration = std::make_unique<btDefaultCollisionConfiguration>();
    m_collisionDispatcher = std::make_unique<btCollisionDispatcher>(m_collisionConfiguration.get());
    m_broadphaseInterface = std::make_unique<btDbvtBroadphase>();
    m_constraintSolver = std::make_unique<btSequentialImpulseConstraintSolver>();
    m_collisionWorld = std::make_unique<btDiscreteDynamicsWorld>(
        m_collisionDispatcher.get(),
        m_broadphaseInterface.get(),
        m_constraintSolver.get(),
        m_collisionConfiguration.get());

    //m_collisionWorld = std::make_unique<btCollisionWorld>(m_collisionDispatcher.get(), m_broadphaseInterface.get(), m_collisionConfiguration.get());

    m_collisionWorld->setDebugDrawer(&m_debugDrawer);
    m_collisionWorld->setGravity({ 0.f, -10.f, 0.f });

    cro::Mesh::readVertexData(meshData, m_vertexData, m_indexData);

    //TODO split the collision type by sub-mesh
    /*if ((meshData.attributeFlags & cro::VertexProperty::Colour) == 0)
    {
        LogE << "Mesh has no colour property in vertices. Ground will not be created." << std::endl;
        return;
    }

    std::int32_t colourOffset = 0;
    for (auto i = 0; i < cro::Mesh::Attribute::Colour; ++i)
    {
        colourOffset += meshData.attributes[i];
    }*/

    btTransform transform;
    transform.setIdentity();

    for (auto i = 0u; i < m_indexData.size(); ++i)
    {
        btIndexedMesh tableMesh;
        tableMesh.m_vertexBase = reinterpret_cast<std::uint8_t*>(m_vertexData.data());
        tableMesh.m_numVertices = static_cast<std::int32_t>(meshData.vertexCount);
        tableMesh.m_vertexStride = static_cast<std::int32_t>(meshData.vertexSize);

        tableMesh.m_numTriangles = meshData.indexData[i].indexCount / 3;
        tableMesh.m_triangleIndexBase = reinterpret_cast<std::uint8_t*>(m_indexData[i].data());
        tableMesh.m_triangleIndexStride = 3 * sizeof(std::uint32_t);


        //float terrain = m_vertexData[(m_indexData[i][0] * (meshData.vertexSize / sizeof(float))) + colourOffset] * 255.f;
        //terrain = std::floor(terrain / 10.f);

        m_tableVertices.emplace_back(std::make_unique<btTriangleIndexVertexArray>())->addIndexedMesh(tableMesh);
        m_tableShapes.emplace_back(std::make_unique<btBvhTriangleMeshShape>(m_tableVertices.back().get(), false));

        
        //auto& body = m_tableObjects.emplace_back(std::make_unique<btPairCachingGhostObject>());
        //body->setCollisionShape(m_tableShapes.back().get());

        btRigidBody::btRigidBodyConstructionInfo info(0.f, nullptr, m_tableShapes.back().get());
        auto& body = m_tableObjects.emplace_back(std::make_unique<btRigidBody>(info));
        body->setWorldTransform(transform);
        body->setCollisionFlags(btCollisionObject::CF_STATIC_OBJECT);

        //body->setUserIndex(static_cast<std::int32_t>(terrain)); // set the terrain type
        m_collisionWorld->addCollisionObject(body.get());
    }
}