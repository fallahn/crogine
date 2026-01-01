/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2026
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
#include "GameConsts.hpp"

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


CollisionUtil::FaceData RayResultCallback::getFaceData(const btCollisionWorld::LocalRayResult& rayResult, std::int32_t colourOffset) const
{
    return CollisionUtil::getFaceData(rayResult.m_collisionObject->getCollisionShape(), rayResult.m_localShapeInfo->m_shapePart, rayResult.m_localShapeInfo->m_triangleIndex, colourOffset);
}


//results callback when using a sphere / cylinder shape to test for walls
btScalar SphereResult::addSingleResult(btManifoldPoint& cp, const btCollisionObjectWrapper*, int, int, const btCollisionObjectWrapper* obj0, int partID0, int triangleIndex0)
{
    //offset into colour information from beginning of vertex data
    const auto colourOffset = obj0->getCollisionObject()->getUserIndex();

    //for some reason the object passed in as a param returns a nullptr so we have to hack around it like this
    const auto& arr = *objects;
    const auto [faceNormal, _] = CollisionUtil::getFaceData(arr[obj0->getCollisionObject()->getUserIndex2()]->getCollisionShape(), partID0, triangleIndex0, colourOffset);

    //NOTE the that normal direction *flips* if the centre of
    //the sphere is the other side of the collision plane...
    //but the penetration, not always :S
    //so we use the calculated face normal instead

    const auto normal = btToGlm(/*cp.m_normalWorldOnB*/faceNormal);
    static constexpr float MaxAngle = 6.f / 90.f;

    //only keep this collision if the surface is near vertical
    const auto angle = glm::dot(cro::Transform::Y_AXIS, normal);
    if (angle < MaxAngle
        && angle >= 0.f)
    {
        auto& man = manifolds.emplace_back();
        man.normal = normal;

        //TODO this isn't correct so we're ignoring it when processing the result
        //perhaps we could use the point on worldB property to calculate the correction?
        man.penetration = -cp.m_distance1;

        //LogI << normal << ", " << btToGlm(faceNormal) << std::endl;
    }
    /*else
    {
        LogI << angle << ": threshold not met" << std::endl;
    }*/
    return 0.f; //what is this even returning? bullet docs are *infuriating*
}



//util to fetch face data from collision mesh
CollisionUtil::FaceData CollisionUtil::getFaceData(const btCollisionShape* shape, std::int32_t partID, std::int32_t triangleID, std::int32_t colourOffset)
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

    const auto* triangleShape = static_cast<const btBvhTriangleMeshShape*>(shape);
    const auto* triangleMesh = static_cast<const btTriangleIndexVertexArray*>(triangleShape->getMeshInterface());

    triangleMesh->getLockedReadOnlyVertexIndexBase(
        &vertices, numVertices, verticesType, vertexStride, &indices, indicesStride, numFaces, indicesType, partID
    );

    //ugh different meshes have different strides
    std::array<std::uint32_t, 3u> ind = {};    
    switch (indicesStride)
    {
    default: break;
    case 3:
        ind = CollisionUtil::getIndices<std::uint8_t>(indices + triangleID * indicesStride);
        break;
    case 6:
        ind = CollisionUtil::getIndices<std::uint16_t>(indices + triangleID * indicesStride);
        break;
    case 12:
        ind = CollisionUtil::getIndices<std::uint32_t>(indices + triangleID * indicesStride);
        break;
    }

    const btVector3 va = vertex(ind[0]);
    const btVector3 vb = vertex(ind[1]);
    const btVector3 vc = vertex(ind[2]);
    const btVector3 normal = (vb - va).cross(vc - va).normalized();

    const std::int32_t collisionType = colour(ind[0]);

    triangleMesh->unLockReadOnlyVertexBase(partID);

    return { normal, collisionType };
}