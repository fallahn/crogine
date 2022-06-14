/*-----------------------------------------------------------------------

Matt Marchant 2022
http://trederia.blogspot.com

crogine - Zlib license.

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

#include "NetConf.hpp"
#include <gns/NetHost.hpp>
#include <steam/isteamnetworkingutils.h>

#include <iostream>
#include <sstream>
#include <vector>
#include <cassert>

using namespace gns;

namespace
{
    //horrible static hackery.
#ifdef GNS_OS
    NetHost* activeInstance = nullptr;
#endif
}

NetHost::NetHost()
{
    if (!NetConf::instance)
    {
        NetConf::instance = std::make_unique<NetConf>();
    }
}

NetHost::~NetHost()
{
    stop();
}

//public
bool NetHost::start(const std::string& address, std::uint16_t port, std::size_t maxClient, std::size_t maxChannels, std::uint32_t incoming, std::uint32_t outgoing)
{
    if (m_host)
    {
        std::cerr << "Host is already running" << std::endl;
        return false;
    }

    if (!NetConf::instance->m_initOK)
    {
        std::cerr << "ERROR Unable to initialise network module" << std::endl;
        return false;
    }

    assert(port > 0);

    m_maxClients = std::max(std::size_t(1), maxClient);
    maxChannels = std::max(std::size_t(1), maxChannels);

    //ISockets()->ConfigureConnectionLanes();


    SteamNetworkingIPAddr addr;
    addr.Clear();
    addr.m_port = port;
    
    if (!address.empty())
    {
        std::stringstream ss(address);
        std::string token;
        std::vector<std::string> output;
        while (std::getline(ss, token, '.'))
        {
            if (!token.empty())
            {
                output.push_back(token);
            }
        }

        for (auto i = 0u; i < 4 && i < output.size(); ++i)
        {
            try
            {
                addr.m_ipv4.m_ip[i] = std::atoi(output[i].c_str());
            }
            catch (...) { std::cerr << output[i] << ": invalid IP address fragment.\n"; }
        }
    }

#ifdef GNS_OS
    SteamNetworkingConfigValue_t opt;
    opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)onSteamNetConnectionStatusChanged);
    m_host = ISockets()->CreateListenSocketIP(addr, 1, &opt);
#else
    //steam api uses a macro to register callbacks
    m_host = ISockets()->CreateListenSocketIP(addr, 0, nullptr);
#endif

    if (m_host)
    {
        m_pollGroup = ISockets()->CreatePollGroup();
    }


    return m_host != 0;
}

void NetHost::stop()
{
    if (m_host)
    {
        //for each peer disconnectLater()
        for (auto& p : m_peers)
        {
            ISockets()->FlushMessagesOnConnection(p.m_peer);
            disconnectLater(p);
        }
        m_peers.clear();


        ISockets()->DestroyPollGroup(m_pollGroup);
        m_pollGroup = 0;

        ISockets()->CloseListenSocket(m_host);
        m_host = 0;
    }
}

bool NetHost::pollEvent(NetEvent& dst)
{
    if (!m_host) return false;


    //if we're empty then check for message (and optionally callbacks)
    if (m_events.empty())
    {
#ifdef GNS_OS
        //run callbacks and push them into queue
        //this is done by steam API if not using the open source lib

        //HMMMMM if we're running a local loopback connection RunCallbacks
        //will get called multiple times...

        //ugh - we have to manually register callbacks in the OS version
        activeInstance = this;
        ISockets()->RunCallbacks();
#endif

        //check messages
        ISteamNetworkingMessage* msgs = nullptr;
        auto msgCount = ISockets()->ReceiveMessagesOnPollGroup(m_pollGroup, &msgs, 5);
        for (auto i = 0; i < msgCount; ++i)
        {
            m_events.push(NetEvent());
            auto& evt = m_events.back();
            evt.type = NetEvent::PacketReceived;
            evt.packet.m_data.resize(msgs[i].m_cbSize);
            std::memcpy(evt.packet.m_data.data(), msgs[i].GetData(), msgs[i].m_cbSize);
            evt.sender.m_peer = msgs[i].m_conn;

            msgs[i].Release();
        }
    }

    //parse queue
    if (!m_events.empty())
    {
        dst = m_events.front();
        m_events.pop();
    }
    return !m_events.empty();
}

void NetHost::broadcastPacket(std::uint8_t id, const void* data, std::size_t size, NetFlag flags, std::uint8_t channel)
{
    //send to all peers
    for (const auto& p : m_peers)
    {
        sendPacket(p, id, data, size, flags, channel);
    }
}

void NetHost::sendPacket(const NetPeer& peer, std::uint8_t id, const void* data, std::size_t size, NetFlag flags, std::uint8_t channel)
{
    //TODO use send messages instead?
    //auto* msg = SteamNetworkingUtils()->AllocateMessage(0);
    
    std::vector<std::uint8_t> buff(size + 1);
    buff[0] = id;
    std::memcpy(&buff[1], data, size);
    ISockets()->SendMessageToConnection(peer.m_peer, buff.data(), static_cast<std::uint32_t>(buff.size()), static_cast<std::int32_t>(flags), nullptr);
}

void NetHost::disconnect(NetPeer& peer)
{
    if (m_host && peer.m_peer)
    {
        ISockets()->CloseConnection(peer.m_peer, 0, nullptr, false);
        peer.m_peer = 0;

        //removal from the list is done by callback
        //as is removing from the poll group
    }
}

void NetHost::disconnectLater(NetPeer& peer)
{
    if (m_host && peer.m_peer)
    {        
        ISockets()->CloseConnection(peer.m_peer, 0, nullptr, true);
        peer.m_peer = 0;
    }
}

//private
#ifdef GNS_OS
void NetHost::onSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t * cb)
{
    assert(activeInstance);
    activeInstance->onConnectionStatusChanged(cb);
}
#endif


void NetHost::onConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* cb)
{
    switch (cb->m_info.m_eState)
    {
    default:
        break;
    case k_ESteamNetworkingConnectionState_None:
        //this will be raised when destroying connections
        LOG("Disconnected peer from host");
        [[fallthrough]];
    case k_ESteamNetworkingConnectionState_ClosedByPeer:
    case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
        //make sure they were actually connected first
        if (cb->m_eOldState == k_ESteamNetworkingConnectionState_Connected)
        {
            //remove disconnects from peer list / poll group
            m_peers.erase(std::remove_if(m_peers.begin(), m_peers.end(), 
                [cb](const NetPeer& p)
                {
                    return p.m_peer == cb->m_hConn;
                }), m_peers.end());

            ISockets()->SetConnectionPollGroup(cb->m_hConn, 0);
            ISockets()->CloseConnection(cb->m_hConn, 0, nullptr, false);

            //and push back a net event
            m_events.push(NetEvent());
            m_events.back().type = NetEvent::ClientDisconnect;
            m_events.back().sender.m_peer = cb->m_hConn;
        }
        break;
    case k_ESteamNetworkingConnectionState_Connecting:
    {
        //we actually handle connections here rather than 'connected' events
        //as we have to explicitly accept incoming connections
        if (m_peers.size() < m_maxClients)
        {
            if (ISockets()->AcceptConnection(cb->m_hConn) != k_EResultOK)
            {
                //client gave up connecting before we could accept
                ISockets()->CloseConnection(cb->m_hConn, 0, nullptr, false);
                break;
            }

            if (!ISockets()->SetConnectionPollGroup(cb->m_hConn, m_pollGroup))
            {
                ISockets()->CloseConnection(cb->m_hConn, 0, nullptr, false);
                break;
            }
            m_peers.emplace_back().m_peer = cb->m_hConn;

            m_events.push(NetEvent());
            m_events.back().type = NetEvent::ClientConnect;
            m_events.back().sender.m_peer = cb->m_hConn;
        }
        else
        {
            //TODO we probably need some kind of rejection message
            //see k_ESteamNetConnectionEnd
            ISockets()->CloseConnection(cb->m_hConn, 0, nullptr, false);
        }
    }
        break;
    case k_ESteamNetworkingConnectionState_Connected:
        //this happens when we accept a connection - so we should have already
        //dealt with anything we need to do.
        break;
    }
}