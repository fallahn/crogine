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

#include "AudioRenderer.hpp"
#include "OpenALImpl.hpp"
#include "SDLMixerImpl.hpp"
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

cro::int32 AudioRenderer::requestNewBuffer(const std::string& path)
{
    return m_impl->requestNewBuffer(path);
}

cro::int32 AudioRenderer::requestNewBuffer(const Detail::PCMData& data)
{
    return m_impl->requestNewBuffer(data);
}

void AudioRenderer::deleteBuffer(cro::int32 buffer)
{
    m_impl->deleteBuffer(buffer);
}

int32 AudioRenderer::requestNewStream(const std::string& path)
{
    return m_impl->requestNewStream(path);
}

void AudioRenderer::updateStream(int32 id)
{
    CRO_ASSERT(id > -1, "Must be a valid ID");
    m_impl->updateStream(id);
}

void AudioRenderer::deleteStream(int32 id)
{
    CRO_ASSERT(id > 0, "Must be a valid ID");
    m_impl->deleteStream(id);
}

cro::int32 AudioRenderer::requestAudioEmitter(cro::int32 buffer, bool streaming)
{
    if (buffer < 0) return -1; //streams are 0 based
    return m_impl->requestAudioEmitter(buffer, streaming);
}

void AudioRenderer::deleteAudioEmitter(cro::int32 source)
{
    if (source > 0)
    {
        m_impl->deleteAudioEmitter(source);
    }
}

void AudioRenderer::playSource(int32 src, bool loop)
{
    CRO_ASSERT(src > 0, "Not a valid src ID");
    m_impl->playSource(src, loop);
}

void AudioRenderer::pauseSource(int32 src)
{
    CRO_ASSERT(src > 0, "Not a valid src ID");
    m_impl->pauseSource(src);
}

void AudioRenderer::stopSource(int32 src)
{
    CRO_ASSERT(src > 0, "Not a valid src ID");
    m_impl->stopSource(src);
}

int32 AudioRenderer::getSourceState(int32 src)
{
    if (src < 1) return 2;

    return m_impl->getSourceState(src);
}

void AudioRenderer::setSourcePosition(int32 src, glm::vec3 position)
{
    CRO_ASSERT(src > 0, "Not a valid src ID");
    m_impl->setSourcePosition(src, position);
}

void AudioRenderer::setSourcePitch(int32 src, float pitch)
{
    CRO_ASSERT(src > 0, "Not a valid src ID");
    m_impl->setSourcePitch(src, pitch);
}

void AudioRenderer::setSourceVolume(int32 src, float vol)
{
    CRO_ASSERT(src > 0, "Not a valid src ID");
    m_impl->setSourceVolume(src, vol);
}

void AudioRenderer::setSourceRolloff(int32 src, float rolloff)
{
    CRO_ASSERT(src > 0, "Not a valid src ID");
    m_impl->setSourceRolloff(src, rolloff);
}