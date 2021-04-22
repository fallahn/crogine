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

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/DynamicTreeComponent.hpp>

#include <crogine/ecs/systems/DynamicTreeSystem.hpp>

#include <crogine/detail/glm/gtx/vector_angle.hpp>
#include <crogine/util/Network.hpp>

namespace
{

}

PlayerStateFalling::PlayerStateFalling()
{

}

//public
void PlayerStateFalling::processMovement(cro::Entity entity, Input input, cro::Scene& scene)
{
    auto& player = entity.getComponent<Player>();
    auto& tx = entity.getComponent<cro::Transform>();

    const float multiplier = cro::Util::Net::decompressFloat(input.analogueMultiplier, 8);

    //do air movement if not touching a wall
    if ((player.collisionFlags & (1 << CollisionMaterial::Solid)) == 0)
    {
        if (input.buttonFlags & InputFlag::Left)
        {
            player.velocity.x = std::max(-MaxVelocity, player.velocity.x - (AirAcceleration * multiplier));
            player.direction = Player::Left;
        }
        if (input.buttonFlags & InputFlag::Right)
        {
            player.velocity.x = std::min(MaxVelocity, player.velocity.x + (AirAcceleration * multiplier));
            player.direction = Player::Right;
        }
    }

    //cut jump short if button released
    if ((input.buttonFlags & InputFlag::Jump) == 0)
    {
        player.velocity.y = std::min(player.velocity.y, MinJump);
    }

    //apply gravity
    player.velocity.y = std::max(player.velocity.y + (Gravity * ConstVal::FixedGameUpdate), MaxGravity);

    //adjust for the fact we're looking from behind on the second layer
    auto movement = player.velocity;
    movement.x *= Util::direction(player.collisionLayer);
    tx.move(movement * ConstVal::FixedGameUpdate);

    //do punt
    if ((input.buttonFlags & InputFlag::Punt)
        && (player.previousInputFlags & InputFlag::Punt) == 0)
    {
        punt(entity, scene);
    }
    //don't pick up AND punt in the same frame
    else if ((input.buttonFlags & InputFlag::CarryDrop)
        && (player.previousInputFlags & InputFlag::CarryDrop) == 0)
    {
        if (player.carrying)
        {
            drop(entity, scene);
        }
        else
        {
            carry(entity, scene);
        }
    }

    player.previousInputFlags = input.buttonFlags;
}

void PlayerStateFalling::processCollision(cro::Entity entity, const std::vector<cro::Entity>& collisions)
{
    //narrow phase

    auto& player = entity.getComponent<Player>();
    auto position = entity.getComponent<cro::Transform>().getPosition();
    const auto& collisionComponent = entity.getComponent<CollisionComponent>();

    auto crateRect = collisionComponent.rects[2].bounds;
    crateRect.left += position.x;
    crateRect.bottom += position.y;

    auto footRect = collisionComponent.rects[1].bounds;
    footRect.left += position.x;
    footRect.bottom += position.y;

    auto bodyRect = collisionComponent.rects[0].bounds;
    bodyRect.left += position.x;
    bodyRect.bottom += position.y;

    for (auto e : collisions)
    {
        auto otherPos = e.getComponent<cro::Transform>().getPosition();
        const auto& otherCollision = e.getComponent<CollisionComponent>();
        for (auto i = 0; i < otherCollision.rectCount; ++i)
        {
            auto otherRect = otherCollision.rects[i].bounds;
            otherRect.left += otherPos.x;
            otherRect.bottom += otherPos.y;

            cro::FloatRect overlap;

            //crate collision - prevents dropping inside objects
            if (crateRect.intersects(otherRect, overlap))
            {
                player.collisionFlags |= (1 << CollisionMaterial::Sensor);
            }

            //body collision
            if (bodyRect.intersects(otherRect, overlap))
            {
                //set the flag to what we're touching as long as it's not a foot or sensor
                player.collisionFlags |= ((1 << otherCollision.rects[i].material) & ~((1 << CollisionMaterial::Foot) | (1 << CollisionMaterial::Sensor)));

                auto manifold = calcManifold(bodyRect, otherRect, overlap);

                //foot collision
                if (footRect.intersects(otherRect, overlap))
                {
                    player.collisionFlags |= (1 << CollisionMaterial::Foot);
                }


                switch (otherCollision.rects[i].material)
                {
                default: break;
                case CollisionMaterial::Crate:
                    if (e.getComponent<Crate>().state != Crate::State::Idle)
                    {
                        //only break if we died from a crate
                        break;
                    }
                    //otherwise treat as solid
                    [[fallthrough]];
                case CollisionMaterial::Solid:
                    //correct for position
                    entity.getComponent<cro::Transform>().move(manifold.normal * manifold.penetration);

                    if (player.collisionFlags & (1 << CollisionMaterial::Foot)
                        && manifold.normal.y > 0
                        && player.velocity.y <= 0.f) //don't land if jumped up near edge of a platform and still moving upwards
                    {
                        player.state = Player::State::Walking;
                        player.velocity.y = 0.f;
                        player.velocity.x *= 0.4f;
                    }
                    else
                    {
                        if (manifold.normal.y < 0)
                        {
                            //bonk head
                            player.velocity = glm::reflect(player.velocity, glm::vec3(manifold.normal, 0.f));
                            player.velocity.x *= 0.5f;
                            player.velocity.y *= 0.1f;
                        }

                        else
                        {
                            player.velocity.x *= 0.1f;
                        }
                    }
                    break;
                case CollisionMaterial::Teleport:

                    break;
                }
            }
        }
    }
}