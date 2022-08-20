/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

#include <cstdint>
#include <string>

//just to detect client/server version mismatch
//(terrain data changed between 100 -> 110)
//(model format changed between 120 -> 130)
//(server layout updated 140 -> 150)
//(skybox format changed 150 -> 160)
//(hole count became game rule 170 -> 180)
static constexpr std::uint16_t CURRENT_VER = 190;
static const std::string StringVer("1.9.0");

namespace ScoreType
{
    enum
    {
        Stroke, Match, Skins,

        Count
    };
}

namespace MessageType
{
    enum
    {
        ServerFull,
        NotInLobby,
        MapNotFound,
        BadData,
        VersionMismatch
    };
}

namespace PacketID
{
    enum
    {
        //from server
        PingTime, //< uint16 client id | uint16 time in ms
        ClientConnected, //< uint8 client ID
        ClientDisconnected, //< uint8 client ID
        ConnectionRefused, //< uint8 MessageType
        ConnectionAccepted, //< uint8 assigned player ID (0-3)
        ServerError, //< uint8 MessageType
        StateChange, //< uint8 state ID
        LobbyUpdate, //< ConnectionData array
        NotifyPlayer, //< BilliardsPlayer in billiards (wait for ack)
        SetPlayer, //< ActivePlayer struct in golf, BilliardsPlayer in billiards
        SetHole, //< uint8 hole
        ScoreUpdate, //< ScoreUpdate struct
        HoleWon, //< uint16 client OR'd player winning a match or skins point
        FoulEvent, //< int8 BilliardsEvent foul reason - tells client to display a foul message
        GameEnd, //< uint8 seconds. tells clients to show scoreboard/countdown to lobby, or BilliardsPlayer of winner in billiards
        MaxStrokes, //< player reached stroke limit (so client can print message)

        ActorAnimation, //< Tell player sprite to play the given anim with uint8 ID
        ActorUpdate, //< ActorInfo - ball interpolation
        ActorSpawn, //< ActorInfo
        WindDirection, //< compressed vec3
        BallLanded, //< BallUpdate struct
        Gimme, //< uint16 client << 8 | player turn was ended on a gimme
        TableInfo, //< TableInfo struct
        TargetID, //< uint16 billiards player OR'd ball ID to update the UI

        EntityRemoved, //< uint32 entity ID
        ReadyQuitStatus, //< uint8 flags containing status of ready/quit at round end

        //from client
        RequestGameStart, //uint8 sv::State, ie Golf to start golf, Billiards to start billiards etc
        ClientReady, //< uint8 clientID - requests game data from server. Sent repeatedly until ack'd
        InputUpdate, //< uint8 ID (0-3) Input struct (PlayerInput) for golf, or BilliardBallInput
        PlayerInfo, //< ConnectionData array
        ServerCommand, //< uint8_t command type
        TransitionComplete, //< uint8 clientID, signal hole transition completed
        ReadyQuit, //< uint8 clientID - client wants to toggle skip viewing scores
        BallPlaced, //< BilliardBallInput with position in offset member

        //both directions
        ClientVersion, //uint16 FROM server on join contains the game mode, TO server CURRENT_VER of client. Clients are kicked if this does not match the server
        TurnReady, //< uint8 clientID - ready to take their turn - rebroadcast by server to tell all clients to clear messages
        MapInfo, //< serialised cro::String containing course directory
        ScoreType, //< uint8 ScoreType of golf game
        GimmeRadius, //< uint8 gimme radius of golf
        HoleCount, //< uint8 0 - 2: all, front or back
        LobbyReady, //< uint8 playerID uint8 0 false 1 true
        AchievementGet, //< uint8 client uint8 achievement id (always assume first player on client, as achievements are disabled other wise)
        CPUThink, //< uint8 0 if begin think, 1 end think
        CueUpdate //< BilliardsUpdate to show the 'ghost' cue on remote clients
    };
}

namespace ServerCommand
{
    enum
    {
        //golf
        NextHole,
        NextPlayer,
        GotoGreen,
        EndGame,
        ChangeWind,

        //billiards
        SpawnBall,
        StrikeBall
    };
}
