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

#include "BilliardsSystemReact.hpp"
#include "DebugDraw.hpp"
#include "server/ServerMessages.hpp"

#include <crogine/core/ConfigFile.hpp>
#include <crogine/detail/ModelBinary.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/detail/glm/gtc/type_ptr.hpp>
#include <crogine/graphics/MeshBuilder.hpp>
#include <crogine/util/Random.hpp>

//TODO remove this
#ifdef CRO_DEBUG_

namespace
{
    static inline glm::vec3 rpToGlm(rp3d::Vector3 v)
    {
        return { v.x, v.y, v.z };
    }

    static inline glm::quat rpToGlm(rp3d::Quaternion q)
    {
        return { q.w, q.x, q.y, q.z };
    }

    static inline rp3d::Vector3 glmToRp(glm::vec3 v)
    {
        return { v.x, v.y, v.z };
    }

    static inline rp3d::Quaternion glmToRp(glm::quat q)
    {
        return { q.x,q.y,q.z,q.w };
    }
}

//void BilliardBallReact::getWorldTransform(btTransform& dest) const
//{
//    const auto& tx = m_parent.getComponent<cro::Transform>();
//    dest.setFromOpenGLMatrix(&tx.getWorldTransform()[0][0]);
//}
//
//void BilliardBallReact::setWorldTransform(const btTransform& src)
//{
//    static std::array<float, 16> matrixBuffer = {};
//
//    src.getOpenGLMatrix(matrixBuffer.data());
//    auto mat = glm::make_mat4(matrixBuffer.data());
//
//    auto& tx = m_parent.getComponent<cro::Transform>();
//    tx.setPosition(glm::vec3(mat[3]));
//    tx.setRotation(glm::quat_cast(mat));
//
//    hadUpdate = true;
//}

glm::vec3 BilliardBallReact::getVelocity() const
{
    return rpToGlm(m_physicsBody->getLinearVelocity());
}



BilliardsSystemReact::BilliardsSystemReact(cro::MessageBus& mb)
    : cro::System(mb, typeid(BilliardsSystemReact)),
    m_awakeCount(0),
    m_shotActive(false),
    m_physWorld (nullptr),
    m_ballShape (nullptr),
    m_cueball   (nullptr)
{
    requireComponent<BilliardBallReact>();
    requireComponent<cro::Transform>();

    rp3d::PhysicsWorld::WorldSettings settings;
    settings.gravity = { 0.f, -9.f, 0.f };

    m_physWorld = m_physicsCommon.createPhysicsWorld(settings);
    m_ballShape = m_physicsCommon.createSphereShape(BilliardBallReact::Radius);
}

//BilliardsSystemReact::BilliardsSystemReact(cro::MessageBus& mb, BulletDebug& dd)
//    : BilliardsSystemReact(mb)
//{
//#ifdef CRO_DEBUG_
//    m_collisionWorld->setDebugDrawer(&dd);
//#endif
//
//}

BilliardsSystemReact::~BilliardsSystemReact()
{
    //physics common does this for us :)

    //for (auto& o : m_ballObjects)
    //{
    //    m_collisionWorld->removeCollisionObject(o.get());
    //}

    //for (auto& o : m_tableObjects)
    //{
    //    m_collisionWorld->removeCollisionObject(o.get());
    //}
}

