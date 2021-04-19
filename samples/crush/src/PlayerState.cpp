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
#include "Collision.hpp"
#include "CrateSystem.hpp"
#include "CommonConsts.hpp"
#include "ActorSystem.hpp"
#include "Messages.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/systems/DynamicTreeSystem.hpp>

#include <crogine/ecs/components/DynamicTreeComponent.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>

const cro::FloatRect PlayerState::PuntArea = cro::FloatRect(-0.1f, 0.f, 0.2f, 0.2f);

void PlayerState::punt(cro::Entity entity, cro::Scene& scene)
{
    //TODO check punt meter is full

    auto& player = entity.getComponent<Player>();
    auto collisions = doBroadPhase(entity, scene);

    for (auto other : collisions)
    {
        auto& crate = other.getComponent<Crate>();
        if (crate.state == Crate::State::Idle
            && crate.owner == -1) //no-one else punted it yet
        {
            crate.state = Crate::State::Ballistic;
            crate.velocity.x = PuntVelocity;
            crate.velocity.x *= player.direction;
            crate.velocity.x *= Util::direction(player.collisionLayer);
            crate.owner = player.id;

            break; //only punt one
        }
    }
}

void PlayerState::carry(cro::Entity entity, cro::Scene& scene)
{
    auto& player = entity.getComponent<Player>();
    auto collisions = doBroadPhase(entity, scene);

    for (auto other : collisions)
    {
        auto& crate = other.getComponent<Crate>();
        if (crate.state == Crate::State::Idle
            && crate.owner == -1) //no-one owns it
        {
            crate.owner = player.id;
            if (!player.local)
            {
                scene.destroyEntity(other);
                player.carrying = true;
            }
            else
            {
                //this hides the local entity so it looks like there's
                //an immediate update, then redisplays it after a short
                //time if the server hasn't remotely deleted it.
                other.getComponent<cro::Callback>().active = true;
            }
            break;
        }
    }
}

void PlayerState::drop(cro::Entity entity, cro::Scene& scene)
{
    auto& player = entity.getComponent<Player>();

    CRO_ASSERT(player.carrying, "");

    if (player.local) {}

    auto* msg = scene.postMessage<PlayerEvent>(MessageID::PlayerMessage);
    msg->type = PlayerEvent::DroppedCrate;
    msg->player = entity;

    player.carrying = false;
}

//private
std::vector<cro::Entity> PlayerState::doBroadPhase(cro::Entity entity, cro::Scene& scene)
{
    auto& player = entity.getComponent<Player>();
    auto position = entity.getComponent<cro::Transform>().getPosition();

    auto bb = PlayerBounds;
    bb += position;

    float puntDir = PuntOffset * Util::direction(player.collisionLayer);
    puntDir *= player.direction;

    auto puntArea = PuntArea;
    puntArea.left += puntDir;
    puntArea.left += position.x;
    puntArea.bottom += position.y;

    std::vector<cro::Entity> collisions;

    auto entities = scene.getSystem<cro::DynamicTreeSystem>().query(bb, CollisionID::Crate);
    for (auto other : entities)
    {
        //this assumes that we only got crates returned, and that the crate
        //position is in the centre of the body.
        auto otherPos = other.getComponent<cro::Transform>().getPosition();
        auto otherBounds = other.getComponent<CollisionComponent>().rects[0].bounds;
        CRO_ASSERT(other.getComponent<CollisionComponent>().rects[0].material == CollisionMaterial::Crate, "Wrong object returned!");

        otherBounds.left += otherPos.x;
        otherBounds.bottom += otherPos.y;

        if (puntArea.intersects(otherBounds))
        {
            collisions.push_back(other);
        }
    }

    return collisions;
}