/*-----------------------------------------------------------------------

Matt Marchant 2020
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

#include "PlayerSystem.hpp"
#include "CommonConsts.hpp"

#include <crogine/ecs/components/Transform.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>


PlayerSystem::PlayerSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(PlayerSystem))
{
    requireComponent<Player>();
    requireComponent<cro::Transform>();
}

//public
void PlayerSystem::process(float)
{
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        processInput(entity);
    }
}

void PlayerSystem::reconcile(cro::Entity entity)
{
    //TODO apply position/rotation from
    //server
    //TODO rewind player's last input to timestamp and
    //re-process all succeeding events
    //TODO handle cases where requested timestamp has been pushed out the stack
    processInput(entity);
}

//private
void PlayerSystem::processInput(cro::Entity entity)
{
    auto& player = entity.getComponent<Player>();

    //update all the inputs until 1 before next
    //free input. Remember to take into account
    //the wrap around of the indices
    auto lastIdx = (player.nextFreeInput + (Player::HistorySize - 1)) % Player::HistorySize;
    while (player.lastUpdatedInput != lastIdx)
    {
        processMovement(entity, player.inputStack[player.lastUpdatedInput]);
        processCollision(entity);

        player.lastUpdatedInput = (player.lastUpdatedInput + 1) % Player::HistorySize;
    }
}

void PlayerSystem::processMovement(cro::Entity entity, Input input)
{
    auto& tx = entity.getComponent<cro::Transform>();
    auto rot = tx.getRotation();

    rot.y -= static_cast<float>(input.xMove) * 0.007f;
    rot.x -= static_cast<float>(input.yMove) * 0.007f;
    
    glm::quat rotation = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), rot.y, glm::vec3(0.f, 1.f, 0.f));
    rotation = glm::rotate(rotation, rot.x, glm::vec3(1.f, 0.f, 0.f));

    glm::vec3 forwardVector = rotation * glm::vec3(0.f, 0.f, -1.f);
    glm::vec3 rightVector = rotation * glm::vec3(1.f, 0.f, 0.f);

    const float moveSpeed = 30.f * ConstVal::FixedGameUpdate;
    if (input.buttonFlags & Input::Forward)
    {
        tx.move(forwardVector * moveSpeed);
    }
    if (input.buttonFlags & Input::Backward)
    {
        tx.move(-forwardVector * moveSpeed);
    }

    if (input.buttonFlags & Input::Left)
    {
        tx.move(-rightVector * moveSpeed);
    }
    if (input.buttonFlags & Input::Right)
    {
        tx.move(rightVector * moveSpeed);
    }

    tx.setRotation(rotation);
}

void PlayerSystem::processCollision(cro::Entity)
{

}