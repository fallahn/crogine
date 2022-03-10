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
#include <crogine/graphics/MeshBuilder.hpp>

namespace
{
    glm::vec3 btToGlm(btVector3 v)
    {
        return { v.getX(), v.getY(), v.getZ() };
    }
}

void BilliardBall::getWorldTransform(btTransform& dest) const
{
    const auto& tx = m_parent.getComponent<cro::Transform>();
    dest.setFromOpenGLMatrix(&tx.getWorldTransform()[0][0]);
}

void BilliardBall::setWorldTransform(const btTransform& src)
{
    static std::array<float, 16> matrixBuffer = {};

    src.getOpenGLMatrix(matrixBuffer.data());
    auto mat = glm::make_mat4(matrixBuffer.data());

    auto& tx = m_parent.getComponent<cro::Transform>();
    tx.setPosition(glm::vec3(mat[3]));
    tx.setRotation(glm::quat_cast(mat));
}

const std::array<std::string, CollisionID::Count> CollisionID::Labels =
{
    "Table", "Cushion", "Ball", "Pocket"
};

BilliardsSystem::BilliardsSystem(cro::MessageBus& mb, BulletDebug& dd)
    : cro::System(mb, typeid(BilliardsSystem)),
    m_debugDrawer(dd)
{
    requireComponent<BilliardBall>();
    requireComponent<cro::Transform>();

    m_ballShape = std::make_unique<btSphereShape>(0.0255f);

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

    for (auto entity : getEntities())
    {
        auto& ball = entity.getComponent<BilliardBall>();
        if (ball.m_physicsBody->isActive())
        {
            const auto position = entity.getComponent<cro::Transform>().getPosition();

            //if below the table check for pocketry
            if (position.y < 0)
            {
                auto lastContact = ball.m_pocketContact;
                ball.m_pocketContact = -1;
                for (const auto& pocket : m_pockets)
                {
                    if (pocket.box.contains(position))
                    {
                        ball.m_pocketContact = pocket.id;
                        break;
                    }
                }

                if (lastContact != ball.m_pocketContact)
                {
                    if (ball.m_pocketContact != -1)
                    {
                        //contact begin
                        LogI << "Ball " << ball.id << " in pocket " << ball.m_pocketContact << std::endl;
                    }
                    else
                    {
                        //contact end
                        LogI << "Ball " << ball.id << " finished contact" << std::endl;
                    }
                }
            }
            //check if we disable table contact by looking at our proximity to the pocket
            else
            {
                auto lastContact = ball.m_pocketRadius;
                ball.m_pocketRadius = false;

                for (const auto& pocket : m_pockets)
                {
                    if (glm::length2(pocket.position - glm::vec2(position.x, position.z)) < Pocket::Radius * Pocket::Radius)
                    {
                        ball.m_pocketRadius = true;
                        break;
                    }
                }

                if (lastContact != ball.m_pocketRadius)
                {
                    auto flags = (1 << CollisionID::Cushion) | (1 << CollisionID::Ball);
                    if (!ball.m_pocketRadius)
                    {
                        flags |= (1 << CollisionID::Table);
                    }
                    //apparently the only way to change the grouping - however network lag
                    //seems to cover up any jitter...
                    m_collisionWorld->removeRigidBody(ball.m_physicsBody);
                    m_collisionWorld->addRigidBody(ball.m_physicsBody, (1 << CollisionID::Ball), flags);
                    
                    //hmmm setting this directly doesn't work :(
                    //ball.m_physicsBody->getBroadphaseProxy()->m_collisionFilterGroup = (1 << CollisionID::Ball);
                    //ball.m_physicsBody->getBroadphaseProxy()->m_collisionFilterMask = flags;
                }
            }
        }
    }
}

void BilliardsSystem::initTable(const cro::Mesh::Data& meshData)
{
    cro::Mesh::readVertexData(meshData, m_vertexData, m_indexData);

    /*if ((meshData.attributeFlags & cro::VertexProperty::Colour) == 0)
    {
        LogE << "Mesh has no colour property in vertices. Will not be created." << std::endl;
        return;
    }*/

    if (m_vertexData.empty() || m_indexData.empty())
    {
        LogE << "No collision mesh was loaded" << std::endl;
        return;
    }

    /*std::size_t colourOffset = 0;
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


        //float collisionID = m_vertexData[(m_indexData[i][0] * (meshData.vertexSize / sizeof(float))) + colourOffset] * 255.f;
        //collisionID = std::floor(collisionID / 10.f);

        m_tableVertices.emplace_back(std::make_unique<btTriangleIndexVertexArray>())->addIndexedMesh(tableMesh);
        m_tableShapes.emplace_back(std::make_unique<btBvhTriangleMeshShape>(m_tableVertices.back().get(), false));


        //auto& body = m_tableObjects.emplace_back(std::make_unique<btPairCachingGhostObject>());
        //body->setCollisionShape(m_tableShapes.back().get());
        //body->setUserIndex(static_cast<std::int32_t>(collisionID));
        //m_collisionWorld->addCollisionObject(body.get());

        auto& body = m_tableObjects.emplace_back(std::make_unique<btRigidBody>(createBodyDef(CollisionID::Cushion/*collisionID*/, 0.f, m_tableShapes.back().get())));
        body->setWorldTransform(transform);
        //body->setUserIndex(static_cast<std::int32_t>(collisionID));
        m_collisionWorld->addRigidBody(body.get(), (1<< CollisionID::Cushion), (1 << CollisionID::Ball));
    }

    //create a single flat surface for the table as even a few triangles perturb
    //the physics. Balls check their proximity to pockets and disable table collision
    //when they need to.
    auto& tableShape = m_boxShapes.emplace_back(std::make_unique<btBoxShape>(btBoxShape(/*btVector3(0.93425f, 0.1f, 1.666f) / 2.f)*/btVector3(0.6f, 0.05f, 1.f))));
    transform.setOrigin({ 0.f, -0.05f, 0.f });
    auto& table = m_tableObjects.emplace_back(std::make_unique<btRigidBody>(createBodyDef(CollisionID::Table, 0.f, tableShape.get())));
    table->setWorldTransform(transform);
    m_collisionWorld->addCollisionObject(table.get(), (1 << CollisionID::Table), (1 << CollisionID::Ball));

    //create triggers for each pocket
    //TODO read this from some table data somewhere
    const std::array PocketPositions =
    {
        glm::vec3(0.47f, -0.1f, -0.93f),
        glm::vec3(0.54f, -0.1f, 0.f),
        glm::vec3(0.47f, -0.1f, 0.93f),

        glm::vec3(-0.47f, -0.1f, -0.93f),
        glm::vec3(-0.54f, -0.1f, 0.f),
        glm::vec3(-0.47f, -0.1f, 0.93f)
    };

    const glm::vec3 PocketHalfSize({ 0.075f, 0.1f, 0.075f });
    auto i = 0;

