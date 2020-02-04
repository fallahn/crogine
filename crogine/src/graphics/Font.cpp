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

/*
Based on the source of SFML's font class
by Laurent Gomila et al
*/

#include <crogine/graphics/Font.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/Colour.hpp>
#include "../detail/DistanceField.hpp"

#include <array>
#include <cstring>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_BITMAP_H

using namespace cro;

namespace
{
    template <typename T, typename U>
    inline T reinterpret(const U& input)
    {
        T output;
        std::memcpy(&output, &input, sizeof(U));
        return output;
    }
}

Font::Font()
{

}

Font::~Font()
{
    cleanup();
}

//public
bool Font::loadFromFile(const std::string& path)
{
    //init freetype
    FT_Library library;
    if (FT_Init_FreeType(&library) != 0)
    {
        Logger::log("Failed to load font " + path + ": Failed to init freetype", Logger::Type::Error);
        return false;
    }
    m_library = std::make_any<FT_Library>(library);

    //load the face
    //TODO this currently doesn't work on android because of file system considerations
    //this can be fixed with a custom loader using SDL's RWops
    FT_Face face;
    if (FT_New_Face(library, path.c_str(), 0, &face) != 0)
    {
        Logger::log("Failed to load font " + path + ": Failed creating font face", Logger::Type::Error);
        return false;
    }

    //TODO could use FT's stroke functions for creatnig text outlines

    //using unicode
    if (FT_Select_Charmap(face, FT_ENCODING_UNICODE) != 0)
    {
        Logger::log("Failed to load font " + path + ": failed to select unicode charset", Logger::Type::Error);
        FT_Done_Face(face);
        return false;
    }

    m_face = std::make_any<FT_Face>(face);

    return true;
}

FloatRect Font::getGlyph(uint32 codepoint, uint32 charSize) const
{
    auto& currentGlyphs = m_pages[charSize].glyphs;

    //TODO this assumes pages all contain the default font style
    //ie not bold or underlined - in these cases we'd need to create
    //a unique key instead of just using the codepoint value...

    auto result = currentGlyphs.find(codepoint);
    if (result != currentGlyphs.end())
    {
        return result->second.textureBounds;
    }
    else
    {
        //add the glyph to the page
        auto glyph = loadGlyph(codepoint, charSize);
        return currentGlyphs.insert(std::make_pair(codepoint, glyph)).first->second.textureBounds;
    }

    return {};
}

const Texture& Font::getTexture(uint32 charSize) const
{
    //TODO this may return an invalid texture if the
    //current charSize is not inserted in the page map
    //and is automatically created
    return m_pages[charSize].texture;
}

float Font::getLineHeight(uint32 charSize) const
{
    auto face = std::any_cast<FT_Face>(m_face);
    if (face && setCurrentCharacterSize(charSize))
    {
        //there's some magic going on here...
        return static_cast<float>(face->size->metrics.height) / static_cast<float>(1 << 6);
    }
    return 0.f;
}

//private
Glyph Font::loadGlyph(std::uint32_t codepoint, std::uint32_t charSize) const
{
    Glyph retVal;

    auto face = std::any_cast<FT_Face>(m_face);
    if (!face)
    {
        return retVal;
    }

    if (!setCurrentCharacterSize(charSize))
    {
        return retVal;
    }

    //fetch the glyph
    FT_Int32 flags = FT_LOAD_TARGET_NORMAL | FT_LOAD_FORCE_AUTOHINT;

    if (FT_Load_Char(face, codepoint, flags) != 0)
    {
        return retVal;
    }

    FT_Glyph glyphDesc;
    if (FT_Get_Glyph(face->glyph, &glyphDesc) != 0)
    {
        return retVal;
    }

    //rasterise it
    FT_Glyph_To_Bitmap(&glyphDesc, FT_RENDER_MODE_NORMAL, 0, 1);
    FT_Bitmap& bitmap = reinterpret_cast<FT_BitmapGlyph>(glyphDesc)->bitmap;

    retVal.advance = static_cast<float>(face->glyph->metrics.horiAdvance) / static_cast<float>(1<<6);

    std::int32_t width = bitmap.width;
    std::int32_t height = bitmap.rows;

    if (width > 0 && height > 0)
    {
        const std::uint32_t padding = 2;

        //pad the glyph to stop potential bleed
        width += 2 * padding;
        height += 2 * padding;

        //get the current page
        auto& page = m_pages[charSize];

        //find somewhere to insert the glyph
        retVal.textureBounds = getGlyphRect(page, width, height);

        //readjust texture rect for padding
        retVal.textureBounds.left += padding;
        retVal.textureBounds.bottom += padding;
        retVal.textureBounds.width -= padding * 2;
        retVal.textureBounds.height -= padding * 2;

        retVal.bounds.left = static_cast<float>(face->glyph->metrics.horiBearingX) / static_cast<float>(1 << 6);
        retVal.bounds.bottom = -static_cast<float>(face->glyph->metrics.horiBearingY) / static_cast<float>(1 << 6);
        retVal.bounds.width = static_cast<float>(face->glyph->metrics.width) / static_cast<float>(1 << 6);
        retVal.bounds.height = static_cast<float>(face->glyph->metrics.height) / static_cast<float>(1 << 6);


        //buffer the pixel data and update the page texture
        m_pixelBuffer.resize(width * height * 4);

        auto* current = m_pixelBuffer.data();
        auto* end = current + m_pixelBuffer.size();

        while (current != end)
        {
            (*current++) = 255;
            (*current++) = 255;
            (*current++) = 255;
            (*current++) = 0;
        }

        //copy from rasterised bitmap
        const auto* pixels = bitmap.buffer;
        if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO)
        {
            for(auto y = height - padding - 1; y >= padding; --y)
            {
                for (auto x = padding; x < width - padding; ++x)
                {
                    std::size_t index = x + y * width;
                    m_pixelBuffer[index * 4 + 3] = ((pixels[(x - padding) / 8]) & (1 << (7 - ((x - padding) % 8)))) ? 255 : 0;
                }
                pixels += bitmap.pitch;
            }
        }
        else
        {
            for (auto y = height - padding - 1; y >= padding; --y)
            {
                for (auto x = padding; x < width - padding; ++x)
                {
                    std::size_t index = x + y * width;
                    m_pixelBuffer[index * 4 + 3] = pixels[x - padding];
                }
                pixels += bitmap.pitch;
            }
        }

        //finally copy to texture
        auto x = static_cast<std::uint32_t>(retVal.textureBounds.left) - padding;
        auto y = static_cast<std::uint32_t>(retVal.textureBounds.bottom) - padding;
        auto w = static_cast<std::uint32_t>(retVal.textureBounds.width) + padding * 2;
        auto h = static_cast<std::uint32_t>(retVal.textureBounds.height) + padding * 2;
        page.texture.update(m_pixelBuffer.data(), false, { x,y,w,h });
    }

    return retVal;
}

