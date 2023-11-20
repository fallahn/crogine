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

#include "../PacketIDs.hpp"

#include "Server.hpp"
#include "ServerGolfState.hpp"
#include "ServerLobbyState.hpp"
#include "ServerBilliardsState.hpp"
#include "ServerMessages.hpp"

#include <crogine/core/Log.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/HiResTimer.hpp>

#include <Social.hpp>

#include <functional>

namespace
{
    constexpr std::int32_t MaxGolfPlayers = 16;
    constexpr std::int32_t MaxBilliardsPlayers = 2;
}

Server::Server()
    : m_maxConnections  (ConstVal::MaxClients),
    m_running           (false),
    m_gameMode          (GameMode::None),
    m_maxPlayers        (MaxGolfPlayers),
    m_playerCount       (0),
    m_clientCount       (0)
{

}

Server::~Server()
{
    if (m_running)
    {
        stop();
    }
}

//public
void Server::launch(std::size_t maxConnections, std::int32_t gameMode, bool fastCPU)
{
    //stop any existing instance first
    stop();

    //clear out any old messages
    while (!m_sharedData.messageBus.empty())
    {
        m_sharedData.messageBus.poll();
    }

    m_maxConnections = std::max(std::size_t(1u), std::min(std::size_t(ConstVal::MaxClients), maxConnections));
    m_gameMode = gameMode;

    switch (gameMode)
    {
    default: 
        stop();
        LogE << "Invalid Game Mode: Quitting Server" << std::endl;
        return;
    case GameMode::Golf:
        m_maxPlayers = MaxGolfPlayers;
        break;
    case GameMode::Billiards:
        m_maxPlayers = MaxBilliardsPlayers;
        break;
    }

    m_sharedData.fastCPU = fastCPU;
    m_running = true;
    m_thread = std::make_unique<std::thread>(&Server::run, this);
}

void Server::stop()
{
    if (m_thread)
    {
        m_running = false;
        m_thread->join();

        m_thread.reset();
    }
    m_gameMode = GameMode::None;
    m_playerCount = 0;
}

bool Server::addLocalConnection(net::NetClient& client)
{
#ifdef USE_GNS
    if (!m_running)
    {
        LOG("Server not running", cro::Logger::Type::Error);
        return false;
    }
    return m_sharedData.host.addLocalConnection(client);
#else
    return false;
#endif
}

void Server::setHostID(std::uint64_t id)
{
    m_sharedData.hostID = id;
}

