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

#include "OpenALImpl.hpp"
#include "ALCheck.hpp"
#include "WavLoader.hpp"
#include "VorbisLoader.hpp"
#include "Mp3Loader.hpp"

#include <crogine/detail/Assert.hpp>
#include <crogine/util/String.hpp>
#include <crogine/core/App.hpp>
#include <crogine/core/FileSystem.hpp>
#include <crogine/gui/Gui.hpp>

//oh apple you so quirky
#ifdef __APPLE__
//silence deprecated openal warnings
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
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
    m_nextFreeSource    (0)
{
    for (auto i = 0u; i < m_streamIDs.size(); ++i)
    {
        m_streamIDs[i] = i;
    }
}


//public
bool OpenALImpl::init()
{
    //creates the ImGui window to choose a preferred device
    //and loads the config if it's found
    enumerateDevices();

    if (!m_preferredDevice.empty()
        && m_preferredDevice != "Default")
    {
        m_device = alcOpenDevice(m_preferredDevice.c_str());

        if (!m_device)
        {
            LogW << "Unable to open preferred device " << m_preferredDevice << ". Trying default device." << std::endl;
        }
    }

    if (!m_device)
    {
        /*alcCheck*/(m_device = alcOpenDevice(nullptr));
        if (!m_device)
        {
            LOG("Failed opening valid OpenAL device", Logger::Type::Error);
            return false;
        }
    }
    
    /*alcCheck*/(m_context = alcCreateContext(m_device, nullptr));
    if (!m_context)
    {
        LOG("Failed creating OpenAL context", Logger::Type::Error);
        return false;
    }

    bool current = false;
    /*alcCheck*/(current = alcMakeContextCurrent(m_context));

    return current;
}

