/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/graphics/Transformable2D.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/util/Constants.hpp>

using namespace cro;

Transformable2D::Transformable2D()
    : m_position    (0.f),
    m_rotation      (0.f),
    m_scale         (1.f),
    m_modelMatrix   (1.f),
    m_dirty         (true)
{

}

//public
void Transformable2D::setPosition(glm::vec2 position)
{
    m_position = position;
    m_dirty = true;
}

void Transformable2D::move(glm::vec2 amount)
{
    m_position += amount;
    m_dirty = true;
}

void Transformable2D::setRotation(float rotation)
{
    m_rotation = cro::Util::Const::degToRad * rotation;
    m_dirty = true;
}

float Transformable2D::getRotation() const
{
    return m_rotation * cro::Util::Const::radToDeg;
}

void Transformable2D::rotate(float amount)
{
    m_rotation += cro::Util::Const::degToRad * amount;
    m_dirty = true;
}

void Transformable2D::setScale(glm::vec2 scale)
{
    m_scale = scale;
    m_dirty = true;
}

void Transformable2D::scale(glm::vec2 amount)
{
    m_scale *= amount;
    m_dirty = true;
}

const glm::mat4& Transformable2D::getTransform() const
{
    if (m_dirty)
    {
        updateTransform();
        m_dirty = false;
    }
    return m_modelMatrix;
}

//private
void Transformable2D::updateTransform() const
{
    m_modelMatrix = glm::translate(glm::mat4(1.f), glm::vec3(m_position, 0.f));
    m_modelMatrix = glm::rotate(m_modelMatrix, m_rotation, cro::Transform::Z_AXIS);
    m_modelMatrix = glm::scale(m_modelMatrix, glm::vec3(m_scale, 1.f));
}