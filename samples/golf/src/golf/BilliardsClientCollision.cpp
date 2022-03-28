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
#include "MessageIDs.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/gui/Gui.hpp>

namespace
{
    std::int32_t collisionCount = 0;
}

BilliardsCollisionSystem::BilliardsCollisionSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(BilliardsCollisionSystem))
{
    requireComponent<cro::Transform>();
    requireComponent<BilliardBall>();


    m_collisionCfg = std::make_unique<btDefaultCollisionConfiguration>();
    m_collisionDispatcher = std::make_unique<btCollisionDispatcher>(m_collisionCfg.get());

    m_broadphaseInterface = std::make_unique<btDbvtBroadphase>();
    m_collisionWorld = std::make_unique<btCollisionWorld>(m_collisionDispatcher.get(), m_broadphaseInterface.get(), m_collisionCfg.get());

    m_collisionWorld->setDebugDrawer(&m_debugDrawer);
    m_debugDrawer.setDebugMode(BulletDebug::DebugFlags);

    m_ballShape = std::make_unique<btSphereShape>(BilliardBall::Radius * 1.05f);

#ifdef CRO_DEBUG_
    registerWindow([&]() 
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
            }
            ImGui::End();
        });
#endif
}

BilliardsCollisionSystem::~BilliardsCollisionSystem()
{
    for (auto& obj : m_ballObjects)
    {
        m_collisionWorld->removeCollisionObject(obj.get());
    }

    m_ballObjects.clear();
    //m_groundShapes.clear();
    //m_groundVertices.clear();

    //m_vertexData.clear();
    //m_indexData.clear();
}

//public
void BilliardsCollisionSystem::process(float)
{
    btTransform transform;

    for (auto entity : getEntities())
    {
        //these are a frame late - if there's noticable lag
        //then reiterate below instead
        auto& ball = entity.getComponent<BilliardBall>();
        if (ball.m_prevBallContact != ball.m_ballContact)
        {
            if (ball.m_ballContact == -1)
            {
                //LogI << "Ball ended contact with " << (int)ball.m_prevBallContact << std::endl;
                auto* msg = postMessage<BilliardBallEvent>(MessageID::BilliardsMessage);
                msg->type = BilliardBallEvent::Collision;
                msg->position = entity.getComponent<cro::Transform>().getPosition();
                //msg->first = ball.id;
                //msg->second = ball.m_prevBallContact;
            }
        }
        ball.m_prevBallContact = ball.m_ballContact;
        ball.m_ballContact = -1;

        //TODO pocket collision

        //hm do motion states also work in reverse?
        transform.setFromOpenGLMatrix(&entity.getComponent<cro::Transform>().getLocalTransform()[0][0]);
        entity.getComponent<BilliardBall>().m_collisionBody->setWorldTransform(transform);
    }

    m_collisionWorld->performDiscreteCollisionDetection();


    //look for contacts
    auto manifoldCount = m_collisionDispatcher->getNumManifolds();
    collisionCount = manifoldCount;
    for (auto i = 0; i < manifoldCount; ++i)
    {
        auto manifold = m_collisionDispatcher->getManifoldByIndexInternal(i);
        auto body0 = manifold->getBody0();
        auto body1 = manifold->getBody1();

        if (body0->getUserIndex() == CollisionID::Ball
            && body1->getUserIndex() == CollisionID::Ball)
        {
            auto contactCount = std::min(1, manifold->getNumContacts());
            for (auto j = 0; j < contactCount; ++j)
            {
                auto ballA = static_cast<BilliardBall*>(body0->getUserPointer());
                auto ballB = static_cast<BilliardBall*>(body1->getUserPointer());

                ballA->m_ballContact = ballB->id;
                ballB->m_ballContact = ballA->id;
            }
        }
    }


#ifdef CRO_DEBUG_
    m_collisionWorld->debugDrawWorld();
#endif
}

void BilliardsCollisionSystem::renderDebug(const glm::mat4& mat, glm::uvec2 targetSize)
{
    m_debugDrawer.render(mat, targetSize);
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