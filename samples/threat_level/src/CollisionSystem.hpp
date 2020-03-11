/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
http://trederia.blogspot.com

crogine - Zlib license.

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

#include "PhysicsDebug.hpp"

#include <crogine/ecs/System.hpp>
#include <crogine/ecs/Entity.hpp>
#include <crogine/ecs/Renderable.hpp>
#include <crogine/gui/GuiClient.hpp>

#include <btBulletCollisionCommon.h>

#include <memory>
#include <array>
#include <unordered_map>

namespace cro
{
    /*!
    \brief Collision detection system.
    A wrapper around a bullet physics collision detection world, the collision
    system provides collision detection between entities with collision shape
    components. NOTE that no physics simulation is performed, for that a
    PhysicsSystem should be used. The collision system exists to provide detection
    of collisions between complex shapes with the option of user defined response or
    physics simulation.
    */
    class CollisionSystem : public cro::System, public Renderable, public GuiClient
    {
    public:
        explicit CollisionSystem(cro::MessageBus& mb);
        ~CollisionSystem();

        CollisionSystem(const CollisionSystem&) = delete;
        CollisionSystem& operator = (const CollisionSystem&) = delete;
        CollisionSystem(CollisionSystem&&) = delete;
        CollisionSystem& operator = (CollisionSystem&&) = delete;

        void process(float) override;

        void render(Entity) override;

    private:
        void onEntityAdded(cro::Entity) override;
        void onEntityRemoved(cro::Entity) override;

        std::unique_ptr<btCollisionConfiguration> m_collisionConfiguration;
        std::unique_ptr<btCollisionDispatcher> m_collisionDispatcher;
        std::unique_ptr<btBroadphaseInterface> m_broadphaseInterface;
        std::unique_ptr<btCollisionWorld> m_collisionWorld;

        struct CollisionData final
        {
            btCollisionShape* shape = nullptr;
            std::unique_ptr<btCollisionObject> object;
        };
        std::array<CollisionData, Detail::MinFreeIDs> m_collisionData;
        //std::vector<std::unique_ptr<btCollisionShape>> m_shapeCache;
        std::unordered_map<std::size_t, std::unique_ptr<btCollisionShape>> m_shapeCache;
        std::array<std::unique_ptr<btCompoundShape>, Detail::MinFreeIDs> m_compoundShapes;

        Detail::BulletDebug m_debugDrawer;
    };
}