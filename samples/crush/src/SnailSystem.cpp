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

#include "SnailSystem.hpp"
#include "Collision.hpp"
#include "CommonConsts.hpp"
#include "GameConsts.hpp"
#include "ActorIDs.hpp"
#include "ActorSystem.hpp"
#include "PlayerSystem.hpp"
#include "Messages.hpp"
#include "CrateSystem.hpp"
#include "InterpolationSystem.hpp"

#include <crogine/ecs/Scene.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/DynamicTreeComponent.hpp>

#include <crogine/ecs/systems/DynamicTreeSystem.hpp>

#include <crogine/util/Random.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

namespace
{
    constexpr float RotationSpeed = 20.f;

    struct CrateCollision final
    {
        std::int8_t state = -1;
        std::int8_t owner = -1;
    };
}

SnailSystem::SnailSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(SnailSystem))
{
    requireComponent<Snail>();
    requireComponent<Actor>();
    requireComponent<cro::Transform>();
    requireComponent<cro::DynamicTreeComponent>();
}

//public
void SnailSystem::handleMessage(const cro::Message& msg)
{
    if (msg.id == MessageID::GameMessage)
    {
        const auto& data = msg.getData<GameEvent>();
        if (data.type == GameEvent::GameEnd)
        {
            auto& entities = getEntities();
            for (auto entity : entities)
            {
                entity.getComponent<Snail>().state = Snail::Digging;
            }
        }
    }
}

void SnailSystem::process(float)
{
    m_deadSnails.clear();

    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& snail = entity.getComponent<Snail>();
        if (snail.local)
        {
            processLocal(entity);
        }
        else
        {
            entity.getComponent<Actor>().sleeping = false;

            auto oldState = snail.state;

            switch (snail.state)
            {
            default: break;
            case Snail::Falling:
                processFalling(entity);
                break;
            case Snail::Idle:
                processIdle(entity);
                break;
            case Snail::Walking:
                processWalking(entity);
                break;
            case Snail::Digging:
                processDigging(entity);
                break;
            }

            if (snail.state != oldState)
            {
                //TODO this could share the equivalent crate message
                auto* msg = postMessage<SnailEvent>(MessageID::SnailMessage);
                msg->snail = entity;
                msg->type = SnailEvent::StateChanged;
            }

            if (snail.state == Snail::Dead)
            {
                m_deadSnails.push_back(entity);
            }
        }
    }
}

//private
void SnailSystem::processLocal(cro::Entity entity)
{
    //attempt to correct for collision due to extrapolation
    auto& snail = entity.getComponent<Snail>();

    auto collisions = doBroadPhase(entity);

    auto position = entity.getComponent<cro::Transform>().getPosition();
    const auto& collisionComponent = entity.getComponent<CollisionComponent>();

    auto bodyRect = collisionComponent.rects[0].bounds;
    bodyRect.left += position.x;
    bodyRect.bottom += position.y;

    for (auto e : collisions)
    {
        auto otherPos = e.getComponent<cro::Transform>().getPosition();
        const auto& otherCollision = e.getComponent<CollisionComponent>();
        for (auto i = 0; i < otherCollision.rectCount; ++i)
        {
            auto otherRect = otherCollision.rects[i].bounds;
            otherRect.left += otherPos.x;
            otherRect.bottom += otherPos.y;

            cro::FloatRect overlap;

            if (bodyRect.intersects(otherRect, overlap))
            {
                auto manifold = calcManifold(bodyRect, otherRect, overlap);

                switch (otherCollision.rects[i].material)
                {
                default: break;
                case CollisionMaterial::Solid:
                    //correct for position
                    entity.getComponent<cro::Transform>().move(manifold.normal * manifold.penetration);

                    break;
                }
            }
        }
    }
}

