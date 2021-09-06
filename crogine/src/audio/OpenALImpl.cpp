/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2021
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

#include "OpenALImpl.hpp"
#include "ALCheck.hpp"
#include "WavLoader.hpp"
#include "VorbisLoader.hpp"

#include <crogine/detail/Assert.hpp>
#include <crogine/util/String.hpp>
#include <crogine/core/App.hpp>
#include <crogine/core/FileSystem.hpp>

//oh apple you so quirky
#ifdef __APPLE__
#include <alc.h>
//silence deprecated openal warnings
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#else
#include <AL/al.h>
#endif

#include <array>

using namespace cro;
using namespace cro::Detail;

namespace
{
    constexpr std::size_t STREAM_CHUNK_SIZE = 32768;// 48000u * sizeof(std::uint16_t) * 30; //30 sec of stereo @ highest quality (mono)

    ALenum getFormatFromData(const PCMData& data)
    {
        switch (data.format)
        {
        default:
        case PCMData::Format::MONO8:
            return AL_FORMAT_MONO8;
        case PCMData::Format::MONO16:
            return AL_FORMAT_MONO16;
        case PCMData::Format::STEREO8:
            return AL_FORMAT_STEREO8;
        case PCMData::Format::STEREO16:
            return AL_FORMAT_STEREO16;
        }
    }
}

OpenALImpl::OpenALImpl()
    : m_device          (nullptr),
    m_context           (nullptr),
    m_nextFreeStream    (0),
    m_threadRunning     (false)
{
    for (auto i = 0u; i < m_streamIDs.size(); ++i)
    {
        m_streamIDs[i] = i;
    }
}


//public
bool OpenALImpl::init()
{
    //alCheck doesn't work on alc* functions, dummy.
    
    /*alCheck*/(m_device = alcOpenDevice(nullptr));
    if (!m_device)
    {
        LOG("Failed opening valid OpenAL device", Logger::Type::Error);        
        return false;
    }

    /*alCheck*/(m_context = alcCreateContext(m_device, nullptr));
    if (!m_context)
    {
        LOG("Failed creating OpenAL context", Logger::Type::Error);
        return false;
    }

    bool current = false;
    /*alCheck*/(current = alcMakeContextCurrent(m_context));

    if (current)
    {
        m_threadRunning = true;
        m_updateThread = std::make_unique<std::thread>(&OpenALImpl::streamThreadedUpdate, this);
    }

    return current;
}

void OpenALImpl::shutdown()
{
    m_threadRunning = false;
    m_updateThread->join();

    //make sure to close any open streams
    for (auto i = 0u; i < m_streams.size(); ++i)
    {
        deleteStream(i);
    }

    /*alCheck*/(alcMakeContextCurrent(nullptr));
    /*alCheck*/(alcDestroyContext(m_context));
    /*alCheck*/(alcCloseDevice(m_device));
}

void OpenALImpl::setListenerPosition(glm::vec3 position)
{
    alCheck(alListener3f(AL_POSITION, position.x, position.y, position.z));
}

void OpenALImpl::setListenerOrientation(glm::vec3 forward, glm::vec3 up)
{
    std::array<float, 6u> v = { forward.x, forward.y, forward.z, up.x, up.y, up.z };
    alCheck(alListenerfv(AL_ORIENTATION, v.data()));
}

void OpenALImpl::setListenerVolume(float volume)
{
    alCheck(alListenerf(AL_GAIN, volume));
}

void OpenALImpl::setListenerVelocity(glm::vec3 velocity)
{
    alCheck(alListener3f(AL_VELOCITY, velocity.x, velocity.y, velocity.z));
}

std::int32_t OpenALImpl::requestNewBuffer(const std::string& filePath)
{
    auto path = FileSystem::getResourcePath() + filePath;

    std::unique_ptr<AudioFile> loader;
    
    auto ext = FileSystem::getFileExtension(path);
    PCMData data;
    if (ext == ".wav")
    {      
        loader = std::make_unique<WavLoader>();
        if (loader->open(path))
        {
            data = loader->getData();
        }
    }
    else if (ext == ".ogg")
    {
        loader = std::make_unique<VorbisLoader>();
        if (loader->open(path))
        {
            data = loader->getData();
        }
    }
    else
    {
        Logger::log(ext + ": format not supported", Logger::Type::Error);
    }

    if (data.data)
    {
        return requestNewBuffer(data);
    }
    
    return -1;
}

