/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2025
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

#include <crogine/audio/sound_system/SoundStream.hpp>
#include <crogine/core/Log.hpp>
#include <crogine/detail/Assert.hpp>

#include "../ALCheck.hpp"

#ifdef __APPLE__
#include "../al.h"

//silence deprecated openal warnings
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#else
#include <AL/al.h>
#endif

using namespace cro;

namespace
{
    std::int32_t getFormatFromChannelCount(unsigned int channelCount)
    {
        std::int32_t format = 0;
        switch (channelCount)
        {
        case 1:  
            format = AL_FORMAT_MONO16;
            break;
        case 2:
            format = AL_FORMAT_STEREO16;
            break;
        case 4:
            format = alGetEnumValue("AL_FORMAT_QUAD16");
            break;
        case 6:
            format = alGetEnumValue("AL_FORMAT_51CHN16");
            break;
        case 7:
            format = alGetEnumValue("AL_FORMAT_61CHN16");
            break;
        case 8:
            format = alGetEnumValue("AL_FORMAT_71CHN16");
            break;
        default:
            format = 0;
            break;
        }

        //fixes a bug on macOS
        if (format == -1)
        {
            format = 0;
        }

        return format;
    }
}

SoundStream::SoundStream()
    : m_startState      (Status::Stopped),
    m_isStreaming       (false),
    m_channelCount      (0),
    m_sampleRate        (0),
    m_sampleCount       (0),
    m_format            (0),
    m_loop              (false),
    m_samplesProcessed  (0),
    m_processingInterval(10)
{

}

SoundStream::~SoundStream() noexcept
{
    waitThread();
}

//public
void SoundStream::play()
{
    if (m_format == 0)
    {
        LogE << "OpenAL: Stream not yet initialised - call initialise() first" << std::endl;
        return;
    }

    bool isStreaming = false;
    Status startState = Status::Stopped;

    {
        std::scoped_lock lock(m_mutex);

        isStreaming = m_isStreaming;
        startState = m_startState;
    }

    //play if paused
    if (isStreaming && (startState == Status::Paused))
    {
        std::scoped_lock lock(m_mutex);
        m_startState = Status::Playing;
        alCheck(alSourcePlay(m_alSource));
        return;
    }
    //else stop (and rewind)
    else //if (isStreaming && (startState == Status::Playing))
    {
        stop();
    }

    launchThread(Status::Playing);
}

void SoundStream::pause()
{
    {
        std::scoped_lock lock(m_mutex);
        if (!m_isStreaming)
        {
            return;
        }

        m_startState = Status::Paused;
    }

    alCheck(alSourcePause(m_alSource));
}

void SoundStream::stop()
{
    waitThread();

    onSeek(0);
}

SoundStream::Status SoundStream::getStatus() const
{
    auto status = SoundSource::getStatus();

    if (status == Status::Stopped)
    {
        //allow for lag in time it takes for stream to play
        std::scoped_lock lock(m_mutex);
        if (m_isStreaming)
        {
            status = m_startState;
        }
    }

    return status;
}

std::uint32_t SoundStream::getChannelCount() const
{
    return m_channelCount;
}

std::uint32_t SoundStream::getSampleRate() const
{
    return m_sampleRate;
}

void SoundStream::setPlayingPosition(Time timeOffset)
{
    Status oldStatus = getStatus();

    stop();

    onSeek(timeOffset.asMilliseconds());

    m_samplesProcessed = static_cast<std::uint64_t>(timeOffset.asSeconds() * static_cast<float>(m_sampleRate)) * m_channelCount;

    if (oldStatus == Status::Stopped)
    {
        return;
    }

    launchThread(oldStatus);
}


Time SoundStream::getPlayingPosition() const
{
    if (m_sampleRate && m_channelCount)
    {
        ALfloat secs = 0.f;
        alCheck(alGetSourcef(m_alSource, AL_SEC_OFFSET, &secs));

        return seconds(secs + static_cast<float>(m_samplesProcessed) / static_cast<float>(m_sampleRate) / static_cast<float>(m_channelCount));
    }
    else
    {
        return milliseconds(0);
    }
}

//protected
void SoundStream::initialise(std::uint32_t channelCount, std::uint32_t sampleRate, std::uint64_t sampleCount)
{
    m_channelCount = channelCount;
    m_sampleRate = sampleRate;
    m_sampleCount = sampleCount;
    m_samplesProcessed = 0;
    m_isStreaming = false;

    m_format = getFormatFromChannelCount(channelCount);

    if (m_format == 0)
    {
        m_channelCount = 0;
        m_sampleRate = 0;
        LogE << "OpenAL: " << channelCount << ": unsupported channel count" << std::endl;
    }
}

void SoundStream::setProcessingInterval(std::int32_t interval)
{
    m_processingInterval = interval;
}

std::int64_t SoundStream::onLoop()
{
    onSeek(0);
    return 0;
}

