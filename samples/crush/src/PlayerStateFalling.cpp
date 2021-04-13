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

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/DynamicTreeComponent.hpp>

#include <crogine/ecs/systems/DynamicTreeSystem.hpp>

#include <crogine/detail/glm/gtx/vector_angle.hpp>

namespace
{
    constexpr float Gravity = -96.f;
    constexpr float MaxGravity = -30.f;
}

PlayerStateFalling::PlayerStateFalling()
{

}

//public
void PlayerStateFalling::processMovement(cro::Entity entity, Input input)
{
    auto& player = entity.getComponent<Player>();
    auto& tx = entity.getComponent<cro::Transform>();

    //walking speed in metres per second (1 world unit == 1 metre)
    const float moveSpeed = ConstVal::MoveSpeed * Util::decompressFloat(input.analogueMultiplier, 8) * 0.5f;

    glm::vec3 movement = glm::vec3(0.f);

    //do air movement if not touching a wall - TODO fix player getting stuck on the edge
    if ((player.collisionFlags & (1 << CollisionMaterial::Solid)) == 0)
    {
        if (input.buttonFlags & InputFlag::Left)
        {
            movement.x = -1.f;
        }
        if (input.buttonFlags & InputFlag::Right)
        {
            movement.x += 1.f;
        }

        if (glm::length2(movement) > 1)
        {
            movement = glm::normalize(movement);
        }
        movement *= moveSpeed;
    }

    //apply gravity
    player.velocity.y = std::max(player.velocity.y + (Gravity * ConstVal::FixedGameUpdate), MaxGravity);
    movement += player.velocity;

    tx.move(movement * ConstVal::FixedGameUpdate);


    player.previousInputFlags = input.buttonFlags;
}

void PlayerStateFalling::processCollision(cro::Entity entity, const std::vector<cro::Entity>& collisions)
{
    //narrow phase

    auto& player = entity.getComponent<Player>();
    auto position = entity.getComponent<cro::Transform>().getPosition();
    const auto& collisionComponent = entity.getComponent<CollisionComponent>();

    auto footRect = collisionComponent.rects[1].bounds;
    footRect.left += position.x;
    footRect.bottom += position.y;

    auto bodyRect = collisionComponent.rects[0].bounds;
    bodyRect.left += position.x;
    bodyRect.bottom += position.y;

    glm::vec2 centre = { bodyRect.left + (bodyRect.width / 2.f), bodyRect.bottom + (bodyRect.height / 2.f) };

    for (auto e : collisions)
    {
        auto otherPos = e.getComponent<cro::Transform>().getPosition();
        const auto& otherCollision = e.getComponent<CollisionComponent>();
        for (auto i = 0; i < otherCollision.rectCount; ++i)
        {
            auto otherRect = otherCollision.rects[i].bounds;
            otherRect.left += otherPos.x;
            otherRect.bottom += otherPos.y;

            glm::vec2 otherCentre = { otherRect.left + (otherRect.width / 2.f), otherRect.bottom + (otherRect.height / 2.f) };

            glm::vec2 direction = otherCentre - centre;
            cro::FloatRect overlap;

            //foot collision
            if (footRect.intersects(otherRect, overlap))
            {
                player.collisionFlags |= (1 << CollisionMaterial::Foot);
            }

            //body collision
            if (bodyRect.intersects(otherRect, overlap))
            {
                //set the flag to what we're touching as long as it's not a foor
                player.collisionFlags |= ((1 << otherCollision.rects[i].material) | ~(1 << CollisionMaterial::Foot));

                auto manifold = calcManifold(direction, overlap);
                entity.getComponent<cro::Transform>().move(manifold.normal * manifold.penetration);

                if (player.collisionFlags & (1 << CollisionMaterial::Foot))
                {
                    player.state = Player::State::Walking;
                    player.velocity.y = 0.f;
                }
                else
                {
                    //reflect velocity
                    player.velocity = glm::reflect(player.velocity, glm::vec3(manifold.normal, 0.f));

                    if (manifold.normal.y != 0)
                    {
                        //bonk head
                        player.velocity *= 0.1f;
                    }                    
                }
            }
        }
    }
}

void PlayerStateFalling::processAvatar(cro::Entity)
{

}