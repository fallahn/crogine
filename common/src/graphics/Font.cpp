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
    const FloatRect defaultRect(0.f, 0.f, 0.f, 0.f);
    const uint16 maxGlyphHeight = 40u; //maximum char size in pixels
    const uint16 charCountX = 16u;
    const uint16 charCountY = 16u;
    const uint16 charCount = charCountX * charCountY; //just while we use extended ascii

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
    : m_type(Type::Bitmap)
{

}

//public
bool Font::loadFromFile(const std::string& path)
{
    //TODO need to check if this font as already loaded
    
    if (TTF_WasInit() == 0)
    {
        Logger::log("TTF fonts are unavailable.. failed to init SDL_ttf", Logger::Type::Error);
        return false;
    }

    auto* font = TTF_OpenFont(path.c_str(), maxGlyphHeight);
    if (font)
    {
        uint32 celWidth = 0;
        uint32 celHeight = 0;

        std::array<GlyphData, charCount> glyphData;
        std::array<MetricData, charCount> metricData;
        SDL_Color black, white;
        black.r = 0; black.g = 0; black.b = 0; black.a = 255;
        white.r = 255; white.g = 255; white.b = 255; white.a = 255;
        for (auto i = 0u; i < charCount; ++i)
        {
            TTF_GlyphMetrics(font, i, &metricData[i].minx, &metricData[i].maxx, &metricData[i].miny, &metricData[i].maxy, 0);           
            
            auto* glyph = TTF_RenderGlyph_Shaded(font, i, white, black);          
            if (glyph)
            {
                glyphData[i].width = glyph->pitch;
                glyphData[i].height = glyph->h;
                glyphData[i].data.resize(glyph->pitch * glyph->h);
                auto stride = glyph->format->BytesPerPixel;
                char* pixels = static_cast<char*>(glyph->pixels);
                for (auto j = 0u, k = 0u; j < glyphData[i].data.size(); ++j, k += stride)
                {
                    glyphData[i].data[j] = pixels[k];
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
                glyphData[i].width = 0;
                glyphData[i].height = 0;
            }
        }

        TTF_CloseFont(font);

        //load the glyphs into an image atlas
        auto imgWidth = pow2(celWidth * charCountX);
        auto glyphCountX = imgWidth / celWidth;
        auto glyphCountY = (charCount / glyphCountX) + 1;
        auto imgHeight = pow2(celHeight * glyphCountY);
        auto imgSize = imgWidth * imgHeight;

        std::vector<uint8> imgData(imgSize);
        std::memset(imgData.data(), 0, imgSize);
        FloatRect subRect(0.f, 0.f, static_cast<float>(celWidth), static_cast<float>(celHeight));
        
        std::size_t i = 0;
        for (auto y = 0u; y < glyphCountY; ++y)
        {
            for (auto x = 0u; x < glyphCountX && i < charCount; ++x, ++i)
            {
                auto cx = celWidth * x;
                auto cy = celHeight * y;

                subRect.left = static_cast<float>(cx);
                subRect.bottom = static_cast<float>(imgHeight - (cy + celHeight));
                m_subRects.insert(std::make_pair(static_cast<uint8>(i), subRect));

                //copy row by row
                for (auto j = 0u; j < glyphData[i].height; ++j)
                {
                    std::memcpy(&imgData[((j + cy) * imgWidth) + cx], &glyphData[i].data[j * glyphData[i].width], glyphData[i].width);
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
        m_texture.update(flippedData.data());

        return true;
    }
    else
    {
        Logger::log("Failed opening font " + path, Logger::Type::Error);
    }

    return false;
}

bool Font::loadFromImage(const Image& image, glm::vec2 charSize, Type type)
{
    //TODO check if font was already loaded successfully
    return false;
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