void SnailSystem::processFalling(cro::Entity entity)
{
    auto& snail = entity.getComponent<Snail>();
    snail.sleepTimer = 0.f;

    //apply gravity
    snail.velocity.y = std::max(snail.velocity.y + ((Gravity / 2.f) * ConstVal::FixedGameUpdate), MaxGravity);

    //move
    entity.getComponent<cro::Transform>().move(snail.velocity * ConstVal::FixedGameUpdate);

    //collision update
    auto collisions = doBroadPhase(entity);
    cro::Entity playerCollision;

    auto position = entity.getComponent<cro::Transform>().getPosition();
    const auto& collisionComponent = entity.getComponent<CollisionComponent>();

    auto footRect = collisionComponent.rects[1].bounds;
    footRect.left += position.x;
    footRect.bottom += position.y;

    auto bodyRect = collisionComponent.rects[0].bounds;
    bodyRect.left += position.x;
    bodyRect.bottom += position.y;

    for (auto e : collisions)
    {
        auto otherPos = e.getComponent<cro::Transform>().getPosition();
        const auto& otherCollision = e.getComponent<CollisionComponent>();
        for (auto i = 0; i < otherCollision.rectCount; ++i)
        {
            auto otherRect = otherCollision.rects[i].bounds;
            otherRect.left += otherPos.x;
            otherRect.bottom += otherPos.y;

            cro::FloatRect overlap;

            //snail collision
            if (bodyRect.intersects(otherRect, overlap))
            {
                //set the flag to what we're touching as long as it's not a foot
                snail.collisionFlags |= ((1 << otherCollision.rects[i].material) & ~(1 << CollisionMaterial::Foot));

                auto manifold = calcManifold(bodyRect, otherRect, overlap);

                //foot collision
                if (footRect.intersects(otherRect, overlap))
                {
                    snail.collisionFlags |= (1 << CollisionMaterial::Foot);
                }

                switch (otherCollision.rects[i].material)
                {
                default: break;
                case CollisionMaterial::Crate:
                    //otherwise treat as solid
                    [[fallthrough]];
                case CollisionMaterial::Solid:
                    //correct for position
                    entity.getComponent<cro::Transform>().move(manifold.normal * manifold.penetration);

                    if (snail.collisionFlags & (1 << CollisionMaterial::Foot)
                        && manifold.normal.y > 0)
                    {
                        if (snail.velocity.y <= 0) //landed from above
                        {
                            //slow moving
                            //if (std::abs(snail.velocity.x) < std::abs(snail.velocity.y))
                            {
                                snail.state = Snail::Idle;
                                snail.velocity.y = 0.f;
                            }
                        }
                    }
                    else
                    {
                        snail.velocity = glm::reflect(snail.velocity, manifold.normal);
                        snail.velocity.x *= 0.3f; //0.1 ? ought to make these magic numbers const, compare with player collision for example
                    }
                    break;
                case CollisionMaterial::Body:
                    if (e.getComponent<Player>().state == Player::State::Dead)
                    {
                        //ignore dead peeopelies.
                        snail.collisionFlags &= ~(1 << CollisionMaterial::Body);
                        break;
                    }

                    //TODO this assumes it'll only be touching one player at a time...

                    //hit a player - we'll check the flags in the result at the end
                    //else this may be raised multiple times due to multiple iterations
                    //so we'll just track which player we hit then deal with it in the flag check below
                    if (!playerCollision.isValid()) //don't overwrite data from previous iteration
                    {
                        playerCollision = e;
                    }
                    break;
                }
            }
        }
    }

    if (snail.collisionFlags & (1 << CollisionMaterial::Body))
    {
        //hitting a player - this assumes snails are only simulated
        //server side so the clients won't know they're dead until
        //the server says so.
        killPlayer(playerCollision, snail);
    }

    //switch state if we stopped moving
    if (glm::length2(snail.velocity) < 0.1f)
    {
        snail.velocity = glm::vec3(0.f);
        snail.state = Snail::Idle;
    }
}

