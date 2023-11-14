/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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
#include "../PlayerColours.hpp"
#include "Networking.hpp"

#include <crogine/core/MessageBus.hpp>
#include <crogine/core/String.hpp>

#include <cstdint>
#include <array>
#include <atomic>

namespace sv
{
    struct PlayerInfo final
    {
        cro::String name;
        std::array<std::uint8_t, pc::ColourKey::Count> avatarFlags = {}; //not really flags per se, but let's at least keep naming consistent
        std::uint32_t ballID = 0;
        std::uint32_t hairID = 0;
        std::uint32_t skinID = 0;
        bool flipped = false; //we don't really care about this on the server, but we do need to forward it to clients.
        bool isCPU = false; //only allow CPU players to request predictions
    };

    struct ClientConnection final
    {
        bool ready = false; //< player is ready to recieve game data, not lobby readiness (see GameState)
        bool mapLoaded = false;
        bool connected = false;
        net::NetPeer peer;
        
        //TODO this is basically the same as the ConnectionData struct in client shared data
        std::uint8_t playerCount = 0;
        std::uint64_t peerID = 0;
        std::array<PlayerInfo, ConstVal::MaxPlayers> playerData = {};
    };

    struct SharedData final
    {
        net::NetHost host;
        std::array<sv::ClientConnection, ConstVal::MaxClients> clients;
        cro::MessageBus messageBus;
        cro::String mapDir;
        std::uint8_t scoreType = 0;
        std::uint8_t nightTime = 0;
        std::uint8_t weatherType = 0;
        std::uint8_t gimmeRadius = 0;
        std::uint8_t holeCount = 0;
        std::uint8_t reverseCourse = 0;
        std::uint8_t clubLimit = 0;
        std::uint8_t fastCPU = 1;

        std::array<std::uint8_t, ConstVal::MaxClients> clubLevels = {};

        std::atomic_uint64_t hostID = 0;
    };

    namespace StateID
    {
        enum
        {
            Lobby,
            Golf,
            Billiards,

            Count
        };
    }

    class State
    {
    public:
        virtual ~State() = default;

        virtual void handleMessage(const cro::Message&) = 0;

        //handle incoming network events
        virtual void netEvent(const net::NetEvent&) = 0;
        //broadcast network updates at 20Hz
        virtual void netBroadcast() {};
        //update scene logic at 62.5Hz (16ms)
        virtual std::int32_t process(float) = 0;

        virtual std::int32_t stateID() const = 0;
    };
}