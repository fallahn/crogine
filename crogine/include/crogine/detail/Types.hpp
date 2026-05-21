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

#include <crogine/Config.hpp>

#include <SDL_stdinc.h>
#include <SDL_events.h>
#include <SDL_rwops.h>

namespace cro
{
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

    struct TexturePrecision final
    {
        //32bit Float
        static constexpr std::uint32_t High = 0;
        //16bit Float
        static constexpr std::uint32_t Low = 1;
        //8bit unsigned int
        static constexpr std::uint32_t Default = 2;
    };

    //used to automatically close RWops files
    struct RaiiRWops final
    {
        SDL_RWops* file;
        ~RaiiRWops()
        {
            close();
        }
        RaiiRWops() : file(nullptr) {}
        RaiiRWops(const RaiiRWops&) = delete;
        RaiiRWops& operator = (const RaiiRWops&) = delete;
        
        RaiiRWops(RaiiRWops&&) = default;
        RaiiRWops& operator = (RaiiRWops&&) = default;

        void close()
        {
            if (file)
            {
                SDL_RWclose(file);
                file = nullptr;
            }
        }
    };
}