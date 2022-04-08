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

#include "BilliardsClientCollision.hpp"
#include "BilliardsSystem.hpp"
#include "InterpolationSystem.hpp"
#include "MessageIDs.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/detail/ModelBinary.hpp>
#include <crogine/gui/Gui.hpp>

namespace
{
    std::int32_t collisionCount = 0;
    glm::vec3 rayForward = glm::vec3(0.f);
}

BilliardsCollisionSystem::BilliardsCollisionSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(BilliardsCollisionSystem))
{
    requireComponent<cro::Transform>();
    requireComponent<BilliardBall>();
    requireComponent<InterpolationComponent<InterpolationType::Hermite>>();


    m_collisionCfg = std::make_unique<btDefaultCollisionConfiguration>();
    m_collisionDispatcher = std::make_unique<btCollisionDispatcher>(m_collisionCfg.get());

    m_broadphaseInterface = std::make_unique<btDbvtBroadphase>();
    m_collisionWorld = std::make_unique<btCollisionWorld>(m_collisionDispatcher.get(), m_broadphaseInterface.get(), m_collisionCfg.get());

    m_collisionWorld->setDebugDrawer(&m_debugDrawer);
    m_debugDrawer.setDebugMode(/*BulletDebug::DebugFlags*/0);

    //make this slightly larger as interp means sharp reflections may
    //never actually overlap
    m_ballShape = std::make_unique<btSphereShape>(BilliardBall::Radius * 1.2f);

#ifdef CRO_DEBUG_
    /*registerWindow([&]() 
        {
            if (ImGui::Begin("Client Collision"))
            {
                ImGui::Text("%u bodies", m_ballObjects.size());
                ImGui::Text("Debug Flags: %d", m_debugDrawer.getDebugMode());

                if (!m_ballObjects.empty())
                {
                    auto pos = m_ballObjects.back()->getWorldTransform().getOrigin();
                    ImGui::Text("%3.3f, %3.3f, %3.3f", pos.getX(), pos.getY(), pos.getZ());
                }

                ImGui::Text("Manifold Count: %d", collisionCount);

                ImGui::Text("Ray Forward %3.3f, %3.3f, %3.3f", rayForward.x, rayForward.y, rayForward.z);
            }
            ImGui::End();
        });*/
#endif
}

BilliardsCollisionSystem::~BilliardsCollisionSystem()
{
    for (auto& obj : m_ballObjects)
    {
        m_collisionWorld->removeCollisionObject(obj.get());
    }

    for (auto& obj : m_tableObjects)
    {
        m_collisionWorld->removeCollisionObject(obj.get());
    }
}

//public
void BilliardsCollisionSystem::handleMessage(const cro::Message&)
{

}

void BilliardsCollisionSystem::process(float)
{
    const auto raiseMessage = [&](glm::vec3 position, float volume, std::int32_t collisionID)
    {
        auto* msg = postMessage<BilliardBallEvent>(MessageID::BilliardsMessage);
        msg->type = BilliardBallEvent::Collision;
        msg->position = position;
        msg->data = collisionID;
        msg->volume = volume;
    };

    btTransform transform;

    for (auto entity : getEntities())
    {
        static constexpr float MaxVel = 0.5f;

        //these are a frame late - if there's noticable lag
        //then reiterate below instead
        auto& ball = entity.getComponent<BilliardBall>();
        if (ball.m_prevBallContact != ball.m_ballContact)
        {
            //note this is contact begin
            if (ball.m_ballContact != -1)
            {
                float volume = glm::length(entity.getComponent<InterpolationComponent<InterpolationType::Hermite>>().getVelocity()) / MaxVel;
                raiseMessage(entity.getComponent<cro::Transform>().getPosition(), volume, CollisionID::Ball);

                if (ball.m_ballContact == CueBall)
                {
                    //this is the first collision this turn
                    auto* msg = postMessage<BilliardsCameraEvent>(MessageID::BilliardsCameraMessage);
                    msg->target = entity;
                }
            }
        }
        ball.m_prevBallContact = ball.m_ballContact;
        ball.m_ballContact = -1;

        
        //pocket collision
        if (ball.m_pocketContact != ball.m_prevPocketContact)
        {
            //this is contact begin
            if (ball.m_pocketContact == 1)
            {
                float volume = glm::length(entity.getComponent<InterpolationComponent<InterpolationType::Hermite>>().getVelocity()) / MaxVel;
                raiseMessage(entity.getComponent<cro::Transform>().getPosition(), volume, CollisionID::Pocket);
            }
        }

        ball.m_prevPocketContact = ball.m_pocketContact;
        ball.m_pocketContact = -1;


        //cushion collision
        if (ball.m_cushionContact != ball.m_prevCushionContact)
        {
            if (ball.m_cushionContact == 1)
            {
                float volume = glm::length(entity.getComponent<InterpolationComponent<InterpolationType::Hermite>>().getVelocity()) / MaxVel;
                raiseMessage(entity.getComponent<cro::Transform>().getPosition(), volume, CollisionID::Cushion);
            }
        }

        ball.m_prevCushionContact = ball.m_cushionContact;
        ball.m_cushionContact = -1;

        //hm do motion states also work in reverse?
        transform.setFromOpenGLMatrix(&entity.getComponent<cro::Transform>().getLocalTransform()[0][0]);
        entity.getComponent<BilliardBall>().m_collisionBody->setWorldTransform(transform);
    }

    m_collisionWorld->performDiscreteCollisionDetection();


    //look for contacts
    const auto testBody = [&](const btCollisionObject* body0, const btCollisionObject* body1, const btPersistentManifold* manifold)
    {
        if (body0->getUserIndex() == CollisionID::Ball)
        {
            auto contactCount = manifold->getNumContacts();// std::min(1, manifold->getNumContacts());
            for (auto j = 0; j < contactCount; ++j)
            {
                auto ballA = static_cast<BilliardBall*>(body0->getUserPointer());

                switch (body1->getUserIndex())
                {
                default: break;
                case CollisionID::Ball:
                {
                    auto ballB = static_cast<BilliardBall*>(body1->getUserPointer());
                    ballA->m_ballContact = ballB->id;
                    ballB->m_ballContact = ballA->id;
                }
                break;
                case CollisionID::Pocket:
                    ballA->m_pocketContact = 1; //we're not interested in specific ID, just if it collides
                    break;
                case CollisionID::Cushion:
                    ballA->m_cushionContact = 1;
                    break;
                }
            }
            return true;
        }
        return false;
    };

    auto manifoldCount = m_collisionDispatcher->getNumManifolds();
    collisionCount = manifoldCount;
    for (auto i = 0; i < manifoldCount; ++i)
    {
        auto manifold = m_collisionDispatcher->getManifoldByIndexInternal(i);
        auto body0 = manifold->getBody0();
        auto body1 = manifold->getBody1();

        if (!testBody(body0, body1, manifold))
        {
            testBody(body1, body0, manifold);
        }
    }


#ifdef CRO_DEBUG_
    m_collisionWorld->debugDrawWorld();
#endif
}