std::int32_t OpenALImpl::requestNewBuffer(const PCMData& data)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    ALuint buff;
    alCheck(alGenBuffers(1, &buff));

    ALenum format = getFormatFromData(data);
    alCheck(alBufferData(buff, format, data.data, data.size, data.frequency));

    return buff;
}

void OpenALImpl::deleteBuffer(std::int32_t buffer)
{
    if (buffer > 0)
    {
        LOG("Deleted audio buffer " + std::to_string(buffer), Logger::Type::Info);
        std::lock_guard<std::mutex> lock(m_mutex);

        //hm. Code smell.
        auto buf = static_cast<ALuint>(buffer);
        alCheck(alDeleteBuffers(1, &buf));
    }
}

std::int32_t OpenALImpl::requestNewStream(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    //check we have available streams
    if (m_nextFreeStream >= m_streams.size())
    {
        Logger::log("Maximum number of streams has been reached!", Logger::Type::Warning);
        return -1;
    }

    auto streamID = m_streamIDs[m_nextFreeStream];


    //attempt to open the file
    auto& stream = m_streams[streamID];
    auto ext = FileSystem::getFileExtension(path);
    if (ext == ".wav")
    {
        stream.audioFile = std::make_unique<WavLoader>();
        if (!stream.audioFile->open(path))
        {
            stream.audioFile.reset();
            Logger::log("Failed to open " + path, Logger::Type::Error);
            return -1;
        }
    }
    else if (ext == ".ogg")
    {
        stream.audioFile = std::make_unique<VorbisLoader>();
        if (!stream.audioFile->open(path))
        {
            stream.audioFile.reset();
            Logger::log("Failed to open " + path, Logger::Type::Error);
            return -1;
        }
    }
    else
    {
        Logger::log(ext + ": Unsupported file type.", Logger::Type::Error);
        return -1;
    }
    
    alCheck(alGenBuffers(static_cast<ALsizei>(stream.buffers.size()), stream.buffers.data()));

    if (stream.buffers[0])
    {
        //fill buffers from file
        for (auto b : stream.buffers)
        {
            auto audioData = stream.audioFile->getData(STREAM_CHUNK_SIZE);
            alCheck(alBufferData(b, getFormatFromData(audioData), audioData.data, audioData.size, audioData.frequency));
        }

        //hurrah we has stream
        m_nextFreeStream++;

        return streamID;
    }
    return -1;
}

void OpenALImpl::deleteStream(std::int32_t id)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto& stream = m_streams[id];
    
    //while (stream.updating)
    //{
    //    //wait for thread to finish - we have to wait else
    //    //we can't be sure it's safe to delete the buffers
    //} 

    if (stream.buffers[0])
    {
        alCheck(alDeleteBuffers(static_cast<ALsizei>(stream.buffers.size()), stream.buffers.data()));
        std::fill(stream.buffers.begin(), stream.buffers.end(), 0);
        LOG("Deleted audio stream", Logger::Type::Info);

        stream.audioFile.reset();
        stream.currentBuffer = 0;
        stream.processed = 0;

        m_nextFreeStream--;

        for (auto& i : m_streamIDs)
        {
            if (i == id)
            {
                i = m_streamIDs[m_nextFreeStream];
                m_streamIDs[m_nextFreeStream] = id;
                break;
            }
        }
    }
}

