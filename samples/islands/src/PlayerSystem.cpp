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
#include "ServerPacketData.hpp"
#include "Messages.hpp"
#include "GameConsts.hpp"

#include <crogine/core/App.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/util/Network.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>


PlayerSystem::PlayerSystem(cro::MessageBus& mb)
    : cro::System   (mb, typeid(PlayerSystem)),
    m_heightmap     (IslandTileCount * IslandTileCount, 1.f)
{
    requireComponent<Player>();
    requireComponent<cro::Transform>();
}

//public
void PlayerSystem::handleMessage(const cro::Message& msg)
{

}

void PlayerSystem::process(float)
{
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        processInput(entity);
    }
}

void PlayerSystem::reconcile(cro::Entity entity, const PlayerUpdate& update)
{
    if (entity.isValid())
    {
        auto& tx = entity.getComponent<cro::Transform>();
        auto& player = entity.getComponent<Player>();

        //apply position/rotation from server
        tx.setPosition(update.position);
        tx.setRotation(cro::Util::Net::decompressQuat(update.rotation));

        //rewind player's last input to timestamp and
        //re-process all succeeding events
        auto lastIndex = player.lastUpdatedInput;
        while (player.inputStack[lastIndex].timeStamp > update.timestamp)
        {
            lastIndex = (lastIndex + (Player::HistorySize - 1)) % Player::HistorySize;

            if (player.inputStack[lastIndex].timeStamp == player.inputStack[player.lastUpdatedInput].timeStamp)
            {
                //we've looped all the way around so the requested timestamp must
                //be too far in the past... have to skip this update
                
                //setting the resync flag temporarily ignores input
                //so that the client remains in place until the next update comes
                //in to guarantee input/position etc match
                player.waitResync = true;
                cro::Logger::log("Requested timestamp too far in the past... potential desync!", cro::Logger::Type::Warning);
                return;
            }
        }
        player.lastUpdatedInput = lastIndex;

        const auto& lastInput = player.inputStack[player.lastUpdatedInput];
        if (lastInput.buttonFlags == 0)
        {
            player.waitResync = false;
        }

        processInput(entity);
    }
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
        processAvatar(player.avatar);

        player.lastUpdatedInput = (player.lastUpdatedInput + 1) % Player::HistorySize;
    }
}

void PlayerSystem::processMovement(cro::Entity entity, Input input)
{ 
    auto& player = entity.getComponent<Player>();
    auto& tx = entity.getComponent<cro::Transform>();

    //walking speed in metres per second (1 world unit == 1 metre)
    const float moveSpeed = 10.f * cro::Util::Net::decompressFloat(input.analogueMultiplier, 8) * ConstVal::FixedGameUpdate;

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

void PlayerSystem::processCollision(cro::Entity)
{

}

void PlayerSystem::processAvatar(cro::Entity entity)
{
    auto position = entity.getComponent<cro::Transform>().getWorldPosition();
    position.x += (IslandSize / 2.f); //puts the position relative to the grid - this should be the origin coords
    position.z += (IslandSize / 2.f);

    auto height = readHeightmap(position, m_heightmap);
    entity.getComponent<cro::Transform>().setPosition({ 0.f, height + IslandWorldHeight, 0.f });
}