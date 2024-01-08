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

#include "../Networking.hpp"
#include "ServerState.hpp"

#include <atomic>
#include <memory>
#include <thread>

class Server final
{
public:
    struct GameMode final
    {
        enum
        {
            Golf, Billiards, None
        };
    };

    Server();
    ~Server();

    Server(const Server&) = delete;
    Server(Server&&) = delete;
    Server& operator = (const Server&) = delete;
    Server& operator = (Server&&) = delete;

    //max connections are not game mode dependent as the
    //tutorial shares a game mode with golf, but must be
    //limited to a single connection.
    void launch(std::size_t maxConnections, std::int32_t gameMode, bool fastCPU);
    bool running() const { return m_running; }
    void stop();

    bool addLocalConnection(net::NetClient& client);
    void setHostID(std::uint64_t id);

    //note this is not atomic!
    void setPreferredIP(const std::string& ip) { m_preferredIP = ip; }
    const std::string& getPreferredIP() const { return m_preferredIP; }


private:
    std::size_t m_maxConnections;
    std::string m_preferredIP;
    std::atomic_bool m_running;
    std::unique_ptr<std::thread> m_thread;

    std::unique_ptr<sv::State> m_currentState;
    std::int32_t m_gameMode;
    //depending on the game mode we ask the client for how many players
    //it has and reject or accept the client based on there being enough room
    std::int32_t m_maxPlayers;
    std::int32_t m_playerCount;

    sv::SharedData m_sharedData;

    struct PendingConnection final
    {
        net::NetPeer peer;
        cro::Clock connectionTime;
        static constexpr float Timeout = 15.f;
    };
    std::vector<PendingConnection> m_pendingConnections;

    std::size_t m_clientCount;

    void run();

    void checkPending();
    void validatePeer(net::NetPeer&, std::uint8_t playerCount);

    //returns slot index, or >= MaxClients if full
    std::uint8_t addClient(const net::NetPeer&, std::uint8_t playerCount);
    void removeClient(const net::NetEvent&);
    void removeClient(std::size_t);
    void kickClient(std::size_t);
};
