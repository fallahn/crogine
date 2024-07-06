/*-----------------------------------------------------------------------

Matt Marchant 2024
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
#include "ServerVoice.hpp"

bool VoiceHost::start(std::uint16_t port)
{
    return m_host.start("", port, ConstVal::MaxClients, 1);
}

void VoiceHost::stop()
{
    for (auto& p : m_peers)
    {
        if (p)
        {
            m_host.disconnectLater(p);
        }
    }

    m_host.stop();
}

bool VoiceHost::addLocalConnection(net::NetClient& c)
{
#ifdef USE_GNS
    return m_host.addLocalConnection(c);
#else
    return false;
#endif
}

void VoiceHost::update()
{
    net::NetEvent evt;
    while (m_host.pollEvent(evt))
    {
        if (evt.type == net::NetEvent::ClientConnect)
        {
            //validate somehow? Or just assume this is OK
            //as the client should only be attempting to connect
            //if it has already connected to the main server

            std::uint8_t vID = 0;
            for (auto i = 0u; i < m_peers.size(); ++i)
            {
                if (!m_peers[i])
                {
                    vID = i;
                    break;
                }
            }

            if (vID < ConstVal::MaxClients)
            {
                m_peers[vID] = evt.peer;

                //at the very least we need to reply to the client with
                //their voice channel id
                m_host.sendPacket(evt.peer, VoicePacket::ClientConnect, vID, net::NetFlag::Reliable);
            }
            else
            {
                m_host.sendPacket(evt.peer, PacketID::ConnectionRefused, std::uint8_t(MessageType::ServerFull), net::NetFlag::Reliable);
                m_host.disconnectLater(evt.peer);
            }
        }
        else if (evt.type == net::NetEvent::ClientDisconnect)
        {
            auto result = std::find(m_peers.begin(), m_peers.end(), evt.peer);
            
            if (result != m_peers.end())
            {
                auto vID = std::distance(m_peers.begin(), result);
                m_peers[vID] = net::NetPeer();

                //let remaining connections know so that they can reset their voice decoder
                for (const auto& peer : m_peers)
                {
                    if (peer)
                    {
                        m_host.sendPacket(peer, VoicePacket::ClientDisconnect, std::uint8_t(vID), net::NetFlag::Reliable);
                    }
                }
            }
        }
        else
        {
            //broadcast to all connections except the sender
            auto sender = evt.packet.getID();

            if (sender < ConstVal::MaxClients)
            {
                for (auto i = 0u; i < m_peers.size(); ++i)
                {
                    if (i != sender && m_peers[i])
                    {
                        m_host.sendPacket(m_peers[i], sender, evt.packet.getData(), evt.packet.getSize(), net::NetFlag::Unreliable);
                    }
                }
            }
        }
    }
}