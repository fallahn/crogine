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

#ifndef TL_MESSAGES_HPP_
#define TL_MESSAGES_HPP_

#include <crogine/core/Message.hpp>

#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/glm/vec3.hpp>

namespace MessageID
{
    enum
    {
        BackgroundSystem = cro::Message::Count,
        GameMessage,
        PlayerMessage,
        NpcMessage,
        BuddyMessage,
        WeaponMessage,
        StatsMessage,
        UIMessage
    };
}

struct BackgroundEvent final
{
    enum
    {
        SpeedChange,
        Shake,
        ModeChanged,
        MountCreated
    } type;
    float value = 0.f;
    glm::vec2 position = glm::vec2(0.f);
    cro::uint32 entityID = 0;
};

struct GameEvent final
{
    enum
    {
        GameStart,
        RoundStart,
        RoundEnd,
        BossStart,
        BossEnd,
        GameOver
    }type;
};

struct StatsEvent final
{
    enum
    {
        Score
    }type;
    cro::int32 value;
};

struct PlayerEvent final
{
    enum
    {
        Spawned,
        Died,
        Moved,
        WeaponStateChange,
        HealthChanged,
        CollectedItem,
        GotLife,
        FiredEmp
    }type;
    glm::vec3 position = glm::vec3(0.f);
    cro::uint32 entityID = 0;
    union
    {
        bool weaponActivated;
        float value;
        cro::int32 itemID;
    };
};

struct NpcEvent final
{
    enum
    {
        Died,
        FiredWeapon,
        HealthChanged,
        ExitScreen
    }type;
    cro::int32 npcType = -1;
    glm::vec3 position = glm::vec3(0.f);
    glm::vec3 velocity = glm::vec3(0.f);
    float value = 0.f;
    cro::uint32 entityID = 0;
};

struct BuddyEvent final
{
    enum
    {
        Spawned,
        FiredWeapon,
        Died
    }type;
    glm::vec3 position = glm::vec3(0.f);
};

struct WeaponEvent final
{
    float downgradeTime = 0.f;
    cro::int32 fireMode = 0;
};

struct UIEvent final
{
    enum
    {
        ButtonPressed,
        ButtonReleased
    }type;

    enum
    {
        Emp,
        Pause
    }button;
};

#endif //TL_MESSAGES_HPP_