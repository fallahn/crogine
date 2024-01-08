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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/core/String.hpp>
#include <crogine/detail/SDLResource.hpp>
#include <crogine/graphics/Colour.hpp>
#include <crogine/graphics/Texture.hpp>

#include <map>
#include <unordered_map>
#include <vector>
#include <any>
#include <memory>

namespace cro
{
    class Image;

    //used internally so not exported
    struct Glyph final
    {
        float advance = 0.f;
        FloatRect bounds; //< relative to baseline
        FloatRect textureBounds; //< relative to texture atlas

        //some glyphs, eg emojis, won't want to be multiplied by fill colour
        bool useFillColour = true;
    };

    class Font;
    struct TextContext final
    {
        String string;
        const Font* font = nullptr;
        std::uint32_t charSize = 30u;
        float verticalSpacing = 0.f;
        Colour fillColour = Colour::White;
        Colour outlineColour = Colour::Black;
        float outlineThickness = 0.f;
        Colour shadowColour = Colour::Black;
        glm::vec2 shadowOffset = glm::vec2(0.f);
        bool bold = false;
        std::int32_t alignment = 0;
    };
    
    /*
    \brief Used by SimpleText to be notified when
    font textures are resized
    */
    struct CRO_EXPORT_API FontObserver
    {
        virtual ~FontObserver() {}
        virtual void onFontUpdate() = 0;
        virtual void removeFont() = 0;
    };


    /*!
    \brief Set of codepoint ranges which can be used with
    appendFromFile(). This is a non-exhaustive list and
    any custom range can of course be used, eg for Japanese
    */
    struct CRO_EXPORT_API CodePointRange final
    {
        static constexpr std::array<std::uint32_t, 2> Cyrillic = { 0x0400, 0x052F };
        static constexpr std::array<std::uint32_t, 2> Default = { 0x1, 0xffff };
        static constexpr std::array<std::uint32_t, 2> EmojiLower = { 0x231a, 0x23fe };
        static constexpr std::array<std::uint32_t, 2> EmojiMid = { 0x256d, 0x2bd1 };
        static constexpr std::array<std::uint32_t, 2> EmojiUpper = { 0x10000, 0x10ffff };
        static constexpr std::array<std::uint32_t, 2> Greek = { 0x0370, 0x03FF };
        static constexpr std::array<std::uint32_t, 2> KoreanAlphabet = { 0x3131, 0x3163 };
        static constexpr std::array<std::uint32_t, 2> KoreanCharacters = { 0xAC00, 0xD7A3 };
        static constexpr std::array<std::uint32_t, 2> Thai = { 0x0E00, 0x0E7F };
    };

    /*!
    \brief When appending an external font this struct can
    be configured to control how the font is applied.
    eg Emoji fonts will probably want to have bold, outline
    and fill colour disabled.
    */
    struct CRO_EXPORT_API FontAppendmentContext final
    {
        /*!
        \brief Range of codepoints which this font should cover
        */
        std::array<std::uint32_t, 2> codepointRange = CodePointRange::Default;
        /*!
        \brief Disables or enables bold rendering of glyphs created
        with the appended font
        */
        bool allowBold = true;
        /*!
        \brief Disables or enables outlining of glyphs created with the font
        */
        bool allowOutline = true;
        /*!
        \brief Allow glyphs created with this font to be multiplied by
        the fill colour of the text.
        Coloured glyphs, such as emojis, probably don't want their colour multiplied
        */
        bool allowFillColour = true;
    };

    /*!
    \brief Font class.
    Supports rendering all font types which are supported by libfreetype
    including some colour fonts, eg Emojis. Multiple font files can be
    loaded into a single Font instance and mapped to different codepoint
    ranges.
    \see loadFromFile()
    \see appendFromFile()
    */
    class CRO_EXPORT_API Font final : public Detail::SDLResource
    {
    public:

        Font();
        ~Font();
        Font(const Font&) = delete;
        const Font& operator = (const Font&) = delete;
        Font(Font&&) = delete;
        Font& operator = (Font&&) = delete;

        /*!
        \brief Attempts to load a font from a ttf file on disk.
        This will reset any currently loaded files, and map the
        new file to the default codepage range of 0x1-0xffff
        \param path Path to font file
        \returns true if successful else false
        */
        bool loadFromFile(const std::string& path);

        /*!
        \brief Attempts to load and add the given file to the
        existing font data.
        For example one might load further fonts such as cyrillic
        or emoji fonts, which can be mapped to custom codepoint ranges
        Code point values will overwrite from the start value if existing
        ranges are already mapped, and underwrite (existing high values take
        precedence) from the range end
        \param path A string containing the path to the font to append
        \param ctx FontAppendmentContext containing the rules dictating how
        the appended font should be rendered
        \see FontAppendmentContext
        */
        bool appendFromFile(const std::string& path, FontAppendmentContext ctx);


        /*!
        \brief Attempts to return a float rect representing the sub rectangle of the atlas
        for the given codepoint.
        */
        Glyph getGlyph(std::uint32_t codepoint, std::uint32_t charSize, bool bold = false, float outlineThickness = 0.f) const;

        /*!
        \brief Returns a reference to the texture used by the font.
        Note that different character sizes use different textures, and that
        when a texture is resized internally its GL handle my change
        */
        const Texture& getTexture(std::uint32_t charSize) const;

        /*!
        \brief Returns the line height 
        */
        float getLineHeight(std::uint32_t charSize) const;

        /*!
        \brief Returns the kerning between two characters
        */
        float getKerning(std::uint32_t cpA, std::uint32_t cpB, std::uint32_t charSize) const;

        /*!
        \brief Enables or disables smoothing on the font's internal texture
        The default value is false.
        */
        void setSmooth(bool smooth);

    private:

        bool m_useSmoothing;

        struct Row final
        {
            Row(std::uint32_t t, std::uint32_t h) : width(0), top(t), height(h) {}
            std::uint32_t width = 0;
            std::uint32_t top = 0;
            std::uint32_t height = 0;
        };

        struct Page final
        {
            Page();
            Texture texture;
            std::map<std::uint64_t, Glyph> glyphs;
            std::uint32_t nextRow = 0;
            std::vector<Row> rows;
            bool updated = false;
        };

        mutable std::unordered_map<std::uint32_t, Page> m_pages;
        mutable std::vector<std::uint8_t> m_pixelBuffer;

        struct FontData final
        {
            //use std::any so we don't expose freetype pointers to public API
            std::any face;
            std::any stroker;
            
            FontAppendmentContext context;
        };
        std::vector<FontData> m_fontData;

        const FontData& getFontData(std::uint32_t cp) const;

        Glyph loadGlyph(std::uint32_t cp, std::uint32_t charSize, bool bold, float outlineThickness) const;
        FloatRect getGlyphRect(Page&, std::uint32_t w, std::uint32_t h) const;
        bool setCurrentCharacterSize(std::uint32_t) const;

        void cleanup();

        friend class TextSystem;
        bool pageUpdated(std::uint32_t charSize) const;
        void markPageRead(std::uint32_t charSize) const;


        /*
        SIGH not actually const - but mutable to allow const refs to Fonts
        */
        friend class SimpleText;
        friend class Text;
        mutable std::vector<FontObserver*> m_observers;
        void registerObserver(FontObserver*) const;
        void unregisterObserver(FontObserver*) const;

        bool isRegistered(const FontObserver*) const;
    };
}