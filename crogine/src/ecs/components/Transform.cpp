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

#include <crogine/ecs/components/Transform.hpp>

#include <crogine/detail/glm/gtx/euler_angles.hpp>
#include <crogine/detail/glm/gtx/quaternion.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/glm/gtc/matrix_access.hpp>

using namespace cro;

Transform::Transform()
    : m_origin          (0.f, 0.f, 0.f),
    m_position          (0.f, 0.f, 0.f),
    m_scale             (1.f, 1.f, 1.f),
    m_rotation          (1.f, 0.f, 0.f, 0.f),
    m_transform         (1.f),
    m_parent            (nullptr),
    m_dirtyFlags        (0)
{

}

Transform::Transform(Transform&& other) noexcept
    : m_origin          (0.f, 0.f, 0.f),
    m_position          (0.f, 0.f, 0.f),
    m_scale             (1.f, 1.f, 1.f),
    m_rotation          (1.f, 0.f, 0.f, 0.f),
    m_transform         (1.f),
    m_parent            (nullptr),
    m_dirtyFlags        (0)
{
    if (other.m_parent != this)
    {
        //orphan any children
        for (auto c : m_children)
        {
            c->m_parent = nullptr;
        }

        //and adopt new
        m_parent = other.m_parent;

        other.m_parent = nullptr;

        //swap ourself into siblings list
        if (m_parent)
        {
            auto& siblings = m_parent->m_children;
            for (auto i = 0u; i < siblings.size(); ++i)
            {
                if (siblings[i] == &other)
                {
                    siblings[i] = this;
                    break;
                }
            }
        }
        m_children = std::move(other.m_children);

        //update the children's new parent
        for (auto* c : m_children)
        {
            CRO_ASSERT(c != this, "we already exist in the child list!");

            c->m_parent = this;
        }

        //actually take on the other transform
        setPosition(other.getPosition());
        setRotation(other.getRotation());
        setScale(other.getScale());
        setOrigin(other.getOrigin());
        m_dirtyFlags = Flags::Tx;

        other.reset();
    }
}

Transform& Transform::operator=(Transform&& other) noexcept
{
    if (&other != this && other.m_parent != this)
    {
        //orphan any children
        for (auto c : m_children)
        {
            c->m_parent = nullptr;
        }

        m_parent = other.m_parent;

        other.m_parent = nullptr;

        //swap ourself into siblings list
        if (m_parent)
        {
            auto& siblings = m_parent->m_children;
            for (auto i = 0u; i < siblings.size(); ++i)
            {
                if (siblings[i] == &other)
                {
                    siblings[i] = this;
                    break;
                }
            }
        }

        m_children = std::move(other.m_children);

        //update the children's new parent
        for (auto c : m_children)
        {
            CRO_ASSERT(c != this, "we already exist in the child list!");
            c->m_parent = this;
        }

        //actually take on the other transform
        setPosition(other.getPosition());
        setRotation(other.getRotation());
        setScale(other.getScale());
        setOrigin(other.getOrigin());
        m_dirtyFlags = Flags::Tx;

        other.reset();
    }
    return *this;
}

Transform::~Transform()
{
    //remove this transform from its parent
    if (m_parent)
    {
        auto& siblings = m_parent->m_children;
        siblings.erase(
            std::remove_if(siblings.begin(), siblings.end(),
                [this](const Transform* ptr)
                {return ptr == this; }),
            siblings.end());
    }

    //orphan any children
    for (auto c : m_children)
    {
        c->m_parent = nullptr;
    }
}

//public
void Transform::setOrigin(glm::vec3 o)
{
    m_origin = o;
    m_dirtyFlags |= Tx;
}

void Transform::setOrigin(glm::vec2 o)
{
    setOrigin(glm::vec3(o.x, o.y, 0.f));
}

void Transform::setPosition(glm::vec3 position)
{
    m_position = position;
    m_dirtyFlags |= Tx;
}

void Transform::setPosition(glm::vec2 position)
{
    setPosition(glm::vec3(position.x, position.y, 0.f));
}

void Transform::setRotation(glm::vec3 rotation)
{
    m_rotation = glm::toQuat(glm::orientate3(rotation));
    m_dirtyFlags |= Tx;
}

void Transform::setRotation(glm::quat rotation)
{
    m_rotation = rotation;
    m_dirtyFlags |= Tx;
}

