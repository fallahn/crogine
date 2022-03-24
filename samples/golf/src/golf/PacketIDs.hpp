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
//(server layout updated 140 ->  150)
static constexpr std::uint16_t CURRENT_VER = 150;
static const std::string StringVer("1.5.0");


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
        ClientConnected, //< uint8 client ID
        ClientDisconnected, //< uint8 client ID
        ConnectionRefused, //< uint8 MessageType
        ConnectionAccepted, //< uint8 assigned player ID (0-3)
        ServerError, //< uint8 MessageType
        StateChange, //< uint8 state ID
        LobbyUpdate, //< ConnectionData array
        SetPlayer, //< ActivePlayer struct in golf, player/client in billiards
        SetHole, //< uint8 hole
        ScoreUpdate, //< ScoreUpdate struct
        GameEnd, //< uint8 seconds. tells clients to show scoreboard/countdown to lobby

        ActorAnimation, //< Tell player sprite to play the given anim with uint8 ID
        ActorUpdate, //< ActorInfo - ball interpolation
        ActorSpawn, //< ActorInfo
        WindDirection, //< compressed vec3
        BallLanded, //< BallUpdate struct
        TableInfo, //< TableInfo struct

        EntityRemoved, //< uint32 entity ID
        ReadyQuitStatus, //< uint8 flags containing status of ready/quit at round end

        //from client
        ClientVersion, //uint32 CURRENT_VER of client. Clients are kicked if this does not match the server
        RequestGameStart, //uint8 sv::State, ie Golf to start golf, Billiards to start billiards etc
        ClientReady, //< uint8 clientID - requests game data from server. Sent repeatedly until ack'd
        InputUpdate, //< uint8 ID (0-3) Input struct (PlayerInput) for golf, or BilliardBallInput
        PlayerInfo, //< ConnectionData array
        ServerCommand, //< uint8_t command type
        TransitionComplete, //< uint8 clientID, signal hole transition completed
        ReadyQuit, //< uint8 clientID - client wants to toggle skip viewing scores
        BallPlaced, //< BilliardBallInput with position in offset member

        //both directions
        MapInfo, //< serialised cro::String containing course directory
        LobbyReady, //< uint8 playerID uint8 0 false 1 true
        AchievementGet, //< uint8 client uint8 achievement id (always assume first player on client, as achievements are disabled other wise)
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