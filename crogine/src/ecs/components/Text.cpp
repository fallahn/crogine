/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2021
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

#include "../../detail/glad.hpp"
#include "../../detail/TextConstruction.hpp"

using namespace cro;

Text::Text()
    : m_dirtyFlags      (DirtyFlags::All),
    m_alignment         (Alignment::Left)
{

}

Text::Text(const Font& font)
    : Text()
{
    setFont(font);
}

//public
void Text::setFont(const Font& font)
{
    m_context.font = &font;
    m_dirtyFlags |= DirtyFlags::All;
}

void Text::setCharacterSize(std::uint32_t size)
{
    m_context.charSize = size;
    m_dirtyFlags |= DirtyFlags::All;
}

void Text::setVerticalSpacing(float spacing)
{
    m_context.verticalSpacing = spacing;
    m_dirtyFlags |= DirtyFlags::All;
}

void Text::setString(const String& str)
{
    if (m_context.string != str)
    {
        m_context.string = str;
        m_dirtyFlags |= DirtyFlags::All;
    }
}

void Text::setFillColour(Colour colour)
{
    if (m_context.fillColour != colour)
    {
        m_context.fillColour = colour;
        m_dirtyFlags |= DirtyFlags::Colour;
    }
}

void Text::setOutlineColour(Colour colour)
{
    if (m_context.outlineColour != colour)
    {
        m_context.outlineColour = colour;
        m_dirtyFlags |= DirtyFlags::Colour;
    }
}

void Text::setOutlineThickness(float thickness)
{
    if (m_context.outlineThickness != thickness)
    {
        m_context.outlineThickness = thickness;
        m_dirtyFlags |= DirtyFlags::All;
    }
}

const Font* Text::getFont() const
{
    return m_context.font;
}

std::uint32_t Text::getCharacterSize() const
{
    return m_context.charSize;
}

float Text::getVerticalSpacing() const
{
    return m_context.verticalSpacing;
}

const String& Text::getString() const
{
    return m_context.string;
}

Colour Text::getFillColour() const
{
    return m_context.fillColour;
}

Colour Text::getOutlineColour() const
{
    return m_context.outlineColour;
}

float Text::getOutlineThickness() const
{
    return m_context.outlineThickness;
}

FloatRect Text::getLocalBounds(Entity entity)
{
    CRO_ASSERT(entity.hasComponent<Text>() && entity.hasComponent<Drawable2D>(), "Invalid Entity");

    auto& text = entity.getComponent<Text>();
    auto& drawable = entity.getComponent<Drawable2D>();
    if (text.m_dirtyFlags)
    {
        //check if only the colour needs updating
        if (text.m_dirtyFlags == DirtyFlags::Colour)
        {
            auto& verts = drawable.getVertexData();

            if (text.m_context.outlineThickness == 0)
            {
                for (auto v : verts)
                {
                    v.colour = text.m_context.fillColour;
                }
            }
            else
            {
                for (auto i = 0u; i < verts.size(); ++i)
                {
                    if (i < verts.size() / 2)
                    {
                        verts[i].colour = text.m_context.outlineColour;
                    }
                    else
                    {
                        verts[i].colour = text.m_context.fillColour;
                    }
                }
            }
        }
        else
        {
            text.updateVertices(drawable);
            drawable.setTexture(&text.getFont()->getTexture(text.getCharacterSize()));
            drawable.setPrimitiveType(GL_TRIANGLES);
        }
        text.m_dirtyFlags = 0;
    }
    return drawable.getLocalBounds();
}

void Text::setAlignment(Text::Alignment alignment)
{
    m_alignment = alignment;
    m_dirtyFlags |= DirtyFlags::All;
}

//private
void Text::updateVertices(Drawable2D& drawable)
{
    m_dirtyFlags = 0;

    FloatRect localBounds;

    //skip if nothing to build
    if (!m_context.font || m_context.string.empty())
    {
        drawable.updateLocalBounds(localBounds);
        return;
    }
    
    //update glyphs
    auto& vertices = drawable.getVertexData();
    localBounds = Detail::Text::updateVertices(vertices, m_context);
    auto maxY = localBounds.bottom + localBounds.height;

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