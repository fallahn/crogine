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

#pragma once

#ifdef CRO_DEBUG_
#include "../golf/DebugDraw.hpp"
#endif

#include <crogine/ecs/System.hpp>
#include <crogine/gui/GuiClient.hpp>

#include <btBulletDynamicsCommon.h>
#include <btBulletCollisionCommon.h>

#include <vector>
#include <string>

namespace cro
{
    struct ResourceCollection;
    class ModelDefinition;
    class EnvironmentMap;
}

struct ScrubPhysicsObject final
{
    std::unique_ptr<btRigidBody> physicsBody;
};

class ScrubPhysicsSystem final :public cro::System, public cro::GuiClient
{
public:
    ScrubPhysicsSystem(cro::MessageBus&);
    ~ScrubPhysicsSystem();

    ScrubPhysicsSystem(const ScrubPhysicsSystem&) = delete;
    ScrubPhysicsSystem(ScrubPhysicsSystem&&) = delete;
    ScrubPhysicsSystem& operator = (const ScrubPhysicsSystem&) = delete;
    ScrubPhysicsSystem& operator = (ScrubPhysicsSystem&&) = delete;

    void process(float);

    void loadMeshData();
    void loadBallData(cro::ResourceCollection&, cro::EnvironmentMap*, const std::string&);
    void spawnBall();

    void clearBalls();


    void renderDebug(const glm::mat4& mat, glm::uvec2 targetSize);

private:
    std::unique_ptr<btCollisionConfiguration> m_collisionConfiguration;
    std::unique_ptr<btCollisionDispatcher> m_collisionDispatcher;
    std::unique_ptr<btBroadphaseInterface> m_broadphaseInterface;
    std::unique_ptr<btSequentialImpulseConstraintSolver> m_constraintSolver;
    std::unique_ptr<btDiscreteDynamicsWorld> m_collisionWorld;

    //we have to keep a local copy of the bucket verts as the
    //collision world only maintains pointers to it
    std::vector<float> m_vertexData;
    std::vector<std::vector<std::uint32_t>> m_indexData;

    //these are what do the pointing.
    std::vector<std::unique_ptr<btRigidBody>> m_bucketObjects;
    std::vector<std::unique_ptr<btTriangleIndexVertexArray>> m_bucketVertices;
    std::vector<std::unique_ptr<btBvhTriangleMeshShape>> m_bucketShapes;


    //tracks ball objects
    //std::vector<std::unique_ptr<btRigidBody>> m_ballObjects;
    std::unique_ptr<btSphereShape> m_ballShape; //balls can all share this.


    //model data for visual representation
    std::unique_ptr<cro::ModelDefinition> m_ballDefinition;

#ifdef CRO_DEBUG_
    BulletDebug m_debugDraw;
#endif

    void onEntityRemoved(cro::Entity) override;
};