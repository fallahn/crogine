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

#pragma once

#include "InputParser.hpp"

#include <crogine/ecs/Entity.hpp>

#include <vector>

namespace cro
{
    class Scene;
}

class PlayerState
{
public:
    static constexpr float MaxVelocity = 10.f;
    static constexpr float Acceleration = 1.2f;
    static constexpr float Deceleration = 16.f;
    static constexpr float Friction = Acceleration;
    static constexpr float AirAcceleration = Acceleration * 2.f;
    static constexpr float HardStopAmount = 0.2f;

    static constexpr float JumpImpulse = 24.f;
    static constexpr float MinJump = JumpImpulse * 0.1f;

    static constexpr float TeleportSpeed = 10.f;

    PlayerState() = default;
    virtual ~PlayerState() = default;

    virtual void processMovement(cro::Entity, Input) = 0;
    virtual void processCollision(cro::Entity, const std::vector<cro::Entity>&) = 0;
};

class PlayerStateFalling final : public PlayerState
{
public:
    PlayerStateFalling();

    void processMovement(cro::Entity, Input) override;
    void processCollision(cro::Entity, const std::vector<cro::Entity>&) override;

private:
};

class PlayerStateWalking final : public PlayerState
{
public:
    PlayerStateWalking();

    void processMovement(cro::Entity, Input) override;
    void processCollision(cro::Entity, const std::vector<cro::Entity>&) override;

private:
};

class PlayerStateTeleport final : public PlayerState
{
public:
    PlayerStateTeleport();

    void processMovement(cro::Entity, Input) override;
    void processCollision(cro::Entity, const std::vector<cro::Entity>&) override;

private:
};