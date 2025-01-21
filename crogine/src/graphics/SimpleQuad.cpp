/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2023
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

#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/graphics/Shader.hpp>
#include <crogine/graphics/Texture.hpp>
#include <crogine/graphics/Vertex2D.hpp>

#include <crogine/ecs/components/Sprite.hpp>

#include <crogine/detail/OpenGL.hpp>

using namespace cro;

SimpleQuad::SimpleQuad()
    : m_colour      (cro::Colour::White),
    m_size          (0.f)
{
    setPrimitiveType(GL_TRIANGLES);
}

SimpleQuad::SimpleQuad(const cro::Texture& texture)
    : SimpleQuad()
{
    setTexture(texture);
}

SimpleQuad& SimpleQuad::operator = (const Sprite& sprite)
{
    CRO_ASSERT(sprite.getTexture(), "Sprite has no texture");
    setTexture(*sprite.getTexture());
    setTextureRect(sprite.getTextureRect());
    setColour(sprite.getColour());

    return *this;
}

//public
void SimpleQuad::setTexture(const cro::Texture& texture)
{
    if (texture.getGLHandle() > 0)
    {
        SimpleDrawable::setTexture(texture);
        m_size = glm::vec2(texture.getSize());
        m_uvRect = { 0.f, 0.f, 1.f, 1.f };
        updateVertexData();
    }
    else
    {
        LogE << "Invalid texture supplied to SimpleQuad" << std::endl;
    }
}

void SimpleQuad::setTexture(cro::TextureID texture, glm::uvec2 size)
{
    if (texture.textureID > 0)
    {
        SimpleDrawable::setTexture(texture);
        m_size = glm::vec2(size);
        m_uvRect = { 0.f, 0.f, 1.f, 1.f };
        updateVertexData();
    }
    else
    {
        LogE << "Invalid texture supplied to SimpleQuad" << std::endl;
    }
}

void SimpleQuad::setTextureRect(cro::FloatRect subRect)
{
    if (m_size.x > 0 
        && m_size.y > 0)
    {
        m_uvRect.left = subRect.left / m_size.x;
        m_uvRect.width = subRect.width / m_size.x;
        m_uvRect.bottom = subRect.bottom / m_size.y;
        m_uvRect.height = subRect.height / m_size.y;

        m_size = { subRect.width, subRect.height };
        CRO_ASSERT(m_size.x > 0 && m_size.y > 0, "div by zero!");

        updateVertexData();
    }
}

void SimpleQuad::setColour(const cro::Colour& colour)
{
    m_colour = colour;
    updateVertexData();
}

void SimpleQuad::draw(const glm::mat4& parentTransform)
{
    drawGeometry(getTransform() * parentTransform);
}

//private
void SimpleQuad::updateVertexData()
{
    //TODO we could cache the verts locally
    //to make updating only what's changed faster
    //but probably not worth it.
    static constexpr float Overlap = 1.02f;
    const auto size = m_size * Overlap;
    const auto corner = (size - m_size);

    const auto UVSize = glm::vec2(m_uvRect.width, m_uvRect.height) * Overlap;
    const auto UVCorner = UVSize - glm::vec2(m_uvRect.width, m_uvRect.height);

    std::vector<Vertex2D> vertexData =
    {
        Vertex2D(glm::vec2(0.f, m_size.y), glm::vec2(m_uvRect.left, m_uvRect.bottom + m_uvRect.height), m_colour),
        Vertex2D(glm::vec2(0.f, -corner.y), glm::vec2(m_uvRect.left, m_uvRect.bottom - UVCorner.y), m_colour),
        Vertex2D(glm::vec2(size.x, m_size.y), glm::vec2(m_uvRect.left + UVSize.x, m_uvRect.bottom + m_uvRect.height), m_colour),
        
        Vertex2D(glm::vec2(m_size.x, size.y), glm::vec2(m_uvRect.left + m_uvRect.width, m_uvRect.bottom + UVSize.y), m_colour),
        Vertex2D(glm::vec2(-corner.x, 0.f), glm::vec2(m_uvRect.left - UVCorner.x, m_uvRect.bottom), m_colour),
        Vertex2D(glm::vec2(m_size.x, 0.f), glm::vec2(m_uvRect.left + m_uvRect.width, m_uvRect.bottom), m_colour),
    };

    setVertexData(vertexData);
}