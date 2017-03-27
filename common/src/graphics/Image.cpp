/*-----------------------------------------------------------------------

Matt Marchant 2017
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

#include <SDL_image.h>

#include <array>
#include <cstring>

using namespace cro;

Image::Image()
    : m_format(ImageFormat::None)
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
    { 
        colour.getRedByte(),
        colour.getGreenByte(),
        colour.getBlueByte(),
        colour.getAlphaByte()
    };

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
    auto img = IMG_Load(path.c_str());
    if (img)
    {
        SDL_LockSurface(img);

        ImageFormat::Type format = (img->format->BitsPerPixel == 32) ? ImageFormat::RGBA : ImageFormat::RGB;
        auto result = loadFromMemory(static_cast<uint8*>(img->pixels), img->w, img->h, format);
        
        SDL_UnlockSurface(img);
        SDL_FreeSurface(img);

        return result;
    }
    else
    {
        Logger::log("failed to open image: " + path, Logger::Type::Error);
        return false;
    }
}

bool Image::loadFromMemory(const uint8* px, uint32 width, uint32 height, ImageFormat::Type format)
{
    CRO_ASSERT(width > 0 && height > 0, "Invalid image dimension");

    if (format != ImageFormat::RGB &&
        format != ImageFormat::RGBA)
    {
        Logger::log("Invalid image format, must be RGB or RGBA", Logger::Type::Error);
        return false;
    }

    std::size_t size = (format == ImageFormat::RGB) ? 3 : 4;
    size *= (width * height);

    m_data.resize(size);
    std::memcpy(m_data.data(), px, size);

    m_size = { width, height };
    m_format = format;
    return true;
}

void Image::setTransparencyColour(Colour colour)
{
    CRO_ASSERT(m_format == ImageFormat::RGBA, "RGBA format required");
    CRO_ASSERT(!m_data.empty(), "Image not yet created");

    std::array<uint8, 3u> comp = { colour.getRedByte(), colour.getGreenByte(), colour.getBlueByte() };
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