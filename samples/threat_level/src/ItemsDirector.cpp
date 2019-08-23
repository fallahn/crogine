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

#include "ItemsDirector.hpp"
#include "ItemSystem.hpp"
#include "ResourceIDs.hpp"
#include "Messages.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/ecs/Entity.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/util/Random.hpp>

namespace
{
    const float moveSpeed = -0.8f;
}

ItemDirector::ItemDirector()
    : m_releaseTime (10.f),
    m_itemIndex     (0),
    m_active        (true)
{
    m_itemList = 
    {
        CollectableItem::Buddy,
        CollectableItem::Shield,
        CollectableItem::Shield,
        CollectableItem::EMP,
        CollectableItem::Buddy,
        CollectableItem::Shield,
        CollectableItem::Shield,
        CollectableItem::EMP,
        CollectableItem::Buddy,
        CollectableItem::Life
    };
}

//private
void ItemDirector::handleMessage(const cro::Message& msg)
{
    if (msg.id == MessageID::NpcMessage)
    {
        const auto& data = msg.getData<NpcEvent>();
        switch (data.type)
        {
        default: break;
        case NpcEvent::Died:
            if (m_releaseTime < 6)
            {
                cro::Command cmd;
                cmd.targetFlags = CommandID::Collectable;
                cmd.action = [data](cro::Entity entity, cro::Time)
                {
                    auto& collectable = entity.getComponent<CollectableItem>();
                    if (collectable.type == CollectableItem::WeaponUpgrade && !collectable.active)
                    {
                        collectable.active = true;
                        collectable.speed = moveSpeed - cro::Util::Random::value(0.05f, 0.18f);
                        entity.getComponent<cro::Transform>().setPosition(data.position);
                        //LOG("Spawned Weapon Upgrade " + std::to_string(data.position.x) + ", " + std::to_string(data.position.y), cro::Logger::Type::Info);
                    }
                };
                sendCommand(cmd);
            }

            break;
        }
    }
    else if (msg.id == MessageID::GameMessage)
    {
        const auto& data = msg.getData<GameEvent>();
        switch (data.type)
        {
        case GameEvent::GameStart:
        case GameEvent::RoundStart:
            m_active = true;
            break;
        default:
            m_active = false;
            break;
        }
    }
}

void ItemDirector::handleEvent(const cro::Event&)
{

}

void ItemDirector::process(cro::Time dt)
{
    if (m_active)
    {
        m_releaseTime -= dt.asSeconds();
        if (m_releaseTime < 0)
        {
            //ignore weapon upgrades, these are dropped by NPCs
            auto type = m_itemList[m_itemIndex];
            m_itemIndex = (m_itemIndex + 1) % m_itemList.size();

            cro::Command cmd;
            cmd.targetFlags = CommandID::Collectable;
            cmd.action = [type](cro::Entity entity, cro::Time)
            {
                auto& item = entity.getComponent<CollectableItem>();
                if (item.type == type && !item.active)
                {
                    item.active = true;
                    item.speed = moveSpeed - cro::Util::Random::value(0.05f, 0.18f);
                }
            };
            sendCommand(cmd);
            m_releaseTime = cro::Util::Random::value(8.f, 16.f);
        }
    }
}