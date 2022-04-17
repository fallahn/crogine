/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include "../ALCheck.hpp"

#include <crogine/audio/sound_system/SoundSource.hpp>

#ifdef __APPLE__
#include <al.h>
#include <alc.h>
//silence deprecated openal warnings
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

using namespace cro;

SoundSource::SoundSource()
    : m_alSource    (0)
{
    alCheck(alGenSources(1, &m_alSource));
    alCheck(alSourcei(m_alSource, AL_BUFFER, 0));
}

SoundSource::SoundSource(const SoundSource& other)
    : SoundSource()
{
    setVolume(other.getVolume());
}

SoundSource& SoundSource::operator=(const SoundSource& rhs)
{
    setVolume(rhs.getVolume());

    return *this;
}

SoundSource::~SoundSource()
{
    alCheck(alSourcei(m_alSource, AL_BUFFER, 0));
    alCheck(alDeleteSources(1, &m_alSource));
}

//public
void SoundSource::setVolume(float vol) 
{
    alCheck(alSourcef(m_alSource, AL_GAIN, vol));
}

float SoundSource::getVolume() const
{
    ALfloat ret = 0.f;
    alCheck(alGetSourcef(m_alSource, AL_GAIN, &ret));

    return ret;
}

SoundSource::Status SoundSource::getStatus() const
{
    ALint status;
    alCheck(alGetSourcei(m_alSource, AL_SOURCE_STATE, &status));

    switch (status)
    {
    default:
    case AL_INITIAL:
    case AL_STOPPED:
        return Status::Stopped;
    case AL_PAUSED:
        return Status::Paused;
    case AL_PLAYING:
        return Status::Playing;
    }

    return Status::Stopped;
}