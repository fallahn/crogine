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

#include "../VorbisLoader.hpp"
#include "../WavLoader.hpp"
#include "../Mp3Loader.hpp"

#include <crogine/audio/sound_system/Playlist.hpp>
#include <crogine/core/FileSystem.hpp>
#include <crogine/core/Log.hpp>

#include <chrono>
#include <algorithm>
#include <cstring>

using namespace cro;

namespace
{
    constexpr std::size_t MaxFiles = 50;
    constexpr std::int32_t ChannelCount = 2;
}

Playlist::Playlist()
    : m_outBuffer   (0),
    m_outputOffset  (0),
    m_inBuffer      (0),
    m_fileIndex     (0),
    m_threadRunning (true),
    m_loadNextFile  (false),
    m_thread        (&Playlist::threadFunc, this)
{
    std::fill(m_silence.begin(), m_silence.end(), std::int16_t(0));
}

Playlist::~Playlist()
{
    m_threadRunning = false;
    m_thread.join();
}

//public
void Playlist::addPath(const std::string& path)
{
    if (cro::FileSystem::fileExists(path))
    {
        if (m_filePaths.size() < MaxFiles)
        {
            std::scoped_lock lock(m_mutex);
            m_filePaths.push_back(path);
        }
        else
        {
            LogW << path << ": not added to playlist, max files reached." << std::endl;
        }
    }
    else
    {
        LogW << path << ": not added to playlist, file doesn't exist." << std::endl;
    }
}

const std::int16_t* Playlist::getData(std::int32_t& samples)
{
    //return silence if nothing is loaded
    if (m_inBuffer == m_outBuffer ||
        m_buffers[m_outBuffer].empty())
    {
        if (!m_filePaths.empty())
        {
            m_loadNextFile = true;
        }

        samples = static_cast<std::int32_t>(m_silence.size() / ChannelCount);
        return m_silence.data();
    }

    //arbitrary amount - we make sure below to not load the next buffer
    //until we know it's safe to do so, so let's send ~double samples per sec (at 60Hz)
    static constexpr std::int32_t SampleCount = (44100 / 30) * ChannelCount;
    auto available = std::min((static_cast<std::int32_t>(m_buffers[m_outBuffer].size()) - m_outputOffset), SampleCount);

    samples = available / ChannelCount; //ONE SAMPLE includes 2 CHANNELS
    const auto* retVal = &m_buffers[m_outBuffer][m_outputOffset];

    m_outputOffset += available;
    if (m_outputOffset >= m_buffers[m_outBuffer].size())
    {
        //only load the next buffer if we're ready
        //otherwise buffer loading will loop around and
        //load over the top of the current buffer...
        m_loadNextFile = (((m_inBuffer + MaxBuffers) - m_outBuffer) % MaxBuffers) == 2;

        m_outputOffset = 0;
        m_outBuffer = (m_outBuffer + 1) % MaxBuffers;
    }

    return retVal;
}


//private
void Playlist::threadFunc()
{
    while (m_threadRunning)
    {
        if (m_loadNextFile)
        {
            std::unique_ptr<Detail::AudioFile> audioFile;
            {
                std::scoped_lock lock(m_mutex);
                auto ext = FileSystem::getFileExtension(m_filePaths[m_fileIndex]);
                if (ext == ".ogg")
                {
                    audioFile = std::make_unique<Detail::VorbisLoader>();
                }
                else if (ext == ".wav")
                {
                    audioFile = std::make_unique<Detail::WavLoader>();
                }
                else if (ext == ".mp3")
                {
                    audioFile = std::make_unique<Detail::Mp3Loader>();
                }
                else
                {
                    //better to just refuse loading than to get stuck in
                    //an infinte loop searching for the next file...
                    m_loadNextFile = false;
                    m_fileIndex = (m_fileIndex + 1) % m_filePaths.size();
                    LogW << m_filePaths[m_fileIndex] << " not a valid audio file" << std::endl;
                }
            
                if (audioFile)
                {
                    if (!audioFile->open(m_filePaths[m_fileIndex])
                        || audioFile->getFormat() != Detail::PCMData::Format::STEREO16
                        || audioFile->getSampleRate() != 44100) //hmm what about 48KHz?
                    {
                        m_loadNextFile = false;
                        m_fileIndex = (m_fileIndex + 1) % m_filePaths.size();
                        LogW << m_filePaths[m_fileIndex] << ": could not open file, or file not 16 bit stereo" << std::endl;

                        audioFile.reset();
                    }
                }
            }

            if (audioFile)
            {
                m_buffers[m_inBuffer].clear();

                //set a max file length of ~15 minutes
                static constexpr std::size_t MaxFileSize = 15 * 60 * 44100 * ChannelCount * sizeof(std::int16_t);

                std::vector<std::uint8_t> temp;
                const auto& data = audioFile->getData();
                while (data.data
                    && temp.size() < MaxFileSize)
                {
                    auto old = temp.size();
                    temp.resize(old + data.size);

                    std::memcpy(&temp[old], data.data, data.size);
                    audioFile->getData(); //this actually updates the supposedly const data reference... not the greatest design
                }

                if (!temp.empty())
                {
                    {
                        std::scoped_lock lock(m_mutex);
                        m_fileIndex = (m_fileIndex + 1) % m_filePaths.size();
                    }

                    m_buffers[m_inBuffer].resize(temp.size() / sizeof(std::int16_t));
                    std::memcpy(m_buffers[m_inBuffer].data(), temp.data(), temp.size());

                    m_inBuffer = (m_inBuffer + 1) % MaxBuffers;
                    m_loadNextFile = (((m_inBuffer + MaxBuffers) - m_outBuffer) % MaxBuffers) != 2; //always want to be 2 buffers ahead of output
                }
            }
            
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    }
}