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

#include <crogine/ecs/components/Transform.hpp>

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>

#include <glm/gtc/matrix_transform.hpp>

using namespace cro;

Transform::Transform()
    : m_scale   (1.f, 1.f, 1.f),
    m_parent    (-1, 0),
    m_dirty     (true)
{

}

//public
void Transform::setOrigin(glm::vec3 o)
{
    m_origin = o;
    m_dirty = true;
}

void Transform::setPosition(glm::vec3 position)
{
    m_position = position;
    m_dirty = true;
}

void Transform::setRotation(glm::vec3 rotation)
{
    m_rotation = glm::toQuat(glm::orientate3(rotation));
    m_dirty = true;
}

void Transform::setScale(glm::vec3 scale)
{
    m_scale = scale;
    m_dirty = true;
}

void Transform::move(glm::vec3 distance)
{
    m_position += distance;
    m_dirty = true;
}

void Transform::rotate(glm::vec3 axis, float rotation)
{
    m_rotation = glm::rotate(m_rotation, rotation, glm::normalize(axis));
    m_dirty = true;
}

void Transform::scale(glm::vec3 scale)
{
    m_scale *= scale;
    m_dirty = true;
}

glm::vec3 Transform::getOrigin() const
{
    return m_origin;
}

glm::vec3 Transform::getPosition() const
{
    return m_position;
}

glm::vec3 Transform::getRotation() const
{
    return glm::eulerAngles(m_rotation);
}

glm::vec3 Transform::getScale() const
{
    return m_scale;
}

const glm::mat4& Transform::getLocalTransform() const
{
    if (m_dirty)
    {
        m_dirty = false;

        m_transform = glm::translate(glm::mat4(), m_position + m_origin);
        m_transform *= glm::toMat4(m_rotation);
        m_transform = glm::scale(m_transform, m_scale);
        m_transform = glm::translate(m_transform, -m_origin);
    }

    return m_transform;
}

glm::mat4 Transform::getWorldTransform(std::vector<Entity>& entityList) const
{
    if (m_parent.valid())
    {        
        return m_parent.getComponent<Transform>().getWorldTransform(entityList) * getLocalTransform();
    }
    return getLocalTransform();
}

void Transform::setParent(Entity parent)
{
    CRO_ASSERT(parent.hasComponent<Transform>(), "This entity has no transform");
    m_parent = parent;
}