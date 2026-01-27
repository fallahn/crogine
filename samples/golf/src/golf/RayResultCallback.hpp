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

#pragma once

#include <btBulletCollisionCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>

#include <crogine/detail/glm/vec3.hpp>

#include <cstdint>
#include <vector>
#include <memory>

namespace CollisionUtil
{
    template <typename T>
    std::array<std::uint32_t, 3u> getTriangleIndices(const std::uint8_t* data)
    {
        const auto* i = reinterpret_cast<const T*>(data);
        return std::array<std::uint32_t, 3u>(
        {
            *i,
            *(i + 1),
            *(i + 2)
        });
    }

    struct FaceData final
    {
        btVector3 normal;
        //contains the RGBA value of the first face vertex packed into an int
        std::int32_t collisionType = 0;
    };

    FaceData getFaceData(const btCollisionShape*, std::int32_t partID, std::int32_t triangleID, std::int32_t colourOffet);
}

struct CollisionGroup final
{
    enum
    {
        Ball = 1,
        Terrain = 2
    };
};

struct RayResultCallback final : public btCollisionWorld::AllHitsRayResultCallback//ClosestRayResultCallback
{
    RayResultCallback(const btVector3& rayFromWorld, const btVector3& rayToWorld);
    btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace) override;

    //std::int32_t m_collisionType = 0;
    //R|G|B|A from face, R == terrain
    btAlignedObjectArray<std::int32_t> m_collisionType;

private:

    CollisionUtil::FaceData getFaceData(const btCollisionWorld::LocalRayResult& rayResult, std::int32_t colourOffset) const;
};

//custom callback for sphere intersections
struct SphereResult final : public btCollisionWorld::ContactResultCallback
{
    const std::vector<std::unique_ptr<btPairCachingGhostObject>>* objects = nullptr;

    //we may have multiple contacts in a single collision
    struct Manifold final
    {
        glm::vec3 normal = glm::vec3(0.f);
        float penetration = 0.f;
        std::int32_t terrain = 0;
    };
    std::vector<Manifold> manifolds;

    static constexpr float MaxAngle = 8.f; //degrees.
    float maxTestAngle = MaxAngle;

    btScalar addSingleResult(btManifoldPoint&, const btCollisionObjectWrapper*, int, int, const btCollisionObjectWrapper*, int, int) override;
};