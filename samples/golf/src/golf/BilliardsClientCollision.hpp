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

#pragma once

#include "DebugDraw.hpp"

#include <crogine/ecs/System.hpp>
#include <crogine/gui/GuiClient.hpp>

#include <btBulletCollisionCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>

#include <memory>

/*
NOTE since the introduction of Billiards the collision mesh
classes have become somewhat ambiguous in naming... this class
deals specifically with billiards collision on the client.

Note that the actual billiards physics are done by the
BilliardsSystem class on the server. This class merely raises
events for sound/particle effects and performs ray testing
for UI feedback
*/

class BilliardsCollisionSystem final : public cro::System, public cro::GuiClient
{
public:
    explicit BilliardsCollisionSystem(cro::MessageBus&);

    ~BilliardsCollisionSystem();

    BilliardsCollisionSystem(const BilliardsCollisionSystem&) = delete;
    BilliardsCollisionSystem(BilliardsCollisionSystem&&) = delete;
    BilliardsCollisionSystem& operator = (const BilliardsCollisionSystem&) = delete;
    BilliardsCollisionSystem& operator = (BilliardsCollisionSystem&&) = delete;

    void handleMessage(const cro::Message&) override;
    void process(float) override;

    void initTable(const struct TableData&);

    void toggleDebug();
    void renderDebug(const glm::mat4& viewProj, glm::uvec2 targetSize);

    struct RaycastResult final
    {
        glm::vec3 hitPointWorld = glm::vec3(0.f);
        glm::vec3 normalWorld = glm::vec3(0.f);
        bool hasHit = false;
    };
    RaycastResult rayCast(glm::vec3 worldPos, glm::vec3 direction) const;

private:
    BulletDebug m_debugDrawer;

    std::unique_ptr<btDefaultCollisionConfiguration> m_collisionCfg;
    std::unique_ptr<btCollisionDispatcher> m_collisionDispatcher;
    std::unique_ptr<btBroadphaseInterface> m_broadphaseInterface;
    std::unique_ptr<btCollisionWorld> m_collisionWorld;

    std::vector<std::unique_ptr<btPairCachingGhostObject>> m_ballObjects;
    std::vector<std::unique_ptr<btPairCachingGhostObject>> m_tableObjects; //remind me... why are these separate?
    std::vector<std::unique_ptr<btBvhTriangleMeshShape>> m_tableShapes;
    std::vector<std::unique_ptr<btTriangleIndexVertexArray>> m_tableVertices;

    std::vector<float> m_vertexData;
    std::vector<std::vector<std::uint32_t>> m_indexData;

    std::unique_ptr<btSphereShape> m_ballShape;
    std::unique_ptr<btBoxShape> m_pocketShape;

    void onEntityAdded(cro::Entity) override;
    void onEntityRemoved(cro::Entity) override;
};
