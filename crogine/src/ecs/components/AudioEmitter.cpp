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
    m_velocity          (0.f),
    m_mixerChannel      (0),
    m_transportFlags    (0),
    m_newDataSource     (false),
    m_ID                (-1),
    m_dataSourceID      (-1),
    m_sourceType        (AudioSource::Type::None),
    m_audioSource       (nullptr)
{

}

AudioEmitter::AudioEmitter(const AudioSource& dataSource)
    : AudioEmitter()
{
    setSource(dataSource);
}

AudioEmitter::~AudioEmitter()
{
    reset();
}

AudioEmitter::AudioEmitter(AudioEmitter&& other) noexcept
    : AudioEmitter()
{
    m_newDataSource = true;
    other.m_newDataSource = true;

    if (m_audioSource)
    {
        m_audioSource->removeUser(this);
        m_audioSource->addUser(&other);
    }

    std::swap(m_pitch, other.m_pitch);
    std::swap(m_volume, other.m_volume);
    std::swap(m_rolloff, other.m_rolloff);
    std::swap(m_velocity, other.m_velocity);
    std::swap(m_mixerChannel, other.m_mixerChannel);
    std::swap(m_ID, other.m_ID);
    std::swap(m_dataSourceID, other.m_dataSourceID);
    std::swap(m_sourceType, other.m_sourceType);
    std::swap(m_transportFlags, other.m_transportFlags);
    std::swap(m_state, other.m_state);
    std::swap(m_audioSource, other.m_audioSource);

    if (m_audioSource)
    {
        m_audioSource->removeUser(&other);
        m_audioSource->addUser(this);
    }
}

AudioEmitter& AudioEmitter::operator=(AudioEmitter&& other) noexcept
{
    if (&other != this)
    {
        reset();

        m_newDataSource = true;
        other.m_newDataSource = true;

        std::swap(m_pitch, other.m_pitch);
        std::swap(m_volume, other.m_volume);
        std::swap(m_rolloff, other.m_rolloff);
        std::swap(m_velocity, other.m_velocity);
        std::swap(m_mixerChannel, other.m_mixerChannel);
        std::swap(m_ID, other.m_ID);
        std::swap(m_dataSourceID, other.m_dataSourceID);
        std::swap(m_sourceType, other.m_sourceType);
        std::swap(m_transportFlags, other.m_transportFlags);
        std::swap(m_state, other.m_state);
        std::swap(m_audioSource, other.m_audioSource);

        if (m_audioSource)
        {
            m_audioSource->removeUser(&other);
            m_audioSource->addUser(this);
        }
    }    
    return *this;
}

//public
void AudioEmitter::setSource(const AudioSource& dataSource)
{
    if (m_dataSourceID == dataSource.getID())
    {
        //already have this source
        return;
    }

    //remove from existing source
    if (m_audioSource)
    {
        m_audioSource->removeUser(this);
    }

    m_dataSourceID = dataSource.getID();
    m_sourceType = dataSource.getType();
    m_newDataSource = true;

    dataSource.addUser(this);
    m_audioSource = &dataSource;
}

void AudioEmitter::play()
{
    /*
    Using these flags means there's a delay until the next
    AudioSystem update which actually plays the sound, so we
    set the status here so it will return the expected value
    on any immediate state queries. The AudioSystem will then
    update the state with the correct value.
    */
    if (m_state != State::Playing)
    {
        m_transportFlags |= Play;
        m_state = State::Playing;
    }
}

void AudioEmitter::pause()
{
    m_transportFlags |= Pause;
    m_state = State::Paused;
}

void AudioEmitter::stop()
{
    //note that stopping an emitter resets the ID
    //to 0 so we can't call stop on it again
    if (m_ID)
    {
        m_transportFlags |= Stop;
        m_state = State::Stopped;
    }
}

void AudioEmitter::setPlayingOffset(Time offset)
{
    m_transportFlags |= GotoOffset;
    m_playingOffset = offset;
}

void AudioEmitter::setLooped(bool looped)
{
    if (looped)
    {
        m_transportFlags |= Looped;
    }
    else
    {
        m_transportFlags &= ~Looped;
    }
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

void AudioEmitter::setVelocity(glm::vec3 velocity)
{
    m_velocity = velocity;
}

void AudioEmitter::setMixerChannel(std::uint8_t channel)
{
    CRO_ASSERT(channel < AudioMixer::MaxChannels, "Channel value out of range");
    m_mixerChannel = channel; 
}

//private
void AudioEmitter::reset(const AudioSource* parent)
{
    if (m_ID > 0)
    {
        stop();
        AudioRenderer::stopSource(m_ID);
        AudioRenderer::deleteAudioSource(m_ID);
    }

    //might have added a source but not yet assigned
    //our own ID yet
    //and this might be called from an AudioSource dtor
    if (m_audioSource 
        && parent == nullptr)
    {
        m_audioSource->removeUser(this);
        m_audioSource = nullptr;
    }
    //this was called by the buffer being removed
    else if (parent == m_audioSource)
    {
        m_audioSource = nullptr;
    }

    m_state = State::Stopped;
    m_pitch = 1.f;
    m_volume = 1.f;
    m_rolloff = 1.f;
    m_velocity = glm::vec3(0.f);
    m_mixerChannel = 0;
    m_transportFlags = 0;
    m_newDataSource = false;
    m_ID = -1;
    m_dataSourceID = -1;
    m_sourceType = AudioSource::Type::None;
    m_playingOffset = seconds(0.f);
}