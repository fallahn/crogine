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
            const PCMData& getData(std::size_t chunkSize = 0) const override;

            /*!
            \brief Seek to a specific offset in the file.
            \param offset Time struct containing the amount of time offset
            from the beginning of the stream.
            \returns true if successful else false if out of bounds.
            */
            bool seek(cro::Time) override;

        private:
            RaiiRWops m_file;

            struct WavHeader final
            {
                std::array<cro::int8, 4u> chunkID{};
                cro::uint32 chunkSize = 0;
                std::array<cro::int8, 4u> format{};
                std::array<cro::int8, 4u> subchunk1ID{};
                cro::uint32 subchunk1Size = 0;
                cro::uint16 audioFormat = 0; 
                cro::uint16 channelCount = 0;
                cro::uint32 sampleRate = 0;
                cro::uint32 byteRate = 0;
                cro::uint16 blockAlign = 0;
                cro::uint16 bitsPerSample = 0;
            }m_header;

            struct WavChunk final
            {
                std::array<cro::int8, 4u> ID{};
                cro::uint32 size = 0;
            };

            cro::int64 m_dataStart;
            cro::uint32 m_dataSize;
            cro::uint64 m_bytesPerSecond;

            mutable PCMData m_dataChunk;
            mutable std::vector<cro::uint8> m_sampleBuffer;
        };
    }
}