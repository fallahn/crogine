/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2023
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
by Laurent Gomila et al https://github.com/SFML/SFML/blob/master/src/SFML/Graphics/Font.cpp 
*/

#include "../detail/DistanceField.hpp"

#include <crogine/graphics/Font.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/Colour.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/core/FileSystem.hpp>

#include <array>
#include <cstring>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_BITMAP_H
#include FT_OUTLINE_H
#include FT_STROKER_H

#include <unordered_map>

using namespace cro;

namespace
{
    constexpr float MagicNumber = static_cast<float>(1 << 6);

    //used to create a unique key for bold/outline/codepoint glyphs
    //see https://github.com/SFML/SFML/blob/master/src/SFML/Graphics/Font.cpp#L66
    template <typename T, typename U>
    inline T reinterpret(const U& input)
    {
        CRO_ASSERT(sizeof(T) == sizeof(U), "");
        T output = 0;
        std::memcpy(&output, &input, sizeof(U));
        return output;
    }
    std::uint64_t combine(float outlineThickness, bool bold, std::uint32_t index)
    {
        return (static_cast<std::uint64_t>(reinterpret<std::uint32_t>(outlineThickness)) << 32) | (static_cast<std::uint64_t>(bold) << 31) | index;
    }

    //if we use the same font as multiple sub-fonts (eg emoji font)
    //these structs just makes sure each one is only loaded once
    //TODO we could probably also include the FT face, library and stroker here...
    struct FontDataBuffer final
    {
        const std::uint8_t* buffer = nullptr;
        FT_Long size = 0;
    };

    struct FontDataResource final
    {
        FT_Library library = nullptr;

        FontDataResource()
        {
            //init freetype
            if (FT_Init_FreeType(&library) != 0)
            {
                Logger::log("Failed to init freetype", Logger::Type::Error);
            }
        }

        ~FontDataResource()
        {
            FT_Done_FreeType(library);
        }

        std::unordered_map<std::string, std::vector<std::uint8_t>> fontData;

        FontDataBuffer getFontData(const std::string& path)
        {
            if (!library)
            {
                //we failed for some reason
                return {};
            }

            if (fontData.count(path) == 0)
            {
                RaiiRWops fontFile;
                fontFile.file = SDL_RWFromFile(path.c_str(), "r");
                if (!fontFile.file)
                {
                    Logger::log("Failed opening " + path, Logger::Type::Error);
                    return {};
                }

                std::vector<std::uint8_t> buffer;
                buffer.resize(fontFile.file->size(fontFile.file));
                if (buffer.size() == 0)
                {
                    Logger::log("Could not open " + path + ": files size was 0", Logger::Type::Error);
                    return {};
                }
                SDL_RWread(fontFile.file, buffer.data(), buffer.size(), 1);

                fontData.insert(std::make_pair(path, buffer));
            }

            const auto& data = fontData.at(path);
            FontDataBuffer retVal;
            retVal.buffer = data.data();
            retVal.size = static_cast<FT_Long>(data.size());

            return retVal;
        }

        std::size_t refCount = 0;
    };
    std::unique_ptr<FontDataResource> fontDataResource;
}

Font::Font()
    : m_useSmoothing(false)
{
    if (!fontDataResource)
    {
        fontDataResource = std::make_unique<FontDataResource>();
    }
    fontDataResource->refCount++;
}

Font::~Font()
{
    for (auto o : m_observers)
    {
        o->removeFont();
    }

    cleanup();

    fontDataResource->refCount--;

    if (fontDataResource->refCount == 0)
    {
        fontDataResource.reset();
    }
}

//public
bool Font::loadFromFile(const std::string& filePath)
{
    //remove existing loaded font
    cleanup();

    return appendFromFile(filePath, FontAppendmentContext());
}

