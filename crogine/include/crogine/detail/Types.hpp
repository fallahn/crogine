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

#include <crogine/Config.hpp>

#include <SDL_stdinc.h>
#include <SDL_events.h>
#include <SDL_rwops.h>

#include <cstdint>

/*
Aliases for SDL types
Not strictly necessary as they all boil down to stdint types
*/

namespace cro
{
    //using std::uint8_t = Uint8;
    //using std::int8_t = Sint8;
    //using std::uint16_t = Uint16;
    //using std::int16_t = Sint16;
    //using std::uint32_t = Uint32;
    //using std::int32_t = Sint32;
    //using std::uint64_t = Uint64;
    //using std::int64_t = Sint64;

    using Event = SDL_Event;

    namespace ImageFormat
    {
        enum Type
        {
            None,
            RGB,
            RGBA,
            A
        };
    }

    //used to automatically close RWops files
    struct RaiiRWops final
    {
        SDL_RWops* file;
        ~RaiiRWops()
        {
            if (file)
            {
                SDL_RWclose(file);
            }
        }
        RaiiRWops() : file(nullptr) {}
        RaiiRWops(const RaiiRWops&) = delete;
        RaiiRWops& operator = (const RaiiRWops&) = delete;
        
        RaiiRWops(RaiiRWops&&) = default;
        RaiiRWops& operator = (RaiiRWops&&) = default;
    };
}