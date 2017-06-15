/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine - Zlib license.

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

#include <crogine/ecs/systems/CollisionSystem.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/PhysicsObject.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/App.hpp>
#include <crogine/detail/HashCombine.hpp>

#include "../../imgui/imgui.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>

#include <BulletCollision/CollisionDispatch/btGhostObject.h>

#include <typeinfo>

using namespace cro;

namespace
{
    //struct MyCallback : public btOverlapFilterCallback
    //{
    //    bool needBroadphaseCollision(btBroadphaseProxy* a, btBroadphaseProxy* b) const override
    //    {
    //        //bool collides = (a->m_collisionFilterGroup & b->m_collisionFilterMask) != 0;           
    //        //return collides;
    //        return true;
    //    }
    //};
    //MyCallback callback;

    //void customNearCallback(btBroadphasePair& pair, btCollisionDispatcher& dispatcher, const btDispatcherInfo& info)
    //{
    //    if ((pair.m_pProxy0->m_collisionFilterGroup & pair.m_pProxy1->m_collisionFilterMask) != 0)
    //    {
    //        dispatcher.defaultNearCallback(pair, dispatcher, info);
    //    }
    //}
}

template<>
struct StructHash<PhysicsShape>
{
    std::size_t operator()(const PhysicsShape& s) const
    {
        std::size_t res = 0;

        hash_combine(res, static_cast<int32>(s.type));
        hash_combine(res, s.position.x);
        hash_combine(res, s.position.y);
        hash_combine(res, s.position.z);
        hash_combine(res, s.rotation.x);
        hash_combine(res, s.rotation.y);
        hash_combine(res, s.rotation.z);
        hash_combine(res, s.rotation.w);

        switch (s.type)
        {
        default: break;
        case PhysicsShape::Type::Box:
            hash_combine(res, s.extent.x);
            hash_combine(res, s.extent.y);
            hash_combine(res, s.extent.z);
            break;
        case PhysicsShape::Type::Capsule:
            hash_combine(res, s.radius);
            hash_combine(res, s.length);
            hash_combine(res, static_cast<int32>(s.orientation));
            break;
        case PhysicsShape::Type::Cone:
            hash_combine(res, s.radius);
            hash_combine(res, s.length);
            hash_combine(res, static_cast<int32>(s.orientation));
            break;
        case PhysicsShape::Type::Cylinder:
            hash_combine(res, s.extent.x);
            hash_combine(res, s.extent.y);
            hash_combine(res, s.extent.z);
            hash_combine(res, static_cast<int32>(s.orientation));
            break;
        case PhysicsShape::Type::Sphere:
            hash_combine(res, s.radius);
            break;
        case PhysicsShape::Type::Compound:
            hash_combine(res, s.length);
            break;
        }
        return res;
    }
};

CollisionSystem::CollisionSystem(cro::MessageBus&mb)
    : cro::System(mb, typeid(CollisionSystem))
{
    m_collisionConfiguration = std::make_unique<btDefaultCollisionConfiguration>();
    m_collisionDispatcher = std::make_unique<btCollisionDispatcher>(m_collisionConfiguration.get());
    //m_collisionDispatcher->setNearCallback(customNearCallback);

    m_broadphaseInterface = std::make_unique<btDbvtBroadphase>();
    m_collisionWorld = std::make_unique<btCollisionWorld>(m_collisionDispatcher.get(), m_broadphaseInterface.get(), m_collisionConfiguration.get());

    //m_collisionWorld->getPairCache()->setOverlapFilterCallback(&callback);

    m_collisionWorld->setDebugDrawer(&m_debugDrawer);

    requireComponent<Transform>();
    requireComponent<PhysicsObject>();

    //registers a control to toggle debug output
    //in the status window if it's active
    App::registerStatusOutput([&]()
    {
        static bool showDebug = false;
        bool status = showDebug;
        ImGui::Checkbox("Physics Debug", &showDebug);
        if (status != showDebug)
        {
            showDebug ? m_debugDrawer.setDebugMode(btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawAabb)
                : m_debugDrawer.setDebugMode(0);
        }
    });
}

CollisionSystem::~CollisionSystem()
{
    for (const auto& cd : m_collisionData)
    {
        if (cd.object)
        {
            m_collisionWorld->removeCollisionObject(cd.object.get());
        }
    }
}