std::int32_t OpenALImpl::requestAudioSource(std::int32_t buffer, bool streaming)
{
    CRO_ASSERT(buffer > -1, "Invalid audio buffer");
    std::lock_guard<std::mutex> lock(m_mutex);

    ALuint source;
    alCheck(alGenSources(1, &source));
    if (source > 0)
    {
        if (!streaming)
        {
            alCheck(alSourcei(source, AL_BUFFER, buffer));
        }
        else
        {
            auto& stream = m_streams[buffer];
            stream.sourceID = source;
            alCheck(alSourceQueueBuffers(source, static_cast<ALsizei>(stream.buffers.size()), stream.buffers.data()));
        }
        return source;
    }
    return -1;
}

void OpenALImpl::updateAudioSource(std::int32_t sourceID, std::int32_t bufferID, bool streaming)
{
    CRO_ASSERT(sourceID > 0, "Invalid source ID");
    CRO_ASSERT(bufferID > 0, "Invalid buffer ID");

    //if the src is playing stop it and wait for it to finish
    stopSource(sourceID); //this locks too...


    std::lock_guard<std::mutex> lock(m_mutex);
    auto src = static_cast<ALuint>(sourceID);
    ALenum state;
    alCheck(alGetSourcei(src, AL_SOURCE_STATE, &state));
    while (state == AL_PLAYING)
    {
        alCheck(alGetSourcei(src, AL_SOURCE_STATE, &state));
    }

    if (!streaming)
    {
        alCheck(alSourcei(sourceID, AL_BUFFER, bufferID));
    }
    else
    {
        auto& stream = m_streams[bufferID];
        stream.sourceID = sourceID;
        alCheck(alSourceQueueBuffers(sourceID, static_cast<ALsizei>(stream.buffers.size()), stream.buffers.data()));
    }
}

void OpenALImpl::deleteAudioSource(std::int32_t source)
{
    CRO_ASSERT(source > 0, "Invalid source ID");

    //if the src is playing stop it and wait for it to finish
    //this also locks the mutex!
    stopSource(source);

    std::lock_guard<std::mutex> lock(m_mutex);
    auto src = static_cast<ALuint>(source);
    ALenum state;
    alCheck(alGetSourcei(src, AL_SOURCE_STATE, &state));
    while (state == AL_PLAYING)
    {
        alCheck(alGetSourcei(src, AL_SOURCE_STATE, &state));
    }

    //if this is associated with a stream, delete the stream
    //TODO this should just unassociate the stream and unqueue
    auto result = std::find_if(std::begin(m_streams), std::end(m_streams),
        [source](const OpenALStream& str)
    {
        return str.sourceID == source;
    });
    if (result != m_streams.end())
    {
        /*auto idx = std::distance(std::begin(m_streams), result);
        deleteStream(idx);*/
        result->sourceID = -1;
    }

    alCheck(alSourcei(src, AL_BUFFER, 0));
    alCheck(alDeleteSources(1, &src));

    //LOG("Deleted audio source", Logger::Type::Info);
}

void OpenALImpl::playSource(std::int32_t source, bool looped)
{
    ALuint src = static_cast<ALuint>(source);

    std::lock_guard<std::mutex> lock(m_mutex);
    auto result = std::find_if(std::begin(m_streams), std::end(m_streams),
        [src](const OpenALStream& str)
    {
        return str.sourceID == src;
    });

    if (result == m_streams.end())
    {
        alCheck(alSourcei(src, AL_LOOPING, looped ? AL_TRUE : AL_FALSE));
    }
    else
    {
        result->looped = looped;
    }
    alCheck(alSourcePlay(src));
}

void OpenALImpl::pauseSource(std::int32_t source)
{
    ALuint src = static_cast<ALuint>(source);

    std::lock_guard<std::mutex> lock(m_mutex);
    alCheck(alSourcePause(src));
}

void OpenALImpl::stopSource(std::int32_t source)
{
    ALuint src = static_cast<ALuint>(source);

    std::lock_guard<std::mutex> lock(m_mutex);
    alCheck(alSourceStop(src));
}

std::int32_t OpenALImpl::getSourceState(std::int32_t source) const
{
    ALuint src = static_cast<ALuint>(source);
    ALenum state;

    std::lock_guard<std::mutex> lock(m_mutex);
    alCheck(alGetSourcei(src, AL_SOURCE_STATE, &state));

    if (state == AL_PLAYING)
    {
        return 0;
    }

    if (state == AL_PAUSED)
    {
        return 1;
    }

    return 2;
}