void SnailSystem::processIdle(cro::Entity entity)
{
    //sleep/wake the actor
    auto& snail = entity.getComponent<Snail>();

    float targetRotation = snail.velocity.x > 0 ? cro::Util::Const::PI : 0.f;
    snail.currentRotation += cro::Util::Maths::shortestRotation(snail.currentRotation, targetRotation) * RotationSpeed * ConstVal::FixedGameUpdate;
    entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, snail.currentRotation);

    snail.sleepTimer += ConstVal::FixedGameUpdate;
    entity.getComponent<Actor>().sleeping = snail.sleepTimer > 0.5f;

    if (snail.sleepTimer > cro::Util::Random::value(2, 3))
    {
        snail.sleepTimer = 0.f;
        if (snail.idleCount < 4)
        {
            snail.idleCount++;
            snail.state = Snail::Walking;
            snail.velocity.x = 2.f;
            if ((cro::Util::Random::value(0, 1) % 2) == 0)
            {
                snail.velocity *= -1.f;
            }
        }
        else
        {
            snail.state = Snail::Digging;
            return; //don't do anything else now
        }
    }

    auto collisions = doBroadPhase(entity);
    cro::Entity playerCollision;

    auto position = entity.getComponent<cro::Transform>().getPosition();
    const auto& collisionComponent = entity.getComponent<CollisionComponent>();

    auto bodyRect = collisionComponent.rects[0].bounds;
    bodyRect.left += position.x;
    bodyRect.bottom += position.y;

    for (auto e : collisions)
    {
        auto otherPos = e.getComponent<cro::Transform>().getPosition();
        const auto& otherCollision = e.getComponent<CollisionComponent>();
        for (auto i = 0; i < otherCollision.rectCount; ++i)
        {
            auto otherRect = otherCollision.rects[i].bounds;
            otherRect.left += otherPos.x;
            otherRect.bottom += otherPos.y;

            cro::FloatRect overlap;
            if (bodyRect.intersects(otherRect, overlap))
            {
                //set the flag to what we're touching as long as it's not a foot
                snail.collisionFlags |= ((1 << otherCollision.rects[i].material) & ~(1 << CollisionMaterial::Foot));
                switch (otherCollision.rects[i].material)
                {
                default: break;
                case CollisionMaterial::Crate:
                    if (e.getComponent<Crate>().state != Crate::State::Ballistic
                        && e.getComponent<Crate>().state != Crate::State::Falling)
                    {
                        //ignore idle crates
                        snail.collisionFlags &= ~(1 << CollisionMaterial::Crate);
                    }
                    break;
                case CollisionMaterial::Body:
                    if (e.getComponent<Player>().state == Player::State::Dead)
                    {
                        //ignore dead peeopelies.
                        snail.collisionFlags &= ~(1 << CollisionMaterial::Body);
                        break;
                    }
                    if (!playerCollision.isValid()) //don't overwrite data from previous iteration
                    {
                        playerCollision = e;
                    }
                    break;
                }
            }
        }
    }

    if (snail.collisionFlags & (1 << CollisionMaterial::Body))
    {
        killPlayer(playerCollision, snail);
    }

    if (snail.collisionFlags & (1 << CollisionMaterial::Crate))
    {
        snail.state = Snail::Dead;
    }
}