//private
void Server::run()
{
    if (!m_sharedData.host.start(/*m_preferredIP*/"", ConstVal::GamePort, /*ConstVal::MaxClients*/m_maxConnections, 4))
    {
        m_running = false;
        cro::Logger::log("Failed to start host service", cro::Logger::Type::Error);
        return;
    }    
    
    LOG("Server launched", cro::Logger::Type::Info);

    m_currentState = std::make_unique<sv::LobbyState>(m_sharedData);
    std::int32_t nextState = m_currentState->stateID();

    //network broadcasts are called less regularly
    //than logic updates to the scene
    const cro::Time netFrameTime = cro::milliseconds(50);
    cro::Clock netFrameClock;
    cro::Time netAccumulatedTime;

    cro::HiResTimer updateClock;
    float updateAccumulator = 0.f;

    const cro::Time pingTime = cro::seconds(1.f);
    cro::Clock pingClock;
    cro::Time pingAccumulator;

    while (m_running)
    {
        while (!m_sharedData.messageBus.empty())
        {
            const auto& msg = m_sharedData.messageBus.poll();
            m_currentState->handleMessage(msg);

            if (msg.id == sv::MessageID::ConnectionMessage)
            {
                const auto& data = msg.getData<ConnectionEvent>();
                if (data.type == ConnectionEvent::Kicked
                    && data.clientID < ConstVal::MaxClients)
                {
                    kickClient(data.clientID);
                }
            }
        }

        net::NetEvent evt;
        while(m_sharedData.host.pollEvent(evt))
        {
            m_currentState->netEvent(evt);
        
            //handle connects / disconnects
            if (evt.type == net::NetEvent::ClientConnect)
            {
                //refuse if not in lobby state
                //else add to pending list and await version confirmation
                if (m_currentState->stateID() == sv::StateID::Lobby)
                {
                    m_pendingConnections.emplace_back().peer = evt.peer;
                    m_sharedData.host.sendPacket(evt.peer, PacketID::ClientVersion, std::uint16_t(m_gameMode), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                }
                else
                {
                    //send rejection packet
                    m_sharedData.host.sendPacket(evt.peer, PacketID::ConnectionRefused, std::uint8_t(MessageType::NotInLobby), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                    m_sharedData.host.disconnectLater(evt.peer);
                }
            }
            else if (evt.type == net::NetEvent::ClientDisconnect)
            {
                //remove from client list
                removeClient(evt);
            }
            else if(evt.type == net::NetEvent::PacketReceived)
            {
                switch (evt.packet.getID())
                {
                default: break;
                case PacketID::AchievementGet:
                    //re-broadcast to all clients
                    m_sharedData.host.broadcastPacket(PacketID::AchievementGet, evt.packet.as<std::array<std::uint8_t, 2u>>(), net::NetFlag::Reliable);
                    break;
                case PacketID::ClientVersion:
                {
                    auto clientVer = evt.packet.as<std::uint16_t>();
                    if (clientVer != CURRENT_VER)
                    {
                        m_sharedData.host.sendPacket(evt.peer, PacketID::ConnectionRefused, std::uint8_t(MessageType::VersionMismatch), net::NetFlag::Reliable);
                        LogE << "Client responded with version " << clientVer << ", server is " << CURRENT_VER << std::endl;
                    }
                    else
                    {
                        m_sharedData.host.sendPacket(evt.peer, PacketID::ClientPlayerCount, std::uint8_t(0), net::NetFlag::Reliable);
                    }
                }
                    break;
                case PacketID::ClientPlayerCount:
                    validatePeer(evt.peer, evt.packet.as<std::uint8_t>());
                    break;
                case PacketID::PlayerXP:
                    m_sharedData.host.broadcastPacket(PacketID::PlayerXP, evt.packet.as<std::uint16_t>(), net::NetFlag::Reliable);
                    break;
                case PacketID::ChatMessage:
                    m_sharedData.host.broadcastPacket(PacketID::ChatMessage, evt.packet.as<TextMessage>(), net::NetFlag::Reliable, ConstVal::NetChannelStrings);
                    break;
                }
            }
        }

        checkPending();

        //TODO fix this - we have to wait for at least
        //one connection to happen first
        
        //m_running = m_clientCount != 0;

        //network broadcasts
        netAccumulatedTime += netFrameClock.restart();
        while (netAccumulatedTime > netFrameTime)
        {
            netAccumulatedTime -= netFrameTime;
            m_currentState->netBroadcast();
        }

        //logic updates
        updateAccumulator += updateClock.restart();
        while (updateAccumulator > ConstVal::FixedGameUpdate)
        {
            updateAccumulator -= ConstVal::FixedGameUpdate;
            nextState = m_currentState->process(ConstVal::FixedGameUpdate);
        }

        //broadcast connection quality
        pingAccumulator += pingClock.restart();
        while (pingAccumulator > pingTime)
        {
            pingAccumulator -= pingTime;
            for (auto i = 0u; i < m_sharedData.clients.size(); ++i)
            {
                if (m_sharedData.clients[i].connected)
                {
                    std::uint16_t client = i;
                    std::uint16_t ping = m_sharedData.clients[i].peer.getRoundTripTime();
                    std::uint32_t data = (client << 16) | ping;
                    m_sharedData.host.broadcastPacket(PacketID::PingTime, data, net::NetFlag::Unreliable);
                }
            }
        }

        //switch state if last update returned a new state ID
        if (nextState != m_currentState->stateID())
        {
            switch (nextState)
            {
            default: m_running = false; break;
            case sv::StateID::Golf:
                m_currentState = std::make_unique<sv::GolfState>(m_sharedData);
                break;
            case sv::StateID::Lobby:
                m_currentState = std::make_unique<sv::LobbyState>(m_sharedData);
                break;
            case sv::StateID::Billiards:
                m_currentState = std::make_unique<sv::BilliardsState>(m_sharedData);
                break;
            }

            m_sharedData.host.broadcastPacket(PacketID::StateChange, std::uint8_t(nextState), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
            
            //mitigate large DT which may have built up while new state was loading.
            netFrameClock.restart();
        }
    }

    m_currentState.reset();

    //clear client data
    for (auto& c : m_sharedData.clients)
    {
        m_sharedData.host.disconnect(c.peer);
        c = {};
    }

    m_sharedData.host.stop();

    LOG("Server quit", cro::Logger::Type::Info);
}

void Server::checkPending()
{
    for (auto& [peer, t] : m_pendingConnections)
    {
        if (t.elapsed().asSeconds() > PendingConnection::Timeout)
        {
            m_sharedData.host.sendPacket(peer, PacketID::ConnectionRefused, std::uint8_t(MessageType::VersionMismatch), net::NetFlag::Reliable);
            m_sharedData.host.disconnectLater(peer);
        }

        //TODO regularly send another version request in case a packet was lost?
    }

    m_pendingConnections.erase(std::remove_if(m_pendingConnections.begin(), m_pendingConnections.end(), 
        [](const PendingConnection& pc)
        {
            return pc.connectionTime.elapsed().asSeconds() > PendingConnection::Timeout;        
        }), m_pendingConnections.end());
}

void Server::validatePeer(net::NetPeer& peer, std::uint8_t playerCount)
{
    auto result = std::find_if(m_pendingConnections.begin(), m_pendingConnections.end(),
        [&peer](const PendingConnection& pc)
        {
            return pc.peer == peer;
        });

    if (result != m_pendingConnections.end())
    {
        if (auto i = addClient(peer, playerCount); i >= m_maxConnections)
        {
            //tell client server is full
            m_sharedData.host.sendPacket(peer, PacketID::ConnectionRefused, std::uint8_t(MessageType::ServerFull), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
            m_sharedData.host.disconnectLater(peer);
        }
        else
        {
            //tell the client which player they are
            m_sharedData.host.sendPacket(peer, PacketID::ConnectionAccepted, i, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }

        m_pendingConnections.erase(result);
    }
    else
    {
        //stray connection so boot it?
    }
}

std::uint8_t Server::addClient(const net::NetPeer& peer, std::uint8_t playerCount)
{
    if (m_playerCount + playerCount <= m_maxPlayers)
    {
        std::uint8_t i = 0;
        for (; i < m_sharedData.clients.size(); ++i)
        {
            if (!m_sharedData.clients[i].connected)
            {
                LOG("Added client to server with id " + std::to_string(peer.getID()), cro::Logger::Type::Info);

                m_sharedData.clients[i].connected = true;
                m_sharedData.clients[i].peer = peer;

                //broadcast to all connected clients
                //so they can update lobby view.
                m_sharedData.host.broadcastPacket(PacketID::ClientConnected, i, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

                auto* msg = m_sharedData.messageBus.post<ConnectionEvent>(sv::MessageID::ConnectionMessage);
                msg->clientID = i;
                msg->type = ConnectionEvent::Connected;

                m_clientCount++;
                m_playerCount += playerCount;

                break;
            }
        }
        return i;
    }

    return ConstVal::NullValue;
}

void Server::removeClient(const net::NetEvent& evt)
{
    //remove from pending connection in case client is quitting due to game mode mismatch
    m_pendingConnections.erase(std::remove_if(m_pendingConnections.begin(), m_pendingConnections.end(),
        [&evt](const PendingConnection& pc)
        {
            return pc.peer == evt.peer;
        }), m_pendingConnections.end());

    auto result = std::find_if(m_sharedData.clients.begin(), m_sharedData.clients.end(), 
        [&evt](const sv::ClientConnection& c) 
        {
            return c.peer == evt.peer;
        });

    if (result != m_sharedData.clients.end())
    {
        removeClient(std::distance(m_sharedData.clients.begin(), result));
    }
}

void Server::removeClient(std::size_t clientID)
{
    m_playerCount -= m_sharedData.clients[clientID].playerCount;

    m_sharedData.clients[clientID] = sv::ClientConnection(); //resets the data, setting 'connected' to false etc

    auto* msg = m_sharedData.messageBus.post<ConnectionEvent>(sv::MessageID::ConnectionMessage);
    msg->clientID = static_cast<std::uint8_t>(clientID);
    msg->type = ConnectionEvent::Disconnected;

    m_sharedData.clubLevels[clientID] = 2; //reset this if quitting, else we might clamp to the level of a quit player

    //broadcast to all connected clients
    m_sharedData.host.broadcastPacket(PacketID::ClientDisconnected, static_cast<std::uint8_t>(clientID), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
    LOG("Client disconnected", cro::Logger::Type::Info);

    m_clientCount--;
}

void Server::kickClient(std::size_t clientID)
{
    if (m_sharedData.clients[clientID].connected)
    {
        //we assume host is always 0...
        auto& peer = m_sharedData.clients[clientID].peer;
        m_sharedData.host.sendPacket(peer, PacketID::ConnectionRefused, std::uint8_t(MessageType::Kicked), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
        m_sharedData.host.disconnectLater(peer);

        removeClient(clientID);
    }
}