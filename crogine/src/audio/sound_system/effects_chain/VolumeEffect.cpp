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

#include <crogine/audio/sound_system/effects_chain/VolumeEffect.hpp>

#include <algorithm>
#include <cmath>

using namespace cro;

VolumeEffect::VolumeEffect()
    : m_gain        (1.f),
    m_peakLeft      (0.f),
    m_peakRight     (0.f),
    m_callbackCount (0)
{

}

//public
void VolumeEffect::setGain(float gain)
{
    m_gain = std::clamp(gain, 0.f, 1.5f);
}

void VolumeEffect::process(std::vector<float>& buffer)
{
    m_VU[0] = m_VU[1] = 0.f;
    
    for (auto i = 0u; i < buffer.size(); i += getChannelCount())
    {
        buffer[i] *= m_gain;
        m_VU[0] += std::abs(buffer[i]);

        if (getChannelCount() == 2)
        {
            buffer[i+1] *= m_gain;
            m_VU[1] += std::abs(buffer[i + 1]);
        }
    }

    m_VU[0] /= (buffer.size() / getChannelCount()); 
    m_VU[1] /= (buffer.size() / getChannelCount());

    tick(buffer.size());
}

float VolumeEffect::getVUDecibelsLeft() const
{
    //can't log10 zero
    return std::log10(m_VU[0] + 0.00001f) * 20.f;
}

float VolumeEffect::getVUDecibelsRight() const
{
    return std::log10(m_VU[1] + 0.00001f) * 20.f;
}

void VolumeEffect::reset()
{
    m_peakLeft = m_peakRight = m_VU[0] = m_VU[1] = 0.f;
    m_callbackCount = 0;
}

//private
void VolumeEffect::processEffect()
{
    m_callbackCount++;
    if (m_callbackCount == 60)
    {
        m_peakLeft = m_VU[0];
        m_peakRight = m_VU[1];

        m_callbackCount = 0;
    }
}