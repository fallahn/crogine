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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/detail/SDLResource.hpp>

namespace cro
{
    /*!
    \brief Represents a duration of time
    */
    class CRO_EXPORT_API Time final
    {
    public:
        Time();

        float asSeconds() const;
        int32 asMilliseconds() const;

    private:
        friend CRO_EXPORT_API Time seconds(float);
        friend CRO_EXPORT_API Time milliseconds(int32);

        int32 m_value;
    };

    /*!
    \brief Constructs a time object from a duration of seconds
    */
    CRO_EXPORT_API Time seconds(float);
    /*!
    \brief Constructs a Time object from a duration of milliseconds
    */
    CRO_EXPORT_API Time milliseconds(int32);

    /*!
    \brief Overload of == operator for two Time objects
    */
    CRO_EXPORT_API bool operator == (Time, Time);
    /*!
    brief Overload of != operator for two Time objects
    */
    CRO_EXPORT_API bool operator != (Time, Time);
    /*!
    \brief Overload of > operator for two Time objects
    */
    CRO_EXPORT_API bool operator > (Time, Time);
    /*!
    \brief Overload of < operator for two Time objects
    */
    CRO_EXPORT_API bool operator < (Time, Time);
    /*!
    \brief Overload of >= operator for two Time objects
    */
    CRO_EXPORT_API bool operator >= (Time, Time);
    /*!
    \brief Overload of <= operator for two Time object
    */
    CRO_EXPORT_API bool operator <= (Time, Time);
    /*!
    \brief Overload of - operator for subtracting one Time from another
    */
    CRO_EXPORT_API Time operator - (Time);
    /*!
    \brief Overload of + operator for two Time objects
    */
    CRO_EXPORT_API Time operator + (Time, Time);
    /*!
    \brief Overload of += to add one Time to another
    */
    CRO_EXPORT_API Time& operator += (Time&, Time);
    /*!
    \brief Overload of - for two Time objects
    */
    CRO_EXPORT_API Time operator - (Time, Time);
    /*!
    \brief Overload of -= operator for two Time objects
    */
    CRO_EXPORT_API Time& operator -= (Time&, Time);
    /*!
    \brief Overload of * operator for multiplying a Time object by 
    a floating point value in seconds
    */
    CRO_EXPORT_API Time operator * (Time, float);
    /*!
    \brief Overload of * operator for multiplying a Time object by 
    a number of milliseconds
    */
    CRO_EXPORT_API Time operator * (Time, int32);
    /*
    \brief Overload of * operator to multiply a floating point
    value in seconds by a Time object
    */
    CRO_EXPORT_API Time operator * (float, Time);
    /*!
    \brief Overload of * operator to multiply a number of milliseconds
    by a Time object
    */
    CRO_EXPORT_API Time operator * (int32, Time);
    /*!
    \brief Overload of *= operator to scale a Time object by a number of seconds
    */
    CRO_EXPORT_API Time& operator *= (Time&, float);
    /*!
    \brief Overload of *= operator to scale a Time object by a number of milliseconds
    */
    CRO_EXPORT_API Time& operator *= (Time&, int32);
    /*!
    \brief Overload of / operator to scale a Time object by a number of seconds
    */
    CRO_EXPORT_API Time operator / (Time, float);
    /*!
    \brief Overload of / operator to scale a Time object by a number of milliseconds
    */
    CRO_EXPORT_API Time operator / (Time, int32);
    /*!
    \brief Overload of / operator to scale a Time object by a number of seconds
    */
    CRO_EXPORT_API Time& operator /= (Time&, float);
    /*!
    \brief Overload of / operator to scale a Time object by a number of milliseconds
    */
    CRO_EXPORT_API Time& operator /= (Time&, int32);
    /*!
    \brief OVerload of / operator to divide one Time object by another
    */
    CRO_EXPORT_API Time operator / (Time, Time);
    /*!
    \brief Overload of % opertor to return remainder of one Time object divided by another
    */
    CRO_EXPORT_API Time operator % (Time, Time);
    /*!
    \brief Overload of %= opertor to return remainder of one Time object divided by another
    */
    CRO_EXPORT_API Time& operator %= (Time&, Time);

    /*!
    \brief Clock class, used to measure periods of time
    */
    class CRO_EXPORT_API Clock final : public Detail::SDLResource
    {
    public:
        Clock();

        Time elapsed() const;
        Time restart();
    private:
        Time m_startTime;
    };
}