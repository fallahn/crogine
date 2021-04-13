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
    const float moveSpeed = ConstVal::MoveSpeed * Util::decompressFloat(input.analogueMultiplier, 8);

    glm::vec3 movement = glm::vec3(0.f);

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

    player.velocity.y = std::max(player.velocity.y + (Gravity * ConstVal::FixedGameUpdate), MaxGravity);
    movement += player.velocity;

    tx.move(movement * ConstVal::FixedGameUpdate);


    player.previousInputFlags = input.buttonFlags;
}

void PlayerStateFalling::processCollision(cro::Entity entity, cro::Scene& scene)
{
    auto& player = entity.getComponent<Player>();

    auto position = entity.getComponent<cro::Transform>().getPosition();

    auto bb = PlayerBounds;
    bb[0] += position;
    bb[1] += position;

    auto& collisionComponent = entity.getComponent<CollisionComponent>();
    auto bounds2D = collisionComponent.sumRect;
    bounds2D.left += position.x;
    bounds2D.bottom += position.y;

    std::vector<cro::Entity> collisions;

    //broadphase
    auto entities = scene.getSystem<cro::DynamicTreeSystem>().query(bb, player.collisionLayer + 1);
    for (auto e : entities)
    {
        //make sure we skip our own ent
        if (e != entity)
        {
            auto otherPos = e.getComponent<cro::Transform>().getPosition();
            auto otherBounds = e.getComponent<CollisionComponent>().sumRect;
            otherBounds.left += otherPos.x;
            otherBounds.bottom += otherPos.y;

            if (otherBounds.intersects(bounds2D))
            {
                collisions.push_back(entity);
            }
        }
    }

    //narrow phase
    auto footRect = collisionComponent.rects[1].bounds;
    footRect.left += position.x;
    footRect.bottom += position.y;

    auto bodyRect = collisionComponent.rects[0].bounds;
    bodyRect.left += position.x;
    bodyRect.bottom += position.y;

    player.collisionFlags = 0;

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

            //foot collision
            if (footRect.intersects(otherRect, overlap))
            {
                player.collisionFlags |= (1 << CollisionMaterial::Foot);
            }

            //body collision
            if (bodyRect.intersects(otherRect, overlap))
            {
                player.state = Player::State::Walking;
                player.velocity.y = 0.f;
            }
        }
    }
}

void PlayerStateFalling::processAvatar(cro::Entity)
{

}