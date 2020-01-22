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

#include <crogine/audio/AudioBuffer.hpp>
#include <crogine/detail/Assert.hpp>

#include "AudioRenderer.hpp"
#include "PCMData.hpp"

using namespace cro;

AudioBuffer::AudioBuffer()
{

}

AudioBuffer::~AudioBuffer()
{
    if (getID() > 0)
    {
        AudioRenderer::deleteBuffer(getID());
        setID(-1);
    }
}

AudioBuffer::AudioBuffer(AudioBuffer&& other) noexcept
{
    if (getID() > 0)
    {
        AudioRenderer::deleteBuffer(getID());
        setID(-1);
    }
    
    setID(other.getID());
    other.setID(-1);
}

AudioBuffer& AudioBuffer::operator=(AudioBuffer&& other) noexcept
{
    if (&other != this)
    {
        if (getID() > 0)
        {
            AudioRenderer::deleteBuffer(getID());
            setID(-1);
        }
        
        setID(other.getID());
        other.setID(-1);
    }
    return *this;
}

//public
bool AudioBuffer::loadFromFile(const std::string& path)
{
    if (getID() > 0)
    {
        AudioRenderer::deleteBuffer(getID());
        setID(-1);
    }
    
    setID(AudioRenderer::requestNewBuffer(path));
    return getID() != -1;
}

bool AudioBuffer::loadFromMemory(void* data, uint8 bitDepth, uint32 sampleRate, bool stereo, std::size_t size)
{
    CRO_ASSERT(bitDepth == 8 || bitDepth == 16, "Invalid bitdepth value, must be 8 or 16");

    Detail::PCMData pcmData;
    pcmData.data = data;
    if (stereo)
    {
        pcmData.format = (bitDepth == 8) ? Detail::PCMData::Format::STEREO8 : Detail::PCMData::Format::STEREO16;
    }
    else
    {
        pcmData.format = (bitDepth == 8) ? Detail::PCMData::Format::MONO8 : Detail::PCMData::Format::MONO16;
    }
    pcmData.frequency = sampleRate;
    pcmData.size = static_cast<cro::uint32>(size);
        
    setID(AudioRenderer::requestNewBuffer(pcmData));
    return getID() != -1;
}