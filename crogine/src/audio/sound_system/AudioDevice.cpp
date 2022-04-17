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

//see header file for notes on SFML derivation

#include "../ALCheck.hpp"
#include "AudioDevice.hpp"

#include <crogine/core/Log.hpp>

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
using namespace cro::Detail;

namespace
{
    ALCdevice* audioDevice = nullptr;
    ALCcontext* audioContext = nullptr;

    float globalVolume = 1.f;
}

AudioDevice::AudioDevice()
{
    audioDevice = alcOpenDevice(nullptr);

    if (audioDevice)
    {
        audioContext = alcCreateContext(audioDevice, nullptr);

        if (audioContext)
        {
            alcMakeContextCurrent(audioContext);
            setGlobalVolume(1.f);
        }
        else
        {
            LogE << "Failed creating audio context" << std::endl;
        }
    }
    else
    {
        LogE << "Failed opening audio device" << std::endl;
    }
}

AudioDevice::~AudioDevice()
{
    alcMakeContextCurrent(nullptr);

    if (audioContext != nullptr)
    {
        alcDestroyContext(audioContext);
        audioContext = nullptr;
    }

    if (audioDevice != nullptr)
    {
        alcCloseDevice(audioDevice);
        audioDevice = nullptr;
    }
}

void AudioDevice::setGlobalVolume(float vol)
{
    if (audioContext != nullptr)
    {
        alCheck(alListenerf(AL_GAIN, vol));
    }

    globalVolume = vol;
}

float AudioDevice::getGlobalVolume()
{
    return globalVolume;
}