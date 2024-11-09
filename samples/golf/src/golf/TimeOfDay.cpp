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

#include "TimeOfDay.hpp"
#include "../Sunclock.hpp"
#include "../LatLong.hpp"

#include <Social.hpp>

#include <crogine/core/ConfigFile.hpp>
#include <crogine/core/SysTime.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

#ifdef _WIN32
#include <winnls.h>
#endif

namespace
{
    constexpr float MinLat = -90.f;
    constexpr float MaxLat = 90.f;

    constexpr float MinLon = -180.f;
    constexpr float MaxLon = 180.f;

    constexpr std::time_t FourWeeks = 28 * 24 * 60 * 60;

    const std::string DataFile("latlon.pos");
}

TimeOfDay::TimeOfDay()
    : m_latlon(0.f)
{
    //attempt to read local data file
    const auto path = Social::getBaseContentPath() + DataFile;

    //if file doesn't exist, refresh
    if (!cro::FileSystem::fileExists(path))
    {
        updateLatLon();
    }
    else //if file does exist but timestamp > 28 days, refresh
    {
        cro::ConfigFile cfg;
        if (cfg.loadFromFile(path, false))
        {
            std::uint32_t ts = 0;
            for (const auto& prop : cfg.getProperties())
            {
                if (prop.getName() == "lat_lon")
                {
                    m_latlon = prop.getValue<glm::vec2>();
                    m_latlon.x = std::clamp(m_latlon.x, MinLat, MaxLat);
                    m_latlon.y = std::clamp(m_latlon.y, MinLon, MaxLon);
                }
                else if (prop.getName() == "timestamp")
                {
                    ts = prop.getValue<std::uint32_t>();
                }
            }

            auto tsNow = std::time(nullptr);
            if (ts > tsNow
                || (tsNow - ts) > FourWeeks)
            {
                updateLatLon();
            }
        }
        else
        {
            updateLatLon();
        }
    }
}

//public
std::int32_t TimeOfDay::getTimeOfDay() const
{
    if (glm::length2(m_latlon) == 0)
    {
        //take a best guess based on the system time
        //I mean technically 0, 0 is valid but it's unlikely
        //we're playing golf in the gulf (groan) of Guinea
        const auto hour = cro::SysTime::now().hours();
        switch (hour)
        {
        default:
        case 21:
        case 22:
        case 23:
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
            return Night;
        case 5:
            return Morning;
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
            return Day;
        case 20:
            return Evening;
            break;
        }
    }
    else
    {
        //calculate the sunrise / sunset based on local time
        //and our lat/lon
        //sunrise / sunset is +/- 30 mins of calc'd time
        //else return day or night.
        auto ts = std::time(nullptr);
        std::tm gmtm = *std::gmtime(&ts);
        std::tm localtm = *std::localtime(&ts);

        auto hourDiff = localtm.tm_hour - gmtm.tm_hour;
        auto dayDiff = localtm.tm_yday - gmtm.tm_yday;
        auto yearDiff = localtm.tm_year - gmtm.tm_year;
        
        dayDiff -= (365 * yearDiff); //so this will be wrong on Jan 1st every 4 years... meh.
        hourDiff -= (24 * dayDiff);
        hourDiff = std::clamp(hourDiff, -12, 12);

        Sunclock sunclock(m_latlon.x, m_latlon.y, static_cast<double>(hourDiff));

        auto sunrise = sunclock.sunrise();
        auto risetm = *std::localtime(&sunrise);

        if (risetm.tm_hour == localtm.tm_hour)
        {
            return Morning;
        }

        //LogI << "Sunrise is " << risetm.tm_hour << ":" << risetm.tm_min << std::endl;

        auto sunset = sunclock.sunset();
        auto settm = *std::localtime(&sunset);

        if (settm.tm_hour == localtm.tm_hour)
        {
            return Evening;
        }

        //LogI << "Sunset is " << settm.tm_hour << ":" << settm.tm_min << std::endl;

        if (localtm.tm_hour > risetm.tm_hour
            && localtm.tm_hour < settm.tm_hour)
        {
            return Day;
        }
        return Night;
    }

    return Day;
}

void TimeOfDay::setLatLon(glm::vec2 latlon)
{
    m_latlon = latlon;
    m_latlon.x = std::clamp(m_latlon.x, MinLat, MaxLat);
    m_latlon.y = std::clamp(m_latlon.y, MinLon, MaxLon);

    writeDataFile();
}

//private
void TimeOfDay::updateLatLon()
{
    m_latlon = { 0.f, 0.f };

    //query web latlon - this async so if we get a callback we update
    //the data then, else for now we try to find a country code and
    //use that to approximate our location
    Social::getLatLon();

    auto countryCode = getCountryCode();

    //kinda negates the point of unordered_map but for
    //some reason getCountryCode() isn't returning a string
    //which exactly matches the key values using count() :/
    for (const auto& [name, val] : LatLong)
    {
        if (countryCode.find(name) != std::string::npos)
        {
            m_latlon = val;
            break;
        }
    }

    writeDataFile();
}

std::string TimeOfDay::getCountryCode()
{
    std::string retVal;
#ifdef USE_GNS
    //query Steam
    retVal = Social::getCountryCode();
#endif

    if (retVal.empty())
    {
        //query the OS
#ifdef _WIN32
        static constexpr std::int32_t BuffSize = 8;
        WCHAR buffer[BuffSize] = { 0 };
        if (auto charCount = GetUserDefaultGeoName(buffer, BuffSize); charCount == 0)
        {
            retVal = "US";
        }
        else
        {
            //hm for some reason this isn't creating a precise match
            //which messes up the LatLong lookup (above)
            cro::String temp = cro::String::fromUtf16(std::begin(buffer), std::begin(buffer) + charCount);
            retVal = temp.toAnsiString();
        }
#else
        //TODO figure out linux.
        //mac... well probably toughS
        retVal = "US";
#endif
    }
    return retVal;
}

void TimeOfDay::writeDataFile() const
{
    const auto path = Social::getBaseContentPath() + DataFile;
    cro::ConfigFile cfg;
    cfg.addProperty("lat_lon").setValue(m_latlon);
    cfg.addProperty("timestamp").setValue(static_cast<std::uint32_t>(std::time(nullptr)));
    cfg.save(path); //TODO enable this once we're done testing!
}