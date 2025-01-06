/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2023
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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
#include "DebugDraw.hpp"
#include "GameConsts.hpp"
#include "server/ServerMessages.hpp"

#include <crogine/core/ConfigFile.hpp>
#include <crogine/detail/ModelBinary.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/detail/glm/gtc/type_ptr.hpp>
#include <crogine/graphics/MeshBuilder.hpp>
#include <crogine/util/Random.hpp>

namespace
{

}

//conversion funcs are declared in GameConsts
glm::vec3 btToGlm(btVector3 v)
{
    return { v.getX(), v.getY(), v.getZ() };
}

btVector3 glmToBt(glm::vec3 v)
{
    return { v.x, v.y, v.z };
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
    const auto mat = glm::make_mat4(matrixBuffer.data());

    auto& tx = m_parent.getComponent<cro::Transform>();
    tx.setPosition(glm::vec3(mat[3]));
    tx.setRotation(glm::quat_cast(mat));

    hadUpdate = true;
}

glm::vec3 BilliardBall::getVelocity() const
{
    return btToGlm(m_physicsBody->getLinearVelocity());
}

BilliardsSystem::BilliardsSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(BilliardsSystem)),
    m_awakeCount(0),
    m_shotActive(false),
    m_cueball   (nullptr)
{
    requireComponent<BilliardBall>();
    requireComponent<cro::Transform>();

    m_ballShape = std::make_unique<btSphereShape>(BilliardBall::Radius);

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
    m_collisionWorld->setGravity({ 0.f, -9.f, 0.f });
}

BilliardsSystem::BilliardsSystem(cro::MessageBus& mb, BulletDebug& dd)
    : BilliardsSystem(mb)
{
#ifdef CRO_DEBUG_
    m_collisionWorld->setDebugDrawer(&dd);
#endif

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
    /*
    Increasing the number of steps means there's a chance of
    missing contacts when only checking at game loop speed.
    It also messes with friction values (so requires changing
    the body creation values to be changed). Seems OK like this
    anyway, else we need to look at caching contacts (?) or a more
    frequently called callback in which we can update the ball
    contact IDs
    */
    m_collisionWorld->stepSimulation(dt, 10/*, 1.f/120.f*/);

#ifdef CRO_DEBUG_
    m_collisionWorld->debugDrawWorld();
#endif

    std::int32_t awakeCount = 0;

    //we either have to iterate before AND after collision test
    //else we can do it once and accept the results are one frame late
    for (auto entity : getEntities())
    {
        auto& ball = entity.getComponent<BilliardBall>();
        if (ball.m_prevBallContact != ball.m_ballContact)
        {
            if (ball.m_ballContact == -1)
            {
                //LogI << "Ball ended contact with " << (int)ball.m_prevBallContact << std::endl;
                auto* msg = postMessage<BilliardsEvent>(sv::MessageID::BilliardsMessage);
                msg->type = BilliardsEvent::Collision;
                msg->first = ball.id;
                msg->second = ball.m_prevBallContact;
            }
            /*else
            {
                LogI << "Ball started contact with " << (int)ball.m_ballContact << std::endl;
            }*/
        }
        ball.m_prevBallContact = ball.m_ballContact;
        ball.m_ballContact = -1;

        doPocketCollision(entity);

        //tidy up rogue balls
        auto pos = entity.getComponent<cro::Transform>().getPosition();
        if (pos.y < -2.f)
        {
            getScene()->destroyEntity(entity);

            auto* msg = postMessage<BilliardsEvent>(sv::MessageID::BilliardsMessage);
            msg->type = BilliardsEvent::OutOfBounds;
            msg->first = ball.id;
            //if not in radius then we mostly likely (ugh is that reliable enough?) got knocked off the table
            msg->second = ball.m_inPocketRadius ? 1 : 0;
        }

        if (ball.m_physicsBody->isActive())
        {
            awakeCount++;
        }
    }

    doBallCollision();


    //notify if balls came to rest
    if (m_shotActive
        && awakeCount == 0
        && awakeCount != m_awakeCount)
    {
        auto* msg = postMessage<BilliardsEvent>(sv::MessageID::BilliardsMessage);
        msg->type = BilliardsEvent::TurnEnded;

        m_shotActive = false;
    }
    m_awakeCount = awakeCount;
}