bool Font::appendFromFile(const std::string& filePath, FontAppendmentContext ctx)
{
    CRO_ASSERT(ctx.codepointRange[0] > 0 && ctx.codepointRange[0] < ctx.codepointRange[1], "invalid codepoint range");

    auto path = FileSystem::getResourcePath() + filePath;
    FontData fd;
    fd.context = ctx;

    //load the face    
    auto buffer = fontDataResource->getFontData(filePath);
    if (buffer.buffer == nullptr)
    {
        //failed to load the resource
        return false;
    }


    FT_Face face = nullptr;
    if (FT_New_Memory_Face(fontDataResource->library, buffer.buffer, buffer.size, 0, &face) != 0)
    {
        Logger::log("Failed to load font " + path + ": Failed creating font face", Logger::Type::Error);
        return false;
    }

    //stroker used for rendering outlines
    FT_Stroker stroker = nullptr;
    if (FT_Stroker_New(fontDataResource->library, &stroker) != 0)
    {
        FT_Done_Face(face);
        LogE << "Failed to load font " << path << ": Failed to create stroker" << std::endl;
        return false;
    }


    //using unicode
    if (FT_Select_Charmap(face, FT_ENCODING_UNICODE) != 0)
    {
        Logger::log("Failed to load font " + path + ": failed to select unicode charset", Logger::Type::Error);
        FT_Done_Face(face);
        return false;
    }

    fd.face = std::make_any<FT_Face>(face);
    fd.stroker = std::make_any<FT_Stroker>(stroker);

    m_fontData.emplace_back(std::move(fd));

    if (m_fontData.size() > 1)
    {
        //sort by start range
        std::sort(m_fontData.begin(), m_fontData.end(),
            [](const FontData& a, const FontData& b)
        {
            return a.context.codepointRange[0] < b.context.codepointRange[0];
        });

        //correct range overlap, where start range is priorotised
        for (auto i = 0u; i < m_fontData.size() - 1; ++i)
        {
            if (m_fontData[i].context.codepointRange[1] >= m_fontData[i + 1].context.codepointRange[0])
            {
                m_fontData[i].context.codepointRange[1] = m_fontData[i + 1].context.codepointRange[0] - 1;
            }
        }
    }


    return true;
}

Glyph Font::getGlyph(std::uint32_t codepoint, std::uint32_t charSize, bool bold, float outlineThickness) const
{
    auto& fontData = getFontData(codepoint);
    auto& currentGlyphs = m_pages[charSize].glyphs;
    auto key = combine(outlineThickness, bold, FT_Get_Char_Index(std::any_cast<FT_Face>(fontData.face), codepoint));

    auto result = currentGlyphs.find(key);
    if (result != currentGlyphs.end())
    {
        return result->second;
    }
    else
    {
        //add the glyph to the page
        auto glyph = loadGlyph(codepoint, charSize, bold && fontData.context.allowBold, fontData.context.allowOutline ? outlineThickness : 0.f);
        return currentGlyphs.insert(std::make_pair(key, glyph)).first->second;
    }

    return {};
}

const Texture& Font::getTexture(std::uint32_t charSize) const
{
    //CRO_ASSERT(m_pages.count(charSize) != 0, "Page doesn't exist");
    //TODO this may return an invalid texture if the
    //current charSize is not inserted in the page map
    //and is automatically created
    return m_pages[charSize].texture;
}

float Font::getLineHeight(std::uint32_t charSize) const
{
    CRO_ASSERT(!m_fontData.empty(), "font not loaded");

    auto& fd = m_fontData[0];
    if (fd.face.has_value())
    {
        auto face = std::any_cast<FT_Face>(fd.face);
        if (face && setCurrentCharacterSize(charSize))
        {
            //there's some magic going on here...
            return static_cast<float>(face->size->metrics.height) / MagicNumber;
        }
    }
    return 0.f;
}

float Font::getKerning(std::uint32_t cpA, std::uint32_t cpB, std::uint32_t charSize) const
{
    if (cpA == 0 || cpB == 0 || m_fontData.empty())
    {
        return 0.f;
    }

    FT_Face face = std::any_cast<FT_Face>(m_fontData[0].face);

    if (face && FT_HAS_KERNING(face) && setCurrentCharacterSize(charSize))
    {
        //convert the characters to indices
        FT_UInt index1 = FT_Get_Char_Index(face, cpA);
        FT_UInt index2 = FT_Get_Char_Index(face, cpB);

        //get the kerning vector
        FT_Vector kerning;
        FT_Get_Kerning(face, index1, index2, FT_KERNING_DEFAULT, &kerning);

        //x advance is already in pixels for bitmap fonts
        if (!FT_IS_SCALABLE(face))
        {
            return static_cast<float>(kerning.x);
        }

        //return the x advance
        return static_cast<float>(kerning.x) / MagicNumber;
    }
    else
    {
        //invalid font, or no kerning
        return 0.f;
    }
}

void Font::setSmooth(bool smooth)
{
    if (smooth != m_useSmoothing)
    {
        m_useSmoothing = smooth;

        for (auto& page : m_pages)
        {
            page.second.texture.setSmooth(smooth);
        }
    }
}

//private
const Font::FontData& Font::getFontData(std::uint32_t codepoint) const
{
    CRO_ASSERT(!m_fontData.empty(), "No fonts are loaded");
    for (auto& fd : m_fontData)
    {
        if (codepoint >= fd.context.codepointRange[0]
            && codepoint <= fd.context.codepointRange[1])
        {
            return fd;
        }
    }
    return m_fontData[0];
}