void OpenALImpl::shutdown()
{
    unregisterWindows();
    removeCommands();

    for (auto i = 0u; i < m_nextFreeSource; ++i)
    {
        deleteAudioSource(m_sourcePool[i]);
    }
    alCheck(alDeleteSources(static_cast<ALsizei>(m_sourcePool.size()), m_sourcePool.data()));

    //make sure to close any open streams
    for (auto i = 0u; i < m_streams.size(); ++i)
    {
        if (m_streams[i].sourceID > 0)
        {
            alCheck(alSourceStop(m_streams[i].sourceID));
        }
        deleteStream(i);
    }

    alcCheck(alcMakeContextCurrent(nullptr), m_device);
    alcCheck(alcDestroyContext(m_context), m_device);
    alcCheck(alcCloseDevice(m_device), m_device);
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

glm::vec3 OpenALImpl::getListenerPosition() const
{
    glm::vec3 ret(0.f);
    alCheck(alGetListenerfv(AL_POSITION, &ret[0]));
    return ret;
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
    else if (ext == ".mp3")
    {
        loader = std::make_unique<Mp3Loader>();
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
    //new streams create their own buffers so no need to sync here
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
        //streams delete their own buffers so no need to sync here

        //hm. Code smell.
        auto buf = static_cast<ALuint>(buffer);
        alCheck(alDeleteBuffers(1, &buf));
    }
}

std::int32_t OpenALImpl::requestNewStream(const std::string& path)
{
    //check we have available streams
    if (m_nextFreeStream >= m_streams.size())
    {
        Logger::log("Maximum number of streams has been reached!", Logger::Type::Warning);
        return -1;
    }

    //we shouldn't have to lock here as the stream's thread has not yet been created
    auto streamID = m_streamIDs[m_nextFreeStream];

    //attempt to open the file
    auto& stream = m_streams[streamID];
    CRO_ASSERT(!stream.thread, "this shouldn't be running yet!");

    auto filePath = cro::FileSystem::getResourcePath() + path;

    auto ext = FileSystem::getFileExtension(path);
    if (ext == ".wav")
    {
        stream.audioFile = std::make_unique<WavLoader>();
        if (!stream.audioFile->open(filePath))
        {
            stream.audioFile.reset();
            Logger::log("Failed to open " + path, Logger::Type::Error);
            return -1;
        }
    }
    else if (ext == ".ogg")
    {
        stream.audioFile = std::make_unique<VorbisLoader>();
        if (!stream.audioFile->open(filePath))
        {
            stream.audioFile.reset();
            Logger::log("Failed to open " + path, Logger::Type::Error);
            return -1;
        }
    }
    else if (ext == ".mp3")
    {
        stream.audioFile = std::make_unique<Mp3Loader>();
        if (!stream.audioFile->open(filePath))
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
        //fill buffers from file - first buffer is queued when the source is attached
        for (auto b : stream.buffers)
        {
            auto& audioData = stream.audioFile->getData(STREAM_CHUNK_SIZE);
            alCheck(alBufferData(b, getFormatFromData(audioData), audioData.data, audioData.size, audioData.frequency));
        }

        stream.running = true;
        stream.thread = std::make_unique<std::thread>(&OpenALStream::updateStream, &stream);

        //hurrah we has stream
        m_nextFreeStream++;

        return streamID;
    }
    return -1;
}

void OpenALImpl::deleteStream(std::int32_t id)
{
    auto& stream = m_streams[id];
    if (stream.sourceID > 0)
    {
        alCheck(alSourceStop(stream.sourceID));
    }
    stream.running = false;

    if (stream.thread)
    {
        //we can't just detach as we need to reset the
        //thread pointer.
        stream.thread->join();
        stream.thread.reset();
    }

    if (stream.buffers[0])
    {
        std::int32_t processed = 0;
        alCheck(alGetSourcei(stream.sourceID, AL_BUFFERS_PROCESSED, &processed));
        alCheck(alSourceUnqueueBuffers(stream.sourceID, processed, stream.buffers.data()));

        alCheck(alDeleteBuffers(static_cast<ALsizei>(stream.buffers.size()), stream.buffers.data()));
        std::fill(stream.buffers.begin(), stream.buffers.end(), 0);
        LOG("Deleted audio stream", Logger::Type::Info);

        stream.audioFile.reset();
        stream.currentBuffer = 0;
        stream.sourceID = -1;

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

    ALuint source = 0;
    
    if (!streaming)
    {
        source = getSource();
    }
    else
    {
        alCheck(alGenSources(1, &source));
    }

    if (source > 0)
    {
        if (!streaming)
        {
            alCheck(alSourcei(source, AL_BUFFER, buffer));
        }
        else
        {
            //sync with the stream thread...
            auto& stream = m_streams[buffer];
            while (stream.busy) {}

            stream.accessed = true;
            stream.sourceID = source;
            alCheck(alSourceQueueBuffers(source, static_cast<ALsizei>(stream.buffers.size()), stream.buffers.data()));
            stream.accessed = false;
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
    stopSource(sourceID);

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

        while (stream.busy) {}

        stream.accessed = true;
        stream.sourceID = sourceID;
        alCheck(alSourceQueueBuffers(sourceID, static_cast<ALsizei>(stream.buffers.size()), stream.buffers.data()));
        stream.accessed = false;
    }
}

void OpenALImpl::deleteAudioSource(std::int32_t source)
{
    CRO_ASSERT(source > 0, "Invalid source ID");

    //if the src is playing stop it and wait for it to finish
    stopSource(source);

    auto src = static_cast<ALuint>(source);
    ALenum state;
    alCheck(alGetSourcei(src, AL_SOURCE_STATE, &state));
    while (state == AL_PLAYING)
    {
        alCheck(alGetSourcei(src, AL_SOURCE_STATE, &state));
    }

    //unbind current buffer
    alCheck(alSourcei(src, AL_BUFFER, 0));

    //if this is associated with a stream, delete the stream
    //and return it to the pool
    auto result = std::find_if(std::begin(m_streams), std::end(m_streams),
        [source](const OpenALStream& str)
    {
        return str.sourceID == source;
    });

    if (result != m_streams.end())
    {
        std::int32_t idx = static_cast<std::int32_t>(std::distance(std::begin(m_streams), result));
        deleteStream(idx);
        alCheck(alDeleteSources(1, &src));
    }
    else
    {
        freeSource(src);
    }
}

void OpenALImpl::playSource(std::int32_t source, bool looped)
{
    //OpenAL is supposed to be thread safe according to the spec
    //so we only sync stream threads if we actually touch a stream
    //object and modify it.

    ALuint src = static_cast<ALuint>(source);

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
        while (result->busy) {}

        result->accessed = true;
        result->looped = looped;
        result->accessed = false;
    }
    alCheck(alSourcePlay(src));
}

void OpenALImpl::pauseSource(std::int32_t source)
{
    //recasting to unsigned is a bit smelly...
    ALuint src = static_cast<ALuint>(source);
    alCheck(alSourcePause(src));
}

void OpenALImpl::stopSource(std::int32_t source)
{
    ALuint src = static_cast<ALuint>(source);
    alCheck(alSourceStop(src));
}

void OpenALImpl::setPlayingOffset(std::int32_t source, cro::Time offset)
{
    if (getSourceState(source) == 2)
    {
        return;
    }

    ALuint src = static_cast<ALuint>(source);

    auto result = std::find_if(std::begin(m_streams), std::end(m_streams),
        [src](const OpenALStream& str)
        {
            return str.sourceID == src;
        });

    if (result == m_streams.end())
    {
        alCheck(alSourcef(src, AL_SEC_OFFSET, offset.asSeconds()));
    }
    else
    {
        while (result->busy) {}

        result->accessed = true;
        result->audioFile->seek(offset);
        result->accessed = false;
    }
}

std::int32_t OpenALImpl::getSourceState(std::int32_t source) const
{
    ALuint src = static_cast<ALuint>(source);
    ALenum state;
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
    alCheck(alSource3f(source, AL_POSITION, position.x, position.y, position.z));
    //DPRINT("source Pos", std::to_string(position.x) + ", " + std::to_string(position.y) + ", " + std::to_string(position.z));
}

void OpenALImpl::setSourcePitch(std::int32_t src, float pitch)
{
    alCheck(alSourcef(src, AL_PITCH, pitch));
}

void OpenALImpl::setSourceVolume(std::int32_t src, float volume)
{
    alCheck(alSourcef(src, AL_GAIN, volume));
}

void OpenALImpl::setSourceRolloff(std::int32_t src, float rolloff)
{
    alCheck(alSourcef(src, AL_ROLLOFF_FACTOR, rolloff));
}

void OpenALImpl::setSourceVelocity(std::int32_t src, glm::vec3 velocity)
{
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

void OpenALImpl::printDebug()
{
    ImGui::Text("Source Cache Size %lu", m_sourcePool.size());
    ImGui::Text("Sources In Use %lu", m_nextFreeSource);
}

//private
ALuint OpenALImpl::getSource()
{
    if (m_nextFreeSource == 256)
    {
        return 0;
    }

    if (m_nextFreeSource == m_sourcePool.size())
    {
        //does the actual allocation
        resizeSourcePool();
    }

    return m_sourcePool[m_nextFreeSource++];
}

void OpenALImpl::freeSource(ALuint source)
{
    //need to reset params incase we get recycled
    alCheck(alSourcei(source, AL_LOOPING, AL_FALSE));


    for (auto i = 0u; i < m_nextFreeSource; ++i)
    {
        if (m_sourcePool[i] == source)
        {
            m_nextFreeSource--;
            m_sourcePool[i] = m_sourcePool[m_nextFreeSource];
            m_sourcePool[m_nextFreeSource] = source;
            break;
        }
    }
}

void OpenALImpl::resizeSourcePool()
{
    auto startIndex = m_sourcePool.size();

    if (startIndex < 256u) //OpenAL-soft limit
    {
        m_sourcePool.resize(m_sourcePool.size() + SourceResizeCount);

        alCheck(alGenSources(SourceResizeCount, &m_sourcePool[startIndex]));
    }
}

void OpenALImpl::enumerateDevices()
{
    //check first - this probably doesn't work on mac for example
    auto enumAvailable = alcIsExtensionPresent(nullptr, "ALC_ENUMERATION_EXT");
    if (enumAvailable)
    {
        const auto* deviceList = alcGetString(nullptr, ALC_ALL_DEVICES_SPECIFIER);

        if (deviceList)
        {
            char* pp = SDL_GetPrefPath("Trederia", "common");
            auto prefPath = std::string(pp);
            SDL_free(pp);
            std::replace(prefPath.begin(), prefPath.end(), '\\', '/');
            prefPath += u8"audio_device.cfg";

            auto* next = deviceList + 1;
            std::size_t len = 0;

            while (deviceList && *deviceList != '\0'
                && next
                && *next != '\0')
            {
                m_devices.push_back(std::string(deviceList));
                len = strlen(deviceList);
                deviceList += (len + 1);
                next += (len + 2);
            }
            m_devices.push_back("Default");

            static bool showWindow = false;

            registerCommand("al_config",
                [](const std::string)
                {
                    showWindow = !showWindow;
                });

            registerWindow(
                [&, prefPath]()
                {
                    if (showWindow)
                    {
                        if (ImGui::Begin("Default Audio Device", &showWindow, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
                        {
                            std::vector<const char*> items; //lol.
                            for (const auto& d : m_devices)
                            {
                                items.push_back(d.c_str());
                            }

                            static std::int32_t idx = 0;
                            if (ImGui::BeginListBox("##", ImVec2(-FLT_MIN, 0.f)))
                            {
                                for (auto n = 0u; n < items.size(); ++n)
                                {
                                    const bool selected = (idx == n);
                                    if (ImGui::Selectable(items[n], selected))
                                    {
                                        idx = n;
                                    }

                                    if (selected)
                                    {
                                        ImGui::SetItemDefaultFocus();
                                    }
                                }
                                ImGui::EndListBox();
                            }

                            if (!m_preferredDevice.empty())
                            {
                                ImGui::Text("Current Device: %s", m_preferredDevice.c_str());
                            }
                            else
                            {
                                ImGui::Text("Current Device: Default");
                            }

                            if (ImGui::Button("Make Selected Preferred"))
                            {
                                m_preferredDevice = m_devices[idx];

                                ConfigFile cfg;
                                cfg.addProperty("preferred_device", m_preferredDevice);
                                cfg.save(prefPath);
                            }
                            ImGui::SameLine();
                            ImGui::Text("(Takes effect after restart)");
                        }
                        ImGui::End();
                    }
                });

            //look for device config and load if found
            ConfigFile cfg;
            if (cfg.loadFromFile(prefPath))
            {
                const auto& props = cfg.getProperties();
                for (const auto& prop : props)
                {
                    if (prop.getName() == "preferred_device")
                    {
                        m_preferredDevice = prop.getValue<std::string>();
                    }
                }
            }
        }
    }
}

//stream thread function
void OpenALStream::updateStream()
{
    //this is called by a stream's thread
    while (running)
    {
        //wait for the main thread to finish what it's
        //doing to the stream
        while (accessed) {}

        //check if we need update
        busy = true;

        if (sourceID > -1)
        {
            std::int32_t processed = 0;
            alCheck(alGetSourcei(sourceID, AL_BUFFERS_PROCESSED, &processed));

            //if stopped rewind file and load buffers
            ALenum newState;
            alCheck(alGetSourcei(sourceID, AL_SOURCE_STATE, &newState));
            if (newState != state && newState == AL_STOPPED)
            {
                audioFile->seek(cro::Time());
                processed = static_cast<ALint>(buffers.size());
            }

            //update the buffers if necessary
            if (processed > 0
                && state == AL_PLAYING)
            {
                for (auto i = 0; i < processed; ++i)
                {
                    //fill buffer
                    const auto& data = audioFile->getData(STREAM_CHUNK_SIZE, looped);
                    if (data.size > 0) //only update if we have data else we'll loop even if we don't want to
                    {
                        //unqueue
                        alCheck(alSourceUnqueueBuffers(sourceID, 1, &buffers[currentBuffer]));

                        //refill
                        alCheck(alBufferData(buffers[currentBuffer], getFormatFromData(data), data.data, data.size, data.frequency));

                        //requeue
                        alCheck(alSourceQueueBuffers(sourceID, 1, &buffers[currentBuffer]));

                        //increment currentBuffer
                        currentBuffer = (currentBuffer + 1) % buffers.size();
                    }
                }

                busy = false;
                processed = 0;
            }
            else
            {
                //sleep for a bit
                //it's important to sleep *after* the busy flag is set to
                //false as this is the time the main thread might set the 'accessed' flag
                busy = false;
                std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(50));
            }
            state = newState;
        }
        else
        {
            //sleep for a bit
            busy = false;
            std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(50));
        }
    }
}