//public
void BilliardsSystemReact::process(float dt)
{
    m_physWorld->update(dt);

    /*
    Increasing the number of steps means there's a chance of
    missing contacts when only checking at game loop speed.
    It also messes with friction values (so requires changing
    the body creation values to be changed). Seems OK like this
    anyway, else we need to look at caching contacts (?) or a more
    frequently called callback in which we can update the ball
    contact IDs
    */
    //m_collisionWorld->stepSimulation(dt, 10/*, 1.f/120.f*/);


    std::int32_t awakeCount = 0;

    //we either have to iterate before AND after collision test
    //else we can do it once and accept the results are one frame late
    for (auto entity : getEntities())
    {
        auto& ball = entity.getComponent<BilliardBallReact>();

        //apply position from physics body
        auto& tx = entity.getComponent<cro::Transform>();
        auto pos = rpToGlm(ball.m_physicsBody->getTransform().getPosition());
        auto rot = rpToGlm(ball.m_physicsBody->getTransform().getOrientation());
        tx.setPosition(pos);
        tx.setRotation(rot);
        ball.hadUpdate = true;

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
        pos = entity.getComponent<cro::Transform>().getPosition();
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

void BilliardsSystemReact::initTable(const TableData& tableData)
{
    m_spawnArea = tableData.spawnArea;

    auto meshData = cro::Detail::ModelBinary::read(tableData.collisionModel, m_vertexData, m_indexData);

    if (m_vertexData.empty() || m_indexData.empty())
    {
        LogE << "No collision mesh was loaded" << std::endl;
        return;
    }


    rp3d::Transform transform = rp3d::Transform::identity();
    auto* tableBody = m_physWorld->createRigidBody(transform);
    tableBody->setType(rp3d::BodyType::STATIC);

    auto* tableMesh = m_physicsCommon.createTriangleMesh();

    for (auto i = 0u; i < m_indexData.size(); ++i)
    {
        auto& triangleArray = m_tableVertices.emplace_back(std::make_unique<rp3d::TriangleVertexArray>(
            static_cast<std::uint32_t>(meshData.vertexCount), m_vertexData.data(), static_cast<std::uint32_t>(meshData.vertexSize),
            static_cast<std::uint32_t>(meshData.indexData[i].indexCount / 3), m_indexData[i].data(), static_cast<std::uint32_t>(3 * sizeof(std::uint32_t)),
            rp3d::TriangleVertexArray::VertexDataType::VERTEX_FLOAT_TYPE,
            rp3d::TriangleVertexArray::IndexDataType::INDEX_INTEGER_TYPE));
        
        tableMesh->addSubpart(triangleArray.get());

        //btIndexedMesh tableMesh;
        //tableMesh.m_vertexBase = reinterpret_cast<std::uint8_t*>(m_vertexData.data());
        //tableMesh.m_numVertices = static_cast<std::int32_t>(meshData.vertexCount);
        //tableMesh.m_vertexStride = static_cast<std::int32_t>(meshData.vertexSize);

        //tableMesh.m_numTriangles = meshData.indexData[i].indexCount / 3;
        //tableMesh.m_triangleIndexBase = reinterpret_cast<std::uint8_t*>(m_indexData[i].data());
        //tableMesh.m_triangleIndexStride = 3 * sizeof(std::uint32_t);


        //m_tableVertices.emplace_back(std::make_unique<btTriangleIndexVertexArray>())->addIndexedMesh(tableMesh);
        //m_tableShapes.emplace_back(std::make_unique<btBvhTriangleMeshShape>(m_tableVertices.back().get(), false));

        //auto& body = m_tableObjects.emplace_back(std::make_unique<btRigidBody>(createBodyDef(CollisionID::Cushion, 0.f, m_tableShapes.back().get())));
        //body->setWorldTransform(transform);
        //m_collisionWorld->addRigidBody(body.get(), (1<< CollisionID::Cushion), (1 << CollisionID::Ball));
    }
    auto* meshShape = m_physicsCommon.createConcaveMeshShape(tableMesh);
    auto* collider = tableBody->addCollider(meshShape, transform);
    collider->setCollisionCategoryBits((1 << CollisionID::Cushion));
    collider->setCollideWithMaskBits((1 << CollisionID::Ball));


    //create a single flat surface for the table as even a few triangles perturb
    //the physics. Balls check their proximity to pockets and disable table collision
    //when they need to. This way we can place a pocket anywhere on the surface.
    transform.setPosition({ 0.f, 0.05f, 0.f });
    tableBody = m_physWorld->createRigidBody(transform);
    tableBody->setType(rp3d::BodyType::STATIC);

    auto* tableShape = m_physicsCommon.createBoxShape({ meshData.boundingBox[1].x, 0.05f, meshData.boundingBox[1].z });
    auto* tableCollider = tableBody->addCollider(tableShape, rp3d::Transform::identity());
    tableCollider->setCollisionCategoryBits((1 << CollisionID::Table));
    tableCollider->setCollideWithMaskBits((1 << CollisionID::Ball));

    //auto& tableShape = m_boxShapes.emplace_back(std::make_unique<btBoxShape>(btBoxShape(btVector3(meshData.boundingBox[1].x, 0.05f, meshData.boundingBox[1].z))));
    //transform.setOrigin({ 0.f, -0.05f, 0.f });
    //auto& table = m_tableObjects.emplace_back(std::make_unique<btRigidBody>(createBodyDef(CollisionID::Table, 0.f, tableShape.get())));
    //table->setWorldTransform(transform);
    //m_collisionWorld->addCollisionObject(table.get(), (1 << CollisionID::Table), (1 << CollisionID::Ball));

    //create triggers for each pocket
    static constexpr glm::vec3 PocketHalfSize({ 0.075f, 0.1f, 0.075f });
    static constexpr float WallThickness = 0.025f;
    static constexpr float WallHeight = 0.1f;


    auto i = 0;

    m_pocketWalls[0] = m_physicsCommon.createBoxShape({ Pocket::DefaultRadius, WallHeight, WallThickness });
    m_pocketWalls[1] = m_physicsCommon.createBoxShape({ WallThickness, WallHeight, Pocket::DefaultRadius });

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


            /*auto& obj = m_tableObjects.emplace_back(std::make_unique<btRigidBody>(createBodyDef(CollisionID::Pocket, 0.f, wallShape.get())));
            transform.setOrigin(glmToBt(pocketPos) + (Offsets[j] * (WallThickness + p.radius)));
            obj->setWorldTransform(transform);
            m_collisionWorld->addCollisionObject(obj.get(), (1 << CollisionID::Pocket), (1 << CollisionID::Ball));*/
        }
    }

    if (tableData.pockets.empty())
    {
        LogW << "Pocket info was empty. This might be a boring game." << std::endl;
    }
}

