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

#include "SpawnAreaSystem.hpp"
#include "Messages.hpp"
#include "PlayerSystem.hpp"
#include "ActorIDs.hpp"

SpawnAreaSystem::SpawnAreaSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(SpawnAreaSystem))
{
    requireComponent<SpawnArea>();
}

//public
void SpawnAreaSystem::handleMessage(const cro::Message& msg)
{
    if (msg.id == MessageID::PlayerMessage)
    {
        const auto& data = msg.getData<PlayerEvent>();
        if (data.type == PlayerEvent::Spawned)
        {
            auto id = data.player.getComponent<Player>().avatar.getComponent<Actor>().id;

            //this only works because we're assuming there'll never be more than 4 spawns
            auto& entities = getEntities();
            for (auto entity : entities)
            {
                auto& spawn = entity.getComponent<SpawnArea>();
                if (spawn.playerID == id)
                {
                    spawn.currentTime = SpawnArea::ActiveTime;
                    break;
                }
            }
        }
    }
}

void SpawnAreaSystem::process(float dt)
{
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        entity.getComponent<SpawnArea>().currentTime -= dt;
    }
}

//private
void SpawnAreaSystem::onEntityAdded(cro::Entity entity)
{
    CRO_ASSERT(entity.getComponent<SpawnArea>().playerID < 4, "Player ID not set");
}