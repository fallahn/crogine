/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

#include "CollisionMesh.hpp"
#include "RayResultCallback.hpp"
#include "GameConsts.hpp"

#include <crogine/detail/glm/mat4x4.hpp>

namespace
{

}

CollisionMesh::CollisionMesh()
{
    initCollisionWorld();
}

CollisionMesh::~CollisionMesh()
{
    clearCollisionObjects();
}

//public
void CollisionMesh::updateCollisionMesh(const cro::Mesh::Data& meshData)
{
    clearCollisionObjects();
    cro::Mesh::readVertexData(meshData, m_vertexData, m_indexData);

    std::int32_t colourOffset = 0;
    for (auto i = 0; i < cro::Mesh::Attribute::Colour; ++i)
    {
        colourOffset += static_cast<std::int32_t>(meshData.attributes[i]);
    }


    //we have to create a specific object for each sub mesh
    //to be able to tag it with a different terrain...

    //TODO this is basically the same as the BallSystem - so we probably ought
    //to make this class a member of BallSystem
    for (auto i = 0u; i < m_indexData.size(); ++i)
    {
        btIndexedMesh groundMesh;
        groundMesh.m_vertexBase = reinterpret_cast<std::uint8_t*>(m_vertexData.data());
        groundMesh.m_numVertices = static_cast<int>(meshData.vertexCount);
        groundMesh.m_vertexStride = static_cast<int>(meshData.vertexSize);

        groundMesh.m_numTriangles = meshData.indexData[i].indexCount / 3;
        groundMesh.m_triangleIndexBase = reinterpret_cast<std::uint8_t*>(m_indexData[i].data());
        groundMesh.m_triangleIndexStride = 3 * sizeof(std::uint32_t);

        m_groundVertices.emplace_back(std::make_unique<btTriangleIndexVertexArray>())->addIndexedMesh(groundMesh);
        m_groundShapes.emplace_back(std::make_unique<btBvhTriangleMeshShape>(m_groundVertices.back().get(), false));
        m_groundObjects.emplace_back(std::make_unique<btPairCachingGhostObject>())->setCollisionShape(m_groundShapes.back().get());
        m_groundObjects.back()->setUserIndex(colourOffset); //used by RayResultCallback to read the terrain type
        m_collisionWorld->addCollisionObject(m_groundObjects.back().get(), CollisionGroup::Terrain, CollisionGroup::Ball);
    }

#ifdef CRO_DEBUG_
    if (m_collisionWorld->getDebugDrawer()->getDebugMode())
    {
        m_collisionWorld->debugDrawWorld();
    }
#endif
}

TerrainResult CollisionMesh::getTerrain(glm::vec3 position) const
{
    static const btVector3 RayLength(0.f, -50.f, 0.f);
    auto worldPos = glmToBt(position);
    worldPos -= (RayLength / 2.f);

    TerrainResult retVal;

    //btCollisionWorld::ClosestRayResultCallback res(worldPos, worldPos + RayLength);
    //m_collisionWorld->rayTest(worldPos, worldPos + RayLength, res);

    //TODO this might not stictly be necessary for client collision
    //but we might want to share this class with the ball system eventually
    RayResultCallback res(worldPos, worldPos + RayLength);
    m_collisionWorld->rayTest(worldPos, worldPos + RayLength, res);

    if (res.hasHit())
    {
        retVal.height = res.m_hitPointWorld.y();
        retVal.terrain = (res.m_collisionType >> 24);
        retVal.trigger = ((res.m_collisionType & 0x00ff0000) >> 16);
        retVal.normal = { res.m_hitNormalWorld.getX(), res.m_hitNormalWorld.getY(), res.m_hitNormalWorld.getZ() };
        retVal.wasRayHit = true;
    }

    return retVal;
}

TerrainResult CollisionMesh::getTerrain(glm::vec3 rayStart, glm::vec3 rayDir) const 
{
    auto start = glmToBt(rayStart);
    auto end = glmToBt(rayStart + rayDir);

    TerrainResult retVal;

    RayResultCallback res(start, end);
    m_collisionWorld->rayTest(start, end, res);

    if (res.hasHit())
    {
        retVal.height = res.m_hitPointWorld.y();
        retVal.terrain = (res.m_collisionType >> 24);
        retVal.trigger = ((res.m_collisionType & 0x00ff0000) >> 16);
        retVal.normal = { res.m_hitNormalWorld.getX(), res.m_hitNormalWorld.getY(), res.m_hitNormalWorld.getZ() };
        retVal.wasRayHit = true;
    }

    return retVal;
}

void CollisionMesh::renderDebug(const glm::mat4& mat, glm::uvec2 targetSize)
{
    m_debugDrawer.render(mat, targetSize);
}

void CollisionMesh::setDebugFlags(std::int32_t flags)
{
    m_debugDrawer.setDebugMode(flags);
    m_collisionWorld->debugDrawWorld();
}

//private
void CollisionMesh::initCollisionWorld()
{
    m_collisionCfg = std::make_unique<btDefaultCollisionConfiguration>();
    m_collisionDispatcher = std::make_unique<btCollisionDispatcher>(m_collisionCfg.get());

    m_broadphaseInterface = std::make_unique<btDbvtBroadphase>();
    m_collisionWorld = std::make_unique<btCollisionWorld>(m_collisionDispatcher.get(), m_broadphaseInterface.get(), m_collisionCfg.get());

    m_collisionWorld->setDebugDrawer(&m_debugDrawer);
}

void CollisionMesh::clearCollisionObjects()
{
    for (auto& obj : m_groundObjects)
    {
        m_collisionWorld->removeCollisionObject(obj.get());
    }

    m_groundObjects.clear();
    m_groundShapes.clear();
    m_groundVertices.clear();

    m_vertexData.clear();
    m_indexData.clear();
}
