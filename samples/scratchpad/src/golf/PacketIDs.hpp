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

namespace MessageType
{
    enum
    {
        ServerFull,
        NotInLobby,
        MapNotFound
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
        SetPlayer, //< ActivePlayer struct
        SetHole, //< uint8 hole
        ScoreUpdate, //< ScoreUpdate struct
        GameEnd, //< uint8 seconds. tells clients to show scoreboard/countdown to lobby

        ActorAnimation, //< Tell player sprite to play the given anim with uint8 ID
        ActorUpdate, //< ActorInfo - ball interpolation
        ActorSpawn, //< ActorInfo
        WindDirection, //< compressed vec3
        BallLanded, //< uint8 terrain

        EntityRemoved, //< uint32 entity ID

        //from client
        RequestGameStart,
        ClientReady, //< uint8 playerID - requests game data from server. Sent repeatedly until ack'd
        InputUpdate, //< uint8 ID (0-3) Input struct (PlayerInput)
        PlayerInfo, //< ConnectionData array
        ServerCommand, //< uint8_t command type

        //both directions
        MapInfo, //< serialised cro::String containing course directory
        LobbyReady, //< uint8 playerID uint8 0 false 1 true
    };
}

namespace ServerCommand
{
    enum
    {
        NextHole,
        NextPlayer,
        GotoGreen,
        EndGame
    };
}