FloatRect Font::getGlyphRect(Page& page, std::uint32_t width, std::uint32_t height) const
{
    Row* row = nullptr;
    float bestRatio = 0.f;

    for (auto& currRow : page.rows)
    {
        float ratio = static_cast<float>(height) / currRow.height;

        //ignore rows too short or too tall
        if (ratio < 0.7f || ratio > 1.f)
        {
            continue;
        }

        //check there's space in the row
        if (width > page.texture.getSize().x - currRow.width)
        {
            continue;
        }

        if (ratio < bestRatio)
        {
            continue;
        }

        row = &currRow;
        bestRatio = ratio;
        break;
    }

    //if we didn't find a row, insert one 10% bigger than the glyph
    if (!row)
    {
        std::int32_t rowHeight = height + (height / 10);
        while (page.nextRow + rowHeight > page.texture.getSize().y
            || width >= page.texture.getSize().x)
        {
            auto texWidth = page.texture.getSize().x;
            auto texHeight = page.texture.getSize().y;

            if (texWidth * 2 <= Texture::getMaxTextureSize()
                && texHeight * 2 <= Texture::getMaxTextureSize())
            {
                //increase texture 4 fold
                Texture texture;
                texture.create(texWidth * 2, texHeight * 2);
                texture.setSmooth(true);
                texture.update(page.texture);
                page.texture.swap(texture);
            }
            else
            {
                //doesn't fit :(
                Logger::log("Failed to add new character to font - max texture size reached.", Logger::Type::Error);
                return { 0.f, 0.f, 2.f, 2.f };
            }
        }

        row = &page.rows.emplace_back(page.nextRow, rowHeight);
        page.nextRow += rowHeight;
    }

    FloatRect retVal(row->width, row->top, width, height);
    row->width += width;

    return retVal;
}

bool Font::setCurrentCharacterSize(std::uint32_t size) const
{
    auto face = std::any_cast<FT_Face>(m_face);
    FT_UShort currentSize = face->size->metrics.x_ppem;

    if (currentSize != size)
    {
        FT_Error result = FT_Set_Pixel_Sizes(face, 0, size);

        if (result == FT_Err_Invalid_Pixel_Size)
        {
            Logger::log("Failed to set font face to " + std::to_string(size), Logger::Type::Error);

            //bitmap fonts may fail if the size isn't supported
            if (!FT_IS_SCALABLE(face))
            {
                Logger::log("Available sizes:", Logger::Type::Info);
                std::stringstream ss;
                for (auto i = 0u; i < face->num_fixed_sizes; ++i)
                {
                    ss << ((face->available_sizes[i].y_ppem + 32) >> 6) << " ";
                }
                Logger::log(ss.str(), Logger::Type::Info);
            }
        }

        return result == FT_Err_Ok;
    }
    return true;
}

void Font::cleanup()
{
    auto face = std::any_cast<FT_Face>(m_face);
    if (face)
    {
        FT_Done_Face(face);
    }

    auto library = std::any_cast<FT_Library>(m_library);
    if (library)
    {
        FT_Done_FreeType(library);
    }

    m_face.reset();
    m_library.reset();

    m_pages.clear();
    m_pixelBuffer.clear();
}

Font::Page::Page()
{
    cro::Image img;
    img.create(128, 128, Colour(1.f, 1.f, 1.f, 0.f));
    texture.create(128, 128);
    texture.update(img.getPixelData());
    texture.setSmooth(true);
}