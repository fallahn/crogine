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

#include "OpenALImpl.hpp"
#include "ALCheck.hpp"
#include "WavLoader.hpp"

#include <crogine/detail/Assert.hpp>
#include <crogine/util/String.hpp>
#include <crogine/core/App.hpp>

#include <AL/alc.h>

#include <array>

using namespace cro;
using namespace cro::Detail;


OpenALImpl::OpenALImpl()
    : m_device  (nullptr),
    m_context   (nullptr)
{

}


//public
bool OpenALImpl::init()
{
    alCheck(m_device = alcOpenDevice(nullptr));
    if (!m_device)
    {
        return false;
    }

    alCheck(m_context = alcCreateContext(m_device, nullptr));
    if (!m_context)
    {
        return false;
    }

    bool current = false;
    alCheck(current = alcMakeContextCurrent(m_context));

    return current;
}

void OpenALImpl::shutdown()
{
    alCheck(alcMakeContextCurrent(nullptr));
    alCheck(alcDestroyContext(m_context));
    alCheck(alcCloseDevice(m_device));
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

cro::int32 OpenALImpl::requestNewBuffer(const std::string& path)
{
    WavLoader loader;
    
    auto ext = Util::String::getFileExtension(path);
    PCMData data;
    if (ext == ".wav")
    {      
        if (loader.open(path))
        {
            data = loader.getData();
        }
    }
    else
    {
        Logger::log(ext + ": format not supported", Logger::Type::Error);
    }

    if(data.data)
    {
        ALuint buff;
        alCheck(alGenBuffers(1, &buff));
        
        ALenum format;
        switch (data.format)
        {
        default:
        case PCMData::Format::MONO8:
            format = AL_FORMAT_MONO8;
            break;
        case PCMData::Format::MONO16:
            format = AL_FORMAT_MONO16;
            break;
        case PCMData::Format::STEREO8:
            format = AL_FORMAT_STEREO8;
            break;
        case PCMData::Format::STEREO16:
            format = AL_FORMAT_STEREO16;
            break;
        }

        alCheck(alBufferData(buff, format, data.data, data.size, data.frequency));

        return buff;
    }
    
    return -1;
}

void OpenALImpl::deleteBuffer(cro::int32 buffer)
{
    if (buffer > 0)
    {
        auto buf = static_cast<ALuint>(buffer);
        alCheck(alDeleteBuffers(1, &buf));
        LOG("Deleted audio buffer", Logger::Type::Info);
    }
}

cro::int32 OpenALImpl::requestAudioSource(cro::int32 buffer)
{
    CRO_ASSERT(buffer > 0, "Invalid audio buffer");
    ALuint source;
    alCheck(alGenSources(1, &source));
    if (source > 0)
    {
        alCheck(alSourcei(source, AL_BUFFER, buffer));
        return source;
    }
    return -1;
}

void OpenALImpl::deleteAudioSource(cro::int32 source)
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

    alCheck(alSourcei(src, AL_BUFFER, 0));
    alCheck(alDeleteSources(1, &src));

    LOG("Deleted audio source", Logger::Type::Info);
}

void OpenALImpl::playSource(int32 source, bool looped)
{
    ALuint src = static_cast<ALuint>(source);
    alCheck(alSourcei(src, AL_LOOPING, looped ? AL_TRUE : AL_FALSE));
    alCheck(alSourcePlay(src));
}

void OpenALImpl::pauseSource(int32 source)
{
    ALuint src = static_cast<ALuint>(source);
    alCheck(alSourcePause(src));
}

void OpenALImpl::stopSource(int32 source)
{
    ALuint src = static_cast<ALuint>(source);
    alCheck(alSourceStop(src));
}

int32 OpenALImpl::getSourceState(int32 source) const
{
    ALuint src = static_cast<ALuint>(source);
    ALenum state;
    alCheck(alGetSourcei(src, AL_SOURCE_STATE, &state));

    if (state == AL_PLAYING) return 0;
    if (state == AL_PAUSED) return 1;
    return 2;
}

void OpenALImpl::setSourcePosition(int32 source, glm::vec3 position)
{
    alCheck(alSource3f(source, AL_POSITION, position.x, position.y, position.z));
    //DPRINT("source Pos", std::to_string(position.x) + ", " + std::to_string(position.y) + ", " + std::to_string(position.z));
}

void OpenALImpl::setSourcePitch(int32 src, float pitch)
{
    alCheck(alSourcef(src, AL_PITCH, pitch));
}

void OpenALImpl::setSourceVolume(int32 src, float volume)
{
    alCheck(alSourcef(src, AL_GAIN, volume));
}

void OpenALImpl::setSourceRolloff(int32 src, float rolloff)
{
    alCheck(alSourcef(src, AL_ROLLOFF_FACTOR, rolloff));
}