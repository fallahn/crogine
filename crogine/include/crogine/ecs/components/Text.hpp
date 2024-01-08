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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/core/String.hpp>
#include <crogine/graphics/Font.hpp>
#include <crogine/graphics/Rectangle.hpp>
#include <crogine/graphics/Colour.hpp>
#include <crogine/graphics/Vertex2D.hpp>
#include <crogine/ecs/Entity.hpp>

#include <crogine/detail/glm/vec2.hpp>

namespace cro
{
    struct Glyph;
    class Drawable2D;

    /*!
    \brief 2D Text component.
    Text components, when used in conjunction with Transformable
    and Drawable2D components are used to render strings of Text
    with a RenderSystem2D. The draw order of Text components can
    be set with their Y or Z transform position, depending on
    the sort order set in RenderSystem2D. If a text component
    appears to not render then check that its sort order value is
    above that of any other Drawable2D types, such as Sprites,
    currently being rendered by the same RenderSystem2D.
    Text components also require a TextSystem in the active Scene.
    \see RenderSystem2D::setSortOrder()
    */
    class CRO_EXPORT_API Text final : private FontObserver
    {
    public:
        Text();
        ~Text();

        Text(const Text&);
        Text(Text&&) noexcept;

        Text& operator = (const Text&);
        Text& operator = (Text&&) noexcept;


        /*!
        \brief Construct a Text component with a given Font
        \param font Font to use when rendering
        */
        Text(const Font& font);

        /*!
        \brief Set the font to be used with this Text
        \param font Font to use when rendering
        */
        void setFont(const Font& font);

        /*!
        \brief Set the character size of the Text
        \param size The size of the character in points
        */
        void setCharacterSize(std::uint32_t size);

        /*!
        \brief Sets the vertical spacing between lines of text
        \param spacing By default the spacing is 0. Negative values
        can cause lines to overlap or even appear in reverse order.
        */
        void setVerticalSpacing(float spacing);

        /*!
        \brief Set the string to render with this Text
        \param str A cro::String containing the string to use
        */
        void setString(const String& str);

        /*!
        \brief Set the Text fill colour
        \param colour The colour with which the inner part of the
        text will be rendered. Defaults to cro::Colour::White
        */
        void setFillColour(Colour colour);

        /*!
        \brief Set the colour used for the text outline
        If the outline id not visible make sure that this
        colour is not transparent, and that the outline
        thickness is greater than 0. Defaults to cro::Colour::Black
        \param colour Colour with which to render the outline
        \see setOutlineThickness
        */
        void setOutlineColour(Colour colour);

        /*!
        \brief Set the thickness of the text outline
        Negative values will not have the expected result.
        The outline colour must not be transparent and a value
        greater than zero must be set in order for outlines to appear
        \param outlineThickness The thickness, in pixels, of
        the outline to render
        */
        void setOutlineThickness(float outlineThickness);

        /*!
        \brief Sets the drop-shadow colour, applied when the
        shadow offset is not 0, and the border value is set to 0
        \param colour Colour with which to render the shadow
        */
        void setShadowColour(Colour colour);

        /*!
        \brief Sets the shadow offset value.
        Shadows are only rendered if the Text has no outlining
        \param offset A vec2 containing the offset in X/Y direction
        \see setShadowColour()
        */
        void setShadowOffset(glm::vec2 offset);

        /*!
        \brief Renders this text with bold styling, if the font supports it
        \param bold True to render with bold styling, or false for default rendering
        */
        void setBold(bool bold);

        /*!
        \brief Return a pointer to the active font
        May be nullptr
        */
        const Font* getFont() const;

        /*!
        \brief Returns the current character size
        */
        std::uint32_t getCharacterSize() const;

        /*!
        \brief Returns the current line spacing
        */
        float getVerticalSpacing() const;

        /*!
        \brief Returns a reference to the current string
        */
        const String& getString() const;

        /*!
        \brief Returns the current fill colour
        */
        Colour getFillColour() const;

        /*!
        \brief Gets the colour used to draw the text outline
        */
        Colour getOutlineColour() const;

        /*!
        \brief Gets the thickness of the text outline
        */
        float getOutlineThickness() const;

        /*!
        \brief Gets the current shadow colour. Default is black.
        */
        Colour getShadowColour() const;

        /*!
        \brief Returns the current shadow offset.
        \see setShadowOffset
        */
        glm::vec2 getShadowOffset() const;
        
        /*!
        \brief Returns the current bold setting of this text
        */
        bool getBold() const;

        /*!
        \brief Returns the AABB of the Text component
        The given entity must have at least a Text and
        Drawable2D component attached to it
        */
        static FloatRect getLocalBounds(Entity);

        enum class Alignment
        {
            Left, Right, Centre
        };

        /*!
        \brief Sets whether the Text should be aligned Left,
        Right, or Centre about its origin. Only affects the
        X axis
        */
        void setAlignment(Alignment);

        /*!
        \brief Returns the Text's current Alignment
        */
        Alignment getAlignment() const { return Alignment(m_context.alignment); }

    private:

        TextContext m_context;

        struct DirtyFlags final
        {
            enum
            {
                Colour  = 0x1,
                Font    = 0x2,
                String  = 0x4,
                Texture = 0x8,

                All = Colour | Font | String | Texture
            };
        };
        std::uint16_t m_dirtyFlags;

        void updateVertices(Drawable2D&);

        void onFontUpdate() override;
        void removeFont() override;

        friend class TextSystem;
    };
}