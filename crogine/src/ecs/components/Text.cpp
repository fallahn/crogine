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

#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/graphics/Font.hpp>

#include "../../detail/glad.hpp"

using namespace cro;

Text::Text()
    : m_font            (nullptr),
    m_charSize          (30),
    m_verticalSpacing   (0.f),
    m_fillColour        (Colour::White()),
    //m_outlineThickness(0.f),
    m_dirty             (true),
    m_alignment         (Alignment::Left)
{

}

Text::Text(const Font& font)
    : m_font            (nullptr),
    m_charSize          (30),
    m_verticalSpacing   (0.f),
    m_fillColour        (Colour::White()),
    //m_outlineThickness(0.f),
    m_dirty             (true),
    m_alignment         (Alignment::Left)
{
    setFont(font);
}

//public
void Text::setFont(const Font& font)
{
    m_font = &font;
    m_dirty = true;
}

void Text::setCharacterSize(std::uint32_t size)
{
    m_charSize = size;
    m_dirty = true;
}

void Text::setVerticalSpacing(float spacing)
{
    m_verticalSpacing = spacing;
    m_dirty = true;
}

void Text::setString(const String& str)
{
    if (m_string != str)
    {
        m_string = str;
        m_dirty = true;
    }
}

void Text::setFillColour(Colour colour)
{
    if (m_fillColour != colour)
    {
        //TODO rather than dirty just update the vert
        //colours if the text is not already dirty
        m_fillColour = colour;
        m_dirty = true;
    }
}

//void Text::setOutlineColour(sf::Color colour)
//{
//    if (m_outlineColour != colour)
//    {
//        m_outlineColour = colour;
//        m_dirty = true;
//    }
//}
//
//void Text::setOutlineThickness(float thickness)
//{
//    if (m_outlineThickness != thickness)
//    {
//        m_outlineThickness = thickness;
//        m_dirty = true;
//    }
//}

const Font* Text::getFont() const
{
    return m_font;
}

std::uint32_t Text::getCharacterSize() const
{
    return m_charSize;
}

float Text::getVerticalSpacing() const
{
    return m_verticalSpacing;
}

const String& Text::getString() const
{
    return m_string;
}

Colour Text::getFillColour() const
{
    return m_fillColour;
}

//Colour Text::getOutlineColour() const
//{
//    return m_outlineColour;
//}
//
//float Text::getOutlineThickness() const
//{
//    return m_outlineThickness;
//}

FloatRect Text::getLocalBounds(Entity entity)
{
    CRO_ASSERT(entity.hasComponent<Text>() && entity.hasComponent<Drawable2D>(), "Invalid Entity");

    auto& text = entity.getComponent<Text>();
    auto& drawable = entity.getComponent<Drawable2D>();
    if (text.m_dirty)
    {
        text.updateVertices(drawable);
        drawable.setTexture(&text.getFont()->getTexture(text.getCharacterSize()));
        drawable.setPrimitiveType(GL_TRIANGLES);
    }
    return drawable.getLocalBounds();
}

void Text::setAlignment(Text::Alignment alignment)
{
    m_alignment = alignment;
    m_dirty = true;
}

//private
void Text::updateVertices(Drawable2D& drawable)
{

}

void Text::addQuad(std::vector<Vertex2D>&, glm::vec2 position, Colour, const Glyph& glyph, float outline)
{

}