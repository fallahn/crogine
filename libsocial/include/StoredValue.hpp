/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2023
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

#include <crogine/core/App.hpp>

#include <string>

//utility to read/write player progress values

static inline void readValue(std::int32_t& dst, const std::string& fileName)
{
    auto path = cro::App::getPreferencePath() + fileName;
    if (cro::FileSystem::fileExists(path))
    {
        auto* file = SDL_RWFromFile(path.c_str(), "rb");
        if (file)
        {
            SDL_RWread(file, &dst, sizeof(dst), 1);
        }
        SDL_RWclose(file);
    }
}

static inline void writeValue(std::int32_t src, const std::string& fileName)
{
    auto path = cro::App::getPreferencePath() + fileName;
    auto* file = SDL_RWFromFile(path.c_str(), "wb");
    if (file)
    {
        SDL_RWwrite(file, &src, sizeof(src), 1);
    }
    SDL_RWclose(file);
}

struct StoredValue final
{
    bool loaded = false;
    std::int32_t value = 0;
    const std::string ext;
    void read()
    {
        if (!loaded)
        {
            readValue(value, ext);
            loaded = true;
        }
    }
    void write()
    {
        writeValue(value, ext);
    }

    explicit StoredValue(const std::string& e) : ext(e) {}
};