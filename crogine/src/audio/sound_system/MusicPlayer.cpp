/*-----------------------------------------------------------------------

Matt Marchant 2023
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

#include "../WavLoader.hpp"
#include "../Mp3Loader.hpp"
#include "../VorbisLoader.hpp"
#include "../AudioRenderer.hpp"

#include <crogine/audio/sound_system/MusicPlayer.hpp>

using namespace cro;
using namespace cro::Detail;

MusicPlayer::MusicPlayer()
    : m_bytesPerSample(0)
{

}

MusicPlayer::~MusicPlayer()
{
    //prevents error for forward decl of AudioFile
}

//public
bool MusicPlayer::loadFromFile(const std::string& path)
{
    if (!AudioRenderer::isValid())
    {
        //TODO also check renderer is OpenAL
        LogE << "Unable to open " << path << ": no audio renderer available." << std::endl;
        return false;
    }

    stop();


    m_bytesPerSample = 1; //default this to 1 so we don't get a div0 trying to play an empty file
    auto filePath = cro::FileSystem::getResourcePath() + path;

    auto ext = FileSystem::getFileExtension(path);
    if (ext == ".wav")
    {
        m_audioFile = std::make_unique<WavLoader>();
        if (!m_audioFile->open(filePath))
        {
            m_audioFile.reset();
            Logger::log("Failed to open " + path, Logger::Type::Error);
            return false;
        }
    }
    else if (ext == ".ogg")
    {
        m_audioFile = std::make_unique<VorbisLoader>();
        if (!m_audioFile->open(filePath))
        {
            m_audioFile.reset();
            Logger::log("Failed to open " + path, Logger::Type::Error);
            return false;
        }
    }
    else if (ext == ".mp3")
    {
        m_audioFile = std::make_unique<Mp3Loader>();
        if (!m_audioFile->open(filePath))
        {
            m_audioFile.reset();
            Logger::log("Failed to open " + path, Logger::Type::Error);
            return false;
        }
    }
    else
    {
        Logger::log(ext + ": Unsupported file type.", Logger::Type::Error);
        return false;
    }

    std::int32_t channelCount = 0;
    switch (m_audioFile->getFormat())
    {
    default: break;
    case PCMData::Format::MONO8:
        m_bytesPerSample = 1;
        channelCount = 1;
        break;
    case PCMData::Format::MONO16:
        m_bytesPerSample = 2;
        channelCount = 1;
        break;
    case PCMData::Format::STEREO8:
        m_bytesPerSample = 1;
        channelCount = 2;
        break;
    case PCMData::Format::STEREO16:
        m_bytesPerSample = 2;
        channelCount = 2;
        break;
    }

    initialise(channelCount, m_audioFile->getSampleRate());

    return true;
}

//private
bool MusicPlayer::onGetData(cro::SoundStream::Chunk& chunk)
{
    auto chunkSize = (m_audioFile->getSampleRate() / 1000) * getProcessingInterval() * getChannelCount() * 2;
    auto data = m_audioFile->getData(chunkSize);

    chunk.sampleCount = data.size / m_bytesPerSample;
    chunk.samples = static_cast<std::int16_t*>(data.data);

    return true;
}

void MusicPlayer::onSeek(std::int32_t position)
{
    //hmm is this time, or samples? files use time...
    if (m_audioFile)
    {
        m_audioFile->seek(cro::milliseconds(position));
    }
}