/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2025
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

#include <crogine/Config.hpp>
#include <crogine/detail/Assert.hpp>
#include <crogine/detail/Types.hpp>

#include <cstring>
#include <string>

struct _ENetPacket;
struct _ENetPeer;

namespace cro
{
    /*!
    \brief A peer represents a single connection made up of multiple
    channels between a client and a host.
    */
    struct CRO_EXPORT_API NetPeer final
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

        operator bool() const { return m_peer != nullptr; }

    private:
        _ENetPeer* m_peer = nullptr;

        friend class NetClient;
        friend class NetHost;
    };

    /*!
    \brief Network event.
    These are used to poll NetHost and NetClient objects
    for network activity
    */
    struct CRO_EXPORT_API NetEvent final
    {
        std::uint8_t channel = 0; //! <channel event was received on
        enum
        {
            None,
            ClientConnect,
            ClientDisconnect,
            PacketReceived
        }type = None;

        /*!
        \brief Event packet.
        Contains packet data received by PacketRecieved event.
        Not valid for other event types.
        */
        struct CRO_EXPORT_API Packet final
        {
            Packet();
            ~Packet();

            Packet(const Packet&) = delete;
            Packet(Packet&&) noexcept;
            Packet& operator = (const Packet&) = delete;
            Packet& operator = (Packet&&) noexcept;

            /*!
            \brief The unique ID this packet was tagged with when sent
            */
            std::uint8_t getID() const;

            /*!
            \brief Used to retrieve the data as a specific type.
            Trying to read data as an incorrect type will lead to
            undefined behaviour.
            */
            template <typename T>
            T as() const;

            /*!
            \brief Returns a pointer to the raw packet data, without the ID
            */
            const void* getData() const;
            /*!
            \brief Returns the size of the data, in bytes
            */
            std::size_t getSize() const;

            /*!
            \brief returns the entire packet as raw bytes, including the ID at[0]
            */
            std::vector<std::byte> getDataRaw() const;

        private:
            _ENetPacket* m_packet;
            std::uint8_t m_id;
            void setPacketData(_ENetPacket*);

            friend class NetClient;
            friend class NetHost;
        }packet;

        /*!
        \brief Contains the peer from which this event was transmitted
        */
        NetPeer peer;
    };

#include "NetData.inl"

    /*!
    \brief Reliability enum.
    These are used to flag sent packets with a requested reliability.
    */
    enum class NetFlag
    {
        Reliable = 0x1, //! <packet must be received by the remote connection, and resend attempts are made until delivered
        Unsequenced = 0x2, //! <packet will not be sequenced with other packets. Not supported on reliable packets
        Unreliable = 0x4 //! <packet will be fragments and sent unreliably if it exceeds MTU
    };
}