/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#include "BalloonSystem.hpp"
#include "CommonConsts.hpp"
#include "GameConsts.hpp"
#include "Messages.hpp"
#include "ActorIDs.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>

namespace
{
    constexpr float MoveSpeed = 2.f;
}

BalloonSystem::BalloonSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(BalloonSystem))
{
    requireComponent<Balloon>();
    requireComponent<cro::Transform>();
}

//public
void BalloonSystem::process(float)
{
    constexpr float MaxDistance = MapWidth;

    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& balloon = entity.getComponent<Balloon>();
        auto& tx = entity.getComponent<cro::Transform>();

        tx.move({ MoveSpeed * balloon.direction * ConstVal::FixedGameUpdate, 0.f, 0.f });


        auto position = tx.getPosition();
        if (position.x > MaxDistance
            || position.x < -MaxDistance)
        {
            getScene()->destroyEntity(entity);
        }
        else
        {
            //check to see if we should drop a snail.
            if (position.x > -(MapWidth / 2.f)
                && position.x < (MapWidth / 2.f))
            {
                balloon.currentTime -= ConstVal::FixedGameUpdate;
                if (balloon.currentTime < 0)
                {
                    balloon.currentTime += Balloon::SnailTime;

                    auto* msg = postMessage<GameEvent>(MessageID::GameMessage);
                    msg->type = GameEvent::RequestSpawn;
                    msg->position = position;
                    msg->actorID = ActorID::PoopSnail;
                }
            }
        }
    }
}