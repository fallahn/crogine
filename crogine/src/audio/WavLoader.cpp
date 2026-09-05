/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2026
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

#include "WavLoader.hpp"

#include <crogine/core/Log.hpp>

#include <SDL3/SDL_iostream.h>

using namespace cro;
using namespace cro::Detail;

namespace
{
    constexpr std::uint32_t riffID = 0x46464952;//"RIFF"
    constexpr std::uint32_t formatID = 0x45564157;//"WAVE" 
    constexpr std::uint32_t subchunk1ID = 0x20746D66;//"fmt " 
    constexpr std::uint32_t dataID = 0x61746164; //"DATA"

    std::uint32_t asUint(const std::array<std::int8_t, 4>& data)
    {
        return data[3] << 24 | data[2] << 16 | data[1] << 8 | data[0];
    }

    enum AudioFormat
    {
        PCM = 1,
        mulaw = 6,
        alaw = 7,
        IBM_MuLaw = 257,
        IBM_ALaw = 258,
        ADPCM = 259
    };
}

WavLoader::WavLoader()
    : m_dataStart   (0),
    m_dataSize      (0),
    m_bytesPerSecond(0),
    m_sampleCount   (0)
{

}

//public
bool WavLoader::open(const std::filesystem::path& path)
{
    if (m_file)
    {
        m_file.close();

        m_dataChunk = {};

        m_dataStart = 0;
        m_dataSize = 0;
        m_bytesPerSecond = 0;
        m_sampleCount = 0;
    }
    
    auto file = m_file.open(path, "rb");
    if (file)
    {
        //file opened, let's do stuff!
        auto read = SDL_ReadIO(m_file.filePtr(), &m_header, sizeof(m_header));
        if (read != sizeof(m_header))
        {
            m_file.close();
            
            LogE << "Failed to read wav header for " << path << std::endl;
            return false;
        }

        std::uint32_t ID = asUint(m_header.chunkID);
        if (ID != riffID)
        {
            m_file.close();

            LogE << "Header file invalid ID: " << path << std::endl;
            return false;
        }

        ID = asUint(m_header.format);
        if (ID != formatID)
        {
            m_file.close();

            LogE << path << " is not a WAV format file" << std::endl;
            return false;
        }

        ID = asUint(m_header.subchunk1ID);
        if (ID != subchunk1ID)
        {
            m_file.close();

            LogE << path << ": Invalid header data chunk" << std::endl;
            return false;
        }

        if (m_header.audioFormat != AudioFormat::PCM)
        {
            m_file.close();
            
            LogE << path << ": not in PCM format, only PCM wav files are supported" << std::endl;
            return false;
        }

        if (m_header.bitsPerSample < 8 || m_header.bitsPerSample > 16)
        {
            m_file.close();
            
            LogE << path << ": Invalid Bits per sample, must be 8 or 16" << std::endl;
            return false;
        }

        if (m_header.channelCount > 2 || m_header.channelCount < 1)
        {
            m_file.close();
            
            LogE << path << ": invalid channel count, only mono or stereo wav files are supported" << std::endl;
            return false;
        }

        //read chunk info until we find the data and position our file stream there
        WavChunk chunk;
        read = sizeof(WavChunk);
        ID = 0;
        while (read == sizeof(WavChunk) && ID != dataID)
        {
            read = SDL_ReadIO(m_file.filePtr(), &chunk, sizeof(WavChunk));
            ID = asUint(chunk.ID);
        }

        if (read != sizeof(WavChunk))
        {
            m_file.close();

            LogE << "Failed to find data chunk in " << path << std::endl;
            return false;
        }

        m_dataSize = chunk.size - sizeof(WavChunk); //don't include the chunk header in the data!
        m_dataStart = SDL_SeekIO(m_file.filePtr(), sizeof(WavChunk::size), SDL_IO_SEEK_CUR);

        m_dataChunk.frequency = m_header.sampleRate;
        if (m_header.channelCount == 1)
        {
            m_dataChunk.format = (m_header.bitsPerSample == 8) ? PCMData::Format::MONO8 : PCMData::Format::MONO16;
        }
        else
        {
            m_dataChunk.format = (m_header.bitsPerSample == 8) ? PCMData::Format::STEREO8 : PCMData::Format::STEREO16;
        }

        m_bytesPerSecond = m_header.sampleRate * m_header.channelCount * (m_header.bitsPerSample / 8);
        m_sampleCount = m_dataSize / (m_header.bitsPerSample / 8);

        return true;
    }
    LogE << "Failed to open " << path << std::endl;
    return false; //no file :(
}

const PCMData& WavLoader::getData(std::size_t size, bool looped) const
{
    auto currPos = SDL_SeekIO(m_file.filePtr(), 0, SDL_IO_SEEK_CUR);

    //return null if failed
    if (currPos == -1)
    {
        //return empty so we know we reached the end
        m_dataChunk.size = 0;
        m_dataChunk.data = nullptr;
        return m_dataChunk;
    }

    std::size_t remain = (m_dataStart + m_dataSize) - currPos;
    std::size_t byteCount = (size > 0) ? std::min(remain, size) : remain;

    auto buffSize = std::max(size, byteCount);
    if (m_sampleBuffer.size() < buffSize)
    {
        m_sampleBuffer.resize(buffSize);
    }

    if (SDL_ReadIO(m_file.filePtr(), m_sampleBuffer.data(), byteCount) == 0)
    {
        m_dataChunk.size = 0;
        m_dataChunk.data = nullptr;
        return m_dataChunk;
    }
    
    //go back to beginning of file if looped
    if (remain < size
        && looped)
    {
        auto fill = size - remain;
        SDL_SeekIO(m_file.filePtr(), m_dataStart, SDL_IO_SEEK_SET);
        if (SDL_ReadIO(m_file.filePtr(), m_sampleBuffer.data() + byteCount, fill))
        {
            byteCount += fill;
        }
    }

    m_dataChunk.size = static_cast<std::uint32_t>(byteCount);
    m_dataChunk.data = m_sampleBuffer.data();
    return m_dataChunk;
}

bool WavLoader::seek(cro::Time offset)
{
    if (!m_file)
    {
        return false;
    }

    auto offsetMillis = offset.asMilliseconds();
    auto dest = m_bytesPerSecond * offsetMillis / 1000;

    if (dest < m_dataSize)
    {
        auto result = SDL_SeekIO(m_file.filePtr(), m_dataStart + dest, SDL_IO_SEEK_SET);
        return (result == (m_dataStart + dest));
    }
    return false;
} 
