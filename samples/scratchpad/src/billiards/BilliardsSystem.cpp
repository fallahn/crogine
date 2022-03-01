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

#include "BilliardsSystem.hpp"
#include "../collision/DebugDraw.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/detail/glm/gtc/type_ptr.hpp>

namespace
{
    glm::vec3 btToGlm(btVector3 v)
    {
        return { v.getX(), v.getY(), v.getZ() };
    }
}

BilliardsSystem::BilliardsSystem(cro::MessageBus& mb, BulletDebug& dd)
    : cro::System(mb, typeid(BilliardsSystem)),
    m_debugDrawer(dd)
{
    requireComponent<BilliardBall>();
    requireComponent<cro::Transform>();

    m_ballShape = std::make_unique<btSphereShape>(0.0255f);
}

BilliardsSystem::~BilliardsSystem()
{
    for (auto& o : m_ballObjects)
    {
        m_collisionWorld->removeCollisionObject(o.get());
    }

    for (auto& o : m_tableObjects)
    {
        m_collisionWorld->removeCollisionObject(o.get());
    }
}

//public
void BilliardsSystem::process(float dt)
{
    m_collisionWorld->stepSimulation(dt, 10);
    m_collisionWorld->debugDrawWorld();

    //update all ball positions
    static std::array<float, 16> matrixBuffer = {};

    auto& entities = getEntities();
    for (auto entity : entities)
    {
        const auto& ball = entity.getComponent<BilliardBall>();
        auto& tx = entity.getComponent<cro::Transform>();

        const auto& btx = ball.physicsBody->getWorldTransform();
        btx.getOpenGLMatrix(matrixBuffer.data());
        auto mat = glm::make_mat4(matrixBuffer.data());

        tx.setPosition(glm::vec3(mat[3]));
        tx.setRotation(glm::quat_cast(mat));
    }
}

void BilliardsSystem::initTable(const cro::Mesh::Data& meshData)
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

    //m_collisionWorld = std::make_unique<btCollisionWorld>(m_collisionDispatcher.get(), m_broadphaseInterface.get(), m_collisionConfiguration.get());

    m_collisionWorld->setDebugDrawer(&m_debugDrawer);
    m_collisionWorld->setGravity({ 0.f, -9.f, 0.f });

    cro::Mesh::readVertexData(meshData, m_vertexData, m_indexData);

    //TODO split the collision type by sub-mesh
    //TODO make sure cusion has nicreased restitution
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
        info.m_friction = 0.1f;
        info.m_restitution = 1.f;

        auto& body = m_tableObjects.emplace_back(std::make_unique<btRigidBody>(info));
        body->setWorldTransform(transform);
        //body->setCollisionFlags(btCollisionObject::CF_STATIC_OBJECT);

        //body->setUserIndex(static_cast<std::int32_t>(terrain)); // set the terrain type
        //m_collisionWorld->addCollisionObject(body.get());
        m_collisionWorld->addRigidBody(body.get());
    }
}

#include <crogine/util/Random.hpp>
void BilliardsSystem::applyImpulse()
{
    if (m_ballObjects.size() > 1)
    {
        auto& obj = m_ballObjects[cro::Util::Random::value(0u, m_ballObjects.size() - 1)];
        obj->applyCentralImpulse({ 0.f, 0.f, -0.5f });
    }
}

//private
void BilliardsSystem::onEntityAdded(cro::Entity entity)
{
    btTransform transform;
    transform.setFromOpenGLMatrix(&entity.getComponent<cro::Transform>().getWorldTransform()[0][0]);
    
    btRigidBody::btRigidBodyConstructionInfo info(0.156f, nullptr, m_ballShape.get());
    info.m_restitution = 0.01f;
    info.m_rollingFriction = 0.01f;
    info.m_spinningFriction = 0.01f;
    info.m_friction = 0.01f;
    info.m_linearSleepingThreshold = 0.f;
    info.m_angularSleepingThreshold = 0.f;

    auto& body = m_ballObjects.emplace_back(std::make_unique<btRigidBody>(info));
    body->setWorldTransform(transform);
    
    m_collisionWorld->addRigidBody(body.get());

    entity.getComponent<BilliardBall>().physicsBody = body.get();
}

void BilliardsSystem::onEntityRemoved(cro::Entity entity)
{
    auto* body = entity.getComponent<BilliardBall>().physicsBody;

    m_collisionWorld->removeRigidBody(body);

    m_ballObjects.erase(std::remove_if(m_ballObjects.begin(), m_ballObjects.end(), 
        [body](const std::unique_ptr<btRigidBody>& b)
        {
            return b.get() == body;
        }), m_ballObjects.end());
}