/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2024
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

#include "Terrain.hpp"
#include "DebugDraw.hpp"
#include "RayResultCallback.hpp"

#include <crogine/ecs/System.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/detail/glm/vec3.hpp>

#include <BulletCollision/CollisionDispatch/btGhostObject.h>

#include <memory>

struct GolfBallEvent;
namespace cro
{
    class Image;
}

struct BullsEye final
{
    glm::vec3 position = glm::vec3(0.f);
    float diametre = 1.f;
    bool spawn = false; //if rx packet is false, delete the current target
};

struct BullHit final
{
    glm::vec3 position = glm::vec3(0.f);
    float accuracy = 0;
    std::uint8_t client = 0;
    std::uint8_t player = 0;
};

struct Ball final
{
    static constexpr float Radius = 0.0215f;
    enum class State
    {
        Idle, Flight, Roll, Putt, Paused, Reset
    }state = State::Idle;

    static const std::array<std::string, 5> StateStrings;

    std::uint8_t terrain = TerrainID::Fairway;
    std::uint8_t lie = 0; //0 buried, 1 sitting up
    bool checkGimme = false; //used in pause delay to not insta-gimme

    glm::vec3 velocity = glm::vec3(0.f);
    float delay = 0.f;
    float rotation = 0.f; //animation effect
    float windEffect = 0.f; //how much the ball is being affected by wind (normalised) used for UI display

    glm::vec2 spin = glm::vec2(0.f); //-1 to 1
    glm::vec3 initialForwardVector = glm::vec3(0.f); //normalised forward vector of velocity when impulse applied
    glm::vec3 initialSideVector = glm::vec3(0.f); //normalised right vector of velocity when impulse applied

    glm::vec3 startPoint = glm::vec3(0.f);
    float lastStrokeDistance = 0.f;
    bool hadAir = false; //toggled when passing over hole

    std::uint8_t client = 0; //needs to be tracked when sending multiplayer messages

    //used for wall collision when putting
    btPairCachingGhostObject* collisionObject = nullptr;
};

class BallSystem final : public cro::System
{
public:
    //don't try and create debug drawer on server instances
    //there's no OpenGL context on the server thread.
    explicit BallSystem(cro::MessageBus&, bool debug = false);
    ~BallSystem();

    BallSystem(const BallSystem&) = delete;
    BallSystem& operator = (const BallSystem&) = delete;

    BallSystem(BallSystem&&) = default;
    BallSystem& operator = (BallSystem&&) = delete;

    void process(float) override;

    glm::vec3 getWindDirection() const;
    void forceWindChange();

    //this will modify the hole data by reading the collision
    //mesh and correcting the height on the pin property.
    bool setHoleData(struct HoleData&, bool rebuildMesh = true);

    void setGimmeRadius(std::uint8_t);

    //always spawns at target point of current hole
    //use this func to control when it's spawned.
    //setHoleData always resets any existing.
    const BullsEye& spawnBullsEye();

    struct TerrainResult final
    {
        std::uint8_t terrain = TerrainID::Scrub;
        std::uint8_t trigger = TriggerID::Count;
        glm::vec3 normal = glm::vec3(0.f, 1.f, 0.f);
        glm::vec3 intersection = glm::vec3(0.f);
        float penetration = 0.f; //positive values are down into the ground
    };
    TerrainResult getTerrain(glm::vec3 position, glm::vec3 forward = glm::vec3(0.f, -1.f, 0.f), float rayLength = 20.f) const;

    bool getPuttFromTee() const { return m_puttFromTee; }

    //reducing the timestep runs this faster, though less accurately
    void runPrediction(cro::Entity, float timestep = 1.f/60.f);

    void fastForward(cro::Entity);

#ifdef CRO_DEBUG_
    void setDebugFlags(std::int32_t);
    void renderDebug(const glm::mat4&, glm::uvec2);
#endif

    static constexpr glm::vec3 Gravity = glm::vec3(0.f, -9.8f, 0.f);

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
    bool m_puttFromTee;
    std::uint8_t m_gimmeRadius;
    std::uint8_t m_activeGimme;

    BullsEye m_bullsEye;

    void doCollision(cro::Entity);
    void doBallCollision(cro::Entity);
    void doBullsEyeCollision(glm::vec3, std::uint8_t client);
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

#ifdef CRO_DEBUG_
    std::unique_ptr<BulletDebug> m_debugDraw;
#endif

    struct ProcessFlags final
    {
        enum
        {
            Predicting = (1 << 0),
            FastForward = (1 << 1)
        };
    };
    std::uint32_t m_processFlags;
    void fastProcess(cro::Entity, float);
    GolfBallEvent* postEvent() const;

    void processEntity(cro::Entity, float);

    void initCollisionWorld(bool);
    void clearCollisionObjects();
    bool updateCollisionMesh(const std::string&);
};