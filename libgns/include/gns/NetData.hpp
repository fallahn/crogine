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

#pragma once

#include <gns/Config.hpp>

#include <string>
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <vector>

namespace gns
{
    struct GNS_EXPORT_API NetPeer final
    {
        std::string getAddress() const; //! <String containing the IPv4 address
        std::uint16_t getPort() const; //! <Port number
        std::uint64_t getID() const; //! <Unique ID
        std::uint32_t getRoundTripTime() const; //! <Mean round trip time in milliseconds of a reliable packet

        enum class State
        {
            Disconnected,
            Connecting,
            AcknowledingConnect,
            PendingConnect,
            Succeeded,
            Connected,
            DisconnectLater,
            Disconnecting,
            AcknowledingDisconnect,
            Zombie
        };
        State getState() const; //! <Current state of the peer

        bool operator == (const NetPeer& other) const { return (&other == this || other.m_peer == m_peer); }
        bool operator != (const NetPeer& other) const { return !(other == *this); }

        operator bool() const { return m_peer != 0; }

    private:
        HSteamNetConnection m_peer = 0;

        friend class NetHost;
        friend class NetClient;
    };

    struct GNS_EXPORT_API NetEvent final
    {
        std::uint8_t channel = 0;
        enum
        {
            None,
            ClientConnect,
            ClientDisconnect,
            PacketReceived
        }type = None;

        struct GNS_EXPORT_API Packet final
        {
            std::uint8_t getID() const
            {
                if (!m_data.empty())
                {
                    return std::to_integer<std::uint8_t>(m_data[0]);
                }
                return 0;
            }

            template <typename T>
            T as() const
            {
                assert(sizeof(T) == getSize());

                //TODO remove this copy
                T returnData;
                std::memcpy(&returnData, getData(), getSize());

                return returnData;
                //return reinterpret_cast<const T&>(&m_data[1]);
            }

            const void* getData() const
            {
                assert(m_data.size() > 1);
                return m_data.empty() ? nullptr : &m_data[1];
            }

            std::size_t getSize() const
            {
                return m_data.empty() ? 0 : m_data.size() - 1;
            }

        private:
            std::vector<std::byte> m_data;

            friend class NetHost;
            friend class NetClient;
        }packet;

        NetPeer peer;
    };

    enum class NetFlag
    {
        Reliable = k_nSteamNetworkingSend_Reliable,
        Unreliable = k_nSteamNetworkingSend_Unreliable,
        Unsequenced = k_nSteamNetworkingSend_NoNagle
    };
}