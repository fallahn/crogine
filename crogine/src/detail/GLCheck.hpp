/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#include <crogine/Config.hpp>
#include <crogine/core/Log.hpp>
#ifdef __APPLE__
#include "41/glad.hpp"
#else
#include "glad.hpp"
#endif

#include <string>
#include <sstream>

#pragma once

#ifdef CRO_DEBUG_
#if defined(PLATFORM_DESKTOP) && !defined(GL41)
//use the gl debug context instead (see App.cpp)
#define glCheck(x) do{x; cro::Detail::printLocation(__FILE__, __LINE__, #x);}while (false)
#else
#define glCheck(x) do{x; cro::Detail::glErrorCheck(__FILE__, __LINE__, #x);}while (false)
#endif
#else
#define glCheck(x) (x)
#endif //CRO_DEBUG_

namespace cro
{
    namespace Detail
    {
        static inline void printLocation(const char* file, unsigned int line, const char* expression)
        {
            if (glGetError() != GL_NO_ERROR)
            {
                std::string fileString = file;

                std::stringstream ss;
                ss << fileString.substr(fileString.find_last_of("\\/") + 1) << "(" << line << ")."
                    << "\nExpression: " << expression
                    << std::endl;
                Logger::log(ss.str(), Logger::Type::Error);
            }
        }

        static inline void glErrorCheck(const char* file, unsigned int line, const char* expression)
        {
            //get the last error
            GLenum errorCode = glGetError();
            while (errorCode != GL_NO_ERROR)
            {
                std::string fileString = file;
                std::string error = "Unknown error";
                std::string description = "No description";

                //decode the error code
                switch (errorCode)
                {
                case GL_INVALID_ENUM:
                {
                    error = "GL_INVALID_ENUM";
                    description = "An unacceptable value has been specified for an enumerated argument.";
                    break;
                }

                case GL_INVALID_VALUE:
                {
                    error = "GL_INVALID_VALUE";
                    description = "A numeric argument is out of range.";
                    break;
                }

                case GL_INVALID_OPERATION:
                {
                    error = "GL_INVALID_OPERATION";
                    description = "The specified operation is not allowed in the current state.";
                    break;
                }

                /*case GL_STACK_OVERFLOW:
                {
                    error = "GL_STACK_OVERFLOW";
                    description = "This command would cause a stack overflow.";
                    break;
                }

                case GL_STACK_UNDERFLOW:
                {
                    error = "GL_STACK_UNDERFLOW";
                    description = "This command would cause a stack underflow.";
                    break;
                }*/

                case GL_OUT_OF_MEMORY:
                {
                    error = "GL_OUT_OF_MEMORY";
                    description = "There is not enough memory left to execute the command.";
                    break;
                }

                }

                //log the error
                std::stringstream ss;
                ss << "An internal OpenGL call failed in "
                    << fileString.substr(fileString.find_last_of("\\/") + 1) << "(" << line << ")."
                    << "\nExpression:\n   " << expression
                    << "\nError description:\n   " << error << "\n   " << description << "\n"
                    << std::endl;
                Logger::log(ss.str(), Logger::Type::Error);

                errorCode = glGetError(); //call until all errors are printed
            }
        }
    }
}