/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
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

#include <crogine/detail/OpenGL.hpp>
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
    m_vertexData.clear();
    m_indexData.clear();

    const auto vertStride = cro::Mesh::readVertexData(meshData, m_vertexData, m_indexData);
    setCollisionMesh(meshData, vertStride);
}

void CollisionMesh::updateCollisionMesh(const cro::Mesh::Data& meshData, const std::vector<float>& verts, const std::vector<std::vector<std::uint32_t>>& indices)
{
    //this is only for pre-processing verts in offline mode!
    //use this for tooling when opengl isn't available (eg we're in another thread)
    m_vertexData = verts;

    m_indexData.clear();
    for (const auto& ia : indices)
    {
        auto& i = m_indexData.emplace_back();
        for (auto idx : ia)
        {
            assert(idx < std::numeric_limits<std::uint16_t>::max());
            i.push_back(static_cast<std::uint16_t>(idx));
        }
    }
    //we fudged the indices into 16 bits so we need
    //to hack aruond this...
    auto data = meshData;
    data.indexData[0].format = GL_UNSIGNED_SHORT;

    setCollisionMesh(data, meshData.vertexSize);
}

TerrainResult CollisionMesh::getTerrain(glm::vec3 position) const
{
    static const btVector3 RayLength(0.f, -60.f, 0.f);
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
}

void CollisionMesh::setCollisionMesh(const cro::Mesh::Data& meshData, std::uint32_t vertStride)
{
    clearCollisionObjects();

    std::int32_t colourOffset = 0;
    for (auto i = 0; i < cro::Mesh::Attribute::Colour; ++i)
    {
        colourOffset += static_cast<std::int32_t>(meshData.attributes[i].componentCount);
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
        //readVertexData unpacks everything to float so vert size is larger than meshData believes
        groundMesh.m_vertexStride = static_cast<int>(/*meshData.vertexSize*/vertStride);

        std::int32_t stride = 3;
        PHY_ScalarType indexType = PHY_UCHAR;
        switch (meshData.indexData[0].format)
        {
        default: break;
        case GL_UNSIGNED_SHORT:
            stride *= 2;
            indexType = PHY_SHORT;
            break;
        case GL_UNSIGNED_INT:
            stride *= 4;
            indexType = PHY_INTEGER;
            LogW << "Found integer indexing in collision mesh" << std::endl;
            break;
        }

        groundMesh.m_numTriangles = meshData.indexData[i].indexCount / 3;
        groundMesh.m_triangleIndexBase = reinterpret_cast<std::uint8_t*>(m_indexData[i].data());
        groundMesh.m_triangleIndexStride = stride;

        m_groundVertices.emplace_back(std::make_unique<btTriangleIndexVertexArray>())->addIndexedMesh(groundMesh, indexType);
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