/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2024
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
    : m_texture         (nullptr),
    m_colour            (Colour::White),
    m_dirtyFlags        (DirtyFlags::All),
    m_overrideBlendMode (false),
    m_blendMode         (Material::BlendMode::Alpha)
{

}

Sprite::Sprite(Material::BlendMode mode)
    : m_texture         (nullptr),
    m_colour            (Colour::White),
    m_dirtyFlags        (DirtyFlags::All),
    m_overrideBlendMode (true),
    m_blendMode         (mode)
{

}

Sprite::Sprite(const Texture& texture, Material::BlendMode mode)
    : Sprite(mode)
{
    setTexture(texture);
}

//public
void Sprite::setTexture(const Texture& texture, bool resize)
{
    if (m_texture != &texture)
    {
        m_texture = &texture;
        m_dirtyFlags |= DirtyFlags::Texture;
    }

    if (resize)
    {
        setTextureRect({ glm::vec2(), texture.getSize() });
    }
}

void Sprite::setTextureRect(FloatRect rect)
{
    m_textureRect = rect;
    m_dirtyFlags |= DirtyFlags::Texture;
}

void Sprite::setColour(Colour colour)
{
    m_colour = colour;
    m_dirtyFlags |= DirtyFlags::Colour;
}

FloatRect Sprite::getTextureRectNormalised() const
{
    FloatRect retVal;

    if (m_texture)
    {
        retVal.left = m_textureRect.left / m_texture->getSize().x;
        retVal.width = m_textureRect.width / m_texture->getSize().x;

        retVal.bottom = m_textureRect.bottom / m_texture->getSize().y;
        retVal.height = m_textureRect.height / m_texture->getSize().y;
    }
    return retVal;
}

Colour Sprite::getColour() const
{
    return  m_colour;
}

void Sprite::getVertexData(std::vector<Vertex2D>& verts) const
{
    auto subRect = m_textureRect;
    verts.resize(4);

    verts[0].position = { 0.f, subRect.height };
    verts[1].position = { 0.f, 0.f };
    verts[2].position = { subRect.width, subRect.height };
    verts[3].position = { subRect.width, 0.f };

    //update vert coords
    if (m_texture)
    {
        glm::vec2 texSize = m_texture->getSize();
        verts[0].UV = { subRect.left / texSize.x, (subRect.bottom + subRect.height) / texSize.y };
        verts[1].UV = { subRect.left / texSize.x, subRect.bottom / texSize.y };
        verts[2].UV = { (subRect.left + subRect.width) / texSize.x, (subRect.bottom + subRect.height) / texSize.y };
        verts[3].UV = { (subRect.left + subRect.width) / texSize.x, subRect.bottom / texSize.y };
    }
    //update colour
    verts[0].colour = m_colour;
    verts[1].colour = m_colour;
    verts[2].colour = m_colour;
    verts[3].colour = m_colour;
}