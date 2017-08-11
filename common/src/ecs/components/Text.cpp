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

#include <crogine/ecs/components/Text.hpp>
#include <crogine/graphics/Font.hpp>

using namespace cro;

Text::Text()
    : m_font        (nullptr),
    m_charSize      (0),
    m_blendMode     (Material::BlendMode::Alpha),
    m_dirtyFlags    (Flags::Verts),
    m_scissor       (0),
    m_vboOffset     (0)
{

}

Text::Text(const Font& font)
    : m_font        (&font),
    m_charSize      (30),
    m_blendMode     (Material::BlendMode::Alpha),
    m_dirtyFlags    (Flags::Verts),
    m_scissor       (0),
    m_vboOffset     (0)
{

}

//public
void Text::setString(const std::string& str)
{
    m_string = str;
    m_dirtyFlags |= Flags::Verts;
}

void Text::setCharSize(uint32 charSize)
{
    m_charSize = charSize;
    m_dirtyFlags |= Flags::CharSize;
}

void Text::setColour(Colour colour)
{
    m_colour = colour;
    for (auto& v : m_vertices)
    {
        v.colour = { colour.getRed(), colour.getGreen(), colour.getBlue(), colour.getAlpha() };
    }
    m_dirtyFlags |= Flags::Colours;
}

void Text::setBlendMode(Material::BlendMode mode)
{
    m_blendMode = mode;
    m_dirtyFlags |= Flags::BlendMode;
}

float Text::getLineHeight() const
{
    //TODO return height depending on whether or
    //not the font is a bitmap or SDF
    CRO_ASSERT(m_font, "Font not loaded!");
    return m_font->getLineHeight(m_charSize);
}

const FloatRect& Text::getLocalBounds() const
{
    if (m_dirtyFlags & (Flags::CharSize | Flags::Verts))
    {
        updateLocalBounds();
    }
    return m_localBounds;
}

void Text::setCroppingArea(FloatRect area)
{
    m_croppingArea = area;
    m_dirtyFlags |= Flags::ScissorArea;
}

//private
void Text::updateLocalBounds() const
{
    m_localBounds.width = 0.f;
    m_localBounds.height = 0.f;

    float currWidth = 0.f;
    float currHeight = 0.f;

    for (auto c : m_string)
    {
        if (c == '\n' || c == '\r')
        {
            if (currWidth > m_localBounds.width)
            {
                m_localBounds.width = currWidth;
            }
            currWidth = 0.f;

            m_localBounds.height += currHeight;
            currHeight = 0.f;
        }
        else
        {
            auto glyph = m_font->getGlyph(c, m_charSize);
            currWidth += glyph.width;
            if (currHeight < glyph.height)
            {
                currHeight = glyph.height;
            }
        }

    }
}