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

#include <crogine/ecs/System.hpp>
#include <crogine/detail/NoResize.hpp>
#include <crogine/graphics/MeshData.hpp>
#include <crogine/graphics/BoundingBox.hpp>

#include <btBulletCollisionCommon.h>
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>

#include <memory>
#include <vector>

struct CollisionID final
{
    enum
    {
        Table, Cushion,
        Ball, Pocket,

        Count
    };

    static const std::array<std::string, Count> Labels;
};

struct SnookerID final
{
    enum
    {
        White, Red, 
        Yellow, Green, Brown,
        Blue, Pink, Black
    };
};

static constexpr std::int8_t CueBall = 0;

struct PocketInfo final
{
    glm::vec2 position = glm::vec2(0.f);
    std::int32_t value = 0;
    float radius = 0.06f;
};

struct TableData final
{
    std::string collisionModel;
    std::string viewModel;
    std::vector<PocketInfo> pockets;

    enum Rules
    {
        Eightball,
        Nineball,
        BarBillliards,
        Snooker,

        Void,
        Count
    }rules = Void;
    static const std::array<std::string, Rules::Count> RuleStrings;

    bool loadFromFile(const std::string&);
};

//this needs to be non-resizable as the physics world keeps references to motion states
struct BilliardBall final : public btMotionState, public cro::Detail::NonResizeable
{
    BilliardBall() : m_physicsBody(nullptr) {}
    void getWorldTransform(btTransform& worldTrans) const override;
    void setWorldTransform(const btTransform& worldTrans) override;

    /*
    In pool:
    0 is cue ball
    1 - 7 are spots / Reds
    8 - 8 Ball :)
    9 - 15 are stripes / Yellows

    In snooker/bar billiards this matches SnookerID
    */
    std::int8_t id = 0;
    bool hadUpdate = false;
    static constexpr float Mass = 0.156f;
    static constexpr float Radius = 0.0255f;

private:
    cro::Entity m_parent;
    union
    {
        btRigidBody* m_physicsBody;
        btPairCachingGhostObject* m_collisionBody;
    };
    std::int32_t m_pocketContact = -1; //ID of pocket, or -1
    std::int32_t m_prevPocketContact = -1; //client side only
    bool m_inPocketRadius = false;

    std::int8_t m_ballContact = -1; //other ball or -1
    std::int8_t m_prevBallContact = -1;

    //client side only
    std::int32_t m_cushionContact = -1;
    std::int32_t m_prevCushionContact = -1;

    friend class BilliardsSystem;
    friend class BilliardsCollisionSystem;
};

class BulletDebug;
class BilliardsSystem final : public cro::System
{
public:
    BilliardsSystem(cro::MessageBus&);
    BilliardsSystem(cro::MessageBus&, BulletDebug&);
    ~BilliardsSystem();

    BilliardsSystem(const BilliardsSystem&) = delete;
    BilliardsSystem(BilliardsSystem&&) = delete;

    BilliardsSystem& operator = (const BilliardsSystem&) = delete;
    BilliardsSystem& operator = (BilliardsSystem&&) = delete;

    void process(float) override;
    void initTable(const TableData&);

    void applyImpulse(glm::vec3 dir, glm::vec3 offset);

    bool hasCueball() const { return m_cueball != nullptr; }

private:

    std::int32_t m_awakeCount;
    bool m_shotActive;

    std::unique_ptr<btCollisionConfiguration> m_collisionConfiguration;
    std::unique_ptr<btCollisionDispatcher> m_collisionDispatcher;
    std::unique_ptr<btBroadphaseInterface> m_broadphaseInterface;
    std::unique_ptr<btSequentialImpulseConstraintSolver> m_constraintSolver;
    std::unique_ptr<btDiscreteDynamicsWorld> m_collisionWorld;

    //we have to keep a local copy of the table verts as the
    //collision world only maintains pointers to it
    std::vector<float> m_vertexData;
    std::vector<std::vector<std::uint32_t>> m_indexData;

    //these are what do the pointing.
    std::vector<std::unique_ptr<btRigidBody>> m_tableObjects;
    std::vector<std::unique_ptr<btTriangleIndexVertexArray>> m_tableVertices;
    std::vector<std::unique_ptr<btBvhTriangleMeshShape>> m_tableShapes;


    //tracks ball objects
    std::vector<std::unique_ptr<btRigidBody>> m_ballObjects;
    std::unique_ptr<btSphereShape> m_ballShape; //balls can all share this.
    btRigidBody* m_cueball;

    //table surface
    std::vector<std::unique_ptr<btBoxShape>> m_boxShapes;

    //pocket walls
    std::array<std::unique_ptr<btBoxShape>, 2u> m_pocketWalls;

    //simplified pocketry
    struct Pocket final
    {
        static constexpr float DefaultRadius = 0.06f;

        cro::Box box;
        std::int32_t id = 0;
        std::int32_t value = 0;
        float radius = DefaultRadius;
        glm::vec2 position = glm::vec2(0.f);
    };
    std::vector<Pocket> m_pockets;

#ifdef CRO_DEBUG_
    std::unique_ptr<btCylinderShape> m_pocketShape;
#endif

    btRigidBody::btRigidBodyConstructionInfo createBodyDef(std::int32_t, float, btCollisionShape*, btMotionState* = nullptr);

    void doBallCollision() const;
    void doPocketCollision(cro::Entity) const;

    void onEntityAdded(cro::Entity) override;
    void onEntityRemoved(cro::Entity) override;
};