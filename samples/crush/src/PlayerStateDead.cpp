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
#include "Collision.hpp"
#include "CrateSystem.hpp"

#include <crogine/detail/glm/gtx/norm.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/DynamicTreeComponent.hpp>

namespace
{

}

PlayerStateDead::PlayerStateDead()
{

}

//public
void PlayerStateDead::processMovement(cro::Entity entity, Input input, cro::Scene&)
{
    auto& player = entity.getComponent<Player>();

    //server will delete any carried crates so make sure we
    //reset these properties.
    player.avatar.getComponent<PlayerAvatar>().crateEnt = {};
    player.carrying = false;


    player.resetTime -= ConstVal::FixedGameUpdate;
    if (player.resetTime < 0)
    {
        if (input.buttonFlags & InputFlag::Jump)
        {
            player.resetTime = 1.f;
            player.lives--;
            player.state = player.lives > 0 ? Player::State::Reset : Player::State::Spectate;
        }
    }
}

void PlayerStateDead::processCollision(cro::Entity, const std::vector<cro::Entity>&)
{

}

//------------------------------------------

PlayerStateReset::PlayerStateReset()
{

}

//public
void PlayerStateReset::processMovement(cro::Entity entity, Input, cro::Scene&)
{
    auto& tx = entity.getComponent<cro::Transform>();
    auto& player = entity.getComponent<Player>();

    auto dir = player.spawnPosition - tx.getPosition();
    if (glm::length2(dir) > 0.5f)
    {
        tx.move(dir * 10.f * ConstVal::FixedGameUpdate);
    }
    else
    {
        tx.setPosition(player.spawnPosition);

        //let the server decide when player can be active again
        if (!player.local)
        {
            player.resetTime -= ConstVal::FixedGameUpdate;

            //need to keep this updated so we collide with boxes blocking spawn
            player.collisionLayer = player.spawnPosition.z > 0 ? 0 : 1;
            entity.getComponent<cro::DynamicTreeComponent>().setFilterFlags(player.collisionLayer + 1);

            if (player.resetTime < 0)
            {
                player.resetTime = 1.f;
                player.state = Player::State::Falling;
                player.direction = player.spawnPosition.x > 0 ? Player::Left : Player::Right;
            }
        }
    }
}

void PlayerStateReset::processCollision(cro::Entity entity, const std::vector<cro::Entity>& collisions)
{
    //narrow phase
    auto& player = entity.getComponent<Player>();

    if (player.local)
    {
        return;
    }

    auto position = entity.getComponent<cro::Transform>().getPosition();
    const auto& collisionComponent = entity.getComponent<CollisionComponent>();

    auto bodyRect = collisionComponent.rects[0].bounds;
    bodyRect.left += position.x;
    bodyRect.bottom += position.y;

    for (auto e : collisions)
    {
        if (!e.isValid())
        {
            continue;
        }

        auto otherPos = e.getComponent<cro::Transform>().getPosition();
        const auto& otherCollision = e.getComponent<CollisionComponent>();
        for (auto i = 0; i < otherCollision.rectCount; ++i)
        {
            auto otherRect = otherCollision.rects[i].bounds;
            otherRect.left += otherPos.x;
            otherRect.bottom += otherPos.y;

            cro::FloatRect overlap;

            //body collision
            if (bodyRect.intersects(otherRect, overlap))
            {
                //track which objects we're touching
                player.collisionFlags |= ((1 << otherCollision.rects[i].material) & ~((1 << CollisionMaterial::Foot) | (1 << CollisionMaterial::Sensor)));

                auto manifold = calcManifold(bodyRect, otherRect, overlap);
                switch (otherCollision.rects[i].material)
                {
                default: break;
                case CollisionMaterial::Crate:
                {
                    //this removes any crates colliding with the spawn position
                    e.getComponent<Crate>().health = 0;
                }
                break;
                }
            }
        }
    }
}