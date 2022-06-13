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

#include <gns/NetData.hpp>

#include <sstream>

using namespace gns;

std::string NetPeer::getAddress() const
{
    if (m_peer)
    {
        SteamNetConnectionInfo_t info;
        if (ISockets()->GetConnectionInfo(m_peer, &info))
        {
            const auto* ip = info.m_addrRemote.m_ipv4.m_ip;

            std::stringstream ss;
            ss << ip[0] << "." << ip[1] << "." << ip[2] << "." << ip[3] << std::endl;
            return ss.str();
        }
    }
    return "";
}

std::uint16_t NetPeer::getPort() const
{
    if (m_peer)
    {
        SteamNetConnectionInfo_t info;
        if (ISockets()->GetConnectionInfo(m_peer, &info))
        {
            return info.m_addrRemote.m_port;
        }
    }
    return 0;
}

std::uint32_t NetPeer::getID() const
{
    return m_peer;
}

std::uint32_t NetPeer::getRoundTripTime() const
{
    if (m_peer)
    {
        SteamNetConnectionRealTimeStatus_t info;
        if (ISockets()->GetConnectionRealTimeStatus(m_peer, &info, 0, nullptr))
        {
            return info.m_nPing;
        }
    }
    return 0;
}

NetPeer::State NetPeer::getState() const
{
    if (m_peer)
    {
        SteamNetConnectionRealTimeStatus_t info;
        if (ISockets()->GetConnectionRealTimeStatus(m_peer, &info, 0, nullptr))
        {
            switch (info.m_eState)
            {
            default: 
            case ESteamNetworkingConnectionState::k_ESteamNetworkingConnectionState_Dead:
                return State::Zombie;

            case ESteamNetworkingConnectionState::k_ESteamNetworkingConnectionState_None:
            case ESteamNetworkingConnectionState::k_ESteamNetworkingConnectionState_ClosedByPeer:
            case ESteamNetworkingConnectionState::k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
                return State::Disconnected;

            case ESteamNetworkingConnectionState::k_ESteamNetworkingConnectionState_Connecting:
                return State::Connecting;

            case ESteamNetworkingConnectionState::k_ESteamNetworkingConnectionState_Connected:
                return State::Connected;

            case ESteamNetworkingConnectionState::k_ESteamNetworkingConnectionState_FindingRoute:
                return State::AcknowledingConnect;

            case ESteamNetworkingConnectionState::k_ESteamNetworkingConnectionState_FinWait:
            case ESteamNetworkingConnectionState::k_ESteamNetworkingConnectionState_Linger:
                return State::DisconnectLater;
            }
        }
    }

    return State::Disconnected;
}