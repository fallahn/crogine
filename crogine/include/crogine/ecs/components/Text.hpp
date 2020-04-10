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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/core/String.hpp>
#include <crogine/graphics/Rectangle.hpp>
#include <crogine/graphics/Colour.hpp>
#include <crogine/graphics/Vertex2D.hpp>
#include <crogine/ecs/Entity.hpp>

#include <crogine/detail/glm/vec2.hpp>

namespace cro
{
    struct Glyph;
    class Font;
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
    class CRO_EXPORT_API Text final
    {
    public:
        Text();

        /*!
        \brief Construct a Text component with a given Font
        */
        Text(const Font&);

        /*!
        \brief Set the font to be used with this Text
        */
        void setFont(const Font&);

        /*!
        \brief Set the character size of the Text
        */
        void setCharacterSize(std::uint32_t);

        /*!
        \brief Sets the vertical spacing between lines of text
        */
        void setVerticalSpacing(float);

        /*!
        \brief Set the string to render with this Text
        */
        void setString(const String&);

        /*!
        \brief Set the Text fill colour
        */
        void setFillColour(Colour);

        //void setOutlineColour(Colour)
        //void setOutlineThickness(float)

        /*!
        \brief Return a pointer to the active font
        May be nullptr
        */
        const Font* getFont() const;

        /*!
        \brief Return the current character size
        */
        std::uint32_t getCharacterSize() const;

        /*!
        \brief Returns the current line spacing
        */
        float getVerticalSpacing() const;

        /*!
        \brief Return the current string
        */
        const String& getString() const;

        /*!
        \brief Return the current fill colour
        */
        Colour getFillColour() const;

        //Colour getOutlineColour() const;
        //float getOutlineThickness() const;
        
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
        Alignment getAlignment() const { return m_alignment; }

    private:

        String m_string;
        const Font* m_font;
        std::uint32_t m_charSize;
        float m_verticalSpacing;
        Colour m_fillColour;

        //Colour m_outlineColour;
        //float m_outlineThickness;

        bool m_dirty;
        Alignment m_alignment;

        void updateVertices(Drawable2D&);
        void addQuad(std::vector<Vertex2D>&, glm::vec2 position, Colour, const Glyph& glyph, glm::vec2 textureSize, float = 0.f);

        friend class TextSystem;
    };
}