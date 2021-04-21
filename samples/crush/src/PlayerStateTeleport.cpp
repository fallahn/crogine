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
#include "GameConsts.hpp"
#include "CommonConsts.hpp"
#include "Collision.hpp"
#include "CrateSystem.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/DynamicTreeComponent.hpp>

PlayerStateTeleport::PlayerStateTeleport()
{

}

//public
void PlayerStateTeleport::processMovement(cro::Entity entity, Input, cro::Scene&)
{
    auto& player = entity.getComponent<Player>();
    std::int32_t targetLayer = static_cast<std::int32_t>((player.collisionLayer + 1) % 2);

    float targetPos = LayerDepth * Util::direction(targetLayer);

    auto& tx = entity.getComponent<cro::Transform>();
    auto position = tx.getPosition();
    float move = targetPos - position.z;
    
    if (std::abs(move) > 0.1f)
    {
        tx.move({ 0.f, 0.f, move * TeleportSpeed * ConstVal::FixedGameUpdate });
    }
    else
    {
        position.z = targetPos;
        tx.setPosition(position);

        player.collisionLayer = targetLayer;
        player.state = Player::State::Falling;

        entity.getComponent<cro::DynamicTreeComponent>().setFilterFlags(targetLayer + 1);

        //if we carried a crate over make sure it knows which layer it's on
        auto crateEnt = player.avatar.getComponent<PlayerAvatar>().crateEnt;
        if (crateEnt.isValid())
        {
            crateEnt.getComponent<Crate>().collisionLayer = targetLayer;
            crateEnt.getComponent<cro::DynamicTreeComponent>().setFilterFlags((targetLayer + 1) | CollisionID::Crate);
        }
    }
}

void PlayerStateTeleport::processCollision(cro::Entity, const std::vector<cro::Entity>&)
{

}