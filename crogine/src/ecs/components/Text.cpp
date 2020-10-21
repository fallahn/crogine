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
    m_dirty = false;

    auto& vertices = drawable.getVertexData();
    vertices.clear();

    FloatRect localBounds;

    //skip if nothing to build
    if (!m_font || m_string.empty())
    {
        drawable.updateLocalBounds(localBounds);
        return;
    }
    
    //update glyphs
    glm::vec2 textureSize = m_font->getTexture(m_charSize).getSize();
    float xOffset = static_cast<float>(m_font->getGlyph(L' ', m_charSize).advance);
    float yOffset = static_cast<float>(m_font->getLineHeight(m_charSize));
    float x = 0.f;
    float y = 0.f;// static_cast<float>(m_charSize);

    float minX = x;
    float minY = y;
    float maxX = 0.f;
    float maxY = 0.f;

    std::uint32_t prevChar = 0;
    const auto& string = m_string;
    for (auto i = 0u; i < string.size(); ++i)
    {
        std::uint32_t currChar = string[i];

        x += m_font->getKerning(prevChar, currChar, m_charSize);
        prevChar = currChar;

        //whitespace chars
        if (currChar == ' ' || currChar == '\t' || currChar == '\n')
        {
            minX = std::min(minX, x);
            minY = std::min(minY, y);

            switch (currChar)
            {
            default: break;
            case ' ':
                x += xOffset;
                break;
            case '\t':
                x += xOffset * 4.f; //4 spaces for tab suckas
                break;
            case '\n':
                y -= yOffset + m_verticalSpacing;
                x = 0.f;
                break;
            }

            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);

            continue; //skip quad for whitespace
        }

        //create the quads. //TODO reimplement (font doesn't yet support it)
        //auto addOutline = [&]()
        //{
        //    const auto& glyph = m_font->getGlyph(currChar, m_charSize, m_outlineThickness);

        //    float left = glyph.bounds.left;
        //    float top = glyph.bounds.top;
        //    float right = glyph.bounds.left + glyph.bounds.width;
        //    float bottom = glyph.bounds.top + glyph.bounds.height;

        //    //add the outline glyph to the vertices
        //    addQuad(vertices, glm::vec2(x, y), m_outlineColour, glyph, m_outlineThickness);

        //    minX = std::min(minX, x + left - m_outlineThickness);
        //    maxX = std::max(maxX, x + right - m_outlineThickness);
        //    minY = std::min(minY, y + top - m_outlineThickness);
        //    maxY = std::max(maxY, y + bottom - m_outlineThickness);
        //};

        //if outline is larger, add first
        /*if (m_outlineThickness > 0)
        {
            addOutline();
        }*/

        const auto& glyph = m_font->getGlyph(currChar, m_charSize);
        addQuad(vertices, glm::vec2(x, y), m_fillColour, glyph, textureSize);

        //else add outline on top TODO this just doesn't make sense, don't bother
        /*if (m_outlineThickness < 0)
        {
            addOutline();
        }*/

        //only do this if not outlined
        //if (m_outlineThickness == 0)
        {
            float left = glyph.bounds.left;
            float top = glyph.bounds.bottom + glyph.bounds.height;
            float right = glyph.bounds.left + glyph.bounds.width;
            float bottom = glyph.bounds.bottom;

            minX = std::min(minX, x + left);
            maxX = std::max(maxX, x + right);
            minY = std::min(minY, y + bottom);
            maxY = std::max(maxY, y + top);
        }

        x += glyph.advance;
    }

    localBounds.left = minX;
    localBounds.bottom = minY;
    localBounds.width = maxX - minX;
    localBounds.height = maxY - minY;


    //check for alignment
    float offset = 0.f;
    if (m_alignment == Text::Alignment::Centre)
    {
        offset = localBounds.width / 2.f;
    }
    else if (m_alignment == Text::Alignment::Right)
    {
        offset = localBounds.width;
    }
    //if (offset > 0)
    {
        for (auto& v : vertices)
        {
            v.position.x -= offset;
            v.position.y -= maxY;
        }
        localBounds.left -= offset;
        localBounds.bottom -= maxY;
    }

    drawable.updateLocalBounds(localBounds);
}

void Text::addQuad(std::vector<Vertex2D>& vertices, glm::vec2 position, Colour colour, const Glyph& glyph, glm::vec2 textureSize, float outlineThickness)
{
    //this might sound counter intuitive - but we're
    //making the characters top to bottom
    float left = glyph.bounds.left;
    float bottom = glyph.bounds.bottom;
    float right = glyph.bounds.left + glyph.bounds.width;
    float top = glyph.bounds.bottom + glyph.bounds.height;

    float u1 = static_cast<float>(glyph.textureBounds.left) / textureSize.x;
    float v1 = static_cast<float>(glyph.textureBounds.bottom) / textureSize.y;
    float u2 = static_cast<float>(glyph.textureBounds.left + glyph.textureBounds.width) / textureSize.x;
    float v2 = static_cast<float>(glyph.textureBounds.bottom + glyph.textureBounds.height) / textureSize.y;

    vertices.emplace_back(glm::vec2(position.x + left - outlineThickness, position.y + top - outlineThickness), glm::vec2(u1, v1), colour);
    vertices.emplace_back(glm::vec2(position.x + left - outlineThickness, position.y + bottom - outlineThickness), glm::vec2(u1, v2), colour);
    vertices.emplace_back(glm::vec2(position.x + right - outlineThickness, position.y + top - outlineThickness), glm::vec2(u2, v1), colour);

    vertices.emplace_back(glm::vec2(position.x + right - outlineThickness, position.y + top - outlineThickness), glm::vec2(u2, v1), colour);
    vertices.emplace_back(glm::vec2(position.x + left - outlineThickness, position.y + bottom - outlineThickness), glm::vec2(u1, v2), colour);
    vertices.emplace_back(glm::vec2(position.x + right - outlineThickness, position.y + bottom - outlineThickness), glm::vec2(u2, v2), colour);
}