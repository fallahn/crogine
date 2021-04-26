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

#include <crogine/core/Clock.hpp>

#include <SDL.h>

using namespace cro;

//---Time class---//
//constexpr Time::Time() : m_value(0) {}

//public
float Time::asSeconds() const
{
    return static_cast<float>(m_value) / 1000.f;
}

std::int32_t Time::asMilliseconds() const
{
    return m_value;
}

//---clock class---//
Clock::Clock()
    : m_startTime(milliseconds(SDL_GetTicks()))
{

}

//public
Time Clock::elapsed() const
{
    return milliseconds(SDL_GetTicks()) - m_startTime;
}

Time Clock::restart()
{
    Time curTime = milliseconds(SDL_GetTicks());
    Time retVal = curTime - m_startTime;
    m_startTime = curTime;
    return retVal;
}

CRO_EXPORT_API Time cro::seconds(float seconds)
{
    Time t;
    t.m_value = static_cast<std::int32_t>(seconds * 1000.f);
    return t;
}

CRO_EXPORT_API Time cro::milliseconds(std::int32_t millis)
{
    Time t;
    t.m_value = millis;
    return t;
}

//---operator overloads---//
bool cro::operator == (Time l, Time r)
{
    return l.asMilliseconds() == r.asMilliseconds();
}

bool cro::operator != (Time l, Time r)
{
    return l.asMilliseconds() != r.asMilliseconds();
}

bool cro::operator > (Time l, Time r)
{
    return l.asMilliseconds() > r.asMilliseconds();
}

bool cro::operator < (Time l, Time r)
{
    return l.asMilliseconds() < r.asMilliseconds();
}

bool cro::operator >= (Time l, Time r)
{
    return l.asMilliseconds() >= r.asMilliseconds();
}

bool cro::operator <= (Time l, Time r)
{
    return l.asMilliseconds() <= r.asMilliseconds();
}

Time cro::operator - (Time r)
{
    return milliseconds(-r.asMilliseconds());
}

Time cro::operator + (Time l, Time r)
{
    return milliseconds(l.asMilliseconds() + r.asMilliseconds());
}

Time& cro::operator += (Time& l, Time r)
{
    return l = l + r;
}

Time cro::operator - (Time l, Time r)
{
    return milliseconds(l.asMilliseconds() - r.asMilliseconds());
}

Time& cro::operator -= (Time& l, Time r)
{
    return l = l - r;
}

Time cro::operator * (Time l, float r)
{
    return seconds(l.asSeconds() * r);
}

Time cro::operator * (Time l, std::int32_t r)
{
    return milliseconds(l.asMilliseconds() * r);
}

Time cro::operator * (float l, Time r)
{
    return r * l;
}

Time cro::operator * (std::int32_t l, Time r)
{
    return r * l;
}

Time& cro::operator *= (Time& l, float r)
{
    return l = l * r;
}

Time& cro::operator *= (Time& l, std::int32_t r)
{
    return l = l * r;
}

Time cro::operator / (Time l, float r)
{
    return seconds(l.asSeconds() / r);
}

Time cro::operator / (Time l, std::int32_t r)
{
    return milliseconds(l.asMilliseconds() / r);
}

Time& cro::operator /= (Time& l, float r)
{
    return l = l / r;
}

Time& cro::operator /= (Time& l, std::int32_t r)
{
    return l = l / r;
}

Time cro::operator / (Time l, Time r)
{
    return milliseconds(l.asMilliseconds() / r.asMilliseconds());
}

Time cro::operator % (Time l, Time r)
{
    return milliseconds(l.asMilliseconds() % r.asMilliseconds());
}

Time& cro::operator %= (Time& l, Time r)
{
    return l = l % r;
}