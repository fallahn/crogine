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

#include "HudDirector.hpp"
#include "HudItems.hpp"
#include "Messages.hpp"
#include "ItemSystem.hpp"
#include "ResourceIDs.hpp"

#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/core/Clock.hpp>

HudDirector::HudDirector()
    : m_fireMode(0)
{

}

//public
void HudDirector::handleEvent(const cro::Event&)
{

}

void HudDirector::handleMessage(const cro::Message& msg)
{
    if (msg.id == MessageID::PlayerMessage)
    {
        const auto& data = msg.getData<PlayerEvent>();
        switch (data.type)
        {
        default:break;
        case PlayerEvent::CollectedItem:
            if (data.itemID == CollectableItem::Bomb ||
                data.itemID == CollectableItem::EMP)
            {
                auto id = data.itemID;
                cro::Command cmd;
                cmd.targetFlags = CommandID::HudElement;
                cmd.action = [&, id](cro::Entity entity, cro::Time)
                {
                    const auto& hudItem = entity.getComponent<HudItem>();
                    if ((hudItem.type == HudItem::Type::Bomb && id == CollectableItem::Bomb) ||
                        (hudItem.type == HudItem::Type::Emp && id == CollectableItem::EMP))
                    {
                         entity.getComponent<cro::Sprite>().setColour(cro::Colour::Cyan());
                         if (hudItem.type == HudItem::Type::Emp)
                         {
                             auto childEnt = getScene().getEntity(entity.getComponent<cro::Transform>().getChildIDs()[0]);
                             childEnt.getComponent<cro::Callback>().active = true;
                         }
                    }
                };
                sendCommand(cmd);
            }
            break;
        case PlayerEvent::Died:
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::HudElement;
            cmd.action = [&](cro::Entity entity, cro::Time)
            {
                const auto& hudItem = entity.getComponent<HudItem>();
                if (hudItem.type == HudItem::Type::Emp)
                {
                    entity.getComponent<cro::Sprite>().setColour(cro::Colour::White());
                    auto childEnt = getScene().getEntity(entity.getComponent<cro::Transform>().getChildIDs()[0]);
                    childEnt.getComponent<cro::Callback>().active = false;
                    childEnt.getComponent<cro::Model>().setMaterialProperty(0, "u_time", 0.f);
                }
            };
            sendCommand(cmd);
        }
        //yes, this is meant to fall through.
        case PlayerEvent::GotLife:
        {
            cro::int32 lives = static_cast<cro::int32>(data.value);
            cro::Command cmd;
            cmd.targetFlags = CommandID::HudElement;
            cmd.action = [lives](cro::Entity entity, cro::Time)
            {
                const auto& hudItem = entity.getComponent<HudItem>();
                if (hudItem.type == HudItem::Type::Life)
                {
                    if (hudItem.value < lives)
                    {
                        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Cyan());
                    }
                    else
                    {
                        entity.getComponent<cro::Sprite>().setColour(cro::Colour::White());
                    }
                }
            };
            sendCommand(cmd);

            //do a popup
            cmd.targetFlags = CommandID::MeatMan;
            cmd.action = [](cro::Entity entity, cro::Time)
            {
                entity.getComponent<cro::Callback>().active = true;
            };
            sendCommand(cmd);
        }
            break;
        case PlayerEvent::FiredEmp:
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::HudElement;
            cmd.action = [&](cro::Entity entity, cro::Time)
            {
                const auto& hudItem = entity.getComponent<HudItem>();
                if (hudItem.type == HudItem::Type::Emp)
                {
                    entity.getComponent<cro::Sprite>().setColour(cro::Colour::White());
                    auto childEnt = getScene().getEntity(entity.getComponent<cro::Transform>().getChildIDs()[0]);
                    childEnt.getComponent<cro::Callback>().active = false;
                    childEnt.getComponent<cro::Model>().setMaterialProperty(0, "u_time", 0.f);
                }
            };
            sendCommand(cmd);
        }
            break;
        }
    }
    else if (msg.id == MessageID::WeaponMessage)
    {
        const auto& data = msg.getData<WeaponEvent>();
        if (data.fireMode != m_fireMode)
        {
            auto mode = data.fireMode + 1;
            cro::Command cmd;
            cmd.targetFlags = CommandID::HudElement;
            cmd.action = [mode](cro::Entity entity, cro::Time)
            {
                if (entity.getComponent<HudItem>().type == HudItem::Type::TimerIcon)
                {
                    auto& sprite = entity.getComponent<cro::Sprite>();
                    auto subrect = sprite.getTextureRect();
                    auto size = sprite.getSize();
                    subrect.left = mode * size.x;
                    sprite.setTextureRect(subrect);
                }
            };
            sendCommand(cmd);

            m_fireMode = data.fireMode;
        }
    }
}

void HudDirector::process(cro::Time)
{

}