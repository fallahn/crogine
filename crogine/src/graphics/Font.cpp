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

#include <crogine/graphics/Font.hpp>
#include <crogine/graphics/Image.hpp>
#include "../detail/DistanceField.hpp"

#include <array>
#include <cstring>

#include <ft2build.h>

using namespace cro;

namespace
{

}

Font::Font()
    : m_refCount(nullptr)
{

}

Font::~Font()
{

}

//public
bool Font::loadFromFile(const std::string& path)
{
    return false;
}

FloatRect Font::getGlyph(uint32 codepoint, uint32 charSize) const
{
    return {};
}

const Texture& Font::getTexture(uint32 charSize) const
{

}

float Font::getLineHeight(uint32 charSize) const
{

}

//private
void Font::loadGlyph(std::uint32_t codepoints, std::uint32_t charSize)
{

}

FloatRect Font::getGlyphRect(Page& page, std::uint32_t width, std::uint32_t height) const
{
    
}

bool Font::setCurrentCharacterSize(std::uint32_t size) const
{

}

void Font::cleanup()
{

}