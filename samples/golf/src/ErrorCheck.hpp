/*-----------------------------------------------------------------------

Matt Marchant 2020
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

#include <crogine/detail/OpenGL.hpp>
#include <crogine/core/Log.hpp>

#include <sstream>

#ifdef CRO_DEBUG_
#define glCheck(x) do{x; glErrorCheck(__FILE__, __LINE__, #x);}while (false)
#else
#define glCheck(x) (x)
#endif //_DEBUG_

static inline std::string glErrorString(GLenum err)
{
    switch (err)
    {
    case GL_NO_ERROR:                      return "GL_NO_ERROR";
    case GL_INVALID_ENUM:                  return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE:                 return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:             return "GL_INVALID_OPERATION";
    case GL_STACK_OVERFLOW:                return "GL_STACK_OVERFLOW";
    case GL_STACK_UNDERFLOW:               return "GL_STACK_UNDERFLOW";
    case GL_OUT_OF_MEMORY:                 return "GL_OUT_OF_MEMORY";
    case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
    default: return std::to_string(err) + ": Unknown error code";
    }
}

static inline void glErrorCheck(const char* file, unsigned int line, const char* expression)
{
    GLenum err = glGetError();
    while(err != GL_NO_ERROR)
    {
        std::stringstream ss;
        ss << file << ", " << line << ": " << expression << ". " << " glError: " << glErrorString(err) << std::endl;
        cro::Logger::log(ss.str(), cro::Logger::Type::Error);
        err = glGetError();
    }
}