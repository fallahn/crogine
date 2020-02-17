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

#include "ServerState.hpp"
#include "CommonConsts.hpp"

#include <crogine/network/NetHost.hpp>

#include <atomic>
#include <memory>
#include <thread>
#include <array>

namespace Sv
{
    struct ClientConnection final
    {
        //std::uint8_t id = 0;
        bool connected = false;
        cro::NetPeer peer;
    };
}

class Server final
{
public:
    Server();
    ~Server();

    Server(const Server&) = delete;
    Server(Server&&) = delete;
    Server& operator = (const Server&) = delete;
    Server& operator = (Server&&) = delete;

    void launch();
    bool running() const { return m_running; }
    void stop();

private:
    std::atomic_bool m_running;
    std::unique_ptr<std::thread> m_thread;

    std::unique_ptr<Sv::State> m_currentState;

    cro::NetHost m_host;
    std::array<Sv::ClientConnection, ConstVal::MaxClients> m_clients;

    void run();

    bool addClient(const cro::NetEvent&);
    void removeClient(const cro::NetEvent&);
};
