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
#include "AvatarScaleSystem.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/systems/DynamicTreeSystem.hpp>

#include <crogine/ecs/components/DynamicTreeComponent.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>

const cro::FloatRect PlayerState::PuntArea = cro::FloatRect(-0.15f, 0.f, 0.3f, 0.3f);

void PlayerState::punt(cro::Entity entity, cro::Scene& scene)
{
    //TODO check punt meter is full?

    auto& player = entity.getComponent<Player>();
    auto collisions = doBroadPhase(entity, scene);

    for (auto other : collisions)
    {
        auto& crate = other.getComponent<Crate>();
        if ((crate.state == Crate::State::Idle
            || crate.state == Crate::State::Falling)
            && crate.owner == -1) //no-one else punted it yet
        {
            crate.state = Crate::State::Ballistic;
            crate.velocity.x = PuntVelocity * (player.puntLevel / Player::PuntCoolDown);
            crate.velocity.x *= player.direction;
            crate.velocity.x *= Util::direction(player.collisionLayer);
            crate.owner = player.id;

            auto* msg = scene.postMessage<CrateEvent>(MessageID::CrateMessage);
            msg->crate = other;
            msg->type = CrateEvent::StateChanged;

            player.puntLevel = 0.f;
            player.puntLevelLinear = 0.f;

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
            if (!player.local)
            {
                //we'll let this replicate over the network
                other.getComponent<cro::DynamicTreeComponent>().setFilterFlags(0); //don't collide when carrying, player box takes care of that

                player.avatar.getComponent<PlayerAvatar>().crateEnt = other;
                player.carrying = true;

                crate.state = Crate::State::Carried;
                crate.owner = player.id;

                auto* msg = scene.postMessage<CrateEvent>(MessageID::CrateMessage);
                msg->crate = other;
                msg->type = CrateEvent::StateChanged;
            }
            else
            {
                //trigger the local hologram animation - syncing state should correct this if necessary
                //just makes it look more responsive
                player.avatar.getComponent<PlayerAvatar>().holoEnt.getComponent<AvatarScale>().target = 1.f;
            }
            break;
        }
    }
}

void PlayerState::drop(cro::Entity entity, cro::Scene& scene)
{
    auto& player = entity.getComponent<Player>();
    if ((player.collisionFlags & (1 << CollisionMaterial::Sensor)) == 0)
    {
        auto crateEnt = player.avatar.getComponent<PlayerAvatar>().crateEnt;
        if (crateEnt.isValid())
        {
            if (!player.local)
            {
                auto offset = CrateCarryOffset;
                offset.x *= player.direction;
                offset.x *= Util::direction(player.collisionLayer);
                crateEnt.getComponent<cro::Transform>().setPosition(entity.getComponent<cro::Transform>().getPosition() + offset);
                crateEnt.getComponent<Crate>().collisionLayer = player.collisionLayer;
                crateEnt.getComponent<cro::DynamicTreeComponent>().setFilterFlags((player.collisionLayer + 1) | CollisionID::Crate);

                auto& crate = crateEnt.getComponent<Crate>();
                crate.state = Crate::State::Falling;

                crate.velocity.x = player.velocity.x * 2.f;

                auto* msg = scene.postMessage<CrateEvent>(MessageID::CrateMessage);
                msg->crate = crateEnt;
                msg->type = CrateEvent::StateChanged;

                player.avatar.getComponent<PlayerAvatar>().crateEnt = {};
                player.carrying = false;
            }
            else
            {
                //trigger the local hologram animation - syncing state should correct this if necessary
                //just makes it look more responsive
                player.avatar.getComponent<PlayerAvatar>().holoEnt.getComponent<AvatarScale>().target = 0.f;
            }
        }
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