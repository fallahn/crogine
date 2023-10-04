/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2023
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

//TODO remove this
#ifdef CRO_DEBUG_
#include "TableData.hpp"

#include <crogine/ecs/System.hpp>
#include <crogine/detail/NoResize.hpp>
#include <crogine/graphics/MeshData.hpp>
#include <crogine/graphics/BoundingBox.hpp>
#include <crogine/gui/GuiClient.hpp>

#include <BulletCollision/CollisionDispatch/btGhostObject.h> //client side

#include <reactphysics3d/reactphysics3d.h>

#include <memory>
#include <vector>



//this needs to be non-resizable as the physics world keeps references to motion states
struct BilliardBallReact final// : public btMotionState, public cro::Detail::NonResizeable
{
    BilliardBallReact() : m_physicsBody(nullptr) {}
    //void getWorldTransform(btTransform& worldTrans) const override;
    //void setWorldTransform(const btTransform& worldTrans) override;
    glm::vec3 getVelocity() const;

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
    static constexpr float Mass = 0.2f;// 0.156f;
    static constexpr float Radius = 0.0255f;

    std::int8_t getContact() const { return m_ballContact; }

private:
    cro::Entity m_parent;
    union
    {
        rp3d::RigidBody* m_physicsBody = nullptr;
        btPairCachingGhostObject* m_collisionBody; //client side
    };
    std::int32_t m_pocketContact = -1; //ID of pocket, or -1
    std::int32_t m_prevPocketContact = -1; //client side only
    bool m_inPocketRadius = false;

    std::int8_t m_ballContact = -1; //other ball or -1
    std::int8_t m_prevBallContact = -1;

    //client side only
    std::int32_t m_cushionContact = -1;
    std::int32_t m_prevCushionContact = -1;

    friend class BilliardsSystemReact;
    friend class BilliardsCollisionSystem;
};

using BPhysBall = BilliardBallReact;

class BulletDebug;
class BilliardsSystemReact final : public cro::System, public cro::GuiClient
{
public:
    explicit BilliardsSystemReact(cro::MessageBus&);
    //BilliardsSystemReact(cro::MessageBus&, BulletDebug&);
    ~BilliardsSystemReact();

    BilliardsSystemReact(const BilliardsSystemReact&) = delete;
    BilliardsSystemReact(BilliardsSystemReact&&) = delete;

    BilliardsSystemReact& operator = (const BilliardsSystemReact&) = delete;
    BilliardsSystemReact& operator = (BilliardsSystemReact&&) = delete;

    void process(float) override;
    void initTable(const TableData&);

    void applyImpulse(glm::vec3 dir, glm::vec3 offset);

    bool hasCueball() const { return m_cueball != nullptr; }
    glm::vec3 getCueballPosition() const;

    bool isValidSpawnPosition(glm::vec3) const;

private:

    std::int32_t m_awakeCount;
    bool m_shotActive;

    rp3d::PhysicsCommon m_physicsCommon;
    rp3d::PhysicsWorld* m_physWorld;

    //we have to keep a local copy of the table verts as the
    //collision world only maintains pointers to it
    std::vector<float> m_vertexData;
    std::vector<std::vector<std::uint32_t>> m_indexData;

    std::vector<std::unique_ptr<rp3d::TriangleVertexArray>> m_tableVertices;

    //these are what do the pointing.
    //std::vector<rp3d::RigidBody*> m_tableObjects;
    //std::vector<std::unique_ptr<btTriangleIndexVertexArray>> m_tableVertices;
    //std::vector<std::unique_ptr<btBvhTriangleMeshShape>> m_tableShapes;


    //tracks ball objects
    std::vector<rp3d::RigidBody*> m_ballObjects;
    rp3d::SphereShape* m_ballShape; //balls can all share this.
    rp3d::RigidBody* m_cueball;

    //table surface
    std::vector<rp3d::BoxShape*> m_boxShapes;

    //pocket walls
    std::array<rp3d::BoxShape*, 2u> m_pocketWalls = {};

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

    cro::FloatRect m_spawnArea;

    //btRigidBody::btRigidBodyConstructionInfo createBodyDef(std::int32_t, float, btCollisionShape*, btMotionState* = nullptr);

    void doBallCollision() const;
    void doPocketCollision(cro::Entity) const;

    void onEntityAdded(cro::Entity) override;
    void onEntityRemoved(cro::Entity) override;
};

using BPhysSystem = BilliardsSystemReact;
#endif //CRO_DEBUG_