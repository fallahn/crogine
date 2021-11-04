/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#include <crogine/graphics/MeshData.hpp>

#include <btBulletCollisionCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>

#include <memory>

class CollisionMesh final
{
public:
    CollisionMesh();
    ~CollisionMesh();

    CollisionMesh(const CollisionMesh&) = delete;
    CollisionMesh& operator = (const CollisionMesh&) = delete;
    CollisionMesh(CollisionMesh&&) = delete;
    CollisionMesh& operator = (CollisionMesh&&) = delete;

    void updateCollisionMesh(const cro::Mesh::Data&);
    std::pair<float, std::int32_t> getTerrain(glm::vec3 position) const;

    void renderDebug(const glm::mat4& viewProj, glm::uvec2 targetSize);
    void setDebugFlags(std::int32_t);

private:
    BulletDebug m_debugDrawer;

    std::unique_ptr<btDefaultCollisionConfiguration> m_collisionCfg;
    std::unique_ptr<btCollisionDispatcher> m_collisionDispatcher;
    std::unique_ptr<btBroadphaseInterface> m_broadphaseInterface;
    std::unique_ptr<btCollisionWorld> m_collisionWorld;

    std::vector<std::unique_ptr<btPairCachingGhostObject>> m_groundObjects;
    std::vector<std::unique_ptr<btBvhTriangleMeshShape>> m_groundShapes;
    std::vector<std::unique_ptr<btTriangleIndexVertexArray>> m_groundVertices;

    std::vector<float> m_vertexData;
    std::vector<std::vector<std::uint32_t>> m_indexData;


    void initCollisionWorld();
    void clearCollisionObjects();
};