void BilliardsSystem::initTable(const TableData& tableData)
{
    m_spawnArea = tableData.spawnArea;

    auto meshData = cro::Detail::ModelBinary::read(tableData.collisionModel, m_vertexData, m_indexData);

    if (m_vertexData.empty() || m_indexData.empty())
    {
        LogE << "No collision mesh was loaded" << std::endl;
        return;
    }


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


        m_tableVertices.emplace_back(std::make_unique<btTriangleIndexVertexArray>())->addIndexedMesh(tableMesh);
        m_tableShapes.emplace_back(std::make_unique<btBvhTriangleMeshShape>(m_tableVertices.back().get(), false));

        auto& body = m_tableObjects.emplace_back(std::make_unique<btRigidBody>(createBodyDef(CollisionID::Cushion, 0.f, m_tableShapes.back().get())));
        body->setWorldTransform(transform);
        m_collisionWorld->addRigidBody(body.get(), (1<< CollisionID::Cushion), (1 << CollisionID::Ball));
    }

    //create a single flat surface for the table as even a few triangles perturb
    //the physics. Balls check their proximity to pockets and disable table collision
    //when they need to. This way we can place a pocket anywhere on the surface.
    auto& tableShape = m_boxShapes.emplace_back(std::make_unique<btBoxShape>(btBoxShape(btVector3(meshData.boundingBox[1].x, 0.05f, meshData.boundingBox[1].z))));
    transform.setOrigin({ 0.f, -0.05f, 0.f });
    auto& table = m_tableObjects.emplace_back(std::make_unique<btRigidBody>(createBodyDef(CollisionID::Table, 0.f, tableShape.get())));
    table->setWorldTransform(transform);
    m_collisionWorld->addCollisionObject(table.get(), (1 << CollisionID::Table), (1 << CollisionID::Ball));

    //create triggers for each pocket
    static constexpr glm::vec3 PocketHalfSize({ 0.075f, 0.1f, 0.075f });
    static constexpr float WallThickness = 0.025f;
    static constexpr float WallHeight = 0.1f;


    auto i = 0;

#ifdef CRO_DEBUG_
    m_pocketShape = std::make_unique<btCylinderShape>(btVector3(Pocket::DefaultRadius, 0.01f, Pocket::DefaultRadius));
#endif

    m_pocketWalls[0] = std::make_unique<btBoxShape>(btVector3(Pocket::DefaultRadius, WallHeight, WallThickness));
    m_pocketWalls[1] = std::make_unique<btBoxShape>(btVector3(WallThickness, WallHeight, Pocket::DefaultRadius));

    const std::array Offsets =
    {
        btVector3(0.f, 0.f, -1.f),
        btVector3(1.f, 0.f, 0.f),
        btVector3(0.f, 0.f, 1.f),
        btVector3(-1.f, 0.f, 0.f)
    };

    for (auto p : tableData.pockets)
    {
        glm::vec3 pocketPos(p.position.x, -WallHeight, -p.position.y);
        if (p.radius <= 0)
        {
            p.radius = Pocket::DefaultRadius;
        }

        //place these in world space for simpler testing
        auto& pocket = m_pockets.emplace_back();
        pocket.box = { pocketPos - PocketHalfSize, pocketPos + PocketHalfSize };
        pocket.id = i++;
        pocket.position = { pocketPos.x, pocketPos.z };
        pocket.value = p.value;

        //wall objects
        for (auto j = 0; j < 4; ++j)
        {
            const auto& wallShape = m_pocketWalls[j % 2];
            auto& obj = m_tableObjects.emplace_back(std::make_unique<btRigidBody>(createBodyDef(CollisionID::Pocket, 0.f, wallShape.get())));
            transform.setOrigin(glmToBt(pocketPos) + (Offsets[j] * (WallThickness + p.radius)));
            obj->setWorldTransform(transform);
            m_collisionWorld->addCollisionObject(obj.get(), (1 << CollisionID::Pocket), (1 << CollisionID::Ball));
        }

#ifdef CRO_DEBUG_
        //just so something shows in debug drawer
        //collision is actually done in process() by checking radial proximity.
        auto& cylinder = m_tableObjects.emplace_back(std::make_unique<btRigidBody>(createBodyDef(CollisionID::Table, 0.f, m_pocketShape.get())));
        cylinder->setCollisionFlags(cylinder->getCollisionFlags() | btRigidBody::CollisionFlags::CF_NO_CONTACT_RESPONSE);
        transform.setOrigin({ pocketPos.x, 0.f, pocketPos.z });
        cylinder->setWorldTransform(transform);
        m_collisionWorld->addCollisionObject(cylinder.get());
#endif
    }

    if (tableData.pockets.empty())
    {
        LogW << "Pocket info was empty. This might be a boring game." << std::endl;
    }
}

void BilliardsSystem::applyImpulse(glm::vec3 dir, glm::vec3 offset)
{
    if (m_cueball)
    {
        m_cueball->activate();
        m_cueball->applyImpulse(glmToBt(dir), glmToBt(offset / 4.f));
        m_shotActive = true;

        auto* msg = postMessage<BilliardsEvent>(sv::MessageID::BilliardsMessage);
        msg->type = BilliardsEvent::TurnBegan;
    }
}

glm::vec3 BilliardsSystem::getCueballPosition() const
{
    if (m_cueball)
    {
        return btToGlm(m_cueball->getWorldTransform().getOrigin());
    }
    return glm::vec3(0.f);
}

bool BilliardsSystem::isValidSpawnPosition(glm::vec3 position) const
{
    glm::vec2 ballPos(position.x, -position.z);
    return m_spawnArea.contains(ballPos);
}

