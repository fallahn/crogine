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

#include <crogine/ecs/components/Drawable2D.hpp>
#include "../../detail/glad.hpp"

#include <limits>

using namespace cro;

Drawable2D::Drawable2D()
    : m_texture         (nullptr),
    m_shader            (nullptr),
    m_customShader      (false),
    m_applyDefaultShader(true),
    m_blendMode         (Material::BlendMode::Alpha),
    m_primitiveType     (GL_TRIANGLE_STRIP),
    m_vbo               (0),
    m_vao               (0),
    m_updateBufferData  (false),
    m_lastSortValue     (0.f)
{

}

//public
void Drawable2D::setTexture(Texture* texture)
{
    m_texture = texture;
    m_applyDefaultShader = !m_customShader;
}

void Drawable2D::setShader(Shader* shader)
{
    m_shader = shader;
    m_customShader = (shader != nullptr);
    m_applyDefaultShader = !m_customShader;

    applyShader();
}

void Drawable2D::setBlendMode(Material::BlendMode mode)
{
    m_blendMode = mode;
}

void Drawable2D::setVertexData(const std::vector<Vertex2D>& data)
{
    m_vertices = data;
    updateLocalBounds();
}

void Drawable2D::setPrimitiveType(std::uint32_t primitiveType)
{
    m_primitiveType = primitiveType;
}

Texture* Drawable2D::getTexture()
{
    return m_texture;
}

const Texture* Drawable2D::getTexture() const
{
    return m_texture;
}

Shader* Drawable2D::getShader()
{
    return m_shader;
}

const Shader* Drawable2D::getShader() const
{
    return m_shader;
}

Material::BlendMode Drawable2D::getBlendMode() const
{
    return m_blendMode;
}

std::vector<Vertex2D>& Drawable2D::getVertexData()
{
    return m_vertices;
}

const std::vector<Vertex2D>& Drawable2D::getVertexData() const
{
    return m_vertices;
}

std::uint32_t Drawable2D::getPrimitiveType() const
{
    return m_primitiveType;
}

FloatRect Drawable2D::getLocalBounds() const
{
    return m_localBounds;
}

void Drawable2D::updateLocalBounds()
{
    if (m_vertices.empty()) return;

    auto xExtremes = std::minmax_element(m_vertices.begin(), m_vertices.end(),
        [](const Vertex2D& lhs, const Vertex2D& rhs)
        {
            return lhs.position.x < rhs.position.x;
        });

    auto yExtremes = std::minmax_element(m_vertices.begin(), m_vertices.end(),
        [](const Vertex2D& lhs, const Vertex2D& rhs)
        {
            return lhs.position.y < rhs.position.y;
        });

    m_localBounds.left = xExtremes.first->position.x;
    m_localBounds.bottom = yExtremes.first->position.y;
    m_localBounds.width = xExtremes.second->position.x - m_localBounds.left;
    m_localBounds.height = yExtremes.second->position.y - m_localBounds.bottom;

    m_updateBufferData = true; //tells the system we have new data to upload
}
//private
void Drawable2D::applyShader()
{
    if (m_shader)
    {

    }
}