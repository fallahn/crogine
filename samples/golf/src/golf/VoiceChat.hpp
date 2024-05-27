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

#pragma once

#include "GameConsts.hpp"

#include <crogine/audio/DynamicAudioStream.hpp>
#include <crogine/audio/sound_system/OpusEncoder.hpp>
#include <crogine/audio/sound_system/SoundRecorder.hpp>

#include <array>
#include <memory>

struct SharedStateData;
class VoiceChat final
{
public:
    explicit VoiceChat(SharedStateData&);

    void connect();
    void disconnect();
    void captureVoice();

    void process();

    //TODO raise a message ofzo when a new stream is created/destroyed
    //so that the active scene can act on it accordingly
    const cro::DynamicAudioStream* getStream(std::size_t idx) const;


private:
    SharedStateData& m_sharedData;

    cro::SoundRecorder m_recorder;
    std::vector<std::uint8_t> m_opusPacket;

    std::array<std::unique_ptr<cro::Opus>, ConstVal::MaxClients> m_decoders;
    std::array<std::unique_ptr<cro::DynamicAudioStream>, ConstVal::MaxClients> m_audioStreams;
    void addSource(std::size_t);
    void removeSource(std::size_t);


    static constexpr std::size_t PacketBufferSize = 10;
    using Packet = std::vector<std::uint8_t>;

    class PacketBuffer final
    {
    public:
        PacketBuffer()
            : m_front(0), m_back(0), m_nextFree(0), m_size(0), m_data()
        {

        }

        void pop_front()
        {
            CRO_ASSERT(m_size > 0, "Invalid buffer size");
            
            m_front = (m_front + 1) % m_data.size();
            m_size--;
        }

        //NOTE this *swaps* in the packet so the input will be MUTATED
        void push_back(Packet& value)
        {
            CRO_ASSERT(m_size < m_data.size(), "Buffer full");
            m_data[m_nextFree].swap(value);
            m_back = m_nextFree;
            m_nextFree = (m_back + 1) % m_data.size();

            m_size++;
        }

        Packet& front()
        {
            CRO_ASSERT(m_size > 0, "Invalid buffer size");
            return m_data[m_front];
        }

        const Packet& front() const
        {
            CRO_ASSERT(m_size > 0, "Invalid buffer size");
            return front();
        }

        Packet& back()
        {
            CRO_ASSERT(m_size > 0, "Invalid buffer size");
            return m_data[m_back];
        }

        const Packet& back() const
        {
            CRO_ASSERT(m_size > 0, "Invalid buffer size");
            return back();
        }

        std::size_t size() const
        {
            return m_size;
        }

        constexpr std::size_t capacity() const
        {
            return PacketBufferSize;
        }

    private:
        std::size_t m_front;
        std::size_t m_back;
        std::size_t m_nextFree;
        std::size_t m_size;
        std::array<Packet, PacketBufferSize> m_data;
    };

    std::array<PacketBuffer, ConstVal::MaxClients> m_packetBuffers;
};