void Transform::setRotation(float radians)
{
    setRotation(glm::vec3(0.f, 0.f, radians));
}

void Transform::setScale(glm::vec3 scale)
{
    m_scale = scale;
    m_dirtyFlags |= Tx;
}

void Transform::setScale(glm::vec2 scale)
{
    setScale(glm::vec3(scale.x, scale.y, 1.f));
}

void Transform::move(glm::vec3 distance)
{
    m_position += distance;
    m_dirtyFlags |= Tx;
}

void Transform::move(glm::vec2 distance)
{
    move(glm::vec3(distance.x, distance.y, 0.f));
}

void Transform::rotate(glm::vec3 axis, float rotation)
{
    m_rotation = glm::rotate(m_rotation, rotation, glm::normalize(axis));
    m_dirtyFlags |= Tx;
}

void Transform::rotate(float amount)
{
    rotate(glm::vec3(0.f, 0.f, 1.), amount);
}

void Transform::scale(glm::vec3 scale)
{
    m_scale *= scale;
    m_dirtyFlags |= Tx;
}

void Transform::scale(glm::vec2 amount)
{
    scale(glm::vec3(amount.x, amount.y, 0.f));
}

glm::vec3 Transform::getOrigin() const
{
    return m_origin;
}

glm::vec3 Transform::getPosition() const
{
    return m_position;// +(m_origin * m_scale);
}

glm::vec3 Transform::getWorldPosition() const
{
    return glm::vec3(getWorldTransform()[3]) - ((m_origin * m_scale) * m_rotation);
}

glm::vec3 Transform::getRotation() const
{
    return glm::eulerAngles(m_rotation);
}

glm::quat Transform::getRotationQuat() const
{
    return m_rotation;
}

glm::vec3 Transform::getScale() const
{
    return m_scale;
}

const glm::mat4& Transform::getLocalTransform() const
{
    if (m_dirtyFlags & Tx)
    {
        m_transform = glm::translate(glm::mat4(1.f), m_position);
        m_transform *= glm::toMat4(m_rotation);
        m_transform = glm::scale(m_transform, m_scale);
        m_transform = glm::translate(m_transform, -m_origin);
    }

    return m_transform;
}

glm::mat4 Transform::getWorldTransform() const
{
    if (m_parent)
    {
        return m_parent->getWorldTransform() * getLocalTransform();
    }
    return getLocalTransform();
}

glm::vec3 Transform::getForwardVector() const
{
    const auto& tx = getWorldTransform();
    return -glm::column(tx, 2);
}

glm::vec3 Transform::getUpVector() const
{
    const auto& tx = getWorldTransform();
    return glm::column(tx, 1);
}

glm::vec3 Transform::getRightVector() const
{
    const auto& tx = getWorldTransform();
    return glm::column(tx, 0);
}

bool Transform::addChild(Transform& child)
{
    CRO_ASSERT(&child != this, "can't parent to ourselves");

    if (m_children.size() < MaxChildren)
    {
        //remove any existing parent
        if (child.m_parent)
        {
            if (child.m_parent == this)
            {
                return true; //already added!
            }

            auto& otherSiblings = child.m_parent->m_children;
            otherSiblings.erase(std::remove_if(otherSiblings.begin(), otherSiblings.end(),
                [&child](const Transform* ptr)
                {
                    return ptr == &child;
                }), otherSiblings.end());
        }
        child.m_parent = this;

        m_children.push_back(&child);

        return true;
    }
    Logger::log("Could not add child transform, max children has been reached");
    return false;
}

void Transform::removeChild(Transform& tx)
{
    if (tx.m_parent != this) return;

    tx.m_parent = nullptr;

    m_children.erase(std::remove_if(m_children.begin(), m_children.end(),
        [&tx](const Transform* ptr)
        {
            return ptr == &tx;
        }), m_children.end());
}

//private
void Transform::reset()
{
    m_origin = glm::vec3(0.f, 0.f, 0.f);
    m_position = glm::vec3(0.f, 0.f, 0.f);
    m_scale = glm::vec3(1.f, 1.f, 1.f);
    m_rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
    m_transform = glm::mat4(1.f);
    m_parent = nullptr;
    m_dirtyFlags = 0;

    m_children.clear();
}