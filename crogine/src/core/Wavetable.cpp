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

#include <crogine/core/Wavetable.hpp>
#include <crogine/util/Wavetable.hpp>

#include <crogine/detail/Assert.hpp>

using namespace cro;

Wavetable::Wavetable(Waveform waveform, float frequency, float amplitude, float updateRate)
    : m_timestep    (1.f / updateRate),
    m_accumulator   (0.f),
    m_currentIndex  (0)
{
    CRO_ASSERT(waveform == Waveform::Sine, "Other waveforms not yet implemented");

    m_wavetable = Util::Wavetable::sine(frequency, amplitude, updateRate);
}

Wavetable::Wavetable(const std::vector<float>& data, float updaterate)
    : m_timestep    (1.f / updaterate),
    m_accumulator   (0.f),
    m_currentIndex  (0)
{
    m_wavetable = data;
}

float Wavetable::fetch(Time dt) const
{
    CRO_ASSERT(!m_wavetable.empty(), "Invalid wavetable data");
    
    m_accumulator  += dt.asSeconds();
    while (m_accumulator > m_timestep)
    {
        m_accumulator -= m_timestep;
        m_currentIndex = (m_currentIndex + 1) % m_wavetable.size();
    }

    return m_wavetable[m_currentIndex];
}