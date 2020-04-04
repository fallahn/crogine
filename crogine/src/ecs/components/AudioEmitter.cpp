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

#include <crogine/ecs/components/AudioEmitter.hpp>
#include <crogine/audio/AudioBuffer.hpp>
#include <crogine/audio/AudioStream.hpp>
#include <crogine/audio/AudioMixer.hpp>
#include <crogine/detail/Assert.hpp>

#include "../../audio/AudioRenderer.hpp"

#include <algorithm>

using namespace cro;

AudioEmitter::AudioEmitter()
    : m_state           (State::Stopped),
    m_pitch             (1.f),
    m_volume            (1.f),
    m_rolloff           (1.f),
    m_mixerChannel      (0),
    m_transportFlags    (0),
    m_newDataSource     (false),
    m_ID                (-1),
    m_dataSourceID      (-1),
    m_sourceType        (AudioSource::Type::None)
{

}

AudioEmitter::AudioEmitter(const AudioSource& dataSource)
    : m_state           (State::Stopped),
    m_pitch             (1.f),
    m_volume            (1.f),
    m_rolloff           (1.f),
    m_mixerChannel      (0),
    m_transportFlags    (0),
    m_newDataSource     (true),
    m_ID                (-1),
    m_dataSourceID      (dataSource.getID()),
    m_sourceType        (dataSource.getType())
{

}

AudioEmitter::~AudioEmitter()
{
    if (m_ID > 0)
    {
        AudioRenderer::deleteAudioEmitter(m_ID);
    }
}

AudioEmitter::AudioEmitter(AudioEmitter&& other) noexcept
{
    m_pitch = other.m_pitch;
    other.m_pitch = 1.f;

    m_volume = other.m_volume;
    other.m_volume = 1.f;

    m_rolloff = other.m_rolloff;
    other.m_rolloff = 1.f;
    
    m_mixerChannel = other.m_mixerChannel;
    other.m_mixerChannel = 0;

    m_ID = other.m_ID;
    other.m_ID = -1;

    m_dataSourceID = other.m_dataSourceID;
    other.m_dataSourceID = -1;

    m_sourceType = other.m_sourceType;
    other.m_sourceType = AudioSource::Type::None;

    m_newDataSource = true;
    other.m_newDataSource = false;

    m_transportFlags = other.m_transportFlags;
    other.m_transportFlags = 0;

    other.m_state = State::Stopped;
}

AudioEmitter& AudioEmitter::operator=(AudioEmitter&& other) noexcept
{
    if (&other != this)
    {
        m_pitch = other.m_pitch;
        other.m_pitch = 1.f;

        m_volume = other.m_volume;
        other.m_volume = 1.f;

        m_rolloff = other.m_rolloff;
        other.m_rolloff = 1.f;
        
        m_mixerChannel = other.m_mixerChannel;
        other.m_mixerChannel = 0;

        m_ID = other.m_ID;
        other.m_ID = -1;

        m_dataSourceID = other.m_dataSourceID;
        other.m_dataSourceID = -1;

        m_sourceType = other.m_sourceType;
        other.m_sourceType = AudioSource::Type::None;

        m_newDataSource = true;
        other.m_newDataSource = false;

        other.m_state = State::Stopped;
    }    
    return *this;
}

//public
void AudioEmitter::setSource(const AudioSource& dataSource)
{
    if (m_ID > 0)
    {
        AudioRenderer::deleteAudioEmitter(m_ID);
        m_ID = -1;
    }

    m_dataSourceID = dataSource.getID();
    m_sourceType = dataSource.getType();
    m_newDataSource = true;
}

void AudioEmitter::play(bool looped)
{
    m_transportFlags |= Play;
    if (looped)
    {
        m_transportFlags |= Looped;
    }
}

void AudioEmitter::pause()
{
    m_transportFlags |= Pause;
}

void AudioEmitter::stop()
{
    m_transportFlags |= Stop;
}

void AudioEmitter::setPitch(float pitch)
{
    m_pitch = std::max(0.f, pitch);
}

void AudioEmitter::setVolume(float volume)
{
    m_volume = std::max(0.f, volume);
}

void AudioEmitter::setRolloff(float rolloff)
{
    m_rolloff = std::max(0.f, rolloff);
}

void AudioEmitter::setMixerChannel(uint8 channel)
{
    CRO_ASSERT(channel < AudioMixer::MaxChannels, "Channel value out of range");
    m_mixerChannel = channel; 
}