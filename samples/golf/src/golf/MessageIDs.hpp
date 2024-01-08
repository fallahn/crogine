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

#include <crogine/core/Message.hpp>
#include <crogine/detail/glm/vec3.hpp>

namespace cl::MessageID
{
    enum
    {
        //this is a fudge to stop it clashing with sv::MessageID
        //as systems like the BallSystem raise them locally when
        //used in modes like the driving range.
        GolfMessage = 400,//cro::Message::Count
        SceneMessage,
        CollisionMessage,
        SystemMessage,
        AchievementMessage,
        BilliardsMessage,
        BilliardsCameraMessage,
        AIMessage,

        Count
    };
}

struct GolfEvent final
{
    enum
    {
        HitBall,
        ClubChanged,
        ClubSwing,
        RequestNewPlayer,
        SetNewPlayer,
        HookedBall,
        SlicedBall,
        BallLanded,
        Scored,
        NiceShot,
        PowerShot,
        DriveComplete,
        HoleInOne,
        HoleWon,
        HoleDrawn,
        DroneHit,
        BirdHit,
        TargetHit,
        Gimme,
        RoundEnd,
        ClientDisconnect
    }type = HitBall;

    glm::vec3 position = glm::vec3(0.f);
    float travelDistance = 0.f; //this is sqr
    float pinDistance = 0.f; //this is sqr
    
    union
    {
        std::uint8_t terrain = 0;
        std::uint8_t score;
        std::uint8_t player;
    };
    union
    {
        std::uint8_t club = 0;
        std::uint8_t client;
    };
};

struct SceneEvent
{
    enum
    {
        TransitionStart,
        TransitionComplete,
        RequestSwitchCamera,
        PlayerIdle,
        PlayerRotate,
        PlayerBad,
        MinimapUpdated,
        ChatMessage,
        Poke,
        PlayerEliminated
    }type = TransitionComplete;

    //union
    //{
        std::int32_t data = -1;
        float rotation = 0.f;
    //};
    float rotationPercent = 0.f;
};

struct CollisionEvent final
{
    enum Type
    {
        Begin, End, NearMiss
    }type = Begin;

    enum Special
    {
        Billboard = -3,
        Firework,
        FlagPole
    };

    glm::vec3 position = glm::vec3(0.f);
    std::int32_t terrain = 0;
    std::int32_t clubID = -1;
};

struct SystemEvent final
{
    enum
    {
        PostProcessToggled,
        PostProcessIndexChanged,
        StateRequest,
        InputActivated,
        ShadowQualityChanged,
        TreeQualityChanged,
        MenuChanged,
        RestartActiveMode, //currently just driving range, but might apply somewhere else one day :)
        RequestOSK,
        SubmitOSK, //OSK was closed and there's data in the buffer to be read
        CancelOSK
    }type = PostProcessToggled;
    std::int32_t data = -1;
};

struct AchievementEvent final
{
    std::uint8_t id = 0;
};

struct BilliardBallEvent final
{
    enum
    {
        GameStarted,
        ShotTaken,
        BallPlaced,
        Collision,
        TurnStarted,
        PocketStart,
        PocketEnd,
        Foul,
        Score,
        GameEnded
    }type = ShotTaken;
    std::int32_t data = -1; //collision ID
    float volume = 1.f;
    glm::vec3 position = glm::vec3(0.f);
};

struct BilliardsCameraEvent final
{
    enum
    {
        NewTarget
    }type = NewTarget;
    cro::Entity target;
};

struct AIEvent final
{
    enum
    {
        BeginThink,
        EndThink,
        Predict
    }type = BeginThink;
    float power = 0.f;
};