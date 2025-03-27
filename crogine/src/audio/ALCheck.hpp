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

#pragma once

#ifndef CRO_DEBUG_
#define alCheck(x) x;
#define alcCheck(x,y) x;
#else
#ifdef __APPLE__
#include "al.h"
#else
#include <AL/al.h>
#endif
#include <crogine/core/Log.hpp>

#include <sstream>

#define alCheck(x) alGetError(); x; al::errorCheck(__FILE__, __LINE__);
#define alcCheck(x, y) alcGetError(y); x; al::contextErrorCheck(__FILE__, __LINE__);
namespace al
{
    static inline void contextErrorCheck(const char* file, int line)
    {
        //TODO implement this
    }

    static inline void errorCheck(const char* file, int line)
    {
        auto error = alGetError();
        auto errorCount = 20; //just to stop infinite loops
        while (error != AL_NO_ERROR && errorCount)
        {
            std::string message;
            switch (error)
            {
            default: 
                message = std::to_string(error);
                break;
            case AL_INVALID_ENUM:
                message = "AL_INVALID_ENUM: error in enumerated argument";
                break;
            case AL_INVALID_NAME:
                message = "AL_INVALID_NAME: invalid name specified";
                break;
            case AL_INVALID_OPERATION:
                message = "AL_INVALID_OPERATION: operation is not acceptable in current state";
                //NOTE: getting this when trying to delete buffers? Make sure an AudioResource
                //out-lives any Scene/System which has AudioSources in use that use that buffer.
                break;
            case AL_INVALID_VALUE:
                message = "AL_INVALID_VALUE: value is out of range";
                break;
            case AL_OUT_OF_MEMORY:
                message = "AL_OUT_OF_MEMORY: not enough memory left for current operation (too many sounds exist?)";
                break;
            }
            std::stringstream sts;
            sts << "OpenAL: error - " << message << " in " << file << " line " << line << std::endl;
            LOG(sts.str(), cro::Logger::Type::Error);

            error = alGetError();
            errorCount--;
        }
    }
}
#endif //_DEBUG_