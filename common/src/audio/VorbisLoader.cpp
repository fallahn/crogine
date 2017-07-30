/*-----------------------------------------------------------------------

Matt Marchant 2017
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

#include "VorbisLoader.hpp"

using namespace cro;
using namespace cro::Detail;

VorbisLoader::VorbisLoader()
    : m_vorbisFile  (nullptr),
    m_channelCount  (0)
{

}

VorbisLoader::~VorbisLoader()
{
    if (m_vorbisFile)
    {
        stb_vorbis_close(m_vorbisFile);
    }
}

//public
bool VorbisLoader::open(const std::string& path)
{
    //close any open files
    if (m_file.file)
    {
        SDL_RWclose(m_file.file);
        m_file.file = nullptr;
    }
    if (m_vorbisFile)
    {
        stb_vorbis_close(m_vorbisFile);
        m_vorbisFile = nullptr;
    }
    
    m_file.file = SDL_RWFromFile(path.c_str(), "rb");
    if (!m_file.file)
    {
        Logger::log("Failed opening " + path, Logger::Type::Error);
        return false;
    }
    
    //read header
    int32 err;
    m_vorbisFile = stb_vorbis_open_file(m_file.file, 0, &err, nullptr);
    if (!m_vorbisFile)
    {
        SDL_RWclose(m_file.file);
        m_file.file = nullptr;

        Logger::log("Failed opening vorbis file, error " + std::to_string(err), Logger::Type::Error);
        return false;
    }

    auto info = stb_vorbis_get_info(m_vorbisFile);
    if (info.channels > 2)
    {
        SDL_RWclose(m_file.file);
        m_file.file = nullptr;

        stb_vorbis_close(m_vorbisFile);
        m_vorbisFile = nullptr;

        Logger::log("Found " + std::to_string(info.channels) + " channels in " + path + ", currently only mono and stereo files are supported.", Logger::Type::Error);
        Logger::log(path + ": not loaded.", Logger::Type::Error);
        return false;
    }

    //TODO unlike opus the format may be 8 or 24 bit
    m_dataChunk.format = (info.channels == 1) ? PCMData::Format::MONO16 : PCMData::Format::STEREO16;
    m_dataChunk.frequency = info.sample_rate;
    m_channelCount = info.channels;

    return true;
}

const PCMData& VorbisLoader::getData(std::size_t size) const
{
    CRO_ASSERT(m_vorbisFile, "File not open");
    
    //TODO read entire file if size == 0

    auto shortSize = size / 2;
    if (shortSize > m_buffer.size())
    {
        m_buffer.resize(shortSize);
    }

    m_dataChunk.size = stb_vorbis_get_samples_short_interleaved(m_vorbisFile, m_channelCount, m_buffer.data(), shortSize);
    m_dataChunk.size *= m_channelCount * 2; //TODO replace '2' with bytes per sample based on bit depth
    m_dataChunk.data = (m_dataChunk.size) ? m_buffer.data() : nullptr;

    return m_dataChunk;
}

bool VorbisLoader::seek(cro::Time offset)
{
    if (m_vorbisFile)
    {
        auto mills = offset.asMilliseconds();
        auto sample = (m_dataChunk.frequency * mills) / 1000;

        stb_vorbis_seek(m_vorbisFile, sample);

        return true;
    }

    return false;
}