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

#include <cstdint>
#include <queue>
#include <vector>

/*
Note that in the open source version each client connection will be calling RunCallbacks() when
the event queue is polled. This is fine for a single connection (the general use case) but
multiple connections will call RunCallbacks() multiple times per frame. Although this is not
the case when using steamworks, note that in all cases pollEvent *doesn't* return events specific
to one connection, rather it returns all events. Again not a problem with a single connection
but if multiple connections really are needed then perhaps pollEvent() should be made static and
used only once per frame.
*/

namespace gns
{
    class GNS_EXPORT_API NetClient final
    {
    public:
        NetClient();
        ~NetClient();

        NetClient(const NetClient&) = delete;
        NetClient(NetClient&&) = delete;
        NetClient& operator = (const NetClient&) = delete;
        NetClient& operator = (NetClient&&) = delete;

        //note that none of these params have any effect, they're there for compatibility
        bool create(std::size_t maxChannels = 1, std::size_t maxClients = 1, std::uint32_t incoming = 0, std::uint32_t outgoing = 0);
        bool connect(const std::string& address, std::uint16_t port, std::uint32_t timeout = 5000);
        void disconnect();
        bool pollEvent(NetEvent&);

        template <typename T>
        void sendPacket(std::uint8_t id, const T& data, NetFlag flags, std::uint8_t channel = 0)
        {
            sendPacket(id, (void*)&data, sizeof(T), flags, channel);
        }

        void sendPacket(std::uint8_t id, const void* data, std::size_t size, NetFlag flags, std::uint8_t channel = 0);

        const NetPeer& getPeer() const { return m_peer; }

    private:
        NetPeer m_peer;
        std::queue<NetEvent> m_events;
        std::vector<std::uint8_t> m_packetBuffer;

#ifdef GNS_OS
        static void onSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t*);
        void onConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t*);
#else
        STEAM_CALLBACK(NetHost, onSteamNetConnectionStatusChanged, SteamNetConnectionStatusChangedCallback_t);
#endif
    };
}