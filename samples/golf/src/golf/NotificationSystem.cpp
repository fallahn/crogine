/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

#include "NotificationSystem.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Text.hpp>

namespace
{
    constexpr float CharTime = 0.04f;
    constexpr float HoldTime = 5.f;
}

NotificationSystem::NotificationSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(NotificationSystem))
{
    requireComponent<cro::Text>();
    requireComponent<Notification>();
}

//public
void NotificationSystem::process(float dt)
{
    auto& entities = getEntities();
    if (!entities.empty())
    {
        //only process the first one so messages are displayed sequentially
        auto& notification = entities[0].getComponent<Notification>();
        auto& text = entities[0].getComponent<cro::Text>();

        switch (notification.state)
        {
        default: break;
        case Notification::In:
        {
            notification.currentTime += dt;
            if (notification.currentTime > CharTime)
            {
                notification.currentTime -= CharTime;
                notification.charPosition++;

                text.setString(notification.message.substr(0, notification.charPosition));

                if (notification.charPosition == notification.message.size())
                {
                    notification.currentTime = HoldTime;
                    notification.state = Notification::Hold;
                }
            }
        }
            break;
        case Notification::Hold:
            notification.currentTime -= dt;
            if (notification.currentTime < 0)
            {
                notification.state = Notification::Out;
                notification.currentTime = 0.f;
            }
            break;
        case Notification::Out:
        {
            notification.currentTime += dt;
            if (notification.currentTime > CharTime)
            {
                notification.currentTime -= CharTime;
                notification.charPosition--;

                text.setString(notification.message.substr(0, notification.charPosition));
                
                if (notification.charPosition == 1)
                {
                    getScene()->destroyEntity(entities[0]);
                }
            }
        }
            break;
        }
    }
}