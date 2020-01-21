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

#include <crogine/detail/Types.hpp>

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
                std::size_t start = 0u;
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
            \brief Emulates the C function fgetc with a RWops file/stream
            */
            static inline int rwgetc(SDL_RWops *file)
            {
                unsigned char c;
                return SDL_RWread(file, &c, 1, 1) == 1 ? c : EOF;
            }

            /*!
            \brief Emulates the C function fgets with an SDL rwops file
            */
            static inline char* rwgets(char* dest, int32 size, SDL_RWops* file, cro::int64* bytesRead)
            {
                std::size_t count = 0;
                char* compareStr = dest;

                char readChar = 0;

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

            /*!
            \brief Attempts to return a string containing the file extension
            of a given path, including the period (.)
            */
            static inline std::string getFileExtension(const std::string& path)
            {
                if (path.find_last_of(".") != std::string::npos)
                {
                    return toLower(path.substr(path.find_last_of(".")));
                }
                else
                {
                    return {};
                }
            }

            /*!
            \brief Attempts to return the name of a file at the end of
            a given file path
            */
            static inline std::string getFileName(const std::string& path)
            {
                //TODO this doesn't actually check that there is a file at the
                //end of the path, or that it's even a valid path...

                static auto searchFunc = [](const char seperator, const std::string& path)->std::string
                {
                    std::size_t i = path.rfind(seperator, path.length());
                    if (i != std::string::npos)
                    {
                        return(path.substr(i + 1, path.length() - i));
                    }

                    return path;
                };


#ifdef _WIN32 //try windows formatted paths first
                std::string retVal = searchFunc('\\', path);
                if (!retVal.empty()) return retVal;
#endif

                return searchFunc('/', path);
            }

            /*!
            \brief Attempts to return the path of a given filepath without
            the file name, including trailing separator char.
            */
            static inline std::string getFilePath(const std::string& path)
            {
                //TODO this doesn't actually check that there is a file at the
                //end of the path, or that it's even a valid path...

                static auto searchFunc = [](const char seperator, const std::string& path)->std::string
                {
                    std::size_t i = path.rfind(seperator, path.length());
                    if (i != std::string::npos)
                    {
                        return(path.substr(0, i + 1));
                    }

                    return {};
                };


#ifdef _WIN32 //try windows formatted paths first
                std::string retVal = searchFunc('\\', path);
                if (!retVal.empty()) return retVal;
#endif

                return searchFunc('/', path);
            }

            /*!
            \brief Returns the position of the nth instance of char c
            or std::string::npos if not found
            */

            static inline std::size_t findNthOf(const std::string& str, char c, uint32 n)
            {
                if (n == 0)
                {
                    return std::string::npos;
                }

                std::size_t currPos = 0;
                std::size_t currOffset = 0;
                std::size_t i = 0;

                while (i < n)
                {
                    currPos = str.find(c, currOffset);
                    if (currPos == std::string::npos)
                    {
                        break;
                    }
                    currOffset = currPos + 1;
                    ++i;
                }
                return currPos;
            }
        }
    }
}