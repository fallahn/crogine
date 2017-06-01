/*-----------------------------------------------------------------------

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

-----------------------------------------------------------------------*/

#ifndef CRO_UTIL_STRING_HPP_
#define CRO_UTIL_STRING_HPP_

#include <SDL_rwops.h>

#include <string>
#include <algorithm>

namespace cro
{
    namespace Util
    {
        namespace String
        {
            /*!
            \brief Remove all isntances of c from line
            */
            static inline void removeChar(std::string& line, const char c)
            {
                line.erase(std::remove(line.begin(), line.end(), c), line.end());
            }

            /*!
            \brief Replaces all instances of c with r
            */
            static inline void replace(std::string& str, const char c, const char r)
            {
                auto start = 0u;
                auto next = str.find_first_of(c, start);
                while (next != std::string::npos && start < str.length())
                {
                    str.replace(next, 1, &r);
                    start = ++next;
                    next = str.find_first_of(c, start);
                    if (next > str.length()) next = std::string::npos;
                }
            }

            /*!
            \brief Converts the given string to lowercase
            */
            static inline std::string toLower(const std::string& str)
            {
                std::string retVal(str);
                std::transform(retVal.begin(), retVal.end(), retVal.begin(), ::tolower);
                return retVal;
            }

            /*!
            \brief Emulates the C function fgets with an SDL rwops file
            */
            static inline char* rwgets(char* dest, int32 size, SDL_RWops* file, int64* bytesRead)
            {
                int32 count;
                char* compareStr = dest;

                char readChar;

                while (--size > 0 && (count = file->read(file, &readChar, 1, 1)) != 0)
                {
                    //if ((*compareStr++ = readChar) == '\n') break;
                    (*bytesRead)++;
                    if (readChar != '\r' && readChar != '\n') *compareStr++ = readChar;
                    if (readChar == '\n') break;
                }
                *compareStr = '\0';
                return (count == 0 && compareStr == dest) ? nullptr : dest;
            }
        }
    }
}

#endif //CRO_UTIL_STRING_HPP_