Glyph Font::loadGlyph(std::uint32_t codepoint, std::uint32_t charSize, bool bold, float outlineThickness) const
{
    Glyph retVal;

    if (m_fontData.empty())
    {
        return retVal;
    }

    auto& fd = getFontData(codepoint);

    auto face = std::any_cast<FT_Face>(fd.face);
    if (!face)
    {
        return retVal;
    }

    if (!setCurrentCharacterSize(charSize))
    {
        return retVal;
    }

    //fetch the glyph
    FT_Int32 flags = FT_LOAD_TARGET_NORMAL | FT_LOAD_FORCE_AUTOHINT | FT_LOAD_COLOR;

    if (FT_Load_Char(face, codepoint, flags) != 0)
    {
        return retVal;
    }

    FT_Glyph glyphDesc;
    if (FT_Get_Glyph(face->glyph, &glyphDesc) != 0)
    {
        return retVal;
    }

    //get the outline
    constexpr FT_Pos weight = (1 << 6);
    bool outline = (glyphDesc->format == FT_GLYPH_FORMAT_OUTLINE);
    if (outline)
    {
        if (bold)
        {
            FT_OutlineGlyph outlineGlyph = (FT_OutlineGlyph)glyphDesc;
            FT_Outline_Embolden(&outlineGlyph->outline, weight);
        }

        if (outlineThickness != 0)
        {
            auto stroker = std::any_cast<FT_Stroker>(fd.stroker);
            FT_Stroker_Set(stroker, static_cast<FT_Fixed>(outlineThickness * MagicNumber), FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
            FT_Glyph_Stroke(&glyphDesc, stroker, true);
        }
    }

    //rasterise it
    FT_Bitmap* bitmap = nullptr;
    //FT_Glyph glyphDouble;

    if (fd.context.allowOutline || fd.context.allowBold)
    {
        FT_Glyph_To_Bitmap(&glyphDesc, FT_RENDER_MODE_NORMAL, 0, 1); //this renders alpha coverage only
        bitmap = &reinterpret_cast<FT_BitmapGlyph>(glyphDesc)->bitmap;
    }
    else
    {
        //TODO use this to render a double density texture
        /*FT_Matrix scaleMat;
        scaleMat.xx = 2;
        scaleMat.xy = 0;
        scaleMat.yy = 2;
        scaleMat.yx = 0;

        FT_Glyph_Copy(glyphDesc, &glyphDouble);
        FT_Glyph_Transform(glyphDouble, &scaleMat, nullptr);
        */

        //this is needed if we're rendering colour
        //so assume outlining etc is disabled.
        FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
        bitmap = &face->glyph->bitmap;
    }

    if (!outline)
    {
        if (bold)
        {
            FT_Bitmap_Embolden(fontDataResource->library, bitmap, weight, weight);
        }
    }


    retVal.advance = static_cast<float>(face->glyph->metrics.horiAdvance) / static_cast<float>(1<<6);
    if (bold)
    {
        retVal.advance += static_cast<float>(weight) / MagicNumber; //surely this is the same as adding 1?
    }

    std::int32_t width = bitmap->width;
    std::int32_t height = bitmap->rows;

    if (width > 0 && height > 0)
    {
        const std::uint32_t padding = 2;

        //pad the glyph to stop potential bleed
        width += 2 * padding;
        height += 2 * padding;

        //get the current page
        auto& page = m_pages[charSize];
        page.texture.setSmooth(m_useSmoothing);

        //find somewhere to insert the glyph
        retVal.textureBounds = getGlyphRect(page, width, height);

        //readjust texture rect for padding
        retVal.textureBounds.left += padding;
        retVal.textureBounds.bottom += padding;
        retVal.textureBounds.width -= padding * 2;
        retVal.textureBounds.height -= padding * 2;

        retVal.bounds.left = static_cast<float>(face->glyph->metrics.horiBearingX) / MagicNumber;
        retVal.bounds.bottom = static_cast<float>(face->glyph->metrics.horiBearingY) / MagicNumber;
        retVal.bounds.width = static_cast<float>(face->glyph->metrics.width) / MagicNumber;
        retVal.bounds.height = static_cast<float>(face->glyph->metrics.height) / MagicNumber;
        retVal.bounds.bottom -= retVal.bounds.height;

        if (&fd != &m_fontData[0])
        {
            //scale other characters to match that of the base font
            const float lineHeight = getLineHeight(charSize);
            const float scale = std::min(1.f, lineHeight / retVal.bounds.height);
            retVal.bounds.height *= scale;
            retVal.bounds.width *= scale;

            retVal.bounds.height -= 1.f;
            retVal.bounds.width -= 1.f;
            retVal.bounds.bottom += 1.f;
        }


        //buffer the pixel data and update the page texture
        m_pixelBuffer.resize(width * height * 4);
        std::fill(m_pixelBuffer.begin(), m_pixelBuffer.end(), 0);

        //copy from rasterised bitmap
        const auto* pixels = bitmap->buffer;
        if (bitmap->pixel_mode == FT_PIXEL_MODE_MONO)
        {
            //for(auto y = height - padding - 1; y >= padding; --y)
            for(auto y = padding; y < height - padding; ++y)
            {
                for (auto x = padding; x < width - padding; ++x)
                {
                    const std::size_t index = x + y * width;
                    m_pixelBuffer[index * 4 + 3] = ((pixels[(x - padding) / 8]) & (1 << (7 - ((x - padding) % 8)))) ? 255 : 0;
                }
                pixels += bitmap->pitch;
            }
        }
        else if (bitmap->pixel_mode == FT_PIXEL_MODE_BGRA)
        {
            for (auto y = padding; y < height - padding; ++y)
            {
                for (auto x = padding; x < width - padding; ++x)
                {
                    const std::size_t index = (x + y * width)* 4;
                    const std::size_t xIndex = (x - padding) * 4;

                    m_pixelBuffer[index] = pixels[xIndex + 2];
                    m_pixelBuffer[index + 1] = pixels[xIndex + 1];
                    m_pixelBuffer[index + 2] = pixels[xIndex];

                    m_pixelBuffer[index + 3] = pixels[xIndex + 3];
                }
                pixels += bitmap->pitch;
            }
        }
        else
        {
            //for (auto y = height - padding - 1; y >= padding; --y)
            for (auto y = padding; y < height - padding; ++y)
            {
                for (auto x = padding; x < width - padding; ++x)
                {
                    const std::size_t index = (x + y * width) * 4;
                    
                    m_pixelBuffer[index] = 255;
                    m_pixelBuffer[index + 1] = 255;
                    m_pixelBuffer[index + 2] = 255;
                    m_pixelBuffer[index + 3] = pixels[x - padding];
                }
                pixels += bitmap->pitch;
            }
        }

        //finally copy to texture
        auto x = static_cast<std::uint32_t>(retVal.textureBounds.left) - padding;
        auto y = static_cast<std::uint32_t>(retVal.textureBounds.bottom) - padding;
        auto w = static_cast<std::uint32_t>(retVal.textureBounds.width) + padding * 2;
        auto h = static_cast<std::uint32_t>(retVal.textureBounds.height) + padding * 2;
        page.texture.update(m_pixelBuffer.data(), false, { x,y,w,h });
    }

    retVal.useFillColour = fd.context.allowFillColour;
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
                texture.setSmooth(page.texture.isSmooth());
                texture.update(page.texture);
                
                page.texture.swap(texture);
                page.updated = true;

                for (auto* o : m_observers)
                {
                    o->onFontUpdate();
                }
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

    FloatRect retVal(static_cast<float>(row->width), static_cast<float>(row->top), static_cast<float>(width), static_cast<float>(height));
    row->width += width;

    return retVal;
}

bool Font::setCurrentCharacterSize(std::uint32_t size) const
{
    for (auto& fd : m_fontData)
    {
        auto face = std::any_cast<FT_Face>(fd.face);
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
                    for (auto i = 0; i < face->num_fixed_sizes; ++i)
                    {
                        ss << ((face->available_sizes[i].y_ppem + 32) >> 6) << " ";
                    }
                    Logger::log(ss.str(), Logger::Type::Info);
                }
            }

            if (result != FT_Err_Ok)
            {
                return false;
            }
        }
    }
    return true;
}

