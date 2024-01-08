/*-----------------------------------------------------------------------

Matt Marchant 2023
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

#include "../detail/stb_image.h"
#include "../detail/SDLImageRead.hpp"

#include <crogine/core/Log.hpp>
#include <crogine/graphics/ImageArray.hpp>

#include <cstring>

//TODO we could refactor this for code reuse but... meh
namespace cro::Detail
{
    bool loadFromU8(const std::string& path, std::vector<std::uint8_t>& dst, glm::uvec2& dims, std::uint32_t& channels)
    {
        dst.clear();

        auto* file = SDL_RWFromFile(path.c_str(), "rb");
        if (!file)
        {
            Logger::log("Failed opening " + path, Logger::Type::Error);
            return false;
        }

        STBIMG_stbio_RWops io;
        stbi_callback_from_RW(file, &io);

        std::int32_t w, h, d;
        stbi_info_from_callbacks(&io.stb_cbs, &io, &w, &h, &d);
        file->seek(file, 0, RW_SEEK_SET);

        //if this is RGB pad out to RGBA for row alignment
        std::int32_t wantedChannels = 0;
        if (d == 3)
        {
            wantedChannels = 4;
        }

        auto* img = stbi_load_from_callbacks(&io.stb_cbs, &io, &w, &h, &d, wantedChannels);
        if (img)
        {
            d = (wantedChannels) ? wantedChannels : d;
            auto size = w * h * d;

            dst.resize(size);
            std::memcpy(dst.data(), img, size);

            dims = { w,h };
            channels = d;

            stbi_image_free(img);
            SDL_RWclose(file);

            return true;
        }
        else
        {
            Logger::log("failed to open image: " + path, Logger::Type::Error);
            SDL_RWclose(file);

            return false;
        }

        return false;
    }

    bool loadFromU16(const std::string& path, std::vector<std::uint16_t>& dst, glm::uvec2& dims, std::uint32_t& channels)
    {
        dst.clear();

        auto* file = SDL_RWFromFile(path.c_str(), "rb");
        if (!file)
        {
            Logger::log("Failed opening " + path, Logger::Type::Error);
            return false;
        }

        STBIMG_stbio_RWops io;
        stbi_callback_from_RW(file, &io);

        std::int32_t w, h, d;
        stbi_info_from_callbacks(&io.stb_cbs, &io, &w, &h, &d);
        file->seek(file, 0, RW_SEEK_SET);

        //if this is RGB pad out to RGBA for row alignment
        std::int32_t wantedChannels = 0;
        if (d == 3)
        {
            wantedChannels = 4;
        }


        auto* img = stbi_load_16_from_callbacks(&io.stb_cbs, &io, &w, &h, &d, wantedChannels);
        if (img)
        {
            d = (wantedChannels) ? wantedChannels : d;
            auto size = w * h * d;

            dst.resize(size);
            std::memcpy(dst.data(), img, size * sizeof(std::uint16_t));

            dims = { w,h };
            channels = d;

            stbi_image_free(img);
            SDL_RWclose(file);

            return true;
        }
        else
        {
            Logger::log("failed to open image: " + path, Logger::Type::Error);
            SDL_RWclose(file);

            return false;
        }

        return false;
    }

    bool loadFromFloat(const std::string& path, std::vector<float>& dst, glm::uvec2& dims, std::uint32_t& channels)
    {
        dst.clear();

        auto* file = SDL_RWFromFile(path.c_str(), "rb");
        if (!file)
        {
            Logger::log("Failed opening " + path, Logger::Type::Error);
            return false;
        }

        STBIMG_stbio_RWops io;
        stbi_callback_from_RW(file, &io);

        std::int32_t w, h, d;
        stbi_info_from_callbacks(&io.stb_cbs, &io, &w, &h, &d);
        file->seek(file, 0, RW_SEEK_SET);

        //if this is RGB pad out to RGBA for row alignment
        std::int32_t wantedChannels = 0;
        if (d == 3)
        {
            wantedChannels = 4;
        }

        //TODO the float interface is meant for HDR images
        //this is a hack when using this func to load ldr
        //images and needs to be addressed properly
        stbi_ldr_to_hdr_gamma(1.0f);

        auto* img = stbi_loadf_from_callbacks(&io.stb_cbs, &io, &w, &h, &d, wantedChannels);
        if (img)
        {
            d = (wantedChannels) ? wantedChannels : d;
            auto size = w * h * d;

            dst.resize(size);
            std::memcpy(dst.data(), img, size * sizeof(float));

            dims = { w,h };
            channels = d;

            stbi_image_free(img);
            SDL_RWclose(file);

            stbi_ldr_to_hdr_gamma(2.2f);

            return true;
        }
        else
        {
            Logger::log("failed to open image: " + path, Logger::Type::Error);
            SDL_RWclose(file);

            stbi_ldr_to_hdr_gamma(2.2f);

            return false;
        }


        return false;
    }
}