#ifdef CRO_DEBUG_
    m_pocketShape = std::make_unique<btCylinderShape>(btVector3(Pocket::Radius, 0.01f, Pocket::Radius));
#endif

    for (auto p : PocketPositions)
    {
        //place these in world space for simpler testing
        auto& pocket = m_pockets.emplace_back();
        pocket.box = { p - PocketHalfSize, p + PocketHalfSize };
        pocket.id = i++;
        pocket.position = { p.x, p.z };

#ifdef CRO_DEBUG_
        //just so something shows in debug drawer
        //collision is actually done in process() by checking radial proximity.
        auto& cylinder = m_tableObjects.emplace_back(std::make_unique<btRigidBody>(createBodyDef(CollisionID::Table, 0.f, m_pocketShape.get())));
        cylinder->setCollisionFlags(cylinder->getCollisionFlags() | btRigidBody::CollisionFlags::CF_NO_CONTACT_RESPONSE);
        transform.setOrigin({ p.x, 0.f, p.z });
        cylinder->setWorldTransform(transform);
        m_collisionWorld->addCollisionObject(cylinder.get());
#endif
    }
}

#include <crogine/util/Random.hpp>
void BilliardsSystem::applyImpulse()
{
    if (m_ballObjects.size() > 1)
    {
        auto& obj = m_ballObjects[cro::Util::Random::value(0u, m_ballObjects.size() - 1)];
        obj->activate();
        obj->applyCentralImpulse({ 0.2f * ((cro::Util::Random::value(0,1) * 2) - 1), 0.f, -0.2f * ((cro::Util::Random::value(0,1) * 2) - 1) });
    }
}

//private
btRigidBody::btRigidBodyConstructionInfo BilliardsSystem::createBodyDef(std::int32_t collisionID, float mass, btCollisionShape* shape, btMotionState* motionState)
{
    btRigidBody::btRigidBodyConstructionInfo info(mass, motionState, shape);

    switch (collisionID)
    {
    default: break;
    case CollisionID::Table:
        
        break;
    case CollisionID::Cushion:
        info.m_restitution = 0.5f;
        break;
    case CollisionID::Ball:
        info.m_restitution = 0.5f;
        info.m_rollingFriction = 0.1f;
        info.m_spinningFriction = 0.1f;
        info.m_friction = 0.01f;
        info.m_linearSleepingThreshold = 0.001f; //if this is 0 then we never sleep...
        info.m_angularSleepingThreshold = 0.001f;
        break;
    }

    return info;
}

void BilliardsSystem::onEntityAdded(cro::Entity entity)
{
    auto& ball = entity.getComponent<BilliardBall>();
    ball.m_parent = entity;

    btTransform transform;
    transform.setFromOpenGLMatrix(&entity.getComponent<cro::Transform>().getWorldTransform()[0][0]);
    
    auto& body = m_ballObjects.emplace_back(std::make_unique<btRigidBody>(createBodyDef(CollisionID::Ball, 0.156f, m_ballShape.get(), &ball)));
    body->setWorldTransform(transform);
    body->setUserIndex(CollisionID::Ball);
    
    m_collisionWorld->addRigidBody(body.get(), (1 << CollisionID::Ball), (1 << CollisionID::Table) | (1 << CollisionID::Cushion) | (1 << CollisionID::Ball));

    ball.m_physicsBody = body.get();
}

void BilliardsSystem::onEntityRemoved(cro::Entity entity)
{
    auto* body = entity.getComponent<BilliardBall>().m_physicsBody;

    m_collisionWorld->removeRigidBody(body);

    m_ballObjects.erase(std::remove_if(m_ballObjects.begin(), m_ballObjects.end(), 
        [body](const std::unique_ptr<btRigidBody>& b)
        {
            return b.get() == body;
        }), m_ballObjects.end());
}