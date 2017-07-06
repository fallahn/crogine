/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#include "PlayerWeaponSystem.hpp"
#include "Messages.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/PhysicsObject.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/util/Constants.hpp>

namespace
{
    const float laserSpeed = 9.f;
}

PlayerWeaponSystem::PlayerWeaponSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(PlayerWeaponSystem))
{
    requireComponent<PlayerWeapon>();
    requireComponent<cro::Transform>();
}

//public
void PlayerWeaponSystem::handleMessage(const cro::Message& msg)
{
    if (msg.id == MessageID::WeaponMessage)
    {
        const auto& data = msg.getData<WeaponEvent>();
        spawnLaser(laserSpeed * data.direction, data.position);
    }
}

void PlayerWeaponSystem::process(cro::Time dt)
{
    float dtSec = dt.asSeconds();
    processLasers(dtSec);
}

//private
void PlayerWeaponSystem::onEntityAdded(cro::Entity entity)
{
    auto& weapon = entity.getComponent<PlayerWeapon>();
    switch (weapon.type)
    {
    default:break;
    case PlayerWeapon::Laser:
        weapon.damage = 10.f;
        m_deadLaserCount++;
        m_deadLasers.push_back(entity.getIndex());
        m_aliveLasers.push_back(-1);
        break;
    }
}

void PlayerWeaponSystem::spawnLaser(float velocity, glm::vec3 position)
{
    if (m_deadLaserCount > 0)
    {
        m_deadLaserCount--;
        auto entity = getScene()->getEntity(m_deadLasers[m_deadLaserCount]);
        m_aliveLasers[m_aliveLaserCount] = m_deadLasers[m_deadLaserCount];
        m_aliveLaserCount++;


        if (velocity < 0)
        {
            entity.getComponent<cro::Transform>().setRotation({ 0.f, cro::Util::Const::PI, 0.f });
        }
        else
        {
            entity.getComponent<cro::Transform>().setRotation({ });
        }

        entity.getComponent<cro::Transform>().setPosition(position);
        entity.getComponent<PlayerWeapon>().velocity.x = velocity;
    }
}

void PlayerWeaponSystem::processLasers(float dtSec)
{
    for (auto i = 0u; i < m_aliveLaserCount; ++i)
    {
        auto ent = getScene()->getEntity(m_aliveLasers[i]);
        const auto& weapon = ent.getComponent<PlayerWeapon>();
        ent.getComponent<cro::Transform>().move(weapon.velocity * dtSec);

        //check collisions and deadify
        const auto& phys = ent.getComponent<cro::PhysicsObject>();
        if (phys.getCollisionCount() > 0)
        {
            ent.getComponent<cro::Transform>().setPosition(glm::vec3(-100.f));

            m_deadLasers[m_deadLaserCount] = m_aliveLasers[i];
            m_deadLaserCount++;

            m_aliveLaserCount--;
            m_aliveLasers[i] = m_aliveLasers[m_aliveLaserCount];
            i--;
        }
    }
}