//public
void CollisionSystem::process(cro::Time dt)
{
    //update the collision transforms
    auto& entities = getEntities();
    for (auto& entity : entities)
    {
        const auto& tx = entity.getComponent<Transform>();
        auto rot = tx.getRotationQuat();
        auto pos = tx.getWorldPosition();
        btTransform btXf(btQuaternion(rot.x, rot.y, rot.z, rot.w), btVector3(pos.x, pos.y, pos.z));
        m_collisionData[entity.getIndex()].object->setWorldTransform(btXf);

        auto& data = entity.getComponent<PhysicsObject>();
        data.m_collisionCount = 0;
    }

    //perform collisions
    m_collisionWorld->performDiscreteCollisionDetection();
    
    //read out results
    auto manifoldCount = m_collisionDispatcher->getNumManifolds();    
    for (auto i = 0; i < manifoldCount; ++i)
    {
        auto manifold = m_collisionDispatcher->getManifoldByIndexInternal(i);
        auto body0 = manifold->getBody0();
        auto body1 = manifold->getBody1();

        //update the phys objects with collision data
        PhysicsObject* po = static_cast<PhysicsObject*>(body0->getUserPointer());
        if (po->m_collisionCount < PhysicsObject::MaxCollisions)
        {
            po->m_collisionIDs[po->m_collisionCount++] = body1->getUserIndex();
        }

        po = static_cast<PhysicsObject*>(body1->getUserPointer());
        if (po->m_collisionCount < PhysicsObject::MaxCollisions)
        {
            po->m_collisionIDs[po->m_collisionCount++] = body0->getUserIndex();
        }

        //performs a narrow phase pass
        /*manifold->refreshContactPoints(body0->getWorldTransform(), body1->getWorldTransform());
        auto contactCount = manifold->getNumContacts();
        int l = 0;*/
    }
    DPRINT("Collision count", std::to_string(manifoldCount));


    m_collisionWorld->debugDrawWorld();
}

void CollisionSystem::render(Entity camera)
{
    const auto& tx = camera.getComponent<Transform>();
    const auto& camComponent = camera.getComponent<Camera>();

    auto viewProj = camComponent.projection * glm::inverse(tx.getWorldTransform());

    applyViewport(camComponent.viewport);
    m_debugDrawer.render(viewProj);
}

