/*---------------------------------------------------------------------- -

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

---------------------------------------------------------------------- -*/

//custom assertion macro which is more verbose than standard assert

#ifndef CRO_ASSERT_HPP_
#define CRO_ASSERT_HPP_

#include <crogine/core/Log.hpp>

#include <sstream>

#ifdef _MSC_VER
#define __func__ __FUNCTION__
#endif //_MSC_VER

#ifdef _DEBUG_
#define CRO_ASSERT(condition, message) \
do \
{ \
    if(!(condition)) \
    { \
        std::stringstream ss; \
        ss << "Assertion failed in " << __FILE__ << ", function `" << __func__ << "`, line " << __LINE__ << ": " << message; \
        cro::Logger::log(ss.str(), cro::Logger::Type::Error, cro::Logger::Output::All); \
        abort(); \
    } \
}while (false)

#define CRO_WARNING(condition, message) \
do \
{ \
    if((condition)) \
    { \
        std::stringstream ss; \
        ss << "in " << __FILE__ << ", function `" << __func__ << "`, line " << __LINE__ << ": " << message; \
        cro::Logger::log(ss.str(), cro::Logger::Type::Warning, cro::Logger::Output::All); \
    } \
}while (false)
#else

#define CRO_ASSERT(condition, message)
#define CRO_WARNING(condition, message)

#endif //_DEBUG_

#endif //CRO_ASSERT_HPP_
