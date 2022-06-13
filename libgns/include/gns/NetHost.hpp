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
#include <gns/NetData.hpp>

#include <string>
#include <cstdint>

namespace gns
{
    class GNS_EXPORT_API NetHost final
    {
    public:
        NetHost();
        ~NetHost();

        NetHost(const NetHost&) = delete;
        NetHost& operator = (const NetHost&) = delete;
        NetHost(NetHost&&) = delete;
        NetHost& operator = (NetHost&&) = delete;

        /*TODO maxChannels, incoming and outgoing do nothing in this implementation*/
        bool start(const std::string& address, std::uint16_t port, std::size_t maxClient, std::size_t maxChannels, std::uint32_t incoming = 0, std::uint32_t outgoing = 0);

        void stop();

        bool pollEvent(NetEvent&);

        template <typename T>
        void broadcastPacket(std::uint8_t id, const T& data, NetFlag flags, std::uint8_t channel = 0);

        void broadcastPacket(std::uint8_t id, const void* data, std::size_t size, NetFlag flags, std::uint8_t channel = 0);

        template <typename T>
        void sendPacket(const NetPeer& peer, std::uint8_t id, const T& data, NetFlag flags, std::uint8_t channel = 0);

        void sendPacket(const NetPeer& peer, std::uint8_t id, const void* data, std::size_t size, NetFlag flags, std::uint8_t channel = 0);

        void disconnect(NetPeer&);

        void disconnectLater(NetPeer&);

    private:
        HSteamListenSocket m_host = 0;
        std::size_t m_maxClients = 0;
    };

#include "NetHost.inl"
}