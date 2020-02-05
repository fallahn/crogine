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

#include "BossSystem.hpp"

#include <crogine/ecs/Entity.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/core/Clock.hpp>

BossSystem::BossSystem(cro::MessageBus& mb)
    :cro::System(mb, typeid(BossSystem))
{
    requireComponent<cro::Transform>();
    requireComponent<cro::Model>();
    requireComponent<Boss>();
}

//public
void BossSystem::handleMessage(const cro::Message& msg)
{

}

void BossSystem::process(float dt)
{
    auto& entities = getEntities();

    for (auto& entity : entities)
    {
        const auto& boss = entity.getComponent<Boss>();
        switch (boss.state)
        {
        default:break;
        case Boss::Appearance:
            processAppearance(entity, dt);
            break;
        case Boss::Shooting:
            processShooting(entity, dt);
            break;
        }
    }

}

//private
void BossSystem::processAppearance(cro::Entity entity, float dt)
{
    auto& boss = entity.getComponent<Boss>();
    auto& tx = entity.getComponent<cro::Transform>();

    tx.move({ boss.moveSpeed * dt, 0.f, 0.f });

    auto visible = entity.getComponent<cro::Model>().isVisible();
    if (!visible && boss.wantsReset)
    {
        tx.setPosition({ 100.f, 1000.f, 9.3f });
        boss.wantsReset = false;
        boss.state = Boss::Idle;
    }
    else
    {
        boss.wantsReset = true;
    }
}

void BossSystem::processShooting(cro::Entity, float)
{

}