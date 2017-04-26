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

float Text::getLineHeight() const
{
    //TODO return height depending on whether or
    //not the font is a bitmap or SDF
    CRO_ASSERT(m_font, "Font not loaded!");
    return m_font->getLineHeight();
}

//private