void BilliardsSystemReact::applyImpulse(glm::vec3 impulse, glm::vec3 relPos)
{
    if (m_cueball)
    {
        //TODO see which if the below is the correct one
        m_cueball->setIsActive(true);
        m_cueball->applyLocalForceAtLocalPosition(glmToRp(impulse), glmToRp(relPos / 4.f));
        //m_cueball->applyWorldForceAtLocalPosition(glmToRp(impulse), glmToRp(relPos / 4.f));
        m_shotActive = true;

        auto* msg = postMessage<BilliardsEvent>(sv::MessageID::BilliardsMessage);
        msg->type = BilliardsEvent::TurnBegan;
    }
}

glm::vec3 BilliardsSystemReact::getCueballPosition() const
{
    if (m_cueball)
    {
        return rpToGlm(m_cueball->getTransform().getPosition());
    }
    return glm::vec3(0.f);
}

bool BilliardsSystemReact::isValidSpawnPosition(glm::vec3 position) const
{
    glm::vec2 ballPos(position.x, -position.z);
    return m_spawnArea.contains(ballPos);
}

//private
//btRigidBody::btRigidBodyConstructionInfo BilliardsSystemReact::createBodyDef(std::int32_t collisionID, float mass, btCollisionShape* shape, btMotionState* motionState)
//{
//    btVector3 inertia(0.f, 0.f, 0.f);
//    if (mass > 0.f)
//    {
//        shape->calculateLocalInertia(mass, inertia);
//    }
//
//
//    btRigidBody::btRigidBodyConstructionInfo info(mass, motionState, shape, inertia);
//
//    switch (collisionID)
//    {
//    default: break;
//    case CollisionID::Table:
//        //info.m_friction = 0.26f;
//        info.m_friction = 0.3f;
//        break;
//    case CollisionID::Cushion:
//        info.m_restitution = 1.f;// 0.5f;
//        info.m_friction = 0.28f;
//        break;
//    case CollisionID::Ball:
//        info.m_restitution = 0.5f;
//        info.m_rollingFriction = 0.0025f;
//        info.m_spinningFriction = 0.001f;// hmm if this is too high then the balls curve a lot, but too low and they never come to rest...
//        //info.m_friction = 0.15f;
//        info.m_friction = 0.3f;
//        info.m_linearSleepingThreshold = 0.003f; //if this is 0 then we never sleep...
//        info.m_angularSleepingThreshold = 0.003f;
//        break;
//    }
//
//    return info;
//}

