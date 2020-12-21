/*-----------------------------------------------------------------------

Matt Marchant 2020
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
    };
}

namespace CommandPacket
{
    enum
    {
        SetModeFly,
        SetModeWalk
    };
}

namespace PacketID
{
    enum
    {
        //from server
        ClientConnected, //< uint8 connection ID
        ClientDisconnected, //< uint8 connection ID
        ConnectionRefused, //< uint8 MessageType
        ConnectionAccepted, //< uint8 assigned connection ID (0-3)
        StateChange, //< uint8 state ID
        LobbyUpdate, //< LobbyData struct, name string bytes

        PlayerSpawn, //< uint8 ID (0-3) xyz world pos (PlayerInfo struct)
        PlayerUpdate, //< world pos, rotation, uint32 timestamp - used for reconciliation, send directly to targeted peer
        ActorUpdate, //< uint8 ID pos, rotation - used for interpolation of other players and NPCs

        EntityRemoved, //< uint32 entity ID

        Heightmap, //< array of floats

        DayNightUpdate, //< compressed float with the current day/night time

        //from client
        RequestGameStart,
        ClientReady, //< uint8 connectionID - requests game data from server. Sent repeatedly until ack'd
        InputUpdate, //< uint8 ID (0-3) Input struct (PlayerInput)
        PlayerInfo, //< uint8 name length in bytes followed by uint32 array string
        PlayerCount, //< uint16 (connectionID << 8) | playerCount number of local players

        //both directions
        ServerCommand, //< ServerCommand struct - requests server perform some action (may be ignored by server), forwarded to target client if successful
        LobbyReady, //< uint8 playerID uint8 0 false 1 true
    };
}
