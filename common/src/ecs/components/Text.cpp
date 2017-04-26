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

namespace
{
    const float lineHeight = 40.f; //TODO make this a variable with text size/bitmap font height
}

Text::Text()
    : m_font        (nullptr),
    m_dirtyFlags    (Flags::Verts),
    m_vboOffset     (0)
{

}

Text::Text(const Font& font)
    : m_font        (&font),
    m_dirtyFlags    (Flags::Verts),
    m_vboOffset     (0)
{

}

//public
void Text::setString(const std::string& str)
{
    m_string = str;
    m_dirtyFlags |= Flags::Verts;
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

//private
void Text::rebuildVerts() //TODO this should be in text renderer
{
    /*
    0-------2
    |       |
    |       |
    1-------3
    */
    CRO_ASSERT(m_font, "Must construct text with a font!");
    
    m_vertices.clear();
    m_vertices.reserve(m_string.size() * 6); //4 verts per char + degen tri

    float xPos = 0.f; //current char offset
    float yPos = 0.f;
    glm::vec2 texSize(m_font->getTexture().getSize());
    CRO_ASSERT(texSize.x > 0 && texSize.y > 0, "Font texture not loaded!");

    for (auto c : m_string)
    {
        //check for end of lines
        if (c == '\r' || c == '\n')
        {
            xPos = 0.f;
            yPos -= lineHeight;
            continue;
        }

        auto rect = m_font->getGlyph(c);
        Vertex v;
        v.position.x = xPos;
        v.position.y = yPos + rect.height;
        v.position.z = 0.f;

        v.UV.x = rect.left / texSize.x;
        v.UV.y = (rect.bottom + rect.height) / texSize.y;
        m_vertices.push_back(v);
        m_vertices.push_back(v); //twice for degen tri

        v.position.y = yPos;

        v.UV.x = rect.left / texSize.x;
        v.UV.y = rect.bottom / texSize.y;
        m_vertices.push_back(v);

        v.position.x = xPos + rect.width;
        v.position.y = yPos + rect.height;

        v.UV.x = (rect.left + rect.width) / texSize.x;
        v.UV.y = (rect.bottom + rect.height) / texSize.y;
        m_vertices.push_back(v);

        v.position.y = yPos;

        v.UV.x = (rect.left + rect.width) / texSize.x;
        v.UV.y = rect.bottom / texSize.y;
        m_vertices.push_back(v);
        m_vertices.push_back(v); //end degen tri

        xPos += rect.width;
    }

    m_vertices.erase(m_vertices.begin()); //remove front/back degens as these are added by renderer
    m_vertices.pop_back();

    for (auto& v : m_vertices)
    {
        v.colour = { m_colour.getRed(), m_colour.getGreen(), m_colour.getBlue(), m_colour.getAlpha() };
    }
    m_dirtyFlags = 0;
}