void SnailSystem::processWalking(cro::Entity entity)
{
    auto& snail = entity.getComponent<Snail>();
    snail.sleepTimer += ConstVal::FixedGameUpdate;
    entity.getComponent<cro::Transform>().move(snail.velocity * ConstVal::FixedGameUpdate);

    float targetRotation = snail.velocity.x > 0 ? cro::Util::Const::PI : 0.f;
    snail.currentRotation += cro::Util::Maths::shortestRotation(snail.currentRotation, targetRotation) * RotationSpeed * ConstVal::FixedGameUpdate;
    entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, snail.currentRotation);

    cro::Entity playerCollision;
    CrateCollision crateCollision;

    //collision update
    auto collisions = doBroadPhase(entity);

    auto position = entity.getComponent<cro::Transform>().getPosition();
    const auto& collisionComponent = entity.getComponent<CollisionComponent>();

    auto footRect = collisionComponent.rects[1].bounds;
    footRect.left += position.x;
    footRect.bottom += position.y;

    auto bodyRect = collisionComponent.rects[0].bounds;
    bodyRect.left += position.x;
    bodyRect.bottom += position.y;

    for (auto e : collisions)
    {
        auto otherPos = e.getComponent<cro::Transform>().getPosition();
        const auto& otherCollision = e.getComponent<CollisionComponent>();
        for (auto i = 0; i < otherCollision.rectCount; ++i)
        {
            auto otherRect = otherCollision.rects[i].bounds;
            otherRect.left += otherPos.x;
            otherRect.bottom += otherPos.y;

            cro::FloatRect overlap;

            //foot collision
            if (footRect.intersects(otherRect, overlap))
            {
                snail.collisionFlags |= (1 << CollisionMaterial::Foot);
            }

            //snail collision
            if (bodyRect.intersects(otherRect, overlap))
            {
                //set the flag to what we're touching as long as it's not a foot
                snail.collisionFlags |= ((1 << otherCollision.rects[i].material) & ~(1 << CollisionMaterial::Foot));

                auto manifold = calcManifold(bodyRect, otherRect, overlap);

                switch (otherCollision.rects[i].material)
                {
                default: break;
                case CollisionMaterial::Crate:
                    crateCollision.state = e.getComponent<Crate>().state;
                    crateCollision.owner = e.getComponent<Crate>().owner;
                    [[fallthrough]];
                case CollisionMaterial::Spawner:
                    //otherwise treat as solid
                    [[fallthrough]];
                case CollisionMaterial::Solid:
                    //correct for position
                    entity.getComponent<cro::Transform>().move(manifold.normal * manifold.penetration);
                    break;
                case CollisionMaterial::Body:
                    if (e.getComponent<Player>().state == Player::State::Dead)
                    {
                        //ignore dead peeopelies.
                        snail.collisionFlags &= ~(1 << CollisionMaterial::Body);
                        break;
                    }
                    if (!playerCollision.isValid()) //don't overwrite data from previous iteration
                    {
                        playerCollision = e;
                    }
                    break;
                }
            }
        }
    }


    //check the result
    if ((snail.collisionFlags & (1 << CollisionMaterial::Foot)) == 0)
    {
        snail.state = Snail::Falling;
    }
    else
    {
        if (snail.collisionFlags & (1 << CollisionMaterial::Solid))
        {
            snail.velocity.x *= -1.f;
        }

        if (snail.collisionFlags & (1 << CollisionMaterial::Body))
        {
            killPlayer(playerCollision, snail);
        }

        if (snail.collisionFlags & (1 << CollisionMaterial::Crate))
        {
            if (crateCollision.state == Crate::Ballistic
                || crateCollision.state == Crate::Falling)
            {
                snail.state = Snail::Dead;
                snail.playerID = crateCollision.owner;
            }
            else
            {
                snail.velocity.x *= -1.f;
            }
        }

        if (snail.collisionFlags == (1 << CollisionMaterial::Foot))
        {
            if (snail.sleepTimer > 3)
            {
                snail.sleepTimer = 0.f;
                snail.state = Snail::Idle;
            }
        }
    }
}

void SnailSystem::processDigging(cro::Entity entity)
{
    auto& snail = entity.getComponent<Snail>();
    snail.sleepTimer += ConstVal::FixedGameUpdate;
    if (snail.sleepTimer > 1.f)
    {
        snail.state = Snail::Dead;
    }
}

std::vector<cro::Entity> SnailSystem::doBroadPhase(cro::Entity entity)
{
    entity.getComponent<Snail>().collisionFlags = 0;
    auto position = entity.getComponent<cro::Transform>().getPosition();

    auto bb = SnailBounds;
    bb += position;

    auto& collisionComponent = entity.getComponent<CollisionComponent>();
    auto bounds2D = collisionComponent.sumRect;
    bounds2D.left += position.x;
    bounds2D.bottom += position.y;

    std::vector<cro::Entity> collisions;

    //broadphase
    auto entities = getScene()->getSystem<cro::DynamicTreeSystem>()->query(bb, (entity.getComponent<Snail>().collisionLayer + 1));
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

    return collisions;
}

void SnailSystem::killPlayer(cro::Entity playerEnt, Snail& crate)
{
    if (playerEnt.isValid()
        && playerEnt.hasComponent<Player>())
    {
        auto& player = playerEnt.getComponent<Player>();
        if ((player.state == Player::State::Walking
            || player.state == Player::State::Falling))
        {
            player.state = Player::State::Dead;

            auto* msg = postMessage<PlayerEvent>(MessageID::PlayerMessage);
            //msg->data = collision.owner; //TODO signal this was a snail
            msg->type = PlayerEvent::Died;
            msg->player = playerEnt;
        }
    }
}