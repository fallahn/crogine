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
#include "InterpolationSystem.hpp"

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

            auto* msg = scene.postMessage<CrateEvent>(MessageID::CrateMessage);
            msg->crate = other;
            msg->type = CrateEvent::StateChanged;

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
            other.getComponent<cro::DynamicTreeComponent>().setFilterFlags(0); //don't collide when carrying, player box takes care of that
            other.getComponent<cro::Transform>().setPosition(CrateCarryOffset);

            player.avatar.getComponent<cro::Transform>().addChild(other.getComponent<cro::Transform>());
            player.avatar.getComponent<PlayerAvatar>().crateEnt = other;
            player.carrying = true;

            if (!player.local)
            {
                crate.state = Crate::State::Carried;
                crate.owner = player.id;

                auto* msg = scene.postMessage<CrateEvent>(MessageID::CrateMessage);
                msg->crate = other;
                msg->type = CrateEvent::StateChanged;
            }
            else
            {
                //pick up the local crate and attach it to the player
                //activating the callback begins a timeout which drops
                //the crate if the server doesn't confirm the crate was
                //picked up
                other.getComponent<cro::Transform>().move(player.avatar.getComponent<cro::Transform>().getOrigin());
                other.getComponent<InterpolationComponent>().setEnabled(false);
            }
            break;
        }
    }
}

void PlayerState::drop(cro::Entity entity, cro::Scene& scene)
{
    auto& player = entity.getComponent<Player>();

    auto crateEnt = player.avatar.getComponent<PlayerAvatar>().crateEnt;
    if (crateEnt.isValid())
    {
        crateEnt.getComponent<Crate>().collisionLayer = player.collisionLayer;
        crateEnt.getComponent<cro::DynamicTreeComponent>().setFilterFlags((player.collisionLayer + 1) | CollisionID::Crate);
        crateEnt.getComponent<cro::Transform>().setPosition(crateEnt.getComponent<cro::Transform>().getWorldPosition());
        player.avatar.getComponent<cro::Transform>().removeChild(crateEnt.getComponent<cro::Transform>());

        if (!player.local)
        {
            auto& crate = crateEnt.getComponent<Crate>();
            crate.state = Crate::State::Falling;

            auto* msg = scene.postMessage<CrateEvent>(MessageID::CrateMessage);
            msg->crate = crateEnt;
            msg->type = CrateEvent::StateChanged;
        }
        else
        {
            crateEnt.getComponent<InterpolationComponent>().setEnabled(true);
        }

        player.avatar.getComponent<PlayerAvatar>().crateEnt = {};
        player.carrying = false;
    }
}

//private
std::vector<cro::Entity> PlayerState::doBroadPhase(cro::Entity entity, cro::Scene& scene)
{
    auto& player = entity.getComponent<Player>();
    auto position = entity.getComponent<cro::Transform>().getPosition();

    auto bb = PlayerBounds;
    bb += position;

    float searchDir = PuntOffset * Util::direction(player.collisionLayer);
    searchDir *= player.direction;

    auto searchArea = PuntArea;
    searchArea.left += searchDir;
    searchArea.left += position.x;
    searchArea.bottom += position.y;

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

        if (searchArea.intersects(otherBounds))
        {
            collisions.push_back(other);
        }
    }

    return collisions;
}