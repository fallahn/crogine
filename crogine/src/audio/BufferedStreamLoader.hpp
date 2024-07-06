/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#include "AudioFile.hpp"

#include <mutex>

namespace cro::Detail
{
    /*!
    \brief emulates an audio file but expects buffered
    data to be streamed from elsewhere. Streams may have 1 or 2
    channels and are always 16 bit.
    */
    class BufferedStreamLoader final : public AudioFile
    {
    public:

        explicit BufferedStreamLoader(std::uint32_t channelCount, std::uint32_t sampleRate);

        ~BufferedStreamLoader();
        BufferedStreamLoader(const BufferedStreamLoader&) = delete;
        BufferedStreamLoader(BufferedStreamLoader&&) noexcept = delete;
        const BufferedStreamLoader& operator = (const BufferedStreamLoader&) = delete;
        BufferedStreamLoader& operator = (BufferedStreamLoader&&) noexcept = delete;

        bool open(const std::string&) override;

        const PCMData& getData(std::size_t = 0, bool looped = false) const override;

        void updateBuffer(const int16_t*, std::int32_t);

        bool seek(cro::Time) override { return false; }

        PCMData::Format getFormat() const override { return m_dataChunk.format; }

        std::int32_t getSampleRate() const override { return m_dataChunk.frequency; }

        std::uint64_t getSampleCount() const override { return m_sampleCount; }

    private:

        std::int32_t m_channelCount;
        std::uint64_t m_sampleCount;

        mutable PCMData m_dataChunk;
        mutable std::vector<std::int16_t> m_buffer;
        mutable std::vector<std::int16_t> m_doubleBuffer;

        mutable std::mutex m_mutex;
    };
}