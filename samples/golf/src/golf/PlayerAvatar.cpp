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

namespace
{

}

PlayerAvatar::PlayerAvatar(const std::string& path)
    : m_target(nullptr)
{
    //load into image
    if (m_image.loadFromFile(path) //MUST be RGBA for colour replace to work.
        && m_image.getFormat() == cro::ImageFormat::RGBA)
    {
        //cache color indices
        auto stride = 4u;
        auto length = m_image.getSize().x * m_image.getSize().y * stride;
        const auto* pixels = m_image.getPixelData();

        for (auto i = 0u; i < length; i += stride)
        {
            auto alpha = pixels[i + (stride - 1)];

            switch (pixels[i])
            {
            default: break;
            case pc::Keys[pc::ColourKey::Bottom].light:
                if (alpha) m_keyIndicesLight[pc::ColourKey::Bottom].push_back(i / stride);
                break;
            case pc::Keys[pc::ColourKey::Bottom].dark:
                if (alpha) m_keyIndicesDark[pc::ColourKey::Bottom].push_back(i / stride);
                break;

            case pc::Keys[pc::ColourKey::Top].light:
                if (alpha) m_keyIndicesLight[pc::ColourKey::Top].push_back(i / stride);
                break;
            case pc::Keys[pc::ColourKey::Top].dark:
                if (alpha) m_keyIndicesDark[pc::ColourKey::Top].push_back(i / stride);
                break;

            case pc::Keys[pc::ColourKey::Skin].light:
                if (alpha) m_keyIndicesLight[pc::ColourKey::Skin].push_back(i / stride);
                break;
            case pc::Keys[pc::ColourKey::Skin].dark:
                if (alpha) m_keyIndicesDark[pc::ColourKey::Skin].push_back(i / stride);
                break;

            case pc::Keys[pc::ColourKey::Hair].light:
                if (alpha) m_keyIndicesLight[pc::ColourKey::Hair].push_back(i / stride);
                break;
            case pc::Keys[pc::ColourKey::Hair].dark:
                if (alpha) m_keyIndicesDark[pc::ColourKey::Hair].push_back(i / stride);
                break;
            }
        }
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

    const auto imgWidth = m_image.getSize().x;
    //cache the colour and only update if it changed.
    if (auto colour = cro::Colour(pc::Palette[pairIdx].light); m_lightColours[idx] != colour)
    {

        for (auto i : m_keyIndicesLight[idx])
        {
            //set the colour in the image at this index
            auto x = i % imgWidth;
            auto y = i / imgWidth;

            m_image.setPixel(x, y, colour);
        }
        m_lightColours[idx] = colour;
    }


    if (auto colour = cro::Colour(pc::Palette[pairIdx].dark); m_darkColours[idx] != colour)
    {
        for (auto i : m_keyIndicesDark[idx])
        {
            auto x = i % imgWidth;
            auto y = i / imgWidth;

            m_image.setPixel(x, y, colour);
        }
        m_darkColours[idx] = colour;
    }
}

void PlayerAvatar::apply()
{
    if (m_target)
    {
        m_target->loadFromImage(m_image);
    }
}