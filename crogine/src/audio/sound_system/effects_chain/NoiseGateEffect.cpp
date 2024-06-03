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

#include <crogine/audio/sound_system/effects_chain/NoiseGateEffect.hpp>

#include <cmath>

using namespace cro;

namespace
{
    constexpr std::int32_t ThresholdLimit = 1000;
}

NoiseGateEffect::NoiseGateEffect()
    : m_gain        (0.f),
    m_thresholdCount(0),
    m_thresholdTime (0),
    m_threshold     (0.f),
    m_decay         (0),
    m_decayFrames   (0),
    m_decayCount    (0),
    m_decayStep     (0.f),
    m_attack        (0),
    m_attackFrames  (0),
    m_attackCount   (0),
    m_attackStep    (0.f),
    m_state         (State::Closed)
{

}

//public
void NoiseGateEffect::process(std::vector<float>& data)
{
    switch (m_state)
    {
    default:
        //count the number of samples above or below thresh
        for (auto i = 0u; i < data.size(); i += getChannelCount())
        {
            if (std::abs(data[i]) > m_threshold)
            {
                m_thresholdCount = std::min(m_thresholdCount + 1, ThresholdLimit);
            }
            else
            {
                m_thresholdCount = std::max(m_thresholdCount - 1, -ThresholdLimit);
            }

            if (m_state == State::Open
                && m_thresholdCount < -m_thresholdTime)
            {
                //close the gate
                m_decayCount = m_decayFrames;
                m_state = State::Closing;
            }
            else if (m_state == State::Closed
                && m_thresholdCount > m_thresholdTime)
            {
                //open the gate
                m_attackCount = m_attackFrames;
                m_state = State::Opening;
            }
        }        
        break;
    case State::Closing:
        for (auto i = 0u; i < data.size(); i += getChannelCount())
        {
            m_gain = std::max(0.f, m_gain - m_decayStep);
            m_decayCount--;

            if (m_decayCount == 0)
            {
                m_state = State::Closed;
                m_gain = 0.f;
                m_thresholdCount = 0;
                break;
            }
        }
        break;
    case State::Opening:
        for (auto i = 0u; i < data.size(); i += getChannelCount())
        {
            m_gain = std::min(1.f, m_gain + m_attackStep);
            m_attackCount--;

            if (m_attackCount == 0)
            {
                m_state = State::Open;
                m_gain = 1.f;
                m_thresholdCount = 0;
                break;
            }
        }
        break;
    }

    //tick(data.size());

    for (auto& f : data)
    {
        f *= m_gain;
    }
}

void NoiseGateEffect::setDecayTime(std::uint16_t d)
{
    m_decay = std::min(std::uint16_t(1000u), d);

    //use the sample rate to convert to frames
    //need at least one frame to preveny div0
    m_decayFrames = 1 + ((getSampleRate() / 1000) * m_decay);
    m_decayStep = 1.f / m_decayFrames;
}

void NoiseGateEffect::setAttackTime(std::uint16_t a)
{
    m_attack = std::min(std::uint16_t(1000), a);

    //use the sample rate to convert to frames
    m_attackFrames = 1 + ((getSampleRate() / 1000) * m_attack);
    m_attackStep = 1.f / m_attackFrames;
}

void NoiseGateEffect::reset()
{
    m_gain = 0.f;
    m_thresholdCount = 0;
    m_thresholdTime = getSampleRate() / 100;
    m_threshold = 0.f;
    m_state = State::Closed;

    m_decayCount = 0;
    m_attackCount = 0;

    //this recalcs the number of frames for attack/decay as the channels
    //or sample rate may have been updated
    setDecayTime(m_decay);
    setAttackTime(m_attack);
}

//private
void NoiseGateEffect::processEffect()
{
    //TODO this isn't used (we're not calling tick())

    switch (m_state)
    {
    default: break;
    case State::Closed:
        //TODO check if we're above threshold for > X
        //and switch state to opening if so

        /*if ()
        {
            m_attackCount = m_attackFrames;
            m_state = State::Opening;
        }*/

        break;
    case State::Open:
        //TODO check if we dropped below threshold for > X
        //and set to closing if so

        /*if ()
        {
            m_decayCount = m_decayFrames;
            m_state = State::Closing;
        }*/

        break;
    }
}