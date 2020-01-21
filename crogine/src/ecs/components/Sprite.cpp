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

#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/graphics/Texture.hpp>

using namespace cro;

Sprite::Sprite()
    : m_textureID   (0),
    m_dirty         (true),
    m_vboOffset     (0),
    m_visible       (false),
    m_blendMode     (Material::BlendMode::Alpha),
    m_needsSorting  (false),
    m_animationCount(0)
{
    for (auto& q : m_quad)
    {
        q.colour = { 1.f, 1.f, 1.f, 1.f };
        q.position.w = 1.f;
    }
}

//public
void Sprite::setTexture(const Texture& t)
{
    m_textureID = t.getGLHandle();
    FloatRect size = { 0.f, 0.f, static_cast<float>(t.getSize().x), static_cast<float>(t.getSize().y) };
    //setSize({ size.width, size.height });
    m_textureSize.x = size.width;
    m_textureSize.y = size.height;
    setTextureRect(size);

    m_needsSorting = true;
}

void Sprite::setSize(glm::vec2 size)
{
    /*
      0-------2
      |       |
      |       |
      1-------3
    */
    m_quad[0].position.x = 0.f;
    m_quad[0].position.y = size.y;
    
    m_quad[1].position.x = 0.f;
    m_quad[1].position.y = 0.f;

    m_quad[2].position.x = size.x;
    m_quad[2].position.y = size.y;

    m_quad[3].position.x = size.x;
    m_quad[3].position.y = 0.f;

    m_localBounds.width = size.x;
    m_localBounds.height = size.y;

    m_dirty = true;
}

void Sprite::setTextureRect(FloatRect subRect)
{
    setSize({ subRect.width, subRect.height });

    
    float width = m_textureSize.x;
    float height = m_textureSize.y;
    
    CRO_ASSERT(width != 0 && height != 0, "Invalid width or height - is the texture set?");

    m_quad[0].UV.x = subRect.left / width;
    m_quad[0].UV.y = (subRect.bottom + subRect.height) / height;
    
    m_quad[1].UV.x = subRect.left / width;
    m_quad[1].UV.y = subRect.bottom / height;

    m_quad[2].UV.x = (subRect.left + subRect.width) / width;
    m_quad[2].UV.y = (subRect.bottom + subRect.height) / height;

    m_quad[3].UV.x = (subRect.left + subRect.width) / width;
    m_quad[3].UV.y = subRect.bottom / height;

    m_dirty = true;

    m_textureRect = subRect;
}

void Sprite::setColour(Colour colour)
{
    for (auto& v : m_quad)
    {
        v.colour = { colour.getRed(), colour.getGreen(), colour.getBlue(), colour.getAlpha() };
    }
    m_dirty = true;
}

glm::vec2 Sprite::getSize() const
{
    return m_quad[2].position;
}

Colour Sprite::getColour() const
{
    return  Colour(m_quad[0].colour);
}

void Sprite::setBlendMode(Material::BlendMode mode)
{
    m_blendMode = mode;
    m_needsSorting = true;
}