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

#include "CommonConsts.hpp"

#include <crogine/network/NetData.hpp>
#include <crogine/network/NetHost.hpp>
#include <crogine/core/MessageBus.hpp>
#include <crogine/core/String.hpp>

#include <cstdint>
#include <array>

namespace sv
{
    struct ClientConnection final
    {
        std::size_t playerCount = 1; //< number of players to create in a local game
        bool ready = false; //< player is ready to recieve game data, not lobby readiness (see GameState)
        bool connected = false;
        cro::NetPeer peer;
        cro::String name;
        std::int32_t latency = 0;
    };

    struct SharedData final
    {
        cro::NetHost host;
        std::array<sv::ClientConnection, ConstVal::MaxClients> clients;
        cro::MessageBus messageBus;
        cro::String mapName;
        std::array<std::int32_t, 4u> scores = {}; //in player ID order. TODO this could be running XP
    };

    namespace StateID
    {
        enum
        {
            Lobby,
            Game
        };
    }

    class State
    {
    public:
        virtual ~State() = default;

        virtual void handleMessage(const cro::Message&) = 0;

        //handle incoming network events
        virtual void netEvent(const cro::NetEvent&) = 0;
        //broadcast network updates at 20Hz
        virtual void netBroadcast() {};
        //update scene logic at 62.5Hz (16ms)
        virtual std::int32_t process(float) = 0;

        virtual std::int32_t stateID() const = 0;
    };
}