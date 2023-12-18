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

#include "ScoreType.hpp"

#include <cstdint>
#include <string>
#include <array>

namespace MessageType
{
    enum
    {
        ServerFull,
        NotInLobby,
        MapNotFound,
        BadData,
        VersionMismatch,
        Kicked
    };
}

namespace Emote
{
    enum
    {
        Happy, Grumpy, Laughing, Sad
    };
}

namespace MaxStrokeID
{
    enum
    {
        Default = 0,
        Forfeit
    };
}

struct Activity final
{
    enum
    {
        CPUThinkStart,
        CPUThinkEnd,
        PlayerThinkStart,
        PlayerThinkEnd,
        PlayerChatStart,
        PlayerChatEnd,
        PlayerIdleStart,
        PlayerIdleEnd
    };
    std::uint8_t type = 0;
    std::uint8_t client = 0;
};

struct TextMessage final
{
    std::uint8_t client = 0;

    //this is the max message bytes - total message length is
    //this plus 1 for nullterm
    static constexpr std::size_t MaxBytes = 8192; 
    std::array<char, MaxBytes + 1> messageData = {};
    TextMessage() { std::fill(messageData.begin(), messageData.end(), 0); }
};

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
        SetHole, //< uint16(uint8 hole | uint8 par)
        SetPar, //< uint8 par
        ScoreUpdate, //< ScoreUpdate struct
        HoleWon, //< uint16 client OR'd player winning a match or skins point
        FoulEvent, //< int8 BilliardsEvent foul reason - tells client to display a foul message
        GameEnd, //< uint8 seconds. tells clients to show scoreboard/countdown to lobby, or BilliardsPlayer of winner in billiards
        MaxStrokes, //< uint8 MaxStrokeID player reached stroke limit (so client can print message)
        MaxClubs, //< tells the client to use this club set

        ActorAnimation, //< Tell player sprite to play the given anim with uint8 ID
        ActorUpdate, //< ActorInfo - ball interpolation
        ActorSpawn, //< ActorInfo
        WindDirection, //< compressed vec3
        BallLanded, //< BallUpdate struct
        Gimme, //< uint16 client << 8 | player turn was ended on a gimme
        Elimination, //< uint16 client << 8 | player was eliminated
        TableInfo, //< TableInfo struct
        TargetID, //< uint16 billiards player OR'd ball ID to update the UI
        ServerAchievement, //< uint8 client, uint8 player, uint8 achievement id - up to client to decide if to award

        EntityRemoved, //< uint32 entity ID
        ReadyQuitStatus, //< uint8 flags containing status of ready/quit at round end
        BullsEye, //< bullseye struct
        BullHit, //< BullHit struct
        FlagHit, //< BullHit struct
        WarnTime, //< uint8 warning time for AFK in seconds
        WeatherChange, //< 0 off 1 on uint8
        Poke, //< uint8 0 - only sent to specific client

        //from client
        RequestGameStart, //uint8 sv::State, ie Golf to start golf, Billiards to start billiards etc
        ClientReady, //< uint8 clientID - requests game data from server. Sent repeatedly until ack'd
        InputUpdate, //< InputUpdate struct for golf, or BilliardBallInput
        PlayerInfo, //< ConnectionData array
        ServerCommand, //< uint16_t command type | optionally client target
        TransitionComplete, //< uint8 clientID, signal hole transition completed
        NewPlayer, //< animation completed on the client and new player active
        ReadyQuit, //< uint8 clientID - client wants to toggle skip viewing scores
        BallPlaced, //< BilliardBallInput with position in offset member
        SkipTurn, //< uint8 clientID - requests server fast forward current turn
        ClubLevel, //< uint8 client ID | uint8 client club level (max clubs - used to limit to lowest player)

        //both directions
        ClientVersion, //uint16 FROM server on join contains the game mode, TO server CURRENT_VER of client. Clients are kicked if this does not match the server
        ClientPlayerCount, //uint8_t 0 if from server, client count if from client. Server tracks total players and rejects connections which would exceed maximum players
        TurnReady, //< uint8 clientID - ready to take their turn - rebroadcast by server to tell all clients to clear messages
        MapInfo, //< serialised cro::String containing course directory
        ScoreType, //< uint8 ScoreType of golf game
        NightTime, //< uint8 0 false else true
        WeatherType, //< uint8 WeatherType
        FastCPU, //< uint8 0 false else true
        GimmeRadius, //< uint8 gimme radius of golf
        HoleCount, //< uint8 0 - 2: all, front or back
        ReverseCourse, //< uint8 0 false else true
        ClubLimit, //< uint8 0 false else true
        LobbyReady, //< uint8 playerID uint8 0 false 1 true
        AchievementGet, //< uint8 client uint8 achievement id (always assume first player on client, as achievements are disabled other wise)
        Activity, //< Activity struct contains start/end events for 'thinking'
        CueUpdate, //< BilliardsUpdate to show the 'ghost' cue on remote clients
        NewLobbyReady, //< uint64 lobbyID - broadcast by host when returning from existing game and relayed by server
        Emote, //< uint32 00|client|player|emoteID
        LevelUp, //< uint64 00|00|client|player|level (level is 4 bytes)
        BallPrediction, //< InputUpdate if from client, vec3 if from server
        PlayerXP, //<uint16 level << 8 | client - used to share client xp/level info
        ChatMessage //TextMessage struct
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
        GotoHole,
        EndGame,
        ChangeWind,
        SkipTurn,
        KickClient,
        PokeClient,
        ForfeitClient,

        //billiards
        SpawnBall,
        StrikeBall
    };
}
