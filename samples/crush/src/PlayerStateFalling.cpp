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

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/DynamicTreeComponent.hpp>

#include <crogine/ecs/systems/DynamicTreeSystem.hpp>

#include <crogine/detail/glm/gtx/vector_angle.hpp>

PlayerStateFalling::PlayerStateFalling()
{

}

//public
void PlayerStateFalling::processMovement(cro::Entity entity, Input input)
{
    auto& player = entity.getComponent<Player>();
    auto& tx = entity.getComponent<cro::Transform>();

    //walking speed in metres per second (1 world unit == 1 metre)
    const float moveSpeed = 10.f * Util::decompressFloat(input.analogueMultiplier) * ConstVal::FixedGameUpdate;

    glm::vec3 movement = glm::vec3(0.f);

    if (input.buttonFlags & InputFlag::Up)
    {
        movement.z = -1.f;
    }
    if (input.buttonFlags & InputFlag::Down)
    {
        movement.z += 1.f;
    }

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
    tx.move(movement * moveSpeed);
}

void PlayerStateFalling::processCollision(cro::Entity entity, cro::Scene& scene)
{
    auto& player = entity.getComponent<Player>();

    auto position = entity.getComponent<cro::Transform>().getPosition();
    auto bb = PlayerBounds;
    bb[0] += position;
    bb[1] += position;

    auto entities = scene.getSystem<cro::DynamicTreeSystem>().query(bb, player.collisionLayer + 1);
    //cro::Console::printStat("Collision Ents", std::to_string(entities.size()));

    for (auto e : entities)
    {
        if (e.hasComponent<cro::Model>())
        {
            e.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", cro::Colour::Red());
        }
    }
}

void PlayerStateFalling::processAvatar(cro::Entity)
{

}