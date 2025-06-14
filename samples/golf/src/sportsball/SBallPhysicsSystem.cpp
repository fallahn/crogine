/*-----------------------------------------------------------------------

Matt Marchant 2025
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

#include "SBallPhysicsSystem.hpp"
#include "SBallConsts.hpp"
#include "../golf/GameConsts.hpp" //convert bt/glm vectors

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>

#include <crogine/detail/glm/gtc/type_ptr.hpp>

namespace
{
    //constexpr float WallThickness = 0.5f;
    constexpr float CollisionMargin = 0.004f;
}

SBallPhysicsSystem::SBallPhysicsSystem(cro::MessageBus& mb)
    : cro::System   (mb, typeid(SBallPhysicsSystem)),
    m_boxLeft       ({1.f, 0.f, 0.f}, -BoxWidth / 2.f),
    m_boxRight      ({-1.f, 0.f, 0.f}, -BoxWidth / 2.f),
    m_boxFloor      ({ 0.f, 1.f, 0.f }, 0.f)
{
    requireComponent<cro::Transform>();
    requireComponent<SBallPhysics>();

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
    m_collisionWorld->setGravity({ 0.f, -9.8f, 0.f });

    //create the 'box' static shape

    btRigidBody::btRigidBodyConstructionInfo info(0.f, nullptr, &m_boxFloor, {0.f,0.f,0.f});
    info.m_restitution = 0.4f;
    info.m_friction = 0.9f;
    info.m_rollingFriction = 0.003f;
    info.m_spinningFriction = 0.01f;

    auto& b0 = m_box.emplace_back(std::make_unique<btRigidBody>(info));
    b0->setUserIndex(-1);
    m_collisionWorld->addCollisionObject(b0.get());

    info = { 0.f, nullptr, &m_boxLeft, { 0.f,0.f,0.f } };
    auto& b1 = m_box.emplace_back(std::make_unique<btRigidBody>(info));
    b1->setUserIndex(-1);
    m_collisionWorld->addCollisionObject(b1.get());

    info = { 0.f, nullptr, &m_boxRight, { 0.f,0.f,0.f } };
    auto& b2 = m_box.emplace_back(std::make_unique<btRigidBody>(info));
    b2->setUserIndex(-1);
    m_collisionWorld->addCollisionObject(b2.get());

#ifdef CRO_DEBUG_
    m_debugDraw.setDebugMode(BulletDebug::DebugFlags);
    m_collisionWorld->setDebugDrawer(&m_debugDraw);
#endif
}

SBallPhysicsSystem::~SBallPhysicsSystem()
{
    //tidy up remaining phys objects
    clearBalls();
    for (auto& b : m_box)
    {
        m_collisionWorld->removeCollisionObject(b.get());
    }
}

//public
void SBallPhysicsSystem::process(float dt)
{
    //step physics
    m_collisionWorld->stepSimulation(dt, 6, 1.f / 240.f);
    //m_collisionWorld->stepSimulation(dt/*, 6, 1.f / 240.f*/);

    //used to hold temp matrix before applying to entities
    static std::array<float, 16> matrixBuffer = {};

    for (auto entity : getEntities())
    {
        auto& body = entity.getComponent<SBallPhysics>();
        body.collisionHandled = false; //reset for this frame to allow new collisions

        auto& tx = entity.getComponent<cro::Transform>();

        CRO_ASSERT(body.body, "");

        //we could make this also a motion state but its main
        //advantage is only interpolation, which we're not using anyway
        body.body->getWorldTransform().getOpenGLMatrix(matrixBuffer.data());
        const auto mat = glm::make_mat4(matrixBuffer.data());
        tx.setPosition(glm::vec3(mat[3]));
        tx.setRotation(glm::quat_cast(mat));

        //remove anything that fell out of the world
        /*if (tx.getPosition().y < -2.f)
        {
            getScene()->destroyEntity(entity);
        }*/

        const auto y = tx.getPosition().y;

        //out of the top
        if (y > OOB)
        {
            auto* msg = postMessage<sb::GameEvent>(sb::MessageID::GameMessage);
            msg->type = sb::GameEvent::OutOfBounds;
        }


        //crude bottom of the box collision

        const auto vel = glm::length2(btToGlm(body.body->getInterpolationLinearVelocity()));
        if (vel > 0.1f
            && (y - body.rad) < 0 
            && y < body.lastY)
        {
            body.boxCollisionCount++;

            //hit the bottom
            if (body.boxCollisionCount == 1)
            {
                auto* msg = postMessage<sb::CollisionEvent>(sb::MessageID::CollisionMessage);
                msg->type = sb::CollisionEvent::FastCol;
            }
        }
        else
        {
            body.boxCollisionCount = std::max(0, body.boxCollisionCount - 1);
        }
        body.lastY = y;
    }

    const auto manifoldCount = m_collisionDispatcher->getNumManifolds();
    for (auto i = 0; i < manifoldCount; ++i)
    {
        auto manifold = m_collisionDispatcher->getManifoldByIndexInternal(i);
        auto body0 = manifold->getBody0();
        auto body1 = manifold->getBody1();

        auto* phys0 = reinterpret_cast<SBallPhysics*>(body0->getUserPointer());
        auto* phys1 = reinterpret_cast<SBallPhysics*>(body1->getUserPointer());

        const auto id0 = body0->getUserIndex();
        const auto id1 = body1->getUserIndex();
        
        //*sigh* we actually get better results if we test this ourself
        //bullet seems to fail to report some collisions, even though balls
        //are bouncing off each other.
        if (id0 > -1 && id1 > -1)
        {
            const auto pos0 = phys0->parent.getComponent<cro::Transform>().getPosition();
            const auto pos1 = phys1->parent.getComponent<cro::Transform>().getPosition();
            const auto l2 = glm::length2(pos0 - pos1);

            auto minDist = phys0->rad + phys1->rad;
            minDist *= minDist;

            if (l2 < minDist)
            {
                //ignore this if one object has a collision already - TODO fix this
                //if ((!phys0->collisionHandled && !phys1->collisionHandled))
                {
                    auto* msg = postMessage<sb::CollisionEvent>(sb::MessageID::CollisionMessage);
                    msg->ballID = body0->getUserIndex();
                    msg->entityA = phys0->parent;
                    msg->entityB = phys1->parent;
                    msg->position = pos0 + ((pos1 - pos0) / 2.f);

                    //raise non-matching events too so we can play audio
                    if (body0->getUserIndex() == body1->getUserIndex())
                    {
                        //this is set in message handler
                        /*phys0->collisionHandled = true;
                        phys1->collisionHandled = true;*/
                        msg->type = sb::CollisionEvent::Match;
                    }
                    else
                    {
                        msg->type = sb::CollisionEvent::Default;

                        static constexpr float MinVel = 1.f;

                        //perhaps we need to include velocity so we only play audio on high impact?
                        const auto vel0 = glm::length2(btToGlm(body0->getInterpolationLinearVelocity()));
                        const auto vel1 = glm::length2(btToGlm(body1->getInterpolationLinearVelocity()));

                        if (vel0 > vel1)
                        {
                            if (vel0 > MinVel)
                            {
                                msg->ballID =  body0->getUserIndex();
                                msg->type = sb::CollisionEvent::FastCol;
                            }
                        }
                        else
                        {
                            if (vel1 > MinVel)
                            {
                                msg->ballID = body1->getUserIndex();
                                msg->type = sb::CollisionEvent::FastCol;
                            }
                        }
                    }
                }
            }
        }
    }
}

