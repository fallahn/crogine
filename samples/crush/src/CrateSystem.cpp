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

#include "CrateSystem.hpp"
#include "Collision.hpp"
#include "GameConsts.hpp"
#include "CommonConsts.hpp"

#include <crogine/ecs/Scene.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/DynamicTreeComponent.hpp>

#include <crogine/ecs/systems/DynamicTreeSystem.hpp>

CrateSystem::CrateSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(CrateSystem))
{
    requireComponent<Crate>();
    requireComponent<cro::Transform>();
    requireComponent<cro::DynamicTreeComponent>();
}

//public
void CrateSystem::process(float)
{
    auto& entities = getEntities();
    for (auto& entity : entities)
    {
        const auto& crate = entity.getComponent<Crate>();
        switch (crate.state)
        {
        default: break;
        case Crate::Ballistic:
            processBallistic(entity);
            break;
        case Crate::Carried:
            //processCarried(entity);
            break;
        case Crate::Falling:
            processFalling(entity);
            break;
        case Crate::Idle:
            processIdle(entity);
            break;
        }
    }
}

//private
void CrateSystem::processIdle(cro::Entity)
{
    //test if foot is free and switch to falling
}
#include <iostream>
void CrateSystem::processFalling(cro::Entity entity)
{
    auto& crate = entity.getComponent<Crate>();
    
    //apply gravity
    crate.velocity.y = std::max(crate.velocity.y + (Gravity * ConstVal::FixedGameUpdate), MaxGravity);

    //move
    entity.getComponent<cro::Transform>().move(crate.velocity * ConstVal::FixedGameUpdate);



    //collision update
    auto collisions = doBroadPhase(entity);


    auto position = entity.getComponent<cro::Transform>().getPosition();
    const auto& collisionComponent = entity.getComponent<CollisionComponent>();

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

            //crate collision
            if (bodyRect.intersects(otherRect, overlap))
            {
                //set the flag to what we're touching as long as it's not a foot
                crate.collisionFlags |= ((1 << otherCollision.rects[i].material) & ~(1 << CollisionMaterial::Foot));

                auto manifold = calcManifold(bodyRect, otherRect, overlap);

                //foot collision
                if (footRect.intersects(otherRect, overlap))
                {
                    crate.collisionFlags |= (1 << CollisionMaterial::Foot);
                }


                switch (otherCollision.rects[i].material)
                {
                default: break;
                case CollisionMaterial::Solid:
                    //correct for position
                    entity.getComponent<cro::Transform>().move(manifold.normal * manifold.penetration);

                    if (crate.collisionFlags & (1 << CollisionMaterial::Foot)
                        && manifold.normal.y > 0) //landed from above
                    {
                        crate.state = Crate::State::Idle;
                        crate.velocity.y = 0.f;
                    }
                    break;
                case CollisionMaterial::Teleport:
                    //nothing?
                    break;
                case CollisionMaterial::Body:
                    //hit a player
                    break;
                }
            }
        }
    }
}

void CrateSystem::processBallistic(cro::Entity)
{
    //apply sideways movement

    //apply friction?

    //check for solid/player collision
}

std::vector<cro::Entity> CrateSystem::doBroadPhase(cro::Entity entity)
{
    entity.getComponent<Crate>().collisionFlags = 0;
    auto position = entity.getComponent<cro::Transform>().getPosition();

    auto bb = CrateBounds;
    bb += position;

    auto& collisionComponent = entity.getComponent<CollisionComponent>();
    auto bounds2D = collisionComponent.sumRect;
    bounds2D.left += position.x;
    bounds2D.bottom += position.y;

    std::vector<cro::Entity> collisions;

    //broadphase
    auto entities = getScene()->getSystem<cro::DynamicTreeSystem>().query(bb, entity.getComponent<Crate>().collisionLayer + 1);
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
                collisions.push_back(e);
            }
        }
    }

    return collisions;
}