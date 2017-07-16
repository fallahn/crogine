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

#include <crogine/ecs/components/AudioSource.hpp>
#include <crogine/audio/AudioBuffer.hpp>

#include "../../audio/AudioRenderer.hpp"

using namespace cro;

AudioSource::AudioSource()
    : m_transportFlags  (0),
    m_newBuffer         (false),
    m_ID                (-1),
    m_bufferID          (-1)
{

}

AudioSource::AudioSource(const AudioBuffer& buffer)
    : m_transportFlags(0),
    m_newBuffer(true),
    m_ID(-1),
    m_bufferID(buffer.m_bufferID)
{

}

AudioSource::~AudioSource()
{
    if (m_ID > 0)
    {
        AudioRenderer::deleteAudioSource(m_ID);
    }
}

//public
void AudioSource::setBuffer(const AudioBuffer& buffer)
{
    if (m_ID > 0)
    {
        AudioRenderer::deleteAudioSource(m_ID);
        m_ID = -1;
    }

    m_bufferID = buffer.m_bufferID;
    m_newBuffer = true;
}

void AudioSource::play(bool looped)
{
    m_transportFlags |= Play;
    if (looped)
    {
        m_transportFlags |= Looped;
    }
}

void AudioSource::pause()
{
    m_transportFlags |= Pause;
}

void AudioSource::stop()
{
    m_transportFlags |= Stop;
}