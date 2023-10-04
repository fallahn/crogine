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
#include <crogine/gui/Gui.hpp>

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

    constexpr std::int32_t UpdateCountPerFrame = 2;

    constexpr float CushionBounciness = 0.65f; //no more than 1
    constexpr float CushionFriction = 0.1f;
    constexpr float TableFriction = 0.01f;
    constexpr float BallBounciness = 0.6f;
    constexpr float BallFriction = 0.01f;
}

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
    settings.gravity = rp3d::Vector3(0.f, -9.81f, 0.f);

    m_physWorld = m_physicsCommon.createPhysicsWorld(settings);
    m_physWorld->setEventListener(&m_ballEventListener);

    m_ballShape = m_physicsCommon.createSphereShape(BilliardBallReact::Radius);

    registerWindow([&]()
        {
            if (ImGui::Begin("Billiards"))
            {
                ImGui::Text("Awwake count: %d", m_awakeCount);
            }
            ImGui::End();
        });
}

//BilliardsSystemReact::BilliardsSystemReact(cro::MessageBus& mb, BulletDebug& dd)
//    : BilliardsSystemReact(mb)
//{
//#ifdef CRO_DEBUG_
//    m_collisionWorld->setDebugDrawer(&dd);
//#endif
//
//}


//public
void BilliardsSystemReact::process(float dt)
{
    for (auto i = 0; i < UpdateCountPerFrame; ++i)
    {
        m_physWorld->update(dt / UpdateCountPerFrame);
    }

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

        //TODO this is basically double-handling the collision
        //we should do this in the collision callback AND for every collision
        //not just the first.
        if (ball.m_prevBallContact != ball.m_ballContact)
        {
            if (ball.m_ballContact == -1)
            {
                auto* msg = postMessage<BilliardsEvent>(sv::MessageID::BilliardsMessage);
                msg->type = BilliardsEvent::Collision;
                msg->first = ball.id;
                msg->second = ball.m_prevBallContact;
            }
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
            //if not in radius then we most likely (ugh is that reliable enough?) got knocked off the table
            msg->second = ball.m_inPocketRadius ? 1 : 0;
        }

        if (!ball.m_physicsBody->isSleeping())
        {
            awakeCount++;
        }
    }

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
    }
    auto* meshShape = m_physicsCommon.createConcaveMeshShape(tableMesh);
    auto* collider = tableBody->addCollider(meshShape, transform);
    collider->setCollisionCategoryBits((1 << CollisionID::Cushion));
    collider->setCollideWithMaskBits((1 << CollisionID::Ball));
    collider->getMaterial().setBounciness(CushionBounciness);
    collider->getMaterial().setFrictionCoefficient(CushionFriction);

    //create a single flat surface for the table as even a few triangles perturb
    //the physics. Balls check their proximity to pockets and disable table collision
    //when they need to. This way we can place a pocket anywhere on the surface.
    transform.setPosition({ 0.f, -0.05f, 0.f });
    tableBody = m_physWorld->createRigidBody(transform);
    tableBody->setType(rp3d::BodyType::STATIC);

    auto* tableShape = m_physicsCommon.createBoxShape({ meshData.boundingBox[1].x, 0.05f, meshData.boundingBox[1].z });
    auto* tableCollider = tableBody->addCollider(tableShape, rp3d::Transform::identity());
    tableCollider->setCollisionCategoryBits((1 << CollisionID::Table));
    tableCollider->setCollideWithMaskBits((1 << CollisionID::Ball));
    tableCollider->getMaterial().setFrictionCoefficient(TableFriction);

    //create triggers for each pocket
    static constexpr glm::vec3 PocketHalfSize({ 0.075f, 0.1f, 0.075f });
    static constexpr float WallThickness = 0.025f;
    static constexpr float WallHeight = 0.1f;


    auto i = 0;

    m_pocketWalls[0] = m_physicsCommon.createBoxShape({ Pocket::DefaultRadius, WallHeight, WallThickness });
    m_pocketWalls[1] = m_physicsCommon.createBoxShape({ WallThickness, WallHeight, Pocket::DefaultRadius });

    const std::array Offsets =
    {
        rp3d::Vector3(0.f, 0.f, -1.f),
        rp3d::Vector3(1.f, 0.f, 0.f),
        rp3d::Vector3(0.f, 0.f, 1.f),
        rp3d::Vector3(-1.f, 0.f, 0.f)
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
        //as this is applied as a force we create an impulse by applying 1s worth
        m_cueball->setIsActive(true);
        m_cueball->applyWorldForceAtLocalPosition(glmToRp(impulse * (60.f * UpdateCountPerFrame)), glmToRp(relPos / 4.f));
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
void BilliardsSystemReact::doPocketCollision(cro::Entity entity) const
{
    auto& ball = entity.getComponent<BilliardBallReact>();
    if (!ball.m_physicsBody->isSleeping())
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
    

    auto* body = m_ballObjects.emplace_back(m_physWorld->createRigidBody(transform));
    body->setType(rp3d::BodyType::DYNAMIC);
    body->setAngularDamping(0.1f);
    body->setLinearDamping(0.1f);
    body->setMass(BPhysBall::Mass);
    body->setUserData(&ball);

    auto* collider = body->addCollider(m_ballShape, rp3d::Transform::identity());
    collider->setCollisionCategoryBits((1 << CollisionID::Ball));
    collider->setCollideWithMaskBits((1 << CollisionID::Table) | (1 << CollisionID::Cushion) | (1 << CollisionID::Ball));
    
    auto& mat = collider->getMaterial();
    mat.setFrictionCoefficient(BallFriction);
    mat.setBounciness(BallBounciness);

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
    body->setUserData(nullptr);

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

//collision callback
void BallEventListener::onContact(const rp3d::CollisionCallback::CallbackData& data)
{
    //TODO we should be processing ALL contacts because
    //balls with multiple collisions will miss some out

    for (auto i = 0u; i < data.getNbContactPairs(); ++i)
    {
        auto pair = data.getContactPair(i);
        if ((pair.getBody1()->getCollider(0)->getCollisionCategoryBits() & (1 << CollisionID::Ball))
            && (pair.getBody2()->getCollider(0)->getCollisionCategoryBits() & (1 << CollisionID::Ball)))
        {
            for (auto j = 0u; j < pair.getNbContactPoints(); ++j)
            {
                auto ballA = static_cast<BilliardBallReact*>(pair.getBody1()->getUserData());
                auto ballB = static_cast<BilliardBallReact*>(pair.getBody2()->getUserData());

                //don't overwrite any existing collision this frame
                ballA->m_ballContact = ballA->m_ballContact == -1 ? ballB->id : ballA->m_ballContact;
                ballB->m_ballContact = ballB->m_ballContact == -1 ? ballA->id : ballB->m_ballContact;
            }
        }
    }
}

#endif //CRO_DEBUG_