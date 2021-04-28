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

#include <crogine/core/Message.hpp>
#include <crogine/ecs/Entity.hpp>
#include <crogine/detail/glm/vec3.hpp>

namespace MessageID
{
    enum
    {
        UIMessage = cro::Message::Count,
        PlayerMessage,
        CrateMessage,
        ConnectionMessage,
        GameMessage,
        AvatarMessage,
        ActorMessage
    };
}


struct UIEvent final
{
    enum
    {
        ButtonPressed,
        ButtonReleased
    }type;

    enum Button
    {
        Left,
        Right,
        Jump,
        Fire
    }button;
};

struct PlayerEvent final
{
    enum Type
    {
        None,
        Teleported,
        Landed,
        Jumped,
        DroppedCrate,
        Died,
        Reset,
        Spawned,
        Scored,
        Retired
    }type = None;

    std::int32_t data = -1;
    cro::Entity player;
};

struct CrateEvent final
{
    enum
    {
        StateChanged
    }type = StateChanged;

    cro::Entity crate;
    glm::vec3 position = glm::vec3(0.f);
};

struct ConnectionEvent final
{
    std::uint8_t playerID = 4;
    enum
    {
        Connected, Disconnected
    }type = Connected;
};

struct GameEvent final
{
    enum Type
    {
        GameBegin,
        RoundWarn,
        SuddenDeath,
        GameEnd
    }type = GameBegin;
};

struct AvatarEvent final
{
    enum
    {
        None,
        Died,
        Jumped,
        Landed,
        Teleported,
        DroppedCrate,
        Reset,
        Spawned,
        Scored,
        Retired
    }type = None;
    std::int8_t playerID = -1;
    std::uint8_t lives = 0;
    glm::vec3 position;
};

struct ActorEvent final
{
    enum
    {
        Added,
        Removed
    }type = Added;
    glm::vec3 position = glm::vec3(0.f);
    std::int32_t id = -1;
};