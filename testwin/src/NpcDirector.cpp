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

#include "NpcDirector.hpp"
#include "Messages.hpp"
#include "ResourceIDs.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/components/Transform.hpp>

NpcDirector::NpcDirector()
{

}

//private
void NpcDirector::handleEvent(const cro::Event& evt)
{

}

void NpcDirector::handleMessage(const cro::Message& msg)
{
    if (msg.id == MessageID::BackgroundSystem)
    {
        const auto& data = msg.getData<BackgroundEvent>();
        if (data.type == BackgroundEvent::MountCreated)
        {
            //if a new chunk is created send a message
            //to the turrets to tell them to reposition
            cro::Command cmd;
            cmd.targetFlags = CommandID::Turret;
            cmd.action = [data](cro::Entity entity, cro::Time)
            {
                auto& tx = entity.getComponent<cro::Transform>();
                if (tx.getParentID() == data.entityID)
                {
                    //this our man. Or woman. Turret.
                    tx.setPosition({ data.position, 0.f });
                }
            };
            sendCommand(cmd);
        }
    }
}

void NpcDirector::process(cro::Time)
{

}