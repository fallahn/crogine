/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#pragma once

#include <crogine/core/ConsoleClient.hpp>
#include <crogine/detail/glm/vec2.hpp>

#include <cstdint>
#include <string>

class TimeOfDay final : public cro::ConsoleClient
{
public:
    TimeOfDay();

    enum
    {
        Night, Morning,
        Day, Evening,

        Count
    };

    std::int32_t getTimeOfDay() const;
    void setLatLon(glm::vec2);

private:

    glm::vec2 m_latlon;

    //these are stored for debugging
    //so that's my excuse for the code smell.
    mutable std::string m_sunriseStr;
    mutable std::string m_sunsetStr;

    //attempts to update the lat/lon data stored on disk
    void updateLatLon();

    std::string getCountryCode();
    void writeDataFile() const;
};