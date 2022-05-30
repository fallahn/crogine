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

#include "../PacketIDs.hpp"

#include "Server.hpp"
#include "ServerGolfState.hpp"
#include "ServerLobbyState.hpp"
#include "ServerBilliardsState.hpp"
#include "ServerMessages.hpp"

#include <crogine/core/Log.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/HiResTimer.hpp>

#include <functional>

Server::Server()
    : m_maxConnections  (ConstVal::MaxClients),
    m_running           (false),
    m_gameMode          (GameMode::None)
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
void Server::launch(std::size_t maxConnections, GameMode gameMode)
{
    //stop any existing instance first
    stop();

    //clear out any old messages
    while (!m_sharedData.messageBus.empty())
    {
        m_sharedData.messageBus.poll();
    }

    m_maxConnections = std::max(std::size_t(1u), std::min(ConstVal::MaxClients, maxConnections));
    m_gameMode = gameMode;

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
}

void Server::setHostID(std::uint32_t id)
{
    m_sharedData.hostID = id;
}

//private
void Server::run()
{
    if (!m_sharedData.host.start(m_preferredIP, ConstVal::GamePort, ConstVal::MaxClients, 4))
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

    while (m_running)
    {
        while (!m_sharedData.messageBus.empty())
        {
            m_currentState->handleMessage(m_sharedData.messageBus.poll());
        }

        cro::NetEvent evt;
        while(m_sharedData.host.pollEvent(evt))
        {
            m_currentState->netEvent(evt);
        
            //handle connects / disconnects
            if (evt.type == cro::NetEvent::ClientConnect)
            {
                //refuse if not in lobby state
                //else add to pending list and await version confirmation
                if (m_currentState->stateID() == sv::StateID::Lobby)
                {
                    m_pendingConnections.emplace_back().peer = evt.peer;
                    m_sharedData.host.sendPacket(evt.peer, PacketID::ClientVersion, std::uint16_t(m_gameMode), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
                }
                else
                {
                    //send rejection packet
                    m_sharedData.host.sendPacket(evt.peer, PacketID::ConnectionRefused, std::uint8_t(MessageType::NotInLobby), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
                    m_sharedData.host.disconnectLater(evt.peer);
                }
            }
            else if (evt.type == cro::NetEvent::ClientDisconnect)
            {
                //remove from client list
                removeClient(evt);
            }
            else if(evt.type == cro::NetEvent::PacketReceived)
            {
                if (evt.packet.getID() == PacketID::ClientVersion)
                {
                    if (evt.packet.as<std::uint16_t>() != CURRENT_VER)
                    {
                        m_sharedData.host.sendPacket(evt.peer, PacketID::ConnectionRefused, std::uint8_t(MessageType::VersionMismatch), cro::NetFlag::Reliable);
                    }
                    else
                    {
                        validatePeer(evt.peer);
                    }
                }
                else if (evt.packet.getID() == PacketID::AchievementGet)
                {
                    //re-broadcast to all clients
                    m_sharedData.host.broadcastPacket(PacketID::AchievementGet, evt.packet.as<std::array<std::uint8_t, 2u>>(), cro::NetFlag::Reliable);
                }
            }
        }

        checkPending();

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

            m_sharedData.host.broadcastPacket(PacketID::StateChange, std::uint8_t(nextState), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
            
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
            m_sharedData.host.sendPacket(peer, PacketID::ConnectionRefused, std::uint8_t(MessageType::VersionMismatch), cro::NetFlag::Reliable);
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

void Server::validatePeer(cro::NetPeer& peer)
{
    auto result = std::find_if(m_pendingConnections.begin(), m_pendingConnections.end(),
        [&peer](const PendingConnection& pc)
        {
            return pc.peer == peer;
        });

    if (result != m_pendingConnections.end())
    {
        if (auto i = addClient(peer); i >= /*ConstVal::MaxClients*/m_maxConnections)
        {
            //tell client server is full
            m_sharedData.host.sendPacket(peer, PacketID::ConnectionRefused, std::uint8_t(MessageType::ServerFull), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
            m_sharedData.host.disconnectLater(peer);
        }
        else
        {
            //tell the client which player they are
            m_sharedData.host.sendPacket(peer, PacketID::ConnectionAccepted, i, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }

        m_pendingConnections.erase(result);
    }
    else
    {
        //stray connection so boot it?
    }
}

std::uint8_t Server::addClient(const cro::NetPeer& peer)
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
            m_sharedData.host.broadcastPacket(PacketID::ClientConnected, i, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);

            auto* msg = m_sharedData.messageBus.post<ConnectionEvent>(sv::MessageID::ConnectionMessage);
            msg->clientID = i;
            msg->type = ConnectionEvent::Connected;

            break;
        }
    }

    return i;
}

void Server::removeClient(const cro::NetEvent& evt)
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
        *result = sv::ClientConnection(); //resets the data, setting 'connected' to false etc

        auto playerID = std::distance(m_sharedData.clients.begin(), result);
        auto* msg = m_sharedData.messageBus.post<ConnectionEvent>(sv::MessageID::ConnectionMessage);
        msg->clientID = static_cast<std::uint8_t>(playerID);
        msg->type = ConnectionEvent::Disconnected;

        //broadcast to all connected clients
        m_sharedData.host.broadcastPacket(PacketID::ClientDisconnected, static_cast<std::uint8_t>(playerID), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
        LOG("Client disconnected", cro::Logger::Type::Info);
    }
}