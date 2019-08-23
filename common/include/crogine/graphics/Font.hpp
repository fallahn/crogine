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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/detail/SDLResource.hpp>
#include <crogine/graphics/Texture.hpp>

#include <map>
#include <vector>

struct _TTF_Font;
namespace cro
{
    class Image;

    /*!
    \brief Font class.
    Fonts are created from a given texture atlas which may be a standard
    greyscale image or a signed distance field. Atlases use only the first (red)
    colour channel to multiply the colour of the given text instance when drawn.
    */
    class CRO_EXPORT_API Font final : public Detail::SDLResource
    {
    public:
        struct Glyph final
        {
            FloatRect rect;
            const Texture& texture;
            Glyph(const Texture& t) : texture(t) {}
        };

        enum Type
        {
            Bitmap = 0,
            SDF
        };

        Font();
        ~Font();
        Font(const Font&) = delete;
        const Font& operator = (const Font&) = delete;
        Font(Font&&) = delete;
        Font& operator = (Font&&) = delete;

        /*!
        \brief Attempts to load a font from a ttf file on disk.
        \param path Path to font file
        \returns true if successful else false
        */
        bool loadFromFile(const std::string& path);

        /*!
        \brief Creates a font from the given Image
        \param image Image holding the font atlas from which to create the font
        \param size Width and height of a character within the atlas
        \param type Type of image atlas, either bitmap or signed distance field
        \returns true on success, else false
        */
        bool loadFromImage(const Image& image, glm::vec2 size, Type type = Type::Bitmap);

        /*!
        \brief Attempts to return a float rect representing the sub rectangle of the atlas
        for the give character. Currently only ASCII chars are supported.
        */
        FloatRect getGlyph(char c, uint32 charSize) const;

        /*!
        \brief Returns a reference to the texture used by the font
        */
        const Texture& getTexture(uint32 charSize) const;

        /*!
        \brief Returns the type of this font, either bitmap or signed distance field
        */
        Type getType() const;

        /*!
        \brief Returns the lineheight of bitmap fonts or the default height
        of SDF fonts before resizing
        */
        float getLineHeight(uint32 charSize) const;

    private:

        Type m_type;
        std::string m_path;

        mutable _TTF_Font* m_font;

        struct GlyphData final
        {
            uint32 width, height;
            std::vector<uint8> data;
        };

        struct MetricData final
        {
            int32 minx, maxx, miny, maxy;
        };

        struct Page final
        {
            Texture texture;
            float lineHeight = 0.f;
            std::map<uint8, FloatRect> subrects;
        };
        mutable std::map<uint32, Page> m_pages;
        bool createPage(uint32 charSize) const;
    };
}