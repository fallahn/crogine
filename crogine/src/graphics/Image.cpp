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

#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/Colour.hpp>

#include <crogine/detail/Assert.hpp>
#include <crogine/core/Log.hpp>

#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <SDL_rwops.h>

#include <array>
#include <cstring>

using namespace cro;

namespace
{
    //creates an stbi image load callback using SDL_RWops
    struct STBIMG_stbio_RWops final
    {
        SDL_RWops* src = nullptr;
        stbi_io_callbacks stb_cbs;
        int32 atEOF = 0; //defaults to 0; 1: reached EOF or error on read, 2: error on seek
    };

    static int32 STBIMG__io_read(void* user, char* data, int32 size)
    {
        STBIMG_stbio_RWops* io = (STBIMG_stbio_RWops*)user;

        auto ret = SDL_RWread(io->src, data, sizeof(char), size);
        if (ret == 0)
        {
            //we're at EOF or some error happened
            io->atEOF = 1;
        }
        return ret * sizeof(char);
    }

    static void STBIMG__io_skip(void* user, int32 n)
    {
        STBIMG_stbio_RWops* io = (STBIMG_stbio_RWops*)user;

        if (SDL_RWseek(io->src, n, RW_SEEK_CUR) == -1)
        {
            //an error happened during seeking, hopefully setting EOF will make stb_image abort
            io->atEOF = 2; //set this to 2 for "aborting because seeking failed" (stb_image only cares about != 0)
        }
    }

    static int32 STBIMG__io_eof(void* user)
    {
        STBIMG_stbio_RWops* io = (STBIMG_stbio_RWops*)user;
        return io->atEOF;
    }

    void stbi_callback_from_RW(SDL_RWops* src, STBIMG_stbio_RWops* out)
    {
        CRO_ASSERT(src && out, "Cannot be nullptr!");

        //make sure out is at least initialized to something deterministic
        std::memset(out, 0, sizeof(*out));

        out->src = src;
        out->atEOF = 0;
        out->stb_cbs.read = STBIMG__io_read;
        out->stb_cbs.skip = STBIMG__io_skip;
        out->stb_cbs.eof = STBIMG__io_eof;
    }
}

Image::Image(bool flipOnLoad)
    : m_format  (ImageFormat::None),
    m_flipped   (false),
    m_flipOnLoad(flipOnLoad)
{
}

//public
void Image::create(uint32 width, uint32 height, Colour colour, ImageFormat::Type format)
{
    CRO_ASSERT(width > 0 && height > 0, "Invalid image dimension");
    CRO_ASSERT(format == ImageFormat::RGB || format == ImageFormat::RGBA, "Invalid format");

    std::size_t size = (format == ImageFormat::RGB) ? 3 : 4;
    size *= (width * height);

    m_data.resize(size);

    std::array<uint8, 4u> comp = 
    {{
        colour.getRedByte(),
        colour.getGreenByte(),
        colour.getBlueByte(),
        colour.getAlphaByte()
    }};

    uint32 step = (format == ImageFormat::RGB) ? 3 : 4;
    for (auto i = 0u; i < m_data.size(); i += step)
    {
        m_data[i] = comp[0];
        m_data[i + 1] = comp[1];
        m_data[i + 2] = comp[2];
        if(format == ImageFormat::RGBA)
        {
            m_data[i + 3] = comp[3];
        }
    }

    m_size = { width, height };
    m_format = format;
}

bool Image::loadFromFile(const std::string& path)
{
    auto* file = SDL_RWFromFile(path.c_str(), "rb");
    if (!file)
    {
        Logger::log("Failed opening " + path, Logger::Type::Error);
        return false;
    }

    stbi_set_flip_vertically_on_load(m_flipOnLoad ? 1 : 0);

    STBIMG_stbio_RWops io;
    stbi_callback_from_RW(file, &io);

    int32 w, h, fmt;
    auto* img = stbi_load_from_callbacks(&io.stb_cbs, &io, &w, &h, &fmt, 0);
    if (img)
    {
        m_flipped = true;

        ImageFormat::Type format = (fmt == 4) ? ImageFormat::RGBA : ImageFormat::RGB;
        auto result = loadFromMemory(static_cast<uint8*>(img), w, h, format);
        
        stbi_image_free(img);
        SDL_RWclose(file);

        stbi_set_flip_vertically_on_load(0);

        return result;
    }
    else
    {
        Logger::log("failed to open image: " + path, Logger::Type::Error);
        SDL_RWclose(file);

        stbi_set_flip_vertically_on_load(0);

        return false;
    }
}

bool Image::loadFromMemory(const uint8* px, uint32 width, uint32 height, ImageFormat::Type format)
{
    CRO_ASSERT(width > 0 && height > 0, "Invalid image dimension");

    if (format != ImageFormat::RGB &&
        format != ImageFormat::RGBA && format != ImageFormat::A)
    {
        Logger::log("Invalid image format, must be A, RGB or RGBA", Logger::Type::Error);
        return false;
    }

    std::size_t size = (format == ImageFormat::RGB) ? 3 : (format == ImageFormat::RGBA) ? 4 : 1;
    size *= (width * height);

    m_data.resize(size);
    if (m_flipped)
    {
        //copy row by row starting at bottom
        const auto rowSize = size / height;
        auto dst = m_data.data();
        auto src = px + ((m_data.size()) - rowSize);
        for (auto i = 0u; i < height; ++i)
        {
            std::memcpy(dst, src, rowSize);
            dst += rowSize;
            src -= rowSize;
        }

        m_flipped = false;
    }
    else
    {
        std::memcpy(m_data.data(), px, size);
    }

    m_size = { width, height };
    m_format = format;
    return true;
}

void Image::setTransparencyColour(Colour colour)
{
    CRO_ASSERT(m_format == ImageFormat::RGBA, "RGBA format required");
    CRO_ASSERT(!m_data.empty(), "Image not yet created");

    std::array<uint8, 3u> comp = { {colour.getRedByte(), colour.getGreenByte(), colour.getBlueByte()} };
    for (auto i = 0u; i < m_data.size(); i += 4)
    {
        if (m_data[i] == comp[0] &&
            m_data[i + 1] == comp[1] &&
            m_data[i + 2] == comp[2])
        {
            m_data[i + 3] = 0;
        }
    }
}

glm::uvec2 Image::getSize() const
{
    return m_size;
}

ImageFormat::Type Image::getFormat() const
{
    return m_format;
}

const uint8* Image::getPixelData() const
{
    return m_data.data();
}