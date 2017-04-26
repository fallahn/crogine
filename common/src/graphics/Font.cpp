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

#include <crogine/graphics/Font.hpp>
#include <crogine/graphics/Image.hpp>

#include <SDL_ttf.h>
#include <SDL_pixels.h>
#include <SDL_surface.h>

#include <array>
#include <cstring>

using namespace cro;

namespace
{
    const uint16 maxGlyphHeight = 30u; //maximum char size in pixels
    const uint16 firstChar = 32; //space in ascii
    const uint16 lastChar = 255; //extended ascii end
    const uint16 charCount = lastChar - firstChar;
    const uint16 charCountX = 16; //chars horizontally

    //rounds n to nearest pow2 value for padding images
    uint32 pow2(uint32 n)
    {
        CRO_ASSERT(n, "");
        n--;
        for (auto i = 1u; i < 32u; i *= 2u)
            n |= (n >> i);
        n++;

        return n;
    }
}

Font::Font()
    : m_type    (Type::Bitmap),
    m_font      (nullptr)
{

}

Font::~Font()
{
    if (m_font)
    {
        TTF_CloseFont(m_font);
    }
}

//public
bool Font::loadFromFile(const std::string& path)
{
    if (TTF_WasInit() == 0)
    {
        Logger::log("TTF fonts are unavailable.. failed to init SDL_ttf", Logger::Type::Error);
        return false;
    }

    if (m_font)
    {
        TTF_CloseFont(m_font);
        m_subRects.clear();
    }

    m_font = TTF_OpenFont(path.c_str(), maxGlyphHeight);
    if (m_font)
    {
        uint32 celWidth = 0;
        uint32 celHeight = 0;

        std::array<GlyphData, charCount> glyphData;
        std::array<MetricData, charCount> metricData;
        SDL_Color black, white;
        black.r = 0; black.g = 0; black.b = 0; black.a = 255;
        white.r = 255; white.g = 255; white.b = 255; white.a = 255;
        for (auto i = firstChar; i < lastChar; ++i) //start at 32 (Space)
        {
            auto index = i - firstChar;
            
            TTF_GlyphMetrics(m_font, i, &metricData[index].minx, &metricData[index].maxx, &metricData[index].miny, &metricData[index].maxy, 0);
            
            auto* glyph = TTF_RenderGlyph_Shaded(m_font, i, white, black);          
            if (glyph)
            {
                glyphData[index].width = glyph->pitch;
                glyphData[index].height = glyph->h;
                glyphData[index].data.resize(glyph->pitch * glyph->h);
                auto stride = glyph->format->BytesPerPixel;
                char* pixels = static_cast<char*>(glyph->pixels);
                for (auto j = 0u, k = 0u; j < glyphData[index].data.size(); ++j, k += stride)
                {
                    glyphData[index].data[j] = pixels[k];
                }

                if (glyph->w > celWidth)
                {
                    celWidth = glyph->w;
                }
                if (glyph->h > celHeight)
                {
                    celHeight = glyph->h;
                }
                
                SDL_FreeSurface(glyph);

                //TODO process distance field on glyph and mark font as distance field type
            }
            else
            {
                glyphData[index].width = 0;
                glyphData[index].height = 0;
            }
        }

        
        //load the glyphs into an image atlas
        auto imgWidth = pow2(celWidth * charCountX);
        auto glyphCountX = imgWidth / celWidth;
        auto glyphCountY = (charCount / glyphCountX) + 1;
        auto imgHeight = pow2(celHeight * glyphCountY);
        auto imgSize = imgWidth * imgHeight;

        std::vector<uint8> imgData(imgSize);
        std::memset(imgData.data(), 0, imgSize);
        FloatRect subRect(0.f, 0.f, static_cast<float>(celWidth), static_cast<float>(celHeight));
        
        std::size_t i = firstChar;
        for (auto y = 0u; y < glyphCountY; ++y)
        {
            for (auto x = 0u; x < glyphCountX && i < lastChar; ++x, ++i)
            {
                auto cx = celWidth * x;
                auto cy = celHeight * y;

                auto index = i - firstChar;
                subRect.left = static_cast<float>(cx);
                subRect.bottom = static_cast<float>(imgHeight - (cy + celHeight));
                subRect.width = static_cast<float>(metricData[index].maxx);
                m_subRects.insert(std::make_pair(static_cast<uint8>(i), subRect));

                //copy row by row
                for (auto j = 0u; j < glyphData[index].height; ++j)
                {
                    std::memcpy(&imgData[((j + cy) * imgWidth) + cx], &glyphData[index].data[j * glyphData[index].width], glyphData[index].width);
                }
            }
        }

        //flip
        std::vector<uint8> flippedData(imgData.size());
        auto dst = flippedData.data();
        auto src = imgData.data() + ((flippedData.size()) - imgWidth);
        for (auto i = 0u; i < imgHeight; ++i)
        {
            std::memcpy(dst, src, imgWidth);
            dst += imgWidth;
            src -= imgWidth;
        }

        m_texture.create(imgWidth, imgHeight, ImageFormat::A);
        return m_texture.update(flippedData.data());
    }
    else
    {
        Logger::log("Failed opening font " + path, Logger::Type::Error);
    }

    return false;
}

bool Font::loadFromImage(const Image& image, glm::vec2 charSize, Type type)
{
    CRO_ASSERT(image.getSize().x > 0 && image.getSize().y > 0, "Can't use empty image!");
    CRO_ASSERT(charSize.x > 0 && charSize.y > 0, "Can't use empty char size!");
    
    if (m_font)
    {
        TTF_CloseFont(m_font);
        m_font = nullptr;
        m_subRects.clear();
    }

    //create subrects from char size / image size
    auto charX = image.getSize().x / charSize.x;
    auto charY = image.getSize().y / charSize.y;
    for (auto y = 0; y < charY; ++y)
    {
        for (auto x = 0; x < charX; ++x)
        {
            auto idx = (y * charX + x) + 32; //start at ascii space
            m_subRects.insert(std::make_pair(static_cast<uint8>(idx), FloatRect(charSize.x * x, image.getSize().y - (charSize.y * (y + 1)), charSize.x, charSize.y)));
        }
    }

    m_type = type;

    m_texture.create(image.getSize().x, image.getSize().y, image.getFormat());
    return m_texture.update(image.getPixelData());
}

FloatRect Font::getGlyph(char c) const
{
    if (m_subRects.count(c) == 0)
    {
        //TODO insert a new glyph
        return {};
    }
    return m_subRects.find(c)->second;
}

const Texture& Font::getTexture() const
{
    return m_texture;
}

Font::Type Font::getType() const
{
    return m_type;
}