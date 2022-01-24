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
#include "../CommonConsts.hpp"
#include "../SharedStateData.hpp"
#include "../Utility.hpp"
#include "ServerLobbyState.hpp"
#include "ServerPacketData.hpp"
#include "ServerMessages.hpp"

#include <crogine/core/Log.hpp>
#include <crogine/network/NetData.hpp>

#include <cstring>

using namespace sv;

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

void LobbyState::handleMessage(const cro::Message& msg)
{
    if (msg.id == MessageID::ConnectionMessage)
    {
        const auto& data = msg.getData<ConnectionEvent>();
        if (data.type == ConnectionEvent::Disconnected)
        {
            m_readyState[data.clientID] = false;
        }
    }
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
        case PacketID::MapInfo:
        {
            if (evt.peer.getID() == m_sharedData.hostID)
            {
                m_sharedData.mapDir = deserialiseString(evt.packet);
                //forward to all clients
                m_sharedData.host.broadcastPacket(PacketID::MapInfo, evt.packet.getData(), evt.packet.getSize(), cro::NetFlag::Reliable, ConstVal::NetChannelStrings);
            }
        }
            break;
        case PacketID::RequestGameStart:
            if (evt.peer.getID() == m_sharedData.hostID)
            {
                m_returnValue = StateID::Game;
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
    //find the connection index
    std::uint8_t connectionID = 4;
    for(auto i = 0u; i < ConstVal::MaxClients; ++i)
    {
        if (m_sharedData.clients[i].peer == evt.peer)
        {
            connectionID = i;
            break;
        }
    }

    if (connectionID < ConstVal::MaxClients)
    {
        if (evt.packet.getSize() > 0)
        {
            ConnectionData cd;
            if (cd.deserialise(evt.packet))
            {
                m_sharedData.clients[connectionID].playerCount = cd.playerCount;
                for (auto i = 0u; i < cd.playerCount; ++i)
                {
                    m_sharedData.clients[connectionID].playerData[i].name = cd.playerData[i].name;
                    m_sharedData.clients[connectionID].playerData[i].avatarFlags = cd.playerData[i].avatarFlags;
                    m_sharedData.clients[connectionID].playerData[i].ballID = cd.playerData[i].ballID;
                    m_sharedData.clients[connectionID].playerData[i].hairID = cd.playerData[i].hairID;
                    m_sharedData.clients[connectionID].playerData[i].skinID = cd.playerData[i].skinID;
                    m_sharedData.clients[connectionID].playerData[i].flipped = cd.playerData[i].flipped;
                }
            }
            else
            {
                //reject the client
                m_sharedData.host.sendPacket(evt.peer, PacketID::ConnectionRefused, std::uint8_t(MessageType::BadData), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
                
                auto peer = evt.peer;
                m_sharedData.host.disconnectLater(peer);

                LogE << "Server - rejected client, unable to read player info packet" << std::endl;
            }
        }
    }

    //broadcast all player info (we need to send all clients to the new one, so might as well update everyone)
    for (auto i = 0u; i < ConstVal::MaxClients; ++i)
    {
        const auto& c = m_sharedData.clients[i];
        if (c.connected)
        {
            ConnectionData cd;
            cd.connectionID = static_cast<std::uint8_t>(i);
            cd.playerCount = static_cast<std::uint8_t>(c.playerCount);
            for (auto j = 0u; j < c.playerCount; ++j)
            {
                cd.playerData[j].name = c.playerData[j].name;
                cd.playerData[j].avatarFlags = c.playerData[j].avatarFlags;
                cd.playerData[j].ballID = c.playerData[j].ballID;
                cd.playerData[j].hairID = c.playerData[j].hairID;
                cd.playerData[j].skinID = c.playerData[j].skinID;
                cd.playerData[j].flipped = c.playerData[j].flipped;
            }
            auto buffer = cd.serialise();

            m_sharedData.host.broadcastPacket(PacketID::LobbyUpdate, buffer.data(), buffer.size(), cro::NetFlag::Reliable, ConstVal::NetChannelStrings);
        }

        std::uint8_t ready = m_readyState[i] ? 1 : 0;
        m_sharedData.host.broadcastPacket(PacketID::LobbyReady, std::uint16_t(std::uint8_t(i) << 8 | ready), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);

        auto mapDir = serialiseString(m_sharedData.mapDir);
        m_sharedData.host.broadcastPacket(PacketID::MapInfo, mapDir.data(), mapDir.size(), cro::NetFlag::Reliable, ConstVal::NetChannelStrings);
    }
}