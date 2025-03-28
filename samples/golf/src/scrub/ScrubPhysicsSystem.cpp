/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#include "ScrubPhysicsSystem.hpp"
#include "ScrubConsts.hpp"
#include "../golf/GameConsts.hpp" //convert bt/glm vectors

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/graphics/ModelDefinition.hpp>

#include <crogine/gui/Gui.hpp>
#include <crogine/detail/ModelBinary.hpp>
#include <crogine/detail/glm/gtc/type_ptr.hpp>

namespace
{
    constexpr float BallRad = 0.021f;
    constexpr float RadMultiplier = 7.f;
    constexpr float BallMass = 0.46f;

    constexpr std::size_t MaxBalls = 30;
}

ScrubPhysicsSystem::ScrubPhysicsSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(ScrubPhysicsSystem))
{
    requireComponent<cro::Transform>();
    requireComponent<ScrubPhysicsObject>();

    m_ballShape = std::make_unique<btSphereShape>(BallRad * RadMultiplier);

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
    m_collisionWorld->setGravity({ 0.f, -10.f, 0.f });


    //registerWindow([&]() 
    //    {
    //        ImGui::Begin("Phys");
    //        auto bodyCount = m_collisionWorld->getNumCollisionObjects();
    //        ImGui::Text("Body Count: %d", bodyCount);

    //        for (const auto& o : m_bucketObjects)
    //        {
    //            const auto p = o->getWorldTransform().getOrigin();
    //            ImGui::Text("Bucket Pos: %3.2f, %3.2f, %3.2f", p.getX(), p.getY(), p.getZ());
    //        }

    //        ImGui::End();
    //    });
}

ScrubPhysicsSystem::~ScrubPhysicsSystem()
{
    for (auto& o : m_bucketObjects)
    {
        m_collisionWorld->removeCollisionObject(o.get());
    }
}

//public
void ScrubPhysicsSystem::process(float dt)
{
    //step physics
    m_collisionWorld->stepSimulation(dt,2,1.f/120.f);

    //used to hold temp matrix before applying to entities
    static std::array<float, 16> matrixBuffer = {};

    for (auto entity : getEntities())
    {
        const auto& body = entity.getComponent<ScrubPhysicsObject>();
        auto& tx = entity.getComponent<cro::Transform>();

        CRO_ASSERT(body.physicsBody, "");
        
        //we could make this also a motion state and have the
        //simulation call transform updates for us - though unless
        //there's some advantage to that this is less code. And I'm lazy.
        body.physicsBody->getWorldTransform().getOpenGLMatrix(matrixBuffer.data());
        const auto mat = glm::make_mat4(matrixBuffer.data());
        tx.setPosition(glm::vec3(mat[3]));
        tx.setRotation(glm::quat_cast(mat));


        //remove anything that fell out of the world
        if (tx.getPosition().y < -2.f)
        {
            getScene()->destroyEntity(entity);
        }
    }
}

