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
    PlayerState() = default;
    virtual ~PlayerState() = default;

    virtual void processMovement(cro::Entity, Input) = 0;
    virtual void processCollision(cro::Entity, const std::vector<cro::Entity>&) = 0;
    virtual void processAvatar(cro::Entity) = 0;
};

class PlayerStateFalling final : public PlayerState
{
public:
    PlayerStateFalling();

    void processMovement(cro::Entity, Input) override;
    void processCollision(cro::Entity, const std::vector<cro::Entity>&) override;
    void processAvatar(cro::Entity) override;

private:
};

class PlayerStateWalking final : public PlayerState
{
public:
    PlayerStateWalking();

    void processMovement(cro::Entity, Input) override;
    void processCollision(cro::Entity, const std::vector<cro::Entity>&) override;
    void processAvatar(cro::Entity) override;

private:
};