/*-----------------------------------------------------------------------

Matt Marchant 2023
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

#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"

#include "Mp3Loader.hpp"

#include <algorithm>

using namespace cro;
using namespace cro::Detail;

namespace
{

}

Mp3Loader::Mp3Loader()
    : m_channelCount(0)
{

}

Mp3Loader::~Mp3Loader()
{

}

//public
bool Mp3Loader::open(const std::string& path)
{
    //close any open files
    //if (m_file.file)
    //{
    //    SDL_RWclose(m_file.file);
    //    m_file.file = nullptr;
    //}
    
    if (m_decoder)
    {
        mp3dec_ex_close(m_decoder.get());

        //TODO do we really need to do this?
        m_decoder.reset();
    }

    //m_file.file = SDL_RWFromFile(path.c_str(), "rb");
    //if (!m_file.file)
    //{
    //    Logger::log("Failed opening " + path, Logger::Type::Error);
    //    return false;
    //}


    //read header
    m_decoder = std::make_unique<mp3dec_ex_t>();
    mp3dec_ex_open(m_decoder.get(), path.c_str(), MP3D_SEEK_TO_SAMPLE);
    
    if (!m_decoder->samples)
    {
        /*SDL_RWclose(m_file.file);
        m_file.file = nullptr;*/

        Logger::log("Failed opening mp3 file, error "/* + std::to_string(err)*/, Logger::Type::Error);
        return false;
    }

    if (m_decoder->info.channels > 2)
    {
        /*SDL_RWclose(m_file.file);
        m_file.file = nullptr;

        stb_vorbis_close(m_vorbisFile);
        m_vorbisFile = nullptr;*/

        mp3dec_ex_close(m_decoder.get());

        //TODO do we really need to do this?
        m_decoder.reset();

        Logger::log("Found " + std::to_string(m_decoder->info.channels) + " channels in " + path + ", currently only mono and stereo files are supported.", Logger::Type::Error);
        Logger::log(path + ": not loaded.", Logger::Type::Error);
        return false;
    }

    m_dataChunk.format = (m_decoder->info.channels == 1) ? PCMData::Format::MONO16 : PCMData::Format::STEREO16;
    m_dataChunk.frequency = m_decoder->info.hz;
    m_channelCount = m_decoder->info.channels;

    return true;
}

const PCMData& Mp3Loader::getData(std::size_t size, bool looped) const
{
    CRO_ASSERT(m_decoder, "File not open");
    
    static constexpr std::int32_t bytesPerSample = sizeof(mp3d_sample_t);
    
    
    //read entire file if size == 0
    if (!size)
    {
        //limit this to ~ 1mb
        //larger files should be streamed
        size = 1024000;
    }

    auto shortSize = size / bytesPerSample;
    if (shortSize > m_buffer.size())
    {
        m_buffer.resize(shortSize);
    }

    m_dataChunk.size = 0;

    std::size_t idx = 0;
    while (shortSize > 0)
    {
        //as this is a wrapped func *anyway* it likely reads as much as we request in one go
        auto read = mp3dec_ex_read(m_decoder.get(), &m_buffer[idx], shortSize);

        shortSize -= read;
        idx += read;
        m_dataChunk.size += read;

        if (read == 0)
        {
            if (!looped)
            {
                break; //EOF
            }
            else
            {
                //rewind to beginning of file and continue filling the buffer
                mp3dec_ex_seek(m_decoder.get(), 0);
            }
        }
    }
    
    m_dataChunk.size *= bytesPerSample;
    m_dataChunk.data = (m_dataChunk.size) ? m_buffer.data() : nullptr;

    return m_dataChunk;
}

bool Mp3Loader::seek(cro::Time offset)
{
    if (m_decoder)
    {
        auto mills = offset.asMilliseconds();
        auto sample = (m_dataChunk.frequency * mills) / 1000;

        mp3dec_ex_seek(m_decoder.get(), sample);

        return true;
    }

    return false;
}