void BilliardsCollisionSystem::initTable(const TableData& tableData)
{
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

        auto& obj = m_tableObjects.emplace_back(std::make_unique<btPairCachingGhostObject>());
        obj->setCollisionShape(m_tableShapes.back().get());
        obj->setUserIndex(CollisionID::Cushion);
        m_collisionWorld->addCollisionObject(obj.get());
    }

    //create triggers for each pocket
    static constexpr float PocketHeight = 0.15f;
    const btVector3 PocketHalfSize({ 0.075f, 0.025f, 0.075f });
    m_pocketShape = std::make_unique<btBoxShape>(PocketHalfSize);

    for (auto p : tableData.pockets)
    {
        transform.setOrigin({ p.position.x, -PocketHeight, p.position.y });
        auto& obj = m_tableObjects.emplace_back(std::make_unique<btPairCachingGhostObject>());
        obj->setCollisionShape(m_pocketShape.get());
        obj->setWorldTransform(transform);
        obj->setUserIndex(CollisionID::Pocket);
        m_collisionWorld->addCollisionObject(obj.get());
    }
}

void BilliardsCollisionSystem::toggleDebug()
{
    auto flags = m_debugDrawer.getDebugMode() == 0 ? 3 : 0;
    m_debugDrawer.setDebugMode(flags);
}

void BilliardsCollisionSystem::renderDebug(const glm::mat4& mat, glm::uvec2 targetSize)
{
    m_debugDrawer.render(mat, targetSize);
}

BilliardsCollisionSystem::RaycastResult BilliardsCollisionSystem::rayCast(glm::vec3 pos, glm::vec3 direction) const
{
#ifdef CRO_DEBUG_
    rayForward = direction;
#endif

    auto worldPos = glmToBt(pos);
    auto rayLength = glmToBt(direction);

    btCollisionWorld::ClosestRayResultCallback res(worldPos, worldPos + rayLength);
    if (glm::length2(direction) != 0)
    {
        m_collisionWorld->rayTest(worldPos, worldPos + rayLength, res);
    }

    RaycastResult retval;
    if (res.hasHit())
    {
        retval.hitPointWorld = btToGlm(res.m_hitPointWorld);
        retval.normalWorld = btToGlm(res.m_hitNormalWorld);
        retval.hasHit = true;
    }
    return retval;
}

//private
void BilliardsCollisionSystem::onEntityAdded(cro::Entity e)
{
    btTransform transform;
    transform.setFromOpenGLMatrix(&e.getComponent<cro::Transform>().getWorldTransform()[0][0]);

    auto& ball = e.getComponent<BilliardBall>();

    auto& obj = m_ballObjects.emplace_back(std::make_unique<btPairCachingGhostObject>());
    obj->setCollisionShape(m_ballShape.get());
    obj->setWorldTransform(transform);
    obj->setUserIndex(CollisionID::Ball);
    obj->setUserPointer(&ball);
    m_collisionWorld->addCollisionObject(obj.get());

    ball.m_collisionBody = obj.get();
    ball.m_parent = e;
}

void BilliardsCollisionSystem::onEntityRemoved(cro::Entity e)
{
    auto& ball = e.getComponent<BilliardBall>();

    m_collisionWorld->removeCollisionObject(ball.m_collisionBody);
    ball.m_collisionBody->setUserPointer(nullptr);

    m_ballObjects.erase(std::remove_if(m_ballObjects.begin(), m_ballObjects.end(), 
        [&ball](const std::unique_ptr<btPairCachingGhostObject>& o)
        {
            return o.get() == ball.m_collisionBody;
        }), m_ballObjects.end());
}