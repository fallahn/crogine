/*-----------------------------------------------------------------------

Matt Marchant 2023
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include "WeatherDirector.hpp"
#include "PacketIDs.hpp"

#include <crogine/util/Random.hpp>

namespace
{
    std::array<cro::Time, 10> Times = { };
}

WeatherDirector::WeatherDirector(net::NetHost& host)
    : m_host        (host),
    m_weatherState  (0),
    m_timeIndex     (cro::Util::Random::value(0u, Times.size() - 1))
{
    for (auto& t : Times)
    {
        t = cro::seconds(static_cast<float>(cro::Util::Random::value(50, 120)));
        //t = cro::seconds(static_cast<float>(cro::Util::Random::value(5, 12)));
    }
}

//public
void WeatherDirector::handleMessage(const cro::Message&)
{

}

void WeatherDirector::process(float)
{
    if (m_timer.elapsed() > Times[m_timeIndex])
    {
        m_timer.restart();
        m_timeIndex = (m_timeIndex + 1) % Times.size();
        m_weatherState = ~m_weatherState;

        m_host.broadcastPacket(PacketID::WeatherChange, m_weatherState, net::NetFlag::Reliable);
    }
}