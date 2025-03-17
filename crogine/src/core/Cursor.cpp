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

#include <crogine/core/App.hpp>
#include <crogine/core/Cursor.hpp>
#include <crogine/core/Window.hpp>
#include <crogine/graphics/Image.hpp>

#include <cstring>

using namespace cro;

Cursor::Cursor(SystemCursor type)
    : m_surface (nullptr),
    m_cursor    (nullptr),
    m_inUse     (false)
{
    SDL_SystemCursor cursorType = static_cast<SDL_SystemCursor>(type);
    m_cursor = SDL_CreateSystemCursor(cursorType);

    if (!m_cursor)
    {
        const std::string error = SDL_GetError();
        LogE << "SDL: Failed to create system cursor, reason: " << error << std::endl;
    }
}

Cursor::Cursor(const std::string& path, std::int32_t x, std::int32_t y)
    : m_surface (nullptr),
    m_cursor    (nullptr),
    m_inUse     (false)
{
    Image image(true);
    if (image.loadFromFile(path))
    {
        auto size = image.getSize();
        auto format = image.getFormat();
        if (size.x == 0 || size.y == 0)
        {
            LogE << "No valid image loaded for cursor" << std::endl;
        }
        else if (format == ImageFormat::RGB ||
            format == ImageFormat::RGBA)
        {
            const auto channels = format == ImageFormat::RGB ? 3 : 4;

            auto* data = image.getPixelData();
            auto length = size.x * size.y;
            length *= channels;

            m_imageData.resize(length);
            std::memcpy(m_imageData.data(), data, length);

            static constexpr std::uint32_t rMask = 0x000000ff;
            static constexpr std::uint32_t gMask = 0x0000ff00;
            static constexpr std::uint32_t bMask = 0x00ff0000;
            const std::uint32_t aMask = format == ImageFormat::RGB ? 0 : 0xff000000;

            const std::int32_t depth = 8 * channels;
            const std::int32_t pitch = size.x * channels;

            m_surface = SDL_CreateRGBSurfaceFrom(m_imageData.data(), size.x, size.y, depth, pitch, rMask, gMask, bMask, aMask);

            if (m_surface)
            {
                m_cursor = SDL_CreateColorCursor(m_surface, x, y);
                if (!m_cursor)
                {
                    const std::string error = SDL_GetError();
                    LogE << "SDL: Failed creating colour cursor: " << error << std::endl;

                    SDL_FreeSurface(m_surface);
                    m_surface = nullptr;
                }
            }
            else
            {
                const std::string error = SDL_GetError();
                LogE << "SDL: Failed creating surface for cursor: " << error << std::endl;
            }
        }
        else
        {
            LogE << "Cursor image must be RGB or RGBA format" << std::endl;
        }
    }
    else
    {
        LogE << "Cursor image not loaded." << std::endl;
    }
}

Cursor::~Cursor()
{
    if (m_inUse)
    {
        LogW << "Cursor was in use when it was destroyed" << std::endl;
        SDL_SetCursor(nullptr);
        
        App::getWindow().m_cursor = nullptr;
    }

    if (m_cursor)
    {
        SDL_FreeCursor(m_cursor);
    }

    if (m_surface)
    {
        SDL_FreeSurface(m_surface);
    }
}