void SBallPhysicsSystem::addBallData(BallData&& data)
{
    //hmm could we just create a single unit sized shape and scale
    //it to radius, as we are with the models?
    m_ballShapes.emplace_back(std::make_unique<btSphereShape>(data.radius));

    //this assumes the models are added in BallID order.
    m_ballDefinitions.push_back(std::move(data));
}

void SBallPhysicsSystem::spawnBall(std::int32_t id, glm::vec3 position)
{
    CRO_ASSERT(id < m_ballDefinitions.size(), "");

    const auto& Data = m_ballDefinitions[id];

    auto entity = getScene()->createEntity();
    entity.addComponent<cro::Transform>().setPosition(position);
    entity.getComponent<cro::Transform>().setScale(glm::vec3(Data.radius));

    CRO_ASSERT(Data.modelDef->isLoaded(), "");
    Data.modelDef->createModel(entity);

    //set Physics property
    const auto& shape = m_ballShapes[id];

    btVector3 inertia;
    shape->calculateLocalInertia(Data.mass, inertia);
    auto& phys = entity.addComponent<SBallPhysics>();
    phys.id = id;
    phys.rad = Data.radius + CollisionMargin;

    btRigidBody::btRigidBodyConstructionInfo info(Data.mass, nullptr, shape.get(), inertia);
    info.m_restitution = Data.restititution / 8.f; //high restitution seems to prevent initial contacts registering
    info.m_friction = 0.6f; //hmm should this vary per ball?
    //info.m_rollingFriction = 0.001f;
    //info.m_spinningFriction = 0.005f;

    phys.body = std::make_unique<btRigidBody>(info);
    phys.body->setUserIndex(id);
    phys.body->setUserPointer(&phys);

    phys.parent = entity;

    //set the phys position from new entity
    btTransform transform;
    transform.setFromOpenGLMatrix(&entity.getComponent<cro::Transform>().getWorldTransform()[0][0]);
    phys.body->setWorldTransform(transform);
    phys.body->setLinearFactor({ 1.f, 1.f, 0.f });
    phys.body->setAngularFactor({ 0.f, 1.f, 1.f });
    //phys.body->setActivationState(DISABLE_DEACTIVATION);
    phys.body->applyTorqueImpulse({ 0.01f, 0.002f, 0.01f * cro::Util::Random::value(0,1) });

    m_collisionWorld->addRigidBody(phys.body.get()/*, (1 << sc::CollisionID::Ball), (1 << sc::CollisionID::Bucket) | (1 << sc::CollisionID::Ball)*/);
}

void SBallPhysicsSystem::clearBalls()
{
    for (auto e : getEntities())
    {
        getScene()->destroyEntity(e);
    }
}

#ifdef CRO_DEBUG_
void SBallPhysicsSystem::renderDebug(const glm::mat4& mat, glm::uvec2 targetSize)
{
    m_collisionWorld->debugDrawWorld();
    m_debugDraw.render(mat, targetSize);
}
#endif

//private
void SBallPhysicsSystem::onEntityRemoved(cro::Entity e)
{
    auto& po = e.getComponent<SBallPhysics>();

    m_collisionWorld->removeRigidBody(po.body.get());
    po.body.reset();
}