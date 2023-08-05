/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2023
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

#include <string>
#include <array>
#include <vector>

namespace cro
{
    namespace Detail
    {
        /*!
        \brief Opens a wav file from the given path and allows reading
        in chunks.
        Note this only supports uncompressed LPCM wav files.
        */
        class WavLoader final : public AudioFile
        {
        public:
            WavLoader();
            
            /*!
            \brief Attempts to open the file at the given path
            \returns true if file was opened, else false.
            */
            bool open(const std::string&) override;

            /*!
            \brief Returns a PCMData struct containing the
            information pertinent to the requested data size.
            The struct is null/empty if no file is opened.
            \param chunkSize Number of bytes of the file to read.
            The amount may be smaller if the file has reached the end,
            so the actual size read is stored in the PCMData struct.
            Passing 0 (default) will attempt to read the entire file.
            The data pointed to by the returned struct is only valid while
            the loader exists. Normally this is irrelevant if the data
            has been loaded into an audio buffer, but attempting to read
            the data after the WavLoader has been destroyed results in
            undefined behaviour.
            */
            const PCMData& getData(std::size_t chunkSize = 0, bool looped = false) const override;

            /*!
            \brief Seek to a specific offset in the file.
            \param offset Time struct containing the amount of time offset
            from the beginning of the stream.
            \returns true if successful else false if out of bounds.
            */
            bool seek(cro::Time) override;

            PCMData::Format getFormat() const override { return m_dataChunk.format; }

            std::int32_t getSampleRate() const override { return m_dataChunk.frequency; }

            std::uint64_t getSampleCount() const override { return m_sampleCount; }

        private:
            RaiiRWops m_file;

            struct WavHeader final
            {
                std::array<std::int8_t, 4u> chunkID{};
                std::uint32_t chunkSize = 0;
                std::array<std::int8_t, 4u> format{};
                std::array<std::int8_t, 4u> subchunk1ID{};
                std::uint32_t subchunk1Size = 0;
                std::uint16_t audioFormat = 0; 
                std::uint16_t channelCount = 0;
                std::uint32_t sampleRate = 0;
                std::uint32_t byteRate = 0;
                std::uint16_t blockAlign = 0;
                std::uint16_t bitsPerSample = 0;
            }m_header;

            struct WavChunk final
            {
                std::array<std::int8_t, 4u> ID{};
                std::uint32_t size = 0;
            };

            std::int64_t m_dataStart;
            std::uint32_t m_dataSize;
            std::uint64_t m_bytesPerSecond;
            std::uint64_t m_sampleCount;

            mutable PCMData m_dataChunk;
            mutable std::vector<std::uint8_t> m_sampleBuffer;
        };
    }
}