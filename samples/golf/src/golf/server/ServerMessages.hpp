/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include "../CommonConsts.hpp"

#include <crogine/core/Message.hpp>

namespace sv::MessageID
{
    enum
    {
        ConnectionMessage = cro::Message::Count,
        GolfMessage,
        BilliardsMessage,
        TriggerMessage,
        BullsEyeMessage
    };
}

struct ConnectionEvent final
{
    std::uint8_t clientID = ConstVal::NullValue;
    enum
    {
        Connected, Disconnected, Kicked
    }type = Connected;
};

struct GolfBallEvent final
{
    enum
    {
        None,
        Landed,
        Foul,
        TurnEnded,
        Holed,
        Gimme
    }type = None;
    std::int32_t terrain = -1;
    float distance = 0.f;
    glm::vec3 position = glm::vec3(0.f);
};

struct BilliardsEvent final
{
    enum
    {
        Collision,
        Pocket,
        OutOfBounds,
        TurnBegan,
        TurnEnded,
        PlayerSwitched,
        GameEnded,
        Foul,
        TargetAssigned,
        BallReplaced
    }type = Collision;

    enum
    {
        //foul reasons
        WrongBallHit,
        WrongBallPot,
        OffTable,
        Forfeit,
        NoBallHit,
        CueBallPot,
        FreeTable
    };

    std::int8_t first = -1; //ballA or foul reason
    std::int8_t second = -1; //ballB or pocketID
    glm::vec3 position = glm::vec3(0.f); //only for ball placement
};

struct TriggerEvent final
{
    std::uint8_t triggerID = 25;
};

struct BullsEyeEvent final
{
    glm::vec3 position = glm::vec3(0.f);
    float accuracy = 0.f;
};