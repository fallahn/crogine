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

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>

#include <typeinfo>

using namespace cro;

namespace
{
    btScalar worldSize = 500.f; //TODO make this a variable
    const std::size_t maxObjects = 10000;
}

CollisionSystem::CollisionSystem(cro::MessageBus&mb)
    : cro::System(mb, typeid(CollisionSystem))
{
    m_collisionConfiguration = std::make_unique<btDefaultCollisionConfiguration>();
    m_collisionDispatcher = std::make_unique<btCollisionDispatcher>(m_collisionConfiguration.get());

    btVector3 worldMin(-worldSize, -worldSize, -worldSize);
    btVector3 worldMax(worldSize, worldSize, worldSize);

    //TODO inspect types of broadphase and decide which is best
    m_broadphaseInterface = std::make_unique<bt32BitAxisSweep3>(worldMin, worldMax, maxObjects, nullptr, true);
    m_collisionWorld = std::make_unique<btCollisionWorld>(m_collisionDispatcher.get(), m_broadphaseInterface.get(), m_collisionConfiguration.get());

    m_debugDrawer.setDebugMode(btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawAabb);
    m_collisionWorld->setDebugDrawer(&m_debugDrawer);

    requireComponent<Transform>();
    requireComponent<PhysicsObject>();
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
    }

    m_collisionWorld->performDiscreteCollisionDetection();

    //TODO where do we put the collision data?

    m_collisionWorld->debugDrawWorld();
}

void CollisionSystem::render(Entity camera)
{
    const auto& tx = camera.getComponent<Transform>();
    const auto& camComponent = camera.getComponent<Camera>();

    auto viewProj = camComponent.projection * glm::inverse(tx.getWorldTransform());

    m_debugDrawer.render(viewProj);
}

//private
void CollisionSystem::onEntityAdded(cro::Entity entity)
{
    auto createCollisionShape = [&](const PhysicsShape& shape)->btCollisionShape*
    {
        switch (shape.type)
        {
        default:
        case PhysicsShape::Type::Null:
            return nullptr;
        case PhysicsShape::Type::Box:
            CRO_ASSERT(glm::length2(shape.extent) > 0, "Invalid box points");
            m_shapeCache.emplace_back(std::make_unique<btBoxShape>(btVector3(shape.extent.x, shape.extent.y, shape.extent.z)));
            return m_shapeCache.back().get();
        case PhysicsShape::Type::Capsule:
            CRO_ASSERT(shape.radius > 0, "Capsule requires at least a radius");
            switch (shape.orientation)
            {
            default:
                m_shapeCache.emplace_back(std::make_unique<btCapsuleShape>(shape.radius, shape.length));
                break;
            case PhysicsShape::Orientation::X:
                m_shapeCache.emplace_back(std::make_unique<btCapsuleShapeX>(shape.radius, shape.length));
                break;
            case PhysicsShape::Orientation::Z:
                m_shapeCache.emplace_back(std::make_unique<btCapsuleShapeZ>(shape.radius, shape.length));
                break;
            }
            return m_shapeCache.back().get();
        case PhysicsShape::Type::Cone:
            CRO_ASSERT(shape.radius > 0 && shape.length > 0, "Cone shape requires length and girth");
            switch (shape.orientation)
            {
            default:
                m_shapeCache.emplace_back(std::make_unique<btConeShape>(shape.radius, shape.length));
                break;
            case PhysicsShape::Orientation::X:
                m_shapeCache.emplace_back(std::make_unique<btConeShapeX>(shape.radius, shape.length));
                break;
            case PhysicsShape::Orientation::Z:
                m_shapeCache.emplace_back(std::make_unique<btConeShapeZ>(shape.radius, shape.length));
                break;
            }
            return m_shapeCache.back().get();
        case PhysicsShape::Type::Cylinder:
            CRO_ASSERT(glm::length2(shape.extent), "Cylinder shape requires extent");
            {
                btVector3 extent(shape.extent.x, shape.extent.y, shape.extent.z);
                switch (shape.orientation)
                {
                default:
                    m_shapeCache.emplace_back(std::make_unique<btCylinderShape>(extent));
                    break;
                case PhysicsShape::Orientation::X:
                    m_shapeCache.emplace_back(std::make_unique<btCylinderShapeX>(extent));
                    break;
                case PhysicsShape::Orientation::Z:
                    m_shapeCache.emplace_back(std::make_unique<btCylinderShapeZ>(extent));
                    break;
                }
            }
            return m_shapeCache.back().get();
        case PhysicsShape::Type::Hull:
            Logger::log("Hull shapes not yet implemented", Logger::Type::Warning);
            return nullptr;
        case PhysicsShape::Type::Sphere:
            CRO_ASSERT(shape.radius > 0, "Sphere shapes require a radius");
            m_shapeCache.emplace_back(std::make_unique<btSphereShape>(shape.radius));
            return m_shapeCache.back().get();
            break;
        }
    };
    
    //read component data and create collision object
    const auto& po = entity.getComponent<PhysicsObject>();
    auto idx = entity.getIndex();

    m_collisionData[idx].object = std::make_unique<btCollisionObject>();

    //if more than one shape create compound shape, else single shape
    //TODO we want to hash a collision shape struct in some way so that
    //shapes can be stored in their own cache and reused.
    if (po.m_shapeCount > 1)
    {
        m_shapeCache.emplace_back(std::make_unique<btCompoundShape>(true, po.m_shapeCount));
        btCompoundShape* compoundShape = dynamic_cast<btCompoundShape*>(m_shapeCache.back().get());
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
    m_collisionWorld->addCollisionObject(m_collisionData[idx].object.get());
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