/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

#include "Terrain.hpp"
#include "DebugDraw.hpp"

#include <crogine/ecs/System.hpp>
#include <crogine/core/Clock.hpp>

#include <btBulletCollisionCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>

#include <memory>

namespace cro
{
    class Image;
}

struct RayResultCallback final : public btCollisionWorld::ClosestRayResultCallback
{
    RayResultCallback(const btVector3& rayFromWorld, const btVector3& rayToWorld);
    btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace) override;

private:
    btVector3 getFaceNormal(const btCollisionWorld::LocalRayResult& rayResult) const;
};

struct Ball final
{
    static constexpr float Radius = 0.0215f;
    enum class State
    {
        Idle, Flight, Putt, Paused, Reset
    }state = State::Idle;

    static const std::array<std::string, 5> StateStrings;

    std::uint8_t terrain = TerrainID::Fairway;

    glm::vec3 velocity = glm::vec3(0.f);
    float delay = 0.f;
    float spin = 0.f;

    glm::vec3 startPoint = glm::vec3(0.f);
    float lastStrokeDistance = 0.f;
    bool hadAir = false; //toggled when passing over hole

    //used for wall collision when putting
    btPairCachingGhostObject* collisionObject = nullptr;
};

class BallSystem final : public cro::System
{
public:
    //don't try and create debug drawer on server instances
    //there's no OpenGL context on the server thread.
    BallSystem(cro::MessageBus&, bool debug = false);
    ~BallSystem();

    BallSystem(const BallSystem&) = delete;
    BallSystem& operator = (const BallSystem&) = delete;

    BallSystem(BallSystem&&) = default;
    BallSystem& operator = (BallSystem&&) = default;

    void process(float) override;

    glm::vec3 getWindDirection() const;
    void forceWindChange();

    bool setHoleData(const struct HoleData&, bool rebuildMesh = true);

    void setGimmeRadius(std::uint8_t);

    struct TerrainResult final
    {
        std::uint8_t terrain = TerrainID::Scrub;
        glm::vec3 normal = glm::vec3(0.f, 1.f, 0.f);
        glm::vec3 intersection = glm::vec3(0.f);
        float penetration = 0.f; //positive values are down into the ground
    };
    TerrainResult getTerrain(glm::vec3) const;

#ifdef CRO_DEBUG_
    void setDebugFlags(std::int32_t);
    void renderDebug(const glm::mat4&, glm::uvec2);
#endif

private:

    cro::Clock m_windDirClock;
    cro::Clock m_windStrengthClock;

    cro::Time m_windDirTime;
    cro::Time m_windStrengthTime;

    glm::vec3 m_windDirection;
    glm::vec3 m_windDirSrc;
    glm::vec3 m_windDirTarget;

    float m_windStrength;
    float m_windStrengthSrc;
    float m_windStrengthTarget;

    float m_windInterpTime;
    float m_currentWindInterpTime;

    const HoleData* m_holeData;
    std::uint8_t m_gimmeRadius;

    void doCollision(cro::Entity);
    void updateWind();

    std::unique_ptr<btDefaultCollisionConfiguration> m_collisionCfg;
    std::unique_ptr<btCollisionDispatcher> m_collisionDispatcher;
    std::unique_ptr<btBroadphaseInterface> m_broadphaseInterface;
    std::unique_ptr<btCollisionWorld> m_collisionWorld;

    std::vector<std::unique_ptr<btPairCachingGhostObject>> m_groundObjects;
    std::vector<std::unique_ptr<btBvhTriangleMeshShape>> m_groundShapes;
    std::vector<std::unique_ptr<btTriangleIndexVertexArray>> m_groundVertices;

    std::vector<float> m_vertexData;
    std::vector<std::vector<std::uint32_t>> m_indexData;

    std::vector<std::unique_ptr<btPairCachingGhostObject>> m_ballObjects;
    std::unique_ptr<btSphereShape> m_ballShape;

#ifdef CRO_DEBUG_
    std::unique_ptr<BulletDebug> m_debugDraw;
#endif

    void initCollisionWorld(bool);
    void clearCollisionObjects();
    bool updateCollisionMesh(const std::string&);

    void onEntityAdded(cro::Entity) override;
    void onEntityRemoved(cro::Entity) override;
};