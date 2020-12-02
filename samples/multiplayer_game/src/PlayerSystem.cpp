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

#include <crogine/core/App.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>


PlayerSystem::PlayerSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(PlayerSystem))
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

        player.cameraPitch = Util::decompressFloat(update.pitch);
        player.cameraYaw = Util::decompressFloat(update.yaw);

        //apply position/rotation from server
        tx.setPosition(update.position);
        tx.setRotation(Util::decompressQuat(update.rotation));

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

        auto lastInput = player.inputStack[player.lastUpdatedInput];
        if (lastInput.buttonFlags == 0
            && lastInput.xMove == 0 && lastInput.yMove == 0)
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

        player.lastUpdatedInput = (player.lastUpdatedInput + 1) % Player::HistorySize;
    }
}

void PlayerSystem::processMovement(cro::Entity entity, Input input)
{
    const float moveScale = 0.004f;
    float pitchMove = static_cast<float>(-input.yMove)* moveScale;
    float yawMove = static_cast<float>(-input.xMove)* moveScale;

    //clamp pitch
    auto& player = entity.getComponent<Player>();
    float newPitch = player.cameraPitch + pitchMove;
    const float clamp = 1.4f;
    if (newPitch > clamp)
    {
        pitchMove -= (newPitch - clamp);
        player.cameraPitch = clamp;
    }
    else if (newPitch < -clamp)
    {
        pitchMove -= (newPitch + clamp);
        player.cameraPitch = -clamp;
    }
    else
    {
        player.cameraPitch = newPitch;
    }

    //we need to clamp this to TAU (or ideally +- PI) else more than one rotation
    //introduces very visible jitter
    //player.cameraYaw = std::fmod(player.cameraYaw + yawMove, cro::Util::Const::TAU);
    player.cameraYaw += yawMove;
    if (player.cameraYaw < -cro::Util::Const::PI)
    {
        player.cameraYaw += cro::Util::Const::TAU;
    }
    else if (player.cameraYaw > cro::Util::Const::PI)
    {
        player.cameraYaw -= cro::Util::Const::TAU;
    }

    glm::quat pitch = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), pitchMove, glm::vec3(1.f, 0.f, 0.f));
    glm::quat yaw = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), yawMove, glm::vec3(0.f, 1.f, 0.f));
    
    auto& tx = entity.getComponent<cro::Transform>();
    auto rotation = yaw * tx.getRotation() * pitch;
    tx.setRotation(rotation);

    
    //we only want to rotate around the yaw when walking
    rotation = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), player.cameraYaw, glm::vec3(0.f, 1.f, 0.f));
    glm::vec3 forwardVector = rotation * glm::vec3(0.f, 0.f, -1.f);
    glm::vec3 rightVector = rotation* glm::vec3(1.f, 0.f, 0.f);
    

    //walking speed in metres per second (1 world unit == 1 metre)
    float moveSpeed = 1.6f * ConstVal::FixedGameUpdate;

    if (player.flyMode)
    {
        moveSpeed *= 5.f;
    }

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

    if (player.flyMode)
    {
        if (input.buttonFlags & Input::Jump)
        {
            tx.move(glm::vec3(0.f, 1.f, 0.f) * moveSpeed);
        }
        if (input.buttonFlags & Input::Crouch)
        {
            tx.move(glm::vec3(0.f, -1.f, 0.f) * moveSpeed);
        }
    }
}

void PlayerSystem::processCollision(cro::Entity)
{

}