void BilliardsSystemReact::doBallCollision() const
{
    //TODO replace this with EventListener?

    //auto manifoldCount = m_collisionDispatcher->getNumManifolds();
    //for (auto i = 0; i < manifoldCount; ++i)
    //{
    //    auto manifold = m_collisionDispatcher->getManifoldByIndexInternal(i);
    //    auto body0 = manifold->getBody0();
    //    auto body1 = manifold->getBody1();

    //    if (body0->getUserIndex() == CollisionID::Ball
    //        && body1->getUserIndex() == CollisionID::Ball)
    //    {
    //        auto contactCount = manifold->getNumContacts();
    //        for (auto j = 0; j < contactCount; ++j)
    //        {
    //            auto ballA = static_cast<BilliardBallReact*>(body0->getUserPointer());
    //            auto ballB = static_cast<BilliardBallReact*>(body1->getUserPointer());

    //            //don't overwrite any existing collision this frame
    //            ballA->m_ballContact = ballA->m_ballContact == -1 ? ballB->id : ballA->m_ballContact;
    //            ballB->m_ballContact = ballB->m_ballContact == -1 ? ballA->id : ballB->m_ballContact;
    //        }
    //    }
    //}
}

void BilliardsSystemReact::doPocketCollision(cro::Entity entity) const
{
    auto& ball = entity.getComponent<BilliardBallReact>();
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

                ball.m_physicsBody->getCollider(0)->setCollideWithMaskBits(flags);

                //apparently the only way to change the grouping - however network lag
                //seems to cover up any jitter...
                //m_collisionWorld->removeRigidBody(ball.m_physicsBody);
                //m_collisionWorld->addRigidBody(ball.m_physicsBody, (1 << CollisionID::Ball), flags);
            }
        }
    }
}

void BilliardsSystemReact::onEntityAdded(cro::Entity entity)
{
    auto& ball = entity.getComponent<BilliardBallReact>();
    ball.m_parent = entity;

    //set a random orientation
    entity.getComponent<cro::Transform>().setRotation(cro::Util::Random::quaternion());

    rp3d::Transform transform;
    transform.setFromOpenGL(&entity.getComponent<cro::Transform>().getWorldTransform()[0][0]);
    
    //auto& body = m_ballObjects.emplace_back(std::make_unique<btRigidBody>(createBodyDef(CollisionID::Ball, BilliardBallReact::Mass, m_ballShape.get(), &ball)));
    //body->setWorldTransform(transform);
    //body->setUserIndex(CollisionID::Ball);
    //body->setUserPointer(&ball);
    //body->setCcdMotionThreshold(BilliardBallReact::Radius * 0.5f);
    //body->setCcdSweptSphereRadius(BilliardBallReact::Radius);
    //
    //body->setAnisotropicFriction(m_ballShape->getAnisotropicRollingFrictionDirection(), btCollisionObject::CF_ANISOTROPIC_ROLLING_FRICTION);

    //m_collisionWorld->addRigidBody(body.get(), (1 << CollisionID::Ball), (1 << CollisionID::Table) | (1 << CollisionID::Cushion) | (1 << CollisionID::Ball));

    //ball.m_physicsBody = body.get();

    auto* body = m_ballObjects.emplace_back(m_physWorld->createRigidBody(transform));
    auto* collider = body->addCollider(m_ballShape, rp3d::Transform::identity());
    collider->setCollisionCategoryBits((1 << CollisionID::Ball));
    collider->setCollideWithMaskBits((1 << CollisionID::Table) | (1 << CollisionID::Cushion) | (1 << CollisionID::Ball));

    body->setMass(BPhysBall::Mass);

    ball.m_physicsBody = body;

    if (ball.id == CueBall)
    {
        m_cueball = body;
    }
}

void BilliardsSystemReact::onEntityRemoved(cro::Entity entity)
{
    const auto& ball = entity.getComponent<BilliardBallReact>();

    auto* body = ball.m_physicsBody;
    //body->setUserPointer(nullptr);

    if (m_cueball == body)
    {
        m_cueball = nullptr;
    }

    m_physWorld->destroyRigidBody(body);

    m_ballObjects.erase(std::remove_if(m_ballObjects.begin(), m_ballObjects.end(), 
        [body](const rp3d::RigidBody* b)
        {
            return b == body;
        }), m_ballObjects.end());
}

#endif //CRO_DEBUG_