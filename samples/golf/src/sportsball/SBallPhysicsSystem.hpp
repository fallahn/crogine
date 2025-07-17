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

#pragma once

#ifdef CRO_DEBUG_
#include "../golf/DebugDraw.hpp"
#endif
#include "BallType.hpp"

#include <crogine/ecs/System.hpp>

#include <btBulletDynamicsCommon.h>
#include <btBulletCollisionCommon.h>

#include <memory>
#include <vector>

struct SBallPhysics final
{
    std::int32_t id = -1;
    std::unique_ptr<btRigidBody> body;

    float lastY = 1.f; //crude detection for hittin bottom of the screen
    std::int32_t boxCollisionCount = 0;

    float rad = 0.f;
    bool collisionHandled = false;
    cro::Entity parent;
};

class SBallPhysicsSystem final : public cro::System
{
public:
    SBallPhysicsSystem(cro::MessageBus&);
    ~SBallPhysicsSystem();

    SBallPhysicsSystem(const SBallPhysicsSystem&) = delete;
    SBallPhysicsSystem(SBallPhysicsSystem&&) = delete;
    SBallPhysicsSystem& operator = (const SBallPhysicsSystem&) = delete;
    SBallPhysicsSystem& operator = (SBallPhysicsSystem&&) = delete;

    void process(float) override;

    void addBallData(BallData&&);
    void spawnBall(std::int32_t type, glm::vec3 position);

    void clearBalls();

    void renderDebug(const glm::mat4& mat, glm::uvec2 targetSize);
private:
    std::unique_ptr<btCollisionConfiguration> m_collisionConfiguration;
    std::unique_ptr<btCollisionDispatcher> m_collisionDispatcher;
    std::unique_ptr<btBroadphaseInterface> m_broadphaseInterface;
    std::unique_ptr<btSequentialImpulseConstraintSolver> m_constraintSolver;
    std::unique_ptr<btDiscreteDynamicsWorld> m_collisionWorld;

    //static objects which make up the box walls
    btStaticPlaneShape m_boxLeft;
    btStaticPlaneShape m_boxRight;
    btStaticPlaneShape m_boxFloor;

    //using a compund shape just doesn't work :/
    std::vector<std::unique_ptr<btRigidBody>> m_box;

    //balls can all share this.
    std::vector<std::unique_ptr<btSphereShape>> m_ballShapes;

    //model data for visual representation
    std::vector<BallData> m_ballDefinitions;

#ifdef CRO_DEBUG_
    BulletDebug m_debugDraw;
#endif

    void onEntityRemoved(cro::Entity) override;
};