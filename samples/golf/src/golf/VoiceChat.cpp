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

#include "VoiceChat.hpp"
#include "PacketIDs.hpp"
#include "SharedStateData.hpp"

#include <crogine/core/Console.hpp>

#include <cstring>

namespace
{
    constexpr std::int32_t ChannelCount = 1;
    constexpr std::int32_t SampleRate = 24000;
}

VoiceChat::VoiceChat(SharedStateData& sd)
    : m_sharedData(sd)
{

}

//public
void VoiceChat::connect()
{
    if (m_recorder.openDevice("", ChannelCount, SampleRate, true))
    {
        cro::Console::doCommand("connect_voice");
    }
    else
    {
        LogI << "Did not connect to voice channel, no recording device available" << std::endl;
    }
}

void VoiceChat::disconnect()
{
    if (m_sharedData.voiceConnection.connectionID != ConstVal::NullValue)
    {
        for (auto i = 0u; i < ConstVal::MaxClients; ++i)
        {
            removeSource(i);
        }

        cro::Console::doCommand("disconnect_voice");
        m_recorder.closeDevice();
    }
}

void VoiceChat::captureVoice()
{
    if (m_sharedData.voiceConnection.connectionID != ConstVal::NullValue)
    {
        m_recorder.getEncodedPacket(m_opusPacket);
        if (!m_opusPacket.empty())
        {
            m_sharedData.voiceConnection.netClient.sendPacket(m_sharedData.voiceConnection.connectionID, m_opusPacket.data(), m_opusPacket.size(), net::NetFlag::Unreliable);
        }
    }
}

void VoiceChat::process()
{
    if (m_sharedData.voiceConnection.connected)
    {
        net::NetEvent evt;
        while (m_sharedData.voiceConnection.netClient.pollEvent(evt))
        {
            switch (evt.type)
            {
            default: break;
            case net::NetEvent::PacketReceived:
            {
                auto id = evt.packet.getID();
                switch (id)
                {
                default: break;
                case VoicePacket::Client00:
                case VoicePacket::Client01:
                case VoicePacket::Client02:
                case VoicePacket::Client03:
                case VoicePacket::Client04:
                case VoicePacket::Client05:
                case VoicePacket::Client06:
                case VoicePacket::Client07:
                    //queue the incoming voice data
                {
                    if (m_packetBuffers[id].size() <
                        m_packetBuffers[id].capacity())
                    {
                        std::vector<std::uint8_t> t(evt.packet.getSize());
                        std::memcpy(t.data(), evt.packet.getData(), t.size());
                        m_packetBuffers[id].push_back(t);
                    }
                }
                    break;
                case VoicePacket::ClientConnect:
                    m_sharedData.voiceConnection.connectionID = evt.packet.as<std::uint8_t>();
                    LogI << "Got voice ID of " << (int)evt.packet.as<std::uint8_t>() << std::endl;
                    break;
                case VoicePacket::ClientDisconnect:
                {
                    auto cID = evt.packet.as<std::uint8_t>();
                    removeSource(cID);

                    LogI << "Client " << (int)cID << " left the voice chat" << std::endl;
                }
                    break;
                }
            }
                break;
            }
        }
    }

    //for each buffer of packets
    for (auto i = 0u; i < m_audioStreams.size(); ++i)
    {
        auto& buff = m_packetBuffers[i];
        const auto buffCount = buff.capacity() / 2;
        if (buff.size() > buffCount)
        {
            //if this is a new connection create the stream for it
            if (!m_audioStreams[i])
            {
                addSource(i);
            }

            for (auto j = 0u; j < buffCount; ++j)
            {
                auto data = m_decoders[i]->decode(buff.front());
                m_audioStreams[i]->updateBuffer(data.data(), data.size());

                buff.pop_front();
            }
        }
    }
}

const cro::DynamicAudioStream* VoiceChat::getStream(std::size_t idx) const
{
    CRO_ASSERT(m_audioStreams[idx], "Null audio stream");
    return m_audioStreams[idx].get();
}

//private
void VoiceChat::addSource(std::size_t idx)
{
    if (idx < ConstVal::MaxClients)
    {
        cro::Opus::Context ctx;
        ctx.channelCount = ChannelCount;
        ctx.sampleRate = SampleRate;

        m_decoders[idx] = std::make_unique<cro::Opus>(ctx);
        m_audioStreams[idx] = std::make_unique<cro::DynamicAudioStream>(ChannelCount, SampleRate);

        if (m_creationCallback)
        {
            m_creationCallback(*this, idx);
        }
    }
}

void VoiceChat::removeSource(std::size_t idx)
{
    if (idx < ConstVal::MaxClients)
    {
        if (m_deletionCallback)
        {
            m_deletionCallback(idx);
        }

        m_decoders[idx].reset();
        m_audioStreams[idx].reset(); //HMMMM we need to ensure that any ent using this is destroyed first
        m_packetBuffers[idx] = {};
    }
}