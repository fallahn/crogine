/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#ifndef DH_MESSAGES_HPP_
#define DH_MESSAGES_HPP_

#include <crogine/core/Message.hpp>

#include <crogine/detail/glm/vec3.hpp>

namespace MessageID
{
    enum
    {
        GameMessage = cro::Message::Count,
        UIMessage,
        PlayerMessage,
        WeaponMessage
    };
}


struct GameEvent final
{
    enum
    {
        RoundStart
    }type;
};

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
    enum
    {
        Spawned,
        Died
    }type;
    glm::vec3 position;
};

struct WeaponEvent final
{
    enum
    {
        Laser,
        Pistol,
        Grenade
    }type;

    glm::vec3 direction;
    glm::vec3 position;
};

#endif //DH_MESSAGES_HPP_