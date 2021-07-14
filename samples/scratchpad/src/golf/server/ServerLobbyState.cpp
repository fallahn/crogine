/*-----------------------------------------------------------------------

Matt Marchant 2021
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
#include "../CommonConsts.hpp"
#include "ServerLobbyState.hpp"
#include "ServerPacketData.hpp"

#include <crogine/core/Log.hpp>
#include <crogine/network/NetData.hpp>

#include <cstring>

using namespace Sv;

LobbyState::LobbyState(SharedData& sd)
    : m_returnValue (StateID::Lobby),
    m_sharedData    (sd)
{
    LOG("Entered Server Lobby State", cro::Logger::Type::Info);

    //this is client readyness to receive map data
    for (auto& c : sd.clients)
    {
        c.ready = false;
    }

    //this is lobby readiness
    for (auto& b : m_readyState)
    {
        b = false;
    }
}

void LobbyState::handleMessage(const cro::Message&)
{

}

void LobbyState::netEvent(const cro::NetEvent& evt)
{
    if (evt.type == cro::NetEvent::PacketReceived)
    {
        switch (evt.packet.getID())
        {
        default:break;
        case PacketID::PlayerInfo:
            insertPlayerInfo(evt);
            break;
        case PacketID::LobbyReady:
        {
            std::uint16_t data = evt.packet.as<std::uint16_t>();
            m_readyState[((data & 0xff00) >> 8)] = (data & 0x00ff) ? true : false;
            m_sharedData.host.broadcastPacket(PacketID::LobbyReady, data, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
            break;
        }
    }
}

std::int32_t LobbyState::process(float dt)
{
    return m_returnValue;
}

//private
void LobbyState::insertPlayerInfo(const cro::NetEvent& evt)
{
    std::uint8_t playerID = 4;
    for(auto i = 0u; i < ConstVal::MaxClients; ++i)
    {
        if (m_sharedData.clients[i].peer == evt.peer)
        {
            playerID = i;
            break;
        }
    }

    if (playerID < ConstVal::MaxClients)
    {
        if (evt.packet.getSize() > 0)
        {
            std::uint8_t size = static_cast<const std::uint8_t*>(evt.packet.getData())[0];
            size = std::min(size, static_cast<std::uint8_t>(ConstVal::MaxStringDataSize));

            if (size % sizeof(std::uint32_t) == 0)
            {
                std::vector<std::uint32_t> buffer(size / sizeof(std::uint32_t));
                std::memcpy(buffer.data(), static_cast<const std::uint8_t*>(evt.packet.getData()) + 1, size);

                m_sharedData.clients[playerID].name = cro::String::fromUtf32(buffer.begin(), buffer.end());
            }
        }
    }

    //broadcast all player info (we need to send all clients to the new one, so might as well update everyone)
    for (auto i = 0u; i < ConstVal::MaxClients; ++i)
    {
        const auto& c = m_sharedData.clients[i];
        if (c.connected)
        {
            LobbyData data;
            data.playerID = static_cast<std::uint8_t>(i);
            data.skinFlags = 0;
            data.stringSize = static_cast<std::uint8_t>(c.name.size() * sizeof(std::uint32_t)); //we're not checking valid size here because we assume it was validated on arrival

            std::vector<std::uint8_t> buffer(data.stringSize + sizeof(LobbyData));
            std::memcpy(buffer.data(), &data, sizeof(data));
            std::memcpy(buffer.data() + sizeof(data), c.name.data(), data.stringSize);

            m_sharedData.host.broadcastPacket(PacketID::LobbyUpdate, buffer.data(), buffer.size(), cro::NetFlag::Reliable, ConstVal::NetChannelStrings);
        }

        std::uint8_t ready = m_readyState[i] ? 1 : 0;
        m_sharedData.host.broadcastPacket(PacketID::LobbyReady, std::uint16_t(std::uint8_t(i) << 8 | ready), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
    }
}