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

#include "PlayerSystem.hpp"
#include "CommonConsts.hpp"
#include "ServerPacketData.hpp"
#include "Messages.hpp"
#include "GameConsts.hpp"
#include "Collision.hpp"
#include "DebugDraw.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/DynamicTreeComponent.hpp>

#include <crogine/ecs/systems/DynamicTreeSystem.hpp>

#include <crogine/util/Network.hpp>
#include <crogine/util/Maths.hpp>

PlayerSystem::PlayerSystem(cro::MessageBus& mb)
    : cro::System       (mb, typeid(PlayerSystem))
{
    requireComponent<Player>();
    requireComponent<cro::Transform>();

    m_playerStates[Player::State::Falling] = std::make_unique<PlayerStateFalling>();
    m_playerStates[Player::State::Walking] = std::make_unique<PlayerStateWalking>();
    m_playerStates[Player::State::Teleport] = std::make_unique<PlayerStateTeleport>();
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
        tx.setPosition(cro::Util::Net::decompressVec3(update.position));
        tx.setRotation(cro::Util::Net::decompressQuat(update.rotation));
        update.unpack(player);

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
    //auto lastIdx = (player.nextFreeInput + (Player::HistorySize - 2)) % Player::HistorySize;
    //player.previousInputFlags = player.inputStack[lastIdx].buttonFlags; //adjust for correct value

    auto lastIdx = (player.nextFreeInput + (Player::HistorySize - 1)) % Player::HistorySize;
    while (player.lastUpdatedInput != lastIdx)
    {
        auto lastState = player.state;

        m_playerStates[player.state]->processMovement(entity, player.inputStack[player.lastUpdatedInput], *getScene());
        processCollision(entity, player.state);

        if (lastState != player.state)
        {
            //raise a message to say something happened
            auto* msg = postMessage<PlayerEvent>(MessageID::PlayerMessage);
            msg->player = entity;

            switch (player.state)
            {
            default: break;
            case Player::State::Teleport:
                msg->type = PlayerEvent::Teleported;
                break;
            case Player::State::Walking:
                msg->type = PlayerEvent::Landed;
                break;
            case Player::State::Falling:
                if (player.velocity.y > 0)
                {
                    msg->type = PlayerEvent::Jumped;
                }
                break;
            }
        }

        player.lastUpdatedInput = (player.lastUpdatedInput + 1) % Player::HistorySize;
    }


    //update the avatar (which in turn is sent to client actors)
    auto& avatar = player.avatar.getComponent<PlayerAvatar>();

    auto targetRotation = cro::Util::Const::PI;
    targetRotation *= 1 - ((player.direction + 1) / 2);
    targetRotation += cro::Util::Const::PI * player.collisionLayer;

    avatar.rotation += cro::Util::Maths::shortestRotation(avatar.rotation, targetRotation) * 20.f * ConstVal::FixedGameUpdate;
    player.avatar.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, avatar.rotation);
}

void PlayerSystem::processCollision(cro::Entity entity, std::uint32_t playerState)
{
    //do broadphase pass then send results to specific state for processing
    auto& player = entity.getComponent<Player>();
    player.collisionFlags = 0;

    auto position = entity.getComponent<cro::Transform>().getPosition();

    auto bb = PlayerBounds;
    bb += position;

    auto& collisionComponent = entity.getComponent<CollisionComponent>();
    auto bounds2D = collisionComponent.sumRect;
    bounds2D.left += position.x;
    bounds2D.bottom += position.y;

    std::vector<cro::Entity> collisions;

    //broadphase
    auto entities = getScene()->getSystem<cro::DynamicTreeSystem>().query(bb, player.collisionLayer + 1);
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

#ifdef CRO_DEBUG_
    if (entity.hasComponent<DebugInfo>())
    {
        auto& db = entity.getComponent<DebugInfo>();
        db.nearbyEnts = static_cast<std::int32_t>(entities.size());
        db.collidingEnts = static_cast<std::int32_t>(collisions.size());
        db.bounds = bb;
    }
#endif

    m_playerStates[playerState]->processCollision(entity, collisions);

    //if the collision changed the player state, update the collision
    //again using the new result
    if (player.state != playerState)
    {
        player.collisionFlags = 0;
        m_playerStates[player.state]->processCollision(entity, collisions);
    }
}

//packs/unpacks player data in the the update struct
void PlayerUpdate::pack(const Player& player) 
{
    velocity = cro::Util::Net::compressVec3(player.velocity);
    timestamp = player.inputStack[player.lastUpdatedInput].timeStamp;
    playerID = player.id;
    state = player.state;
    collisionFlags = player.collisionFlags;
    collisionLayer = player.collisionLayer;
    prevInputFlags = player.previousInputFlags;
    direction = player.direction;
    carrying = player.carrying;
}

void PlayerUpdate::unpack(Player& player) const
{
    player.velocity = cro::Util::Net::decompressVec3(velocity);

    //set the state
    player.state = state;
    player.collisionLayer = collisionLayer;
    player.collisionFlags = collisionFlags;
    player.previousInputFlags = prevInputFlags;
    player.carrying = carrying;
    player.direction = direction == -1 ? Player::Left : Player::Right;
}