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

#include "../detail/stb_image.h"
#include "../detail/stb_image_write.h"
#include "../detail/SDLImageRead.hpp"
#include <SDL_rwops.h>

#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/Colour.hpp>

#include <crogine/detail/Assert.hpp>
#include <crogine/core/FileSystem.hpp>
#include <crogine/core/Log.hpp>

#include <array>
#include <cstring>

using namespace cro;

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

bool Image::loadFromFile(const std::string& filePath)
{
    auto path = FileSystem::getResourcePath() + filePath;

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
    return m_data.empty() ? nullptr : m_data.data();
}

void image_writer_func(void* context, void* data, int size)
{
    SDL_RWops* file = (SDL_RWops*)context;
    SDL_RWwrite(file, data, size, 1);
}

bool Image::write(const std::string& path)
{
    if (cro::FileSystem::getFileExtension(path) != ".png")
    {
        Logger::log("Only png files currently supported", Logger::Type::Error);
        return false;
    }

    std::int32_t pixelWidth = 0;
    if (m_format == ImageFormat::RGB)
    {
        pixelWidth = 3;
    }
    else if (m_format == ImageFormat::RGBA)
    {
        pixelWidth = 4;
    }
    else if (m_format == ImageFormat::A)
    {
        pixelWidth = 1;
    }

    if (pixelWidth == 0)
    {
        Logger::log("Only RGB and RGBA format images currently supported", Logger::Type::Error);
        return false;
    }

    stbi_flip_vertically_on_write(m_flipped ? 1 : 0);

    RaiiRWops out;
    out.file = SDL_RWFromFile(path.c_str(), "w");
    return stbi_write_png_to_func(image_writer_func, out.file, m_size.x, m_size.y, pixelWidth, m_data.data(), m_size.x * pixelWidth) != 0;
}