//private
void CollisionSystem::onEntityAdded(cro::Entity entity)
{
    auto createCollisionShape = [&](const PhysicsShape& shape)->btCollisionShape*
    {        
        StructHash<PhysicsShape> sh;
        auto hash = sh(shape);

        if (m_shapeCache.count(hash) > 0) return m_shapeCache.find(hash)->second.get();

        switch (shape.type)
        {
        default:
        case PhysicsShape::Type::Null:
            return nullptr;
        case PhysicsShape::Type::Box:
            CRO_ASSERT(glm::length2(shape.extent) > 0, "Invalid box points");
            m_shapeCache.insert(std::make_pair(hash, std::make_unique<btBoxShape>(btVector3(shape.extent.x, shape.extent.y, shape.extent.z))));
            return m_shapeCache.find(hash)->second.get();
        case PhysicsShape::Type::Capsule:
            CRO_ASSERT(shape.radius > 0, "Capsule requires at least a radius");
            switch (shape.orientation)
            {
            default:
                m_shapeCache.insert(std::make_pair(hash, std::make_unique<btCapsuleShape>(shape.radius, shape.length)));
                break;
            case PhysicsShape::Orientation::X:
                m_shapeCache.insert(std::make_pair(hash, std::make_unique<btCapsuleShapeX>(shape.radius, shape.length)));
                break;
            case PhysicsShape::Orientation::Z:
                m_shapeCache.insert(std::make_pair(hash, std::make_unique<btCapsuleShapeZ>(shape.radius, shape.length)));
                break;
            }
            return m_shapeCache.find(hash)->second.get();
        case PhysicsShape::Type::Cone:
            CRO_ASSERT(shape.radius > 0 && shape.length > 0, "Cone shape requires length and girth");
            switch (shape.orientation)
            {
            default:
                m_shapeCache.insert(std::make_pair(hash, std::make_unique<btConeShape>(shape.radius, shape.length)));
                break;
            case PhysicsShape::Orientation::X:
                m_shapeCache.insert(std::make_pair(hash, std::make_unique<btConeShapeX>(shape.radius, shape.length)));
                break;
            case PhysicsShape::Orientation::Z:
                m_shapeCache.insert(std::make_pair(hash, std::make_unique<btConeShapeZ>(shape.radius, shape.length)));
                break;
            }
            return m_shapeCache.find(hash)->second.get();
        case PhysicsShape::Type::Cylinder:
            CRO_ASSERT(glm::length2(shape.extent), "Cylinder shape requires extent");
            {
                btVector3 extent(shape.extent.x, shape.extent.y, shape.extent.z);
                switch (shape.orientation)
                {
                default:
                    m_shapeCache.insert(std::make_pair(hash, std::make_unique<btCylinderShape>(extent)));
                    break;
                case PhysicsShape::Orientation::X:
                    m_shapeCache.insert(std::make_pair(hash, std::make_unique<btCylinderShapeX>(extent)));
                    break;
                case PhysicsShape::Orientation::Z:
                    m_shapeCache.insert(std::make_pair(hash, std::make_unique<btCylinderShapeZ>(extent)));
                    break;
                }
            }
            return m_shapeCache.find(hash)->second.get();
        case PhysicsShape::Type::Hull:
            Logger::log("Hull shapes not yet implemented", Logger::Type::Warning);
            return nullptr;
        case PhysicsShape::Type::Sphere:
            CRO_ASSERT(shape.radius > 0, "Sphere shapes require a radius");
            m_shapeCache.insert(std::make_pair(hash, std::make_unique<btSphereShape>(shape.radius)));
            return m_shapeCache.find(hash)->second.get();
            break;
        }
    };
    
    //read component data and create collision object
    auto& po = entity.getComponent<PhysicsObject>();
    auto idx = entity.getIndex();

    m_collisionData[idx].object = std::make_unique<btPairCachingGhostObject>();

    //if more than one shape create compound shape, else single shape
    if (po.m_shapeCount > 1)
    {
        PhysicsShape shape;
        shape.type = PhysicsShape::Type::Compound;
        shape.length = static_cast<float>(idx);
        
        StructHash<PhysicsShape> sh;
        auto hash = sh(shape);

        m_shapeCache.insert(std::make_pair(hash, std::make_unique<btCompoundShape>(true, po.m_shapeCount)));
        btCompoundShape* compoundShape = dynamic_cast<btCompoundShape*>(m_shapeCache.find(hash)->second.get());
        m_collisionData[idx].shape = compoundShape;

        for (auto i = 0u; i < po.m_shapeCount; ++i)
        {
            const auto& pos = po.m_shapes[i].position;
            btVector3 tx(pos.x, pos.y, pos.z);

            const auto& rot = po.m_shapes[i].rotation;
            btQuaternion rx(rot.x, rot.y, rot.z, rot.w);

            compoundShape->addChildShape(btTransform(rx, tx), createCollisionShape(po.m_shapes[i]));
        }
    }
    else
    {
        m_collisionData[idx].shape = createCollisionShape(po.m_shapes[0]);
    }

    m_collisionData[idx].object->setCollisionShape(m_collisionData[idx].shape);
    m_collisionData[idx].object->setUserIndex(idx);
    m_collisionData[idx].object->setUserPointer(static_cast<void*>(&po));
    m_collisionWorld->addCollisionObject(m_collisionData[idx].object.get(), po.m_collisionGroups, po.m_collisionFlags);

    //if (m_collisionData[idx].object->isStaticObject())
    //{
    //    Logger::log("Added static object: " + std::to_string(po.m_collisionGroups) + ", " + std::to_string(po.m_collisionFlags), Logger::Type::Info);
    //}
    //else if (m_collisionData[idx].object->isKinematicObject())
    //{
    //    Logger::log("Added kinematic object", Logger::Type::Info);
    //}
}

void CollisionSystem::onEntityRemoved(cro::Entity entity)
{
    std::size_t idx = entity.getIndex();
    if (m_collisionData[idx].object)
    {
        m_collisionWorld->removeCollisionObject(m_collisionData[idx].object.get());
        m_collisionData[idx].object.reset();
        m_collisionData[idx].shape = nullptr;
    }
}
