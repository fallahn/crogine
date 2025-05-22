/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2025
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

//flexible logging class

#pragma once

#include <crogine/Config.hpp>
#include <crogine/core/FileSystem.hpp>
#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/glm/vec3.hpp>
#include <crogine/detail/glm/vec4.hpp>
#include <crogine/graphics/Rectangle.hpp>

#include <string>
#include <list>
#include <ostream>
#include <streambuf>

#ifdef _MSC_VER
#define NOMINMAX
#include <Windows.h>
#endif //_MSC_VER

#define FILE_LINE cro::FileSystem::getFileName(__FILE__) << ", line " << __LINE__ << " "

namespace cro
{
    class String;

    /*!
    \brief Class to allowing messages to be logged to a combination
    of one or more destinations such as the console, log file or
    output window in Visual Studio
    */
    class CRO_EXPORT_API Logger final
    {
    public:
        enum class Output
        {
            Console,
            File,
            All
        };

        enum class Type
        {
            Info,
            Warning,
            Error
        };
        /*!
        \brief Logs a message to a given destination.
        \param message Message to log
        \param type Whether this message gets tagged as information, a warning or an error
        \param output Destination for the message. Can be the console via cout, a log file on disk, or both
        */
        static void log(const std::string& message, Type type = Type::Info, Output output = Output::Console);

        /*!
        \brief Allows logging with C++ streams.
        \param type Type of log message to print.
        Note that this will not output to file, only to the console
        */
        static std::ostream& log(Type type = Type::Info);

    private:
        static std::list<std::string> m_buffer;
        static std::string m_output;

        static void updateOutString(std::size_t maxBuffer);
    };

    namespace Detail
    {
        class LogBuf final : public std::streambuf
        {
        public:
            LogBuf();
            ~LogBuf();

        private:

            int overflow(int character) override;
            int sync() override;
        };

        class LogStream final : public std::ostream
        {
        public:
            LogStream();

        private:
            LogBuf m_buffer;
        };
    }
}

template <typename T>
std::ostream& operator << (std::ostream& out, glm::tvec2<T> v)
{
    out << "{ " << v.x << ", " << v.y << " }";
    return out;
}

template <typename T>
std::ostream& operator << (std::ostream& out, glm::tvec3<T> v)
{
    out << "{ " << v.x << ", " << v.y << ", " << v.z << " }";
    return out;
}

template <typename T>
std::ostream& operator << (std::ostream& out, glm::tvec4<T> v)
{
    out << "{ " << v.x << ", " << v.y << ", " << v.z << ", " << v.w << " }";
    return out;
}

template <typename T>
std::ostream& operator << (std::ostream& out, glm::qua<T> v)
{
    out << "{ " << v.x << ", " << v.y << ", " << v.z << ", " << v.w << " }";
    return out;
}

template <typename T>
std::ostream& operator << (std::ostream& out, cro::Rectangle<T> r)
{
    out << "[ " << r.left << ", " << r.bottom << ", " << r.width << ", " << r.height << " ]";
    return out;
}

#define LogI cro::Logger::log(cro::Logger::Type::Info)
#define LogW cro::Logger::log(cro::Logger::Type::Warning)
#define LogE cro::Logger::log(cro::Logger::Type::Error) << FILE_LINE

#ifndef CRO_DEBUG_
#define LOG(message, type)
#else
#define LOG(message, type) {\
std::string fileName(__FILE__); \
fileName = cro::FileSystem::getFileName(fileName); \
std::stringstream ss; \
ss << message << " (" << fileName << ", " << __LINE__ << ")"; \
cro::Logger::log(ss.str(), type);}
#endif //CRO_DEBUG_