//private
btRigidBody::btRigidBodyConstructionInfo BilliardsSystem::createBodyDef(std::int32_t collisionID, float mass, btCollisionShape* shape, btMotionState* motionState)
{
    btVector3 inertia(0.f, 0.f, 0.f);
    if (mass > 0.f)
    {
        shape->calculateLocalInertia(mass, inertia);
    }


    btRigidBody::btRigidBodyConstructionInfo info(mass, motionState, shape, inertia);

    switch (collisionID)
    {
    default: break;
    case CollisionID::Table:
        //info.m_friction = 0.26f;
        info.m_friction = 0.3f;
        break;
    case CollisionID::Cushion:
        info.m_restitution = 1.f;// 0.5f;
        info.m_friction = 0.28f;
        break;
    case CollisionID::Ball:
        info.m_restitution = 0.5f;
        info.m_rollingFriction = 0.0025f;
        info.m_spinningFriction = 0.001f;// hmm if this is too high then the balls curve a lot, but too low and they never come to rest...
        //info.m_friction = 0.15f;
        info.m_friction = 0.3f;
        info.m_linearSleepingThreshold = 0.003f; //if this is 0 then we never sleep...
        info.m_angularSleepingThreshold = 0.003f;
        break;
    }

    return info;
}

void BilliardsSystem::doBallCollision() const
{
    auto manifoldCount = m_collisionDispatcher->getNumManifolds();
    for (auto i = 0; i < manifoldCount; ++i)
    {
        auto manifold = m_collisionDispatcher->getManifoldByIndexInternal(i);
        auto body0 = manifold->getBody0();
        auto body1 = manifold->getBody1();

        if (body0->getUserIndex() == CollisionID::Ball
            && body1->getUserIndex() == CollisionID::Ball)
        {
            auto contactCount = manifold->getNumContacts();
            for (auto j = 0; j < contactCount; ++j)
            {
                auto ballA = static_cast<BilliardBall*>(body0->getUserPointer());
                auto ballB = static_cast<BilliardBall*>(body1->getUserPointer());

                //don't overwrite any existing collision this frame
                ballA->m_ballContact = ballA->m_ballContact == -1 ? ballB->id : ballA->m_ballContact;
                ballB->m_ballContact = ballB->m_ballContact == -1 ? ballA->id : ballB->m_ballContact;
            }
        }
    }
}

void BilliardsSystem::doPocketCollision(cro::Entity entity) const
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
                    auto* msg = postMessage<BilliardsEvent>(sv::MessageID::BilliardsMessage);
                    msg->type = BilliardsEvent::Pocket;
                    msg->first = ball.id;
                    msg->second = ball.m_pocketContact;
                }
                else
                {
                    //contact end
                    //TODO raise message?
                }
            }
        }
        //check if we disable table contact by looking at our proximity to the pocket
        else
        {
            auto lastContact = ball.m_inPocketRadius;
            ball.m_inPocketRadius = false;

            for (const auto& pocket : m_pockets)
            {
                if (glm::length2(pocket.position - glm::vec2(position.x, position.z)) < pocket.radius * pocket.radius)
                {
                    ball.m_inPocketRadius = true;
                    break;
                }
            }

            if (lastContact != ball.m_inPocketRadius)
            {
                auto flags = (1 << CollisionID::Cushion) | (1 << CollisionID::Ball) | (1 << CollisionID::Pocket);
                if (!ball.m_inPocketRadius)
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

void BilliardsSystem::onEntityAdded(cro::Entity entity)
{
    auto& ball = entity.getComponent<BilliardBall>();
    ball.m_parent = entity;

    //set a random orientation
    entity.getComponent<cro::Transform>().setRotation(cro::Util::Random::quaternion());

    btTransform transform;
    transform.setFromOpenGLMatrix(&entity.getComponent<cro::Transform>().getWorldTransform()[0][0]);
    
    auto& body = m_ballObjects.emplace_back(std::make_unique<btRigidBody>(createBodyDef(CollisionID::Ball, BilliardBall::Mass, m_ballShape.get(), &ball)));
    body->setWorldTransform(transform);
    body->setUserIndex(CollisionID::Ball);
    body->setUserPointer(&ball);
    body->setCcdMotionThreshold(BilliardBall::Radius * 0.5f);
    body->setCcdSweptSphereRadius(BilliardBall::Radius);
    
    body->setAnisotropicFriction(m_ballShape->getAnisotropicRollingFrictionDirection(), btCollisionObject::CF_ANISOTROPIC_ROLLING_FRICTION);

    m_collisionWorld->addRigidBody(body.get(), (1 << CollisionID::Ball), (1 << CollisionID::Table) | (1 << CollisionID::Cushion) | (1 << CollisionID::Ball));

    ball.m_physicsBody = body.get();

    if (ball.id == CueBall)
    {
        m_cueball = body.get();
    }
}

void BilliardsSystem::onEntityRemoved(cro::Entity entity)
{
    const auto& ball = entity.getComponent<BilliardBall>();

    auto* body = ball.m_physicsBody;
    body->setUserPointer(nullptr);

    if (m_cueball == body)
    {
        m_cueball = nullptr;
    }

    m_collisionWorld->removeRigidBody(body);

    m_ballObjects.erase(std::remove_if(m_ballObjects.begin(), m_ballObjects.end(), 
        [body](const std::unique_ptr<btRigidBody>& b)
        {
            return b.get() == body;
        }), m_ballObjects.end());
}