void OpenALImpl::setSourcePosition(std::int32_t source, glm::vec3 position)
{
    //std::lock_guard<std::mutex> lock(m_mutex);
    alCheck(alSource3f(source, AL_POSITION, position.x, position.y, position.z));
    //DPRINT("source Pos", std::to_string(position.x) + ", " + std::to_string(position.y) + ", " + std::to_string(position.z));
}

void OpenALImpl::setSourcePitch(std::int32_t src, float pitch)
{
    //std::lock_guard<std::mutex> lock(m_mutex);
    alCheck(alSourcef(src, AL_PITCH, pitch));
}

void OpenALImpl::setSourceVolume(std::int32_t src, float volume)
{
    //std::lock_guard<std::mutex> lock(m_mutex);
    alCheck(alSourcef(src, AL_GAIN, volume));
}

void OpenALImpl::setSourceRolloff(std::int32_t src, float rolloff)
{
    //std::lock_guard<std::mutex> lock(m_mutex);
    alCheck(alSourcef(src, AL_ROLLOFF_FACTOR, rolloff));
}

void OpenALImpl::setSourceVelocity(std::int32_t src, glm::vec3 velocity)
{
    //std::lock_guard<std::mutex> lock(m_mutex);
    alCheck(alSource3f(src, AL_VELOCITY, velocity.x, velocity.y, velocity.z));
}

void OpenALImpl::setDopplerFactor(float factor)
{
    alCheck(alDopplerFactor(factor));
}

void OpenALImpl::setSpeedOfSound(float speed)
{
    alCheck(alSpeedOfSound(speed));
}

//private
void OpenALImpl::streamThreadedUpdate()
{
    while (m_threadRunning)
    {
        //check active streams to see if they need queuing
        m_mutex.lock();
        for (auto i = 0; i < m_nextFreeStream; ++i)
        {
            auto& stream = m_streams[m_streamIDs[i]]; //hm might jump about a bit - better or worse than checking all and conditional jumping?

            if (stream.sourceID > -1 &&
                !stream.updating)
            {
                alCheck(alGetSourcei(stream.sourceID, AL_BUFFERS_PROCESSED, &stream.processed));

                //if stopped rewind file and load buffers
                ALenum state;
                alCheck(alGetSourcei(stream.sourceID, AL_SOURCE_STATE, &state));
                if (state == AL_STOPPED && state != stream.state)
                {
                    stream.audioFile->seek(cro::Time());
                    stream.processed = static_cast<ALint>(stream.buffers.size());
                }
                stream.state = state;

                if (stream.processed > 0)
                {
                    stream.updating = true;
                    m_pendingUpdates.push_back(&stream);
                }
            }
        }
        m_mutex.unlock();


        //check for new jobs waiting if we have none
        if (m_pendingUpdates.empty())
        {
            std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(20));
        }
        else
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            //update the buffers
            for (auto* stream : m_pendingUpdates)
            {
                for (auto i = 0; i < stream->processed; ++i)
                {
                    //fill buffer
                    auto data = stream->audioFile->getData(STREAM_CHUNK_SIZE, stream->looped);
                    if (data.size > 0) //only update if we have data else we'll loop even if we don't want to
                    {
                        //unqueue
                        alCheck(alSourceUnqueueBuffers(stream->sourceID, 1, &stream->buffers[stream->currentBuffer]));

                        //refill
                        alCheck(alBufferData(stream->buffers[stream->currentBuffer], getFormatFromData(data), data.data, data.size, data.frequency));

                        //requeue
                        alCheck(alSourceQueueBuffers(stream->sourceID, 1, &stream->buffers[stream->currentBuffer]));

                        //increment currentBuffer
                        stream->currentBuffer = (stream->currentBuffer + 1) % stream->buffers.size();
                    }
                }

                stream->updating = false;
                stream->processed = 0;
            }
            m_pendingUpdates.clear();
        }
    }
}