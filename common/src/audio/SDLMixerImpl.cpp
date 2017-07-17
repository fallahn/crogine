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

#include "SDLMixerImpl.hpp"

using namespace cro;
using namespace cro::Detail;

bool SDLMixerImpl::init()
{
    return false;
}

void SDLMixerImpl::shutdown()
{

}

void SDLMixerImpl::setListenerPosition(glm::vec3 position)
{

}

void SDLMixerImpl::setListenerOrientation(glm::vec3 forward, glm::vec3 up)
{

}

void SDLMixerImpl::setListenerVolume(float volume)
{

}

cro::int32 SDLMixerImpl::requestNewBuffer(const std::string& path)
{
    return -1;
}

void SDLMixerImpl::deleteBuffer(cro::int32 buffer)
{

}

cro::int32 SDLMixerImpl::requestAudioSource(cro::int32 buffer)
{
    return -1;
}

void SDLMixerImpl::deleteAudioSource(cro::int32 source)
{

}

void SDLMixerImpl::playSource(cro::int32 source, bool looped)
{

}

void SDLMixerImpl::pauseSource(cro::int32 source)
{

}

void SDLMixerImpl::stopSource(cro::int32 source)
{

}

int32 SDLMixerImpl::getSourceState(int32 source) const
{
    return 2;
}

void SDLMixerImpl::setSourcePosition(int32 source, glm::vec3 position)
{

}

void SDLMixerImpl::setSourcePitch(int32 src, float pitch)
{

}

void SDLMixerImpl::setSourceVolume(int32 src, float vol)
{

}

void SDLMixerImpl::setSourceRolloff(int32 src, float rolloff)
{

}