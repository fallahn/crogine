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

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/systems/DynamicTreeSystem.hpp>

#include <crogine/ecs/components/DynamicTreeComponent.hpp>
#include <crogine/ecs/components/Transform.hpp>

const cro::FloatRect PlayerState::PuntArea = cro::FloatRect(-0.1f, 0.f, 0.2f, 0.2f);

void PlayerState::punt(cro::Entity entity, cro::Scene& scene)
{
    //TODO check punt meter is full
    auto& player = entity.getComponent<Player>();
    auto position = entity.getComponent<cro::Transform>().getPosition();

    auto bb = PlayerBounds;
    bb += position;

    float puntDir = PuntOffset * Util::direction(player.collisionLayer);
    if (player.direction == Player::Left)
    {
        puntDir *= -1.f;
    }

    auto puntArea = PuntArea;
    puntArea.left += puntDir;
    puntArea.left += position.x;
    puntArea.bottom += position.y;

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
            auto& crate = other.getComponent<Crate>();
            if (crate.state == Crate::State::Idle)
            {
                crate.state = Crate::State::Ballistic;
                crate.velocity.x = PuntVelocity * (puntDir / PuntOffset);
            }
        }
    }
}