void ScrubPhysicsSystem::loadMeshData()
{
    auto meshData = cro::Detail::ModelBinary::read("assets/arcade/scrub/models/bucket_collision.cmb", m_vertexData, m_indexData);

    if (m_vertexData.empty() || m_indexData.empty())
    {
        LogE << "No collision mesh was loaded" << std::endl;
        return;
    }

    //TODO we should only ever have one sub-mesh here so this is a
    //little overkill... but more flexible (just in case).
    btTransform transform;
    transform.setOrigin(btVector3(BucketOffset, 0.f, 0.f));
    //transform.setRotation()

    for (auto i = 0u; i < m_indexData.size(); ++i)
    {
        btIndexedMesh bucketMesh;
        bucketMesh.m_vertexBase = reinterpret_cast<std::uint8_t*>(m_vertexData.data());
        bucketMesh.m_numVertices = static_cast<std::int32_t>(meshData.vertexCount);
        bucketMesh.m_vertexStride = static_cast<std::int32_t>(meshData.vertexSize);

        bucketMesh.m_numTriangles = meshData.indexData[i].indexCount / 3;
        bucketMesh.m_triangleIndexBase = reinterpret_cast<std::uint8_t*>(m_indexData[i].data());
        bucketMesh.m_triangleIndexStride = 3 * sizeof(std::uint32_t);


        m_bucketVertices.emplace_back(std::make_unique<btTriangleIndexVertexArray>())->addIndexedMesh(bucketMesh);
        m_bucketShapes.emplace_back(std::make_unique<btBvhTriangleMeshShape>(m_bucketVertices.back().get(), false));

        btRigidBody::btRigidBodyConstructionInfo info(0.f, nullptr, m_bucketShapes.back().get(), btVector3(0.f, 0.f, 0.f));
        info.m_friction = 0.8f;
        info.m_rollingFriction = 0.2f;
        info.m_spinningFriction = 0.2f;
        info.m_restitution = 0.2f;

        auto& body = m_bucketObjects.emplace_back(std::make_unique<btRigidBody>(info));
        //body->setWorldTransform(transform); //HMMM setting a transform on this is a Bad Thing.
        m_collisionWorld->addRigidBody(body.get(), (1 << sc::CollisionID::Bucket), (1 << sc::CollisionID::Ball));
    }

#ifdef CRO_DEBUG_
    m_debugDraw.setDebugMode(BulletDebug::DebugFlags);
    m_collisionWorld->setDebugDrawer(&m_debugDraw);
#endif
}

void ScrubPhysicsSystem::spawnBall(cro::Colour c)
{
    auto entity = getScene()->createEntity();

    const float offset = cro::Util::Random::value(-0.02f, 0.03f);
    entity.addComponent<cro::Transform>().setPosition(BucketSpawnPosition + glm::vec3(offset));
    entity.getComponent<cro::Transform>().setScale(glm::vec3(RadMultiplier));

    CRO_ASSERT(m_ballDefinition->isLoaded(), "");
    m_ballDefinition->createModel(entity);
    entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", c);

    //set Physics property
    btVector3 inertia;
    m_ballShape->calculateLocalInertia(BallMass, inertia);
    auto& phys = entity.addComponent<ScrubPhysicsObject>();

    btRigidBody::btRigidBodyConstructionInfo info(BallMass, nullptr, m_ballShape.get(), inertia);
    info.m_restitution = 0.5f;
    info.m_friction = 0.8f;
    info.m_rollingFriction = 0.001f;
    info.m_spinningFriction = 0.01f;

    phys.physicsBody = std::make_unique<btRigidBody>(info);
    
    //set the phys position from new entity
    btTransform transform;
    transform.setFromOpenGLMatrix(&entity.getComponent<cro::Transform>().getWorldTransform()[0][0]);
    phys.physicsBody->setWorldTransform(transform);
    phys.physicsBody->applyTorqueImpulse({ 0.01f, offset / 2.f, 0.01f * cro::Util::Random::value(0,1) });

    m_collisionWorld->addRigidBody(phys.physicsBody.get(), (1 << sc::CollisionID::Ball), (1 << sc::CollisionID::Bucket) | (1 << sc::CollisionID::Ball));

    //count the existing balls and pop the front if needed
    if (getEntities().size() > MaxBalls)
    {
        getScene()->destroyEntity(getEntities()[0]);
    }
}

void ScrubPhysicsSystem::loadBallData(cro::ResourceCollection& rc, cro::EnvironmentMap* em, const std::string& path)
{
    if (!m_ballDefinition)
    {
        m_ballDefinition = std::make_unique<cro::ModelDefinition>(rc, em);
    }

    m_ballDefinition->loadFromFile(path);
}

void ScrubPhysicsSystem::clearBalls()
{
    for (auto e : getEntities())
    {
        getScene()->destroyEntity(e);
    }
}

#ifdef CRO_DEBUG_
void ScrubPhysicsSystem::renderDebug(const glm::mat4& mat, glm::uvec2 targetSize)
{
    m_collisionWorld->debugDrawWorld();
    m_debugDraw.render(mat, targetSize);
}
#endif

//private
void ScrubPhysicsSystem::onEntityRemoved(cro::Entity e)
{
    auto& po = e.getComponent<ScrubPhysicsObject>();

    m_collisionWorld->removeRigidBody(po.physicsBody.get());
    po.physicsBody.reset();
}