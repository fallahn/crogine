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

#include <gns/NetClient.hpp>

#include <iostream>

using namespace gns;

namespace
{
#ifdef GNS_OS
    NetClient* activeInstance = nullptr;
#endif
}

NetClient::NetClient()
{
    if (!NetConf::instance)
    {
        NetConf::instance = std::make_unique<NetConf>();
    }
}

NetClient::~NetClient()
{
    disconnect();
}

//public
bool NetClient::create(std::size_t, std::size_t, std::uint32_t, std::uint32_t)
{
    return true;
}

bool NetClient::connect(const std::string& address, std::uint16_t port, std::uint32_t)
{
    port = std::max(std::uint16_t(1), port);

    SteamNetworkingIPAddr addr;
    addr.Clear();
    if (!address.empty())
    {
        addr.ParseString(address.c_str());
    }
    addr.m_port = port;

#ifdef GNS_OS
    activeInstance = this;

    SteamNetworkingConfigValue_t opt;
    opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)onSteamNetConnectionStatusChanged);
    m_peer.m_peer = ISockets()->ConnectByIPAddress(addr, 1, &opt);

#else
    //steam api uses a macro to register callbacks
    //m_peer.m_peer = ISockets()->ConnectByIPAddress(addr, 0, nullptr);
    
    SteamNetworkingIdentity id;
    id.SetIPAddr(addr);
    m_peer.m_peer = ISockets()->ConnectP2P(id, 0, 0, nullptr);
#endif
    DLOG("Got peer ID " << m_peer.m_peer);
    return m_peer.m_peer != 0;
}

void NetClient::disconnect()
{
    if (m_peer.m_peer)
    {
        ISockets()->CloseConnection(m_peer.m_peer, 0, nullptr, true);
        m_peer = {};
    }
}

bool NetClient::pollEvent(NetEvent& dst)
{
    if (m_peer.m_peer == 0)
    {
        return false;
    }

    if (m_events.empty())
    {
        //check messages and run callbacks if necessary
        ISteamNetworkingMessage* msgs = nullptr;
        auto msgCount = ISockets()->ReceiveMessagesOnConnection(m_peer.m_peer, &msgs, 1);

        if (msgs)
        {
            {
                m_events.push(NetEvent());
                auto& evt = m_events.back();
                evt.type = NetEvent::PacketReceived;
                evt.packet.m_data.resize(msgs->m_cbSize);
                std::memcpy(evt.packet.m_data.data(), msgs->GetData(), msgs->m_cbSize);
                evt.peer.m_peer = msgs->m_conn;
            }

            msgs->Release();
        }

#ifdef GNS_OS
        //TODO this is no good if we're running a host AND client
        //on the same machine from the same networking instance
        //as it gets called twice FROM DIFFERENT THREADS

        ISockets()->RunCallbacks();
#endif
    }

    if (!m_events.empty())
    {
        dst = std::move(m_events.front());
        m_events.pop();
        return true;
    }

    return false;
}

void NetClient::sendPacket(std::uint8_t id, const void* data, std::size_t size, NetFlag flags, std::uint8_t)
{
    m_packetBuffer.resize(size + 1);
    m_packetBuffer[0] = id;
    std::memcpy(&m_packetBuffer[1], data, size);
    ISockets()->SendMessageToConnection(m_peer.m_peer, m_packetBuffer.data(), static_cast<std::uint32_t>(m_packetBuffer.size()), static_cast<std::int32_t>(flags), nullptr);
}

//private
#ifdef GNS_OS
void NetClient::onSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* cb)
{
    assert(activeInstance != nullptr);
    activeInstance->onConnectionStatusChanged(cb);
}
#endif

void NetClient::onConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* cb)
{
    if (cb->m_hConn == m_peer.m_peer)
    {
        switch (cb->m_info.m_eState)
        {
        default:
        case k_ESteamNetworkingConnectionState_None:
            //this will happen when we disconnect
            DLOG("Client disconnected");
            break;
        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
            //TODO check the reason this was disconnected
            ISockets()->CloseConnection(m_peer.m_peer, 0, nullptr, false);
            m_peer = {};
            DLOG("Client connection failure, reason: " << cb->m_info.m_eState);

            {
                m_events.push(NetEvent());
                m_events.back().type = NetEvent::ClientDisconnect;
                m_events.back().peer.m_peer = cb->m_hConn;
            }

            break;
        case k_ESteamNetworkingConnectionState_Connecting:
            DLOG("Connecting...");
            break;
        case k_ESteamNetworkingConnectionState_Connected:
            DLOG("Connected.");
            {
                m_events.push(NetEvent());
                m_events.back().type = NetEvent::ClientConnect;
                m_events.back().peer.m_peer = cb->m_hConn;
            }
            break;
        }
    }
}