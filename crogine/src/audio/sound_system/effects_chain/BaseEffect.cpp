/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#include <crogine/detail/Assert.hpp>
#include <crogine/audio/sound_system/effects_chain/BaseEffect.hpp>


using namespace cro;

BaseEffect::BaseEffect()
    : m_sampleRate      (0),
    m_channels          (0),
    m_accumulator       (0),
    m_accumulatorStep   (0)
{

}

//protected
void BaseEffect::setAudioParameters(std::int32_t sampleRate, std::int32_t channelCount)
{
    //actually this may be zero on startup as this function is called with
    //the correct values once the audio device is opened by the recorder
    //CRO_ASSERT(sampleRate != 0 && channelCount > 0 && channelCount < 3, "");
    m_sampleRate = sampleRate;
    m_channels = channelCount;
    m_accumulatorStep = (sampleRate / 60) * channelCount;
}

//private
void BaseEffect::tick(std::size_t tickCount)
{
    //step is calculated as the number of samples
    //in approx 1/60 of a second on construction
    //this means that processEffect() is called
    //at ~60HZ on the INCOMING AUDIO ...
    //ie sample time, not real time

    m_accumulator += static_cast<std::int32_t>(tickCount);
    while (m_accumulator > m_accumulatorStep)
    {
        m_accumulator -= m_accumulatorStep;
        processEffect();
    }
}