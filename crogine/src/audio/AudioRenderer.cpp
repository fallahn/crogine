/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2024
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

#include "AudioRenderer.hpp"
#include "OpenALImpl.hpp"
//#include "SDLMixerImpl.hpp"
#include "NullImpl.hpp"

#include <crogine/audio/AudioMixer.hpp>
#include <crogine/detail/Assert.hpp>


using namespace cro;
std::unique_ptr<AudioRendererImpl> AudioRenderer::m_impl;

namespace
{
    bool valid = false;
}

bool AudioRenderer::init()
{
#ifdef AL_AUDIO
    m_impl = std::make_unique<Detail::OpenALImpl>();
#elif defined(SDL_AUDIO)
    m_impl = std::make_unique<Detail::SDLMixerImpl>();
    //m_impl = std::make_unique<Detail::OpenALImpl>();
#endif

    valid = m_impl->init();

    if (!valid) m_impl = std::make_unique<Detail::NullImpl>();

    return valid;
}

void AudioRenderer::shutdown()
{
    CRO_ASSERT(m_impl, "Audio not initialised");
    m_impl->shutdown();
}

bool AudioRenderer::isValid()
{
    return valid;
}

void AudioRenderer::setListenerPosition(glm::vec3 position)
{
    m_impl->setListenerPosition(position);
}

void AudioRenderer::setListenerOrientation(glm::vec3 forward, glm::vec3 up)
{
    m_impl->setListenerOrientation(forward, up);
}

void AudioRenderer::setListenerVolume(float volume)
{
    m_impl->setListenerVolume(std::max(0.f, volume));
}

void AudioRenderer::setListenerVelocity(glm::vec3 velocity)
{
    m_impl->setListenerVelocity(velocity);
}

glm::vec3 AudioRenderer::getListenerPosition()
{
    return m_impl->getListenerPosition();
}

std::int32_t AudioRenderer::requestNewBuffer(const std::string& path)
{
    return m_impl->requestNewBuffer(path);
}

std::int32_t AudioRenderer::requestNewBuffer(const Detail::PCMData& data)
{
    return m_impl->requestNewBuffer(data);
}

void AudioRenderer::deleteBuffer(std::int32_t buffer)
{
    m_impl->deleteBuffer(buffer);
}

std::int32_t AudioRenderer::requestNewStream(const std::string& path)
{
    return m_impl->requestNewStream(path);
}

void AudioRenderer::deleteStream(std::int32_t id)
{
    CRO_ASSERT(id > -1, "Must be a valid ID");
    m_impl->deleteStream(id);
}

std::int32_t AudioRenderer::requestAudioSource(std::int32_t buffer, bool streaming)
{
    if (buffer < 0) return -1; //streams are 0 based
    return m_impl->requestAudioSource(buffer, streaming);
}

void AudioRenderer::updateAudioSource(std::int32_t sourceID, std::int32_t bufferID, bool streaming)
{
    if (sourceID > 0 && bufferID > 0)
    {
        m_impl->updateAudioSource(sourceID, bufferID, streaming);
    }
}

void AudioRenderer::deleteAudioSource(std::int32_t source)
{
    if (source > 0)
    {
        m_impl->deleteAudioSource(source);
    }
}

void AudioRenderer::playSource(std::int32_t src, bool loop)
{
    CRO_ASSERT(src > 0, "Not a valid src ID");
    m_impl->playSource(src, loop);
}

void AudioRenderer::pauseSource(std::int32_t src)
{
    CRO_ASSERT(src > 0, "Not a valid src ID");
    m_impl->pauseSource(src);
}

void AudioRenderer::stopSource(std::int32_t src)
{
    CRO_ASSERT(src > 0, "Not a valid src ID");
    m_impl->stopSource(src);
}

void AudioRenderer::setPlayingOffset(std::int32_t src, cro::Time offset)
{
    CRO_ASSERT(src > 0, "Not a valid source ID");
    CRO_ASSERT(offset.asMilliseconds() >= 0, "offset must not be negative");
    m_impl->setPlayingOffset(src, offset);
}

std::int32_t AudioRenderer::getSourceState(std::int32_t src)
{
    if (src < 1)
    {
        return 2;
    }

    return m_impl->getSourceState(src);
}

void AudioRenderer::setSourcePosition(std::int32_t src, glm::vec3 position)
{
    CRO_ASSERT(src > 0, "Not a valid src ID");
    m_impl->setSourcePosition(src, position);
}

void AudioRenderer::setSourcePitch(std::int32_t src, float pitch)
{
    CRO_ASSERT(src > 0, "Not a valid src ID");
    m_impl->setSourcePitch(src, pitch);
}

void AudioRenderer::setSourceVolume(std::int32_t src, float vol)
{
    CRO_ASSERT(src > 0, "Not a valid src ID");
    m_impl->setSourceVolume(src, vol);
}

void AudioRenderer::setSourceRolloff(std::int32_t src, float rolloff)
{
    CRO_ASSERT(src > 0, "Not a valid src ID");
    m_impl->setSourceRolloff(src, rolloff);
}

void AudioRenderer::setSourceVelocity(std::int32_t src, glm::vec3 velocity)
{
    CRO_ASSERT(src > 0, "Not a valid src ID");
    m_impl->setSourceVelocity(src, velocity);
}

void AudioRenderer::setDopplerFactor(float factor)
{
    CRO_ASSERT(factor >= 0, "Must not be negative");
    m_impl->setDopplerFactor(std::max(0.f, factor));
}

void AudioRenderer::setSpeedOfSound(float speed)
{
    CRO_ASSERT(speed > 0, "Must be more than 0");
    m_impl->setSpeedOfSound(std::max(0.01f, speed));
}

const std::string& AudioRenderer::getActiveDevice()
{
    if (m_impl)
    {
        return m_impl->getActiveDevice();
    }

    static std::string s;
    return s;
}

const std::vector<std::string>& AudioRenderer::getDeviceList()
{
    if (m_impl)
    {
        return m_impl->getDeviceList();
    }

    static std::vector<std::string> s;
    return s;
}

void AudioRenderer::setActiveDevice(const std::string& dev)
{
    if (m_impl)
    {
        m_impl->setActiveDevice(dev);
    }
}

void AudioRenderer::onPlaybackDisconnect()
{
    if (m_impl)
    {
        m_impl->playbackDisconnectEvent();
    }
}

void AudioRenderer::onRecordDisconnect()
{
    if (m_impl)
    {
        m_impl->recordDisconnectEvent();
    }
}

void AudioRenderer::resume() 
{
    if (m_impl)
    {
        m_impl->resume();
    }
}

void AudioRenderer::printDebug()
{
    CRO_ASSERT(m_impl, "");
    m_impl->printDebug();
}