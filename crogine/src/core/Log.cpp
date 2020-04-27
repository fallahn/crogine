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

#include <crogine/core/Log.hpp>
#include <crogine/core/Console.hpp>
#include <crogine/core/SysTime.hpp>
#include <crogine/detail/Types.hpp>

#include <SDL_log.h>

using namespace cro;

std::list<std::string> Logger::m_buffer;
std::string Logger::m_output;

void Logger::log(const std::string& message, Type type, Output output)
{
    std::string outstring;
    switch (type)
    {
    case Type::Info:
    default:
        outstring = "INFO: " + message;
        break;
    case Type::Error:
        outstring = "ERROR: " + message;
        break;
    case Type::Warning:
        outstring = "WARNING: " + message;
        break;
    }
#ifndef __ANDROID__
    if (output == Output::Console || output == Output::All)
    {
        switch (type)
        {
        default:
        case Type::Info:
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s", message.c_str());
            break;
        case Type::Warning:
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", message.c_str());
            break;
        case Type::Error:
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s", message.c_str());
            break;
        }

        Console::print(outstring);

        const std::size_t maxBuffer = 30;
        m_buffer.push_back(outstring);
        if (m_buffer.size() > maxBuffer) m_buffer.pop_front();
        updateOutString(maxBuffer);

#ifdef _MSC_VER
        outstring += "\n";
        OutputDebugStringA(outstring.c_str());
#endif //_MSC_VER
    }
    if (output == Output::File || output == Output::All)
    {
        //output to a log file
        RaiiRWops file;
        file.file = SDL_RWFromFile("output.log", "a");
        if (file.file)
        {
            auto timeStamp = SysTime::timeString();
            timeStamp += " - ";
            timeStamp += SysTime::dateString();
            timeStamp += ": ";

            SDL_RWwrite(file.file, timeStamp.c_str(), 1, timeStamp.size());
            SDL_RWwrite(file.file, outstring.c_str(), 1, outstring.size());
        }
        else
        {
            log(message, type, Output::Console);
            log("Above message was intended for log file. Opening file probably failed.", Type::Warning, Output::Console);
        }
    }
#else
    //use logcat - technically SDL will log successfully on android too
    //so this is unnecessary.
    __android_log_print(ANDROID_LOG_VERBOSE, "CroApp", message.c_str(), 1);
#endif //__ANDROID__
}

//private
void Logger::updateOutString(std::size_t maxBuffer)
{
    static size_t count = 0;
    m_output.append(m_buffer.back());
    m_output.append("\n");
    count++;

    if (count > maxBuffer)
    {
        m_output = m_output.substr(m_output.find_first_of('\n') + 1, m_output.size());
        count--;
    }
}