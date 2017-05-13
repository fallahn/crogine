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
    m_parent    (-1),
    m_lastParent(-1),
    m_id        (-1),
    m_dirtyFlags(0)
{
    for(auto& c : m_children) c = -1;
}

//public
void Transform::setOrigin(glm::vec3 o)
{
    m_origin = o;
    m_dirtyFlags |= Tx;
}

void Transform::setPosition(glm::vec3 position)
{
    m_position = position;
    m_dirtyFlags |= Tx;
}

void Transform::setRotation(glm::vec3 rotation)
{
    m_rotation = glm::toQuat(glm::orientate3(rotation));
    m_dirtyFlags |= Tx;
}

void Transform::setScale(glm::vec3 scale)
{
    m_scale = scale;
    m_dirtyFlags |= Tx;
}

void Transform::move(glm::vec3 distance)
{
    m_position += distance;
    m_dirtyFlags |= Tx;
}

void Transform::rotate(glm::vec3 axis, float rotation)
{
    m_rotation = glm::rotate(m_rotation, rotation, glm::normalize(axis));
    m_dirtyFlags |= Tx;
}

void Transform::scale(glm::vec3 scale)
{
    m_scale *= scale;
    m_dirtyFlags |= Tx;
}

glm::vec3 Transform::getOrigin() const
{
    return m_origin;
}

glm::vec3 Transform::getPosition() const
{
    return m_position;
}

glm::vec3 Transform::getWorldPosition() const
{
    return glm::vec3(getWorldTransform()[3]);
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
    if (m_dirtyFlags & Tx)
    {
        //m_dirtyFlags &= ~Tx;

        //auto offset = m_origin;// *m_scale;

        m_transform = glm::translate(glm::mat4(), m_position + m_origin);
        m_transform *= glm::toMat4(m_rotation);
        m_transform = glm::scale(m_transform, m_scale);
        m_transform = glm::translate(m_transform, -m_origin);
    }

    return m_transform;
}

glm::mat4 Transform::getWorldTransform() const
{
    return (m_parent > -1) ? m_worldTransform : getLocalTransform();
}

void Transform::setParent(Entity parent)
{
    /*if you're setting this and getting apparently no transform at all remember to add a SceneGraph system*/
    
    CRO_ASSERT(parent.hasComponent<Transform>(), "Parent must have a transform component");
    CRO_ASSERT(parent.getIndex() != m_id, "Can't parent to ourself!");
    int32 newID = parent.getIndex();
    if (m_parent == newID) return;

    m_lastParent = m_parent;
    m_parent = newID;

    m_dirtyFlags |= Parent;
}

void Transform::removeParent()
{
    m_lastParent = m_parent;
    m_parent = -1;
    m_dirtyFlags |= Parent;
}

bool Transform::addChild(uint32 id)
{
    auto freeSlot = std::find_if(std::begin(m_children), std::end(m_children), [id](int32 i) {return (i == -1 || i == id); });
    if (freeSlot != std::end(m_children))
    {
        if (*freeSlot > -1)
        {
            m_removedChildren.push_back(*freeSlot);
        }
        
        *freeSlot = id;
        std::sort(std::begin(m_children), std::end(m_children), [](int32 i, int32 j) { return i > j; });

        //mark for update
        m_dirtyFlags |= Child;
        return true;
    }
    return false;
}

void Transform::removeChild(uint32 id)
{
    auto freeSlot = std::find(std::begin(m_children), std::end(m_children), id);
    if (freeSlot != std::end(m_children))
    {
        //if (*freeSlot > -1)
        {
            m_removedChildren.push_back(*freeSlot);
        }
        *freeSlot = -1;
    }

    std::sort(std::begin(m_children), std::end(m_children), [](int32 i, int32 j) { return i > j; });

    //mark for update
    m_dirtyFlags |= Child;
}
