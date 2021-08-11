/*-----------------------------------------------------------------------

Matt Marchant 2021
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include "PlayerAvatar.hpp"

#include <crogine/graphics/Colour.hpp>

PlayerAvatar::PlayerAvatar(const std::string& path)
    : m_target(nullptr)
{
    //load into image
    if (m_image.loadFromFile(path))
    {
        //if successful load into texture
        m_texture.loadFromImage(m_image);

        //and cache color indices
    }
    else
    {
        m_image.create(48, 52, cro::Colour::Magenta);
    }
}

//public
void PlayerAvatar::setTarget(cro::Texture& target)
{
    m_target = &target;
}

void PlayerAvatar::setColour(pc::ColourKey::Index idx, std::int8_t pairIdx)
{
    CRO_ASSERT(pairIdx < pc::ColourID::Count, "");

    for (auto i : m_keyIndices[idx])
    {
        //set the colour in the image at this index
        pc::Palette[pairIdx].light;
        pc::Palette[pairIdx].dark;
    }

    m_texture.loadFromImage(m_image);
}

void PlayerAvatar::apply()
{
    if (m_target)
    {
        m_target->loadFromImage(m_image);
    }
}