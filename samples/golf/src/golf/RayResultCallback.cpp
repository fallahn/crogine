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

#include "RayResultCallback.hpp"
#include "Terrain.hpp"

#include <cmath>
#include <algorithm>

//custom callback to return proper face normal (I wish we could cache these...)
RayResultCallback::RayResultCallback(const btVector3& rayFromWorld, const btVector3& rayToWorld)
    : ClosestRayResultCallback(rayFromWorld, rayToWorld)
{
}

btScalar RayResultCallback::addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace)
{
    //caller already does the filter on the m_closestHitFraction
    btAssert(rayResult.m_hitFraction <= m_closestHitFraction);

    m_closestHitFraction = rayResult.m_hitFraction;
    m_collisionObject = rayResult.m_collisionObject;
    if (rayResult.m_collisionObject->getCollisionFlags() == CollisionGroup::Ball)
    {
        //TODO getFaceData could fill these directly, but it's clearer
        //what's going on if we do it here.

        auto [normal, collisionType] = getFaceData(rayResult, m_collisionObject->getUserIndex());
        m_hitNormalWorld = normal;
        m_collisionType = collisionType;
        m_hitPointWorld.setInterpolate3(m_rayFromWorld, m_rayToWorld, rayResult.m_hitFraction);
    }
    else
    {
        rayResult.m_collisionObject = nullptr; //remove the ball objects then hasHit() returns false
    }
    return rayResult.m_hitFraction;
}

RayResultCallback::FaceData RayResultCallback::getFaceData(const btCollisionWorld::LocalRayResult& rayResult, std::int32_t colourOffset) const
{
    /*
    Respect to https://pybullet.org/Bullet/phpBB3/viewtopic.php?t=12826
    */

    const unsigned char* vertices = nullptr;
    int numVertices = 0;
    int vertexStride = 0;
    PHY_ScalarType verticesType;

    const unsigned char* indices = nullptr;
    int indicesStride = 0;
    int numFaces = 0;
    PHY_ScalarType indicesType;

    const auto vertex = [&](int vertexIndex)
    {
        const auto* data = reinterpret_cast<const btScalar*>(vertices + vertexIndex * vertexStride);
        return btVector3(data[0], data[1], data[2]);
    };

    const auto colour = [&](int vertexIndex)
    {
        const auto* data = reinterpret_cast<const btScalar*>(vertices + vertexIndex * vertexStride);
        auto r = std::clamp(data[colourOffset], 0.f, 1.f) * 255.f;
        auto g = std::clamp(data[colourOffset + 1], 0.f, 1.f) * 255.f;
        auto b = std::clamp(data[colourOffset + 2], 0.f, 1.f) * 255.f;

        r = std::min(std::floor(r / 10.f), static_cast<float>(TerrainID::Stone));
        g = std::floor(g / 10.f);
        b = std::floor(b / 10.f);

        return (std::int32_t(r) << 24) | (std::int32_t(g) << 16) | (std::int32_t(b) << 8);
    };

    const auto* triangleShape = static_cast<const btBvhTriangleMeshShape*>(rayResult.m_collisionObject->getCollisionShape());
    const auto* triangleMesh = static_cast<const btTriangleIndexVertexArray*>(triangleShape->getMeshInterface());

    triangleMesh->getLockedReadOnlyVertexIndexBase(
        &vertices, numVertices, verticesType, vertexStride, &indices, indicesStride, numFaces, indicesType, rayResult.m_localShapeInfo->m_shapePart
    );

    //ugh different meshes have different strides TODO fix collision detection in BallSystem
    //std::array<std::uint32_t, 3u> ind = {};
    //if (indicesType == PHY_SHORT)
    //{
        const auto* i = reinterpret_cast<const std::uint16_t*>(indices + rayResult.m_localShapeInfo->m_triangleIndex * indicesStride);
        const std::array<std::uint32_t, 3u> ind =
        {
            *i,
            *(i + 1),
            *(i + 2)
        };
    //}
    //else
    //{
    //    const auto* i = reinterpret_cast<const std::uint32_t*>(indices + rayResult.m_localShapeInfo->m_triangleIndex * indicesStride);
    //    ind[0] = *i;
    //    ind[1] = *(i + 1);
    //    ind[2] = *(i + 2);
    //}
    btVector3 va = vertex(ind[0]);
    btVector3 vb = vertex(ind[1]);
    btVector3 vc = vertex(ind[2]);
    btVector3 normal = (vb - va).cross(vc - va).normalized();

    std::int32_t collisionType = colour(ind[0]);

    triangleMesh->unLockReadOnlyVertexBase(rayResult.m_localShapeInfo->m_shapePart);

    return { normal, collisionType };
}