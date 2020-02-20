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
#include <crogine/detail/glm/gtc/matrix_transform.hpp>


PlayerSystem::PlayerSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(PlayerSystem)),
    m_windowScale(1.f)
{
    requireComponent<Player>();
    requireComponent<cro::Transform>();

    auto size = cro::App::getWindow().getSize();
    updateWindowScale(size.x, size.y);
}

//public
void PlayerSystem::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_RESIZED)
        {
            updateWindowScale(data.data0, data.data1);
        }
    }
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
                //TODO we need t ohandle this more satisfactorily such as forcing a resync
                cro::Logger::log("Requested timestamp too far in the past... potential desync!", cro::Logger::Type::Warning);
                return;
            }
        }
        player.lastUpdatedInput = lastIndex;

        //apply position/rotation from server
        tx.setPosition(update.position);
        tx.setRotation(Util::decompressQuat(update.rotation));

        processInput(entity);
    }
}

//private
void PlayerSystem::updateWindowScale(std::uint32_t x, std::uint32_t y)
{
    m_windowScale = { static_cast<float>(x) / 1920.f, static_cast<float>(y) / 1080.f };
}

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
    const float moveScale = 0.01f;

    glm::quat pitch = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), static_cast<float>(-input.yMove) * m_windowScale.y * moveScale, glm::vec3(1.f, 0.f, 0.f));
    glm::quat yaw = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), static_cast<float>(-input.xMove) * m_windowScale.x * moveScale, glm::vec3(0.f, 1.f, 0.f));

    auto& tx = entity.getComponent<cro::Transform>();
    auto rotation = yaw * tx.getRotationQuat() * pitch;
    tx.setRotation(rotation);

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
}

void PlayerSystem::processCollision(cro::Entity)
{

}