/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2026
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

//custom assertion macro which is more verbose than standard assert

#pragma once

#include <crogine/core/FileSystem.hpp>
#include <crogine/core/Log.hpp>

#include <cassert>
#include <source_location>
#include <sstream>

#ifdef _MSC_VER
#define __func__ __FUNCTION__
#endif //_MSC_VER

//provide this as a function parameter to print the calling function
//then on an assert or error log caller.function_name()
#if defined CRO_CALLER_ID_ || defined CRO_DEBUG_

#if defined _MSC_VER && !defined CRO_DEBUG_
#pragma message(": Warning <CALLER_ID active> : undefine CRO_CALLER_ID_ in release builds")
#endif

static inline void printCaller(const std::source_location& caller, bool showMessageBox, std::source_location thrower = std::source_location::current())
{
    std::stringstream ss;
    ss << "Assertion failed in " << thrower.function_name() << "\n called from: " 
        << cro::FileSystem::getFileName(caller.file_name()) << ", line " << caller.line() << ", col " << caller.column() << "\n"
        << caller.function_name();
    const auto str = ss.str();
    LogE << str << std::endl;
    if (showMessageBox)
    {
        cro::FileSystem::showMessageBox("Error", str);
    }
}

//function parameter for prototypes
#define CALLER_ID std::source_location caller = std::source_location::current()
//function paramter for definition
#define CALLER std::source_location caller
//assertion - condition, show message box
#define CALLER_ASSERT(x, y) if(!x) printCaller(caller, y);
#else
#define CALLER_ID
#define CALLER
#define CALLER_ASSERT(x, y)
#endif

#ifdef CRO_DEBUG_
#define CRO_ASSERT(condition, message) assert(condition)
//#define CRO_ASSERT(condition, message) \
//do \
//{ \
//    if(!(condition)) \
//    { \
//        std::stringstream ss; \
//        ss << "Assertion failed in " << __FILE__ << ", function `" << __func__ << "`, line " << __LINE__ << ": " << message; \
//        cro::Logger::log(ss.str(), cro::Logger::Type::Error, cro::Logger::Output::All); \
//        abort(); \
//    } \
//}while (false)
//
//#define CRO_WARNING(condition, message) \
//do \
//{ \
//    if((condition)) \
//    { \
//        std::stringstream ss; \
//        ss << "in " << __FILE__ << ", function `" << __func__ << "`, line " << __LINE__ << ": " << message; \
//        cro::Logger::log(ss.str(), cro::Logger::Type::Warning, cro::Logger::Output::All); \
//    } \
//}while (false)
#else

#define CRO_ASSERT(condition, message)
#define CRO_WARNING(condition, message)

#endif //CRO_DEBUG_