/*-----------------------------------------------------------------------

Matt Marchant 2017
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

#include <crogine/ecs/systems/CollisionSystem.hpp>
#include <crogine/core/Clock.hpp>

using namespace cro;

namespace
{
    btScalar worldSize = 500.f;
    const std::size_t maxObjects = 10000;
}

CollisionSystem::CollisionSystem(cro::MessageBus&mb)
    : cro::System(mb, typeid(CollisionSystem))
{
    m_collisionConfiguration = std::make_unique<btDefaultCollisionConfiguration>();
    m_collisionDispatcher = std::make_unique<btCollisionDispatcher>(m_collisionConfiguration.get());

    btVector3 worldMin(-worldSize, -worldSize, -worldSize);
    btVector3 worldMax(worldSize, worldSize, worldSize);

    //TODO inspect types of broadphase and decide which is best
    m_broadphaseInterface = std::make_unique<bt32BitAxisSweep3>(worldMin, worldMax, maxObjects, nullptr, true);
    m_collisionWorld = std::make_unique<btCollisionWorld>(m_collisionDispatcher.get(), m_broadphaseInterface.get(), m_collisionConfiguration.get());
}

//public
void CollisionSystem::process(cro::Time dt)
{
    m_collisionWorld->performDiscreteCollisionDetection();
}

//private
void CollisionSystem::onEntityAdded(cro::Entity entity)
{

}

void CollisionSystem::onEntityRemoved(cro::Entity entity)
{

}