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

#include "PlayerState.hpp"
#include "PlayerSystem.hpp"
#include "CommonConsts.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/DynamicTreeComponent.hpp>

namespace
{
    constexpr float PauseTime = 4.f;
}

PlayerStateDead::PlayerStateDead()
    : m_pauseTime(PauseTime)
{

}

//public
void PlayerStateDead::processMovement(cro::Entity entity, Input, cro::Scene&)
{
    auto& player = entity.getComponent<Player>();

    //server will delete any carried crates so make sure we
    //reset these properties.
    player.avatar.getComponent<PlayerAvatar>().crateEnt = {};
    player.carrying = false;

    //only do this on the server
    //and wait for the client to sync
    if (!player.local)
    {
        m_pauseTime -= ConstVal::FixedGameUpdate;
        if (m_pauseTime < 0)
        {
            m_pauseTime = PauseTime;

            //we need to reset rotation, collision layer property etc.
            player.state = Player::State::Falling;
            player.direction = player.spawnPosition.x > 0 ? Player::Left : Player::Right;
            player.collisionLayer = player.spawnPosition.z > 0 ? 0 : 1;
            entity.getComponent<cro::DynamicTreeComponent>().setFilterFlags(player.collisionLayer + 1);
            entity.getComponent<cro::Transform>().setPosition(player.spawnPosition);
        }
    }
}

void PlayerStateDead::processCollision(cro::Entity, const std::vector<cro::Entity>&)
{

}