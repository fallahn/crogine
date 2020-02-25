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
#include <crogine/graphics/Font.hpp>
#include <crogine/util/String.hpp>

using namespace cro;

Text::Text()
    : m_font        (nullptr),
    m_charSize      (0),
    m_colour        (cro::Colour::White()),
    m_blendMode     (Material::BlendMode::Alpha),
    m_dirtyFlags    (Flags::Verts | Flags::Colours),
    m_scissor       (false),
    m_vboOffset     (0),
    m_alignment     (Left)
{

}

Text::Text(const Font& font)
    : m_font        (&font),
    m_charSize      (30),
    m_colour        (cro::Colour::White()),
    m_blendMode     (Material::BlendMode::Alpha),
    m_dirtyFlags    (Flags::Verts | Flags::Colours),
    m_scissor       (false),
    m_vboOffset     (0),
    m_alignment     (Left)
{

}

//public
void Text::setString(const String& str)
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
        updateVerts();
    }
    return m_localBounds;
}

void Text::setCroppingArea(FloatRect area)
{
    m_croppingArea = area;
    m_scissor = true;
}

void Text::setAlignment(Alignment a)
{
    if (a != m_alignment) m_dirtyFlags |= Flags::Verts;
    m_alignment = a;
}

float Text::getLineWidth(std::size_t idx) const
{
    CRO_ASSERT(m_font, "No font assigned!");
    
    auto start = (idx) ? Util::String::findNthOf(m_string, '\n', idx) : 0;
    if (start == std::string::npos)
    {
        return 0.f;
    }

    float width = 0.f;
    for (auto i = start + 1; i < m_string.size(); ++i)
    {
        if (m_string[i] == '\n')
        {
            break;
        }
        width += m_font->getGlyph(m_string[i], m_charSize).advance;
    }

    return width;
}

//private
void Text::updateVerts() const
{
    /*
    0-------2
    |       |
    |       |
    1-------3
    */
    CRO_ASSERT(m_font, "Must construct text with a font!");
    if (m_string.empty()) return;

    m_vertices.clear();
    m_vertices.reserve(m_string.size() * 6); //4 verts per char + degen tri

    auto getStart = [&](std::size_t idx)->float
    {
        //TODO fix this else it recursively called getLocalBounds() to infinity
        float pos = 0.f;
        if (m_alignment == Text::Right)
        {
            /*float maxWidth = getLocalBounds().width;
            float rowWidth = getLineWidth(idx);
            pos = maxWidth - rowWidth;*/
        }
        else if (m_alignment == Text::Centre)
        {
            /*float maxWidth = getLocalBounds().width;
            float rowWidth = getLineWidth(idx);
            pos = maxWidth - rowWidth;
            pos /= 2.f;*/
        }
        return pos;
    };

    float xPos = getStart(0);
    float yPos = -getLineHeight();
    float lineHeight = -yPos;
    glm::vec2 texSize(m_font->getTexture(m_charSize).getSize());
    CRO_ASSERT(texSize.x > 0 && texSize.y > 0, "Font texture not loaded!");

    float top = 0.f;
    float width = 0.f;
    std::size_t lineCount = 0;

    std::uint32_t prevChar = 0;
    for (auto c : m_string)
    {
        //check for end of lines
        if (c == '\n') //newline is a new line!!
        {
            lineCount++;

            xPos = getStart(lineCount);
            yPos -= lineHeight;

            continue;
        }

        auto glyph = m_font->getGlyph(c, m_charSize);
        auto rect = glyph.textureBounds;
        auto bounds = glyph.bounds;
        auto descender = 8.f;// bounds.bottom + bounds.height;
        Text::Vertex v;

        v.position.x = xPos;
        v.position.y = yPos - bounds.bottom + descender;
        v.position.z = 0.f;

        v.UV.x = rect.left / texSize.x;
        v.UV.y = rect.bottom / texSize.y;

        m_vertices.push_back(v);
        m_vertices.push_back(v); //twice for degen tri


        v.position.y = yPos - bounds.bottom - bounds.height + descender;

        v.UV.x = rect.left / texSize.x;
        v.UV.y = (rect.bottom + rect.height) / texSize.y;

        m_vertices.push_back(v);


        v.position.x = xPos + rect.width;
        v.position.y = yPos - bounds.bottom + descender;

        v.UV.x = (rect.left + rect.width) / texSize.x;
        v.UV.y = rect.bottom / texSize.y;

        m_vertices.push_back(v);


        v.position.y = yPos - bounds.bottom - bounds.height + descender;

        v.UV.x = (rect.left + rect.width) / texSize.x;
        v.UV.y = (rect.bottom + rect.height) / texSize.y;

        m_vertices.push_back(v);
        m_vertices.push_back(v); //end degen tri



        if (v.position.x > width) width = v.position.x;

        xPos += m_font->getKerning(prevChar, c, m_charSize);
        xPos += glyph.advance;
        prevChar = c;
    }

    m_localBounds.bottom = yPos;
    m_localBounds.height = top - yPos;
    m_localBounds.width = width;

    m_vertices.erase(m_vertices.begin()); //remove front/back degens as these are added by renderer
    m_vertices.pop_back();

    for (auto& v : m_vertices)
    {
        v.colour = { m_colour.getRed(), m_colour.getGreen(), m_colour.getBlue(), m_colour.getAlpha() };
    }
    m_dirtyFlags &= ~Flags::Verts;
}