void Font::cleanup()
{
    for (auto& fd : m_fontData)
    {
        if (fd.stroker.has_value())
        {
            auto stroker = std::any_cast<FT_Stroker>(fd.stroker);
            if (stroker)
            {
                FT_Stroker_Done(stroker);
            }
        }

        if (fd.face.has_value())
        {
            auto face = std::any_cast<FT_Face>(fd.face);
            if (face)
            {
                FT_Done_Face(face);
            }
        }

        fd.stroker.reset();
        fd.face.reset();
    }

    m_fontData.clear();

    m_pages.clear();
    m_pixelBuffer.clear();
}

bool Font::pageUpdated(std::uint32_t charSize) const
{
    return m_pages[charSize].updated;
}

void Font::markPageRead(std::uint32_t charSize) const
{
    m_pages[charSize].updated = false;
}

void Font::registerObserver(FontObserver* o) const
{
    m_observers.push_back(o);
}

void Font::unregisterObserver(FontObserver* o) const
{
    m_observers.erase(std::remove_if(m_observers.begin(), m_observers.end(), [o](const FontObserver* ob) { return o == ob; }), m_observers.end());
}

bool Font::isRegistered(const FontObserver* o) const
{
    return std::find_if(m_observers.begin(), m_observers.end(), [o](const FontObserver* fo) {return fo == o; }) != m_observers.end();
}

Font::Page::Page()
{
    cro::Image img;
    img.create(128, 128, Colour(1.f, 1.f, 1.f, 0.f));
    texture.create(128, 128);
    texture.update(img.getPixelData());
}