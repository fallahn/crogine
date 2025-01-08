/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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

#include "../detail/enet/enet/enet.h"

#include <crogine/network/NetData.hpp>

using namespace cro;

NetEvent::Packet::Packet()
    : m_packet(nullptr),
    m_id(0)
{

}

NetEvent::Packet::~Packet()
{
    if (m_packet)
    {
        enet_packet_destroy(m_packet);
    }
}

NetEvent::Packet::Packet(Packet&& other) noexcept
    : m_packet(other.m_packet),
    m_id(other.m_id)
{
    other.m_packet = nullptr;
    other.m_id = 0;
}

NetEvent::Packet& NetEvent::Packet::operator=(NetEvent::Packet&& other) noexcept
{
    if (m_packet)
    {
        enet_packet_destroy(m_packet);
    }

    m_packet = other.m_packet;
    m_id = other.m_id;


    other.m_packet = nullptr;
    other.m_id = 0;

    return *this;
}

//public
std::uint8_t NetEvent::Packet::getID() const
{
    CRO_ASSERT(m_packet, "Not a valid packet instance");
    return m_id;
}

const void* NetEvent::Packet::getData() const
{
    CRO_ASSERT(m_packet, "Not a valid packet instance");
    return &m_packet->data[sizeof(std::uint8_t)];
}

std::size_t NetEvent::Packet::getSize() const
{
    CRO_ASSERT(m_packet, "Not a valid packet instance");
    return m_packet->dataLength - sizeof(std::uint8_t);
}

std::vector<std::byte> NetEvent::Packet::getDataRaw() const
{
    CRO_ASSERT(m_packet, "Not a valid packet instance");
    std::vector<std::byte> ret(m_packet->dataLength);
    
    const auto* data = reinterpret_cast<std::byte*>(m_packet->data);
    std::copy(data, data + m_packet->dataLength, ret.data());
    return ret;
}

//private
void NetEvent::Packet::setPacketData(ENetPacket* packet)
{
    if (m_packet)
    {
        enet_packet_destroy(m_packet);
    }

    m_packet = packet;

    if (m_packet)
    {
        std::memcpy(&m_id, m_packet->data, sizeof(std::uint8_t));
    }
}