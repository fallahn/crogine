/*-----------------------------------------------------------------------

Matt Marchant 2020
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include "PacketIDs.hpp"
#include "WeatherDirector.hpp"
#include "GameConsts.hpp"
#include "CommonConsts.hpp"

#include <crogine/network/NetHost.hpp>

#include <crogine/util/Constants.hpp>

namespace
{
    const cro::Time DayNightFrequency = cro::seconds(1.f);
}

WeatherDirector::WeatherDirector(cro::NetHost& host)
    : m_netHost     (host),
    m_dayNightTime  (0.2f)
{

}

//public
void WeatherDirector::handleMessage(const cro::Message&)
{

}

void WeatherDirector::process(float dt)
{
    m_dayNightTime = std::fmod(m_dayNightTime + ((RadsPerSecond * dt) / cro::Util::Const::TAU), 1.f);

    if (m_dayNightClock.elapsed() > DayNightFrequency)
    {
        m_dayNightClock.restart();

        m_netHost.broadcastPacket(PacketID::DayNightUpdate, Util::compressFloat(m_dayNightTime), cro::NetFlag::Unreliable);
    }
}