//private
void SoundStream::threadFunc()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(m_processingInterval));

    bool requestStop = false;

    {
        std::scoped_lock lock(m_mutex);

        if (m_startState == Status::Stopped)
        {
            m_isStreaming = false;
            return;
        }
    }

    alCheck(alGenBuffers(BufferCount, m_buffers.data()));

    requestStop = fillQueue();

    alCheck(alSourcePlay(m_alSource));

    {
        std::scoped_lock lock(m_mutex);

        if (m_startState == Status::Paused)
        {
            alCheck(alSourcePause(m_alSource));
        }
    }



    //thread loop
    while (1)
    {
        //break out the loop if requested
        {
            std::scoped_lock lock(m_mutex);

            if (!m_isStreaming)
            {
                break;
            }
        }

        //sleep a bit
        if (SoundSource::getStatus() != Status::Stopped)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(m_processingInterval));
        }
        else
        {
            if (!requestStop)
            {
                alCheck(alSourcePlay(m_alSource));
            }
            else
            {
                std::scoped_lock lock(m_mutex);
                m_isStreaming = false;
            }
        }


        ALint processedBuffers = 0;
        alCheck(alGetSourcei(m_alSource, AL_BUFFERS_PROCESSED, &processedBuffers));

        while (processedBuffers--)
        {
            ALuint buffer = 0;
            alCheck(alSourceUnqueueBuffers(m_alSource, 1, &buffer));

            std::uint32_t bufferIndex = 0;
            for (auto i = 0u; i < BufferCount; ++i)
            {
                if (m_buffers[i] == buffer)
                {
                    bufferIndex = i;
                    break;
                }
            }

            if (m_bufferSeeks[bufferIndex] != NoLoop)
            {
                m_samplesProcessed = static_cast<std::uint64_t>(m_bufferSeeks[bufferIndex]);
                m_bufferSeeks[bufferIndex] = NoLoop;
            }
            else
            {
                ALint size = 0;
                ALint bits = 0;

                alCheck(alGetBufferi(buffer, AL_SIZE, &size));
                alCheck(alGetBufferi(buffer, AL_BITS, &bits));

                if (bits == 0)
                {
                    LogE << "OpenAL: Bits in sound stream are 0. Make sure audio is not corrupt and initialise() was called correctly" << std::endl;

                    std::scoped_lock lock(m_mutex);
                    m_isStreaming = false;
                    requestStop = true;
                    break;
                }
                else
                {
                    m_samplesProcessed += static_cast<std::uint64_t>(size / (bits / 8));
                }
            }

            if (!requestStop)
            {
                if (fillAndPushBuffer(bufferIndex))
                {
                    requestStop = true;
                }
            }
        }        
    }

    alCheck(alSourceStop(m_alSource));

    clearQueue();

    m_samplesProcessed = 0;

    alCheck(alSourcei(m_alSource, AL_BUFFER, 0));
    alCheck(alDeleteBuffers(BufferCount, m_buffers.data()));
}

bool SoundStream::fillAndPushBuffer(std::uint32_t bufferID, bool loopImmediate)
{
    bool requestStop = false;

    //call onGetData() until we have some or request to stop
    Chunk chunk;
    for (auto retryCount = 0u; !onGetData(chunk) && (retryCount < BufferRetries); ++retryCount)
    {
        if (!m_loop)
        {
            if (chunk.samples != nullptr && chunk.sampleCount != 0)
            {
                m_bufferSeeks[bufferID] = 0;
            }
            requestStop = true;
            break;
        }

        m_bufferSeeks[bufferID] = onLoop();

        if (chunk.samples != nullptr && chunk.sampleCount != 0)
        {
            break;
        }

        if (loopImmediate && (m_bufferSeeks[bufferID] != NoLoop))
        {
            m_samplesProcessed = static_cast<std::uint64_t>(m_bufferSeeks[bufferID]);
            m_bufferSeeks[bufferID] = NoLoop;
        }
    }

    //fill buffer if we have data and queue it
    if (chunk.samples != nullptr && chunk.sampleCount != 0)
    {
        auto buffer = m_buffers[bufferID];

        auto size = static_cast<ALsizei>(chunk.sampleCount * sizeof(std::int16_t));
        alCheck(alBufferData(buffer, m_format, chunk.samples, size, static_cast<ALsizei>(m_sampleRate)));
        alCheck(alSourceQueueBuffers(m_alSource, 1, &buffer));
    }
    else
    {
        //we have no data so all we can reasonably do is stop
        requestStop = true;
    }

    return requestStop;
}

bool SoundStream::fillQueue()
{
    bool requestStop = false;
    for (auto i = 0u; (i < BufferCount) && !requestStop; ++i)
    {
        if (fillAndPushBuffer(i, (i == 0)))
        {
            requestStop = true;
        }
    }

    return requestStop;
}

void SoundStream::clearQueue()
{
    ALint queuedCount = 0;
    alCheck(alGetSourcei(m_alSource, AL_BUFFERS_QUEUED, &queuedCount));

    ALuint buffer = 0;
    for (auto i = 0; i < queuedCount; ++i)
    {
        alCheck(alSourceUnqueueBuffers(m_alSource, 1, &buffer));
    }
}

void SoundStream::launchThread(SoundStream::Status status)
{
    m_isStreaming = true;
    m_startState = status;

    CRO_ASSERT(!m_thread.joinable(), "");
    m_thread = std::thread(&SoundStream::threadFunc, this);
}

void SoundStream::waitThread()
{
    {
        std::scoped_lock lock(m_mutex);
        m_isStreaming = false;
    }

    if (m_thread.joinable())
    {
        m_thread.join();
    }
}