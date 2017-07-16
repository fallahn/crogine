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

#include "AudioRenderer.hpp"
#include "OpenALImpl.hpp"
#include "SDLMixerImpl.hpp"

#include <crogine/detail/Assert.hpp>


using namespace cro;
std::unique_ptr<AudioRendererImpl> AudioRenderer::m_impl;

bool AudioRenderer::init()
{
#ifdef AL_AUDIO
    m_impl = std::make_unique<Detail::OpenALImpl>();
#elif defined(SDL_AUDIO)
    m_impl = std::make_unique<SDLMixerImpl>();
#endif

    return m_impl->init();
}

void AudioRenderer::shutdown()
{
    CRO_ASSERT(m_impl, "Audio not initialised");
    m_impl->shutdown();
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

void AudioRenderer::deleteBuffer(cro::int32 buffer)
{
    m_impl->deleteBuffer(buffer);
}

cro::int32 AudioRenderer::requestAudioSource(cro::int32 buffer)
{
    if (buffer < 1) return -1;
    return m_impl->requestAudioSource(buffer);
}

void AudioRenderer::deleteAudioSource(cro::int32 source)
{
    m_impl->deleteAudioSource(source);
}