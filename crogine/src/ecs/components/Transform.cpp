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

#include <crogine/ecs/components/Transform.hpp>

#include <crogine/detail/glm/gtx/euler_angles.hpp>
#include <crogine/detail/glm/gtx/quaternion.hpp>
#include <crogine/detail/glm/gtx/matrix_decompose.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/glm/gtc/matrix_access.hpp>

using namespace cro;

Transform::Transform()
    : m_origin              (0.f, 0.f, 0.f),
    m_position              (0.f, 0.f, 0.f),
    m_scale                 (1.f, 1.f, 1.f),
    m_rotation              (1.f, 0.f, 0.f, 0.f),
    m_transform             (1.f),
    m_parent                (nullptr),
    m_depth                 (0),
    m_dirtyFlags            (Flags::Tx),
    m_attachmentTransform   (1.f)
{

}

Transform::Transform(Transform&& other) noexcept
    : m_origin              (0.f, 0.f, 0.f),
    m_position              (0.f, 0.f, 0.f),
    m_scale                 (1.f, 1.f, 1.f),
    m_rotation              (1.f, 0.f, 0.f, 0.f),
    m_transform             (1.f),
    m_parent                (nullptr),
    m_depth                 (0),
    m_dirtyFlags            (Flags::Tx),
    m_attachmentTransform   (1.f)
{
    CRO_ASSERT(other.m_parent != this, "Invalid assignment");

    if (other.m_parent != this)
    {
        //orphan any children (why? we've just been constructed so we shouldn't have any)
        for (auto c : m_children)
        {
            c->m_parent = nullptr;

            while (c->m_depth > 0)
            {
                c->decreaseDepth();
            }
        }

        //and adopt new
        m_parent = other.m_parent;
        m_depth = other.m_depth;

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

            //should already be at the correct depth if
            //we're taking on theexisting depth..?
            while (c->m_depth > m_depth + 1)
            {
                c->decreaseDepth();
            }

            while (c->m_depth <= m_depth)
            {
                c->increaseDepth();
            }
        }

        //actually take on the other transform
        setPosition(other.getPosition());
        setRotation(other.getRotation());
        setScale(other.getScale());
        setOrigin(other.getOrigin());
        m_dirtyFlags = Flags::Tx;
        m_attachmentTransform = other.m_attachmentTransform;
        m_callbacks.swap(other.m_callbacks);

        other.reset();
    }
}

Transform& Transform::operator=(Transform&& other) noexcept
{
    CRO_ASSERT(&other != this && other.m_parent != this, "Invalid assignment");

    if (&other != this && other.m_parent != this)
    {
        //remove this transform from its parent
        if (m_parent)
        {
            m_parent->removeChild(*this);
        }

        /*
        If you get errors here in debug build chances are
        your project requires PARALLEL_GLOBAL_DISABLE to be defined
        */

        //orphan any children
        for (auto c : m_children)
        {
            c->m_parent = nullptr;

            while (c->m_depth > 0)
            {
                c->decreaseDepth();
            }
        }

        m_parent = other.m_parent;
        m_depth = other.m_depth;

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

            while (c->m_depth > m_depth + 1)
            {
                c->decreaseDepth();
            }

            while (c->m_depth <= m_depth)
            {
                c->increaseDepth();
            }
        }

        //actually take on the other transform
        setPosition(other.getPosition());
        setRotation(other.getRotation());
        setScale(other.getScale());
        setOrigin(other.getOrigin());
        m_dirtyFlags = Flags::Tx;
        m_attachmentTransform = other.m_attachmentTransform;
        m_callbacks.swap(other.m_callbacks);

        other.reset();
    }
    return *this;
}

Transform::~Transform()
{
    //remove this transform from its parent
    if (m_parent)
    {
        m_parent->removeChild(*this);
    }

    //orphan any children
    for (auto c : m_children)
    {
        c->m_parent = nullptr;

        while (c->m_depth > 0)
        {
            c->decreaseDepth();
        }
    }
}

//public
void Transform::setOrigin(glm::vec3 o)
{
#ifdef USE_PARALLEL_PROCESSING
    std::scoped_lock l(m_mutex);
#endif

    m_origin = o;
    m_dirtyFlags |= Tx;
}

void Transform::setOrigin(glm::vec2 o)
{
    setOrigin(glm::vec3(o.x, o.y, m_origin.z));
}

void Transform::setPosition(glm::vec3 position)
{
#ifdef USE_PARALLEL_PROCESSING
    std::scoped_lock l(m_mutex);
#endif

    m_position = position;
    m_dirtyFlags |= Tx;
}

void Transform::setPosition(glm::vec2 position)
{
#ifdef USE_PARALLEL_PROCESSING
    std::scoped_lock l(m_mutex);
#endif

    m_position.x = position.x;
    m_position.y = position.t;
    m_dirtyFlags |= Tx;
}

void Transform::setRotation(glm::vec3 axis, float angle)
{
#ifdef USE_PARALLEL_PROCESSING
    std::scoped_lock l(m_mutex);
#endif

    glm::quat q = glm::quat(1.f, 0.f, 0.f, 0.f);
    m_rotation = glm::rotate(q, angle, axis);
    m_dirtyFlags |= Tx;
}

void Transform::setRotation(float radians)
{
    setRotation(Z_AXIS, radians);
}

void Transform::setRotation(glm::quat rotation)
{
#ifdef USE_PARALLEL_PROCESSING
    std::scoped_lock l(m_mutex);
#endif

    m_rotation = rotation;
    m_dirtyFlags |= Tx;
}

void Transform::setRotation(glm::mat4 rotation)
{
#ifdef USE_PARALLEL_PROCESSING
    std::scoped_lock l(m_mutex);
#endif
    m_rotation = glm::quat_cast(rotation);
    m_dirtyFlags |= Tx;
}

void Transform::setScale(glm::vec3 scale)
{
#ifdef USE_PARALLEL_PROCESSING
    std::scoped_lock l(m_mutex);
#endif
    m_scale = scale;
    m_dirtyFlags |= Tx;
}

void Transform::setScale(glm::vec2 scale)
{
    setScale(glm::vec3(scale.x, scale.y, m_scale.z));
}

void Transform::move(glm::vec3 distance)
{
#ifdef USE_PARALLEL_PROCESSING
    std::scoped_lock l(m_mutex);
#endif
    m_position += distance;
    m_dirtyFlags |= Tx;
}

void Transform::move(glm::vec2 distance)
{
    move(glm::vec3(distance.x, distance.y, 0.f));
}

void Transform::rotate(glm::vec3 axis, float rotation)
{
#ifdef USE_PARALLEL_PROCESSING
    std::scoped_lock l(m_mutex);
#endif
    m_rotation = glm::rotate(m_rotation, rotation, glm::normalize(axis));
    m_dirtyFlags |= Tx;
}

void Transform::rotate(float amount)
{
    rotate(Z_AXIS, amount);
}

void Transform::rotate(glm::quat rotation)
{
#ifdef USE_PARALLEL_PROCESSING
    std::scoped_lock l(m_mutex);
#endif
    m_rotation = rotation * m_rotation;
    m_dirtyFlags |= Tx;
}

void Transform::rotate(glm::mat4 rotation)
{
#ifdef USE_PARALLEL_PROCESSING
    std::scoped_lock l(m_mutex);
#endif
    m_rotation = glm::quat_cast(rotation) * m_rotation;
    m_dirtyFlags |= Tx;
}

void Transform::scale(glm::vec3 scale)
{
#ifdef USE_PARALLEL_PROCESSING
    std::scoped_lock l(m_mutex);
#endif
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
    const auto t = glm::vec3(getWorldTransform()[3]);
    return t - ((m_origin * m_scale) * m_rotation);
}

glm::quat Transform::getRotation() const
{
    return m_rotation;
}

float Transform::getRotation2D() const
{
    return glm::eulerAngles(m_rotation).z;
}

glm::quat Transform::getWorldRotation() const
{
    if (m_parent)
    {
        return m_parent->getWorldRotation() * m_rotation;
    }
    return m_rotation;
}

glm::vec3 Transform::getScale() const
{
    return m_scale;
}

glm::vec3 Transform::getWorldScale() const
{
    if (m_parent)
    {
        return m_parent->getWorldScale() * m_scale;
    }
    return m_scale;
}

glm::mat4 Transform::getLocalTransform() const
{
   
    if (m_dirtyFlags & Tx)
    {
#ifdef USE_PARALLEL_PROCESSING
        std::scoped_lock l(m_mutex);
#endif 
        m_transform = glm::translate(glm::mat4(1.f), m_position);
        m_transform *= glm::toMat4(m_rotation);
        m_transform = glm::scale(m_transform, m_scale);
        m_transform = glm::translate(m_transform, -m_origin);

        m_dirtyFlags &= ~Tx;

        doCallbacks();
    }

    return m_attachmentTransform * m_transform;
}

void Transform::setLocalTransform(glm::mat4 transform)
{
#ifdef USE_PARALLEL_PROCESSING
    std::scoped_lock l(m_mutex);
#endif

    m_position = transform[3];
    m_rotation = glm::quat_cast(transform);
    
    //not simple when rotations involved. Based
    //on glm::matrix_decompose()
    std::array<glm::vec3, 3u> row = {};
    for (auto i = 0u; i < 3u; ++i)
    {
        for (auto j = 0u; j < 3u; ++j)
        {
            row[i][j] = transform[i][j] / transform[3][3];
        }
    }
    m_scale.x = glm::length(row[0]);
    m_scale.y = glm::length(row[1]);
    m_scale.z = glm::length(row[2]);
    auto t = glm::cross(row[1], row[2]);
    if (glm::dot(row[0], t) < 0)
    {
        for (auto i = 0u; i < 3; ++i)
        {
            m_scale[i] *= -1.f;
        }
    }
    //m_dirtyFlags |= Tx;
    m_transform = glm::translate(transform, -m_origin);
    m_dirtyFlags &= ~Tx;

    doCallbacks();
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
    auto tx = getWorldTransform();
    return -glm::column(tx, 2);
}

glm::vec3 Transform::getUpVector() const
{
    auto tx = getWorldTransform();
    return glm::column(tx, 1);
}

glm::vec3 Transform::getRightVector() const
{
    auto tx = getWorldTransform();
    return glm::column(tx, 0);
}

bool Transform::addChild(Transform& child)
{
    CRO_ASSERT(&child != this, "can't parent to ourselves");

    //don't really remember why there's a limit
    if (m_children.size() < MaxChildren)
    {
        //remove any existing parent
        if (child.m_parent)
        {
            if (child.m_parent == this)
            {
                return true; //already added!
            }

#ifdef USE_PARALLEL_PROCESSING
            std::scoped_lock l(m_mutex);
#endif

            auto& otherSiblings = child.m_parent->m_children;
            otherSiblings.erase(std::remove_if(otherSiblings.begin(), otherSiblings.end(),
                [&child](const Transform* ptr)
                {
                    return ptr == &child;
                }), otherSiblings.end());
        }

        {
#ifdef USE_PARALLEL_PROCESSING
            std::scoped_lock l(m_mutex);
#endif
            child.m_parent = this;
        }


        //correct the depth
        while (child.m_depth < (m_depth + 1))
        {
            child.increaseDepth();
        }
        while (child.m_depth > (m_depth + 1))
        {
            child.decreaseDepth();
        }


        {
#ifdef USE_PARALLEL_PROCESSING
            std::scoped_lock l(m_mutex);
#endif
            m_children.push_back(&child);
        }

        return true;
    }
    Logger::log("Could not add child transform, max children has been reached");
    return false;
}

void Transform::removeChild(Transform& tx)
{
    if (tx.m_parent != this) return;

    {
#ifdef USE_PARALLEL_PROCESSING
        std::scoped_lock l(m_mutex);
#endif
        tx.m_parent = nullptr;
    }


    while (tx.m_depth > 0)
    {
        tx.decreaseDepth();
    }

#ifdef USE_PARALLEL_PROCESSING
    std::scoped_lock l(m_mutex);
#endif
    m_children.erase(std::remove_if(m_children.begin(), m_children.end(),
        [&tx](const Transform* ptr)
        {
            return ptr == &tx;
        }), m_children.end());
}

void Transform::addCallback(std::function<void()> cb)
{
#ifdef USE_PARALLEL_PROCESSING
    std::scoped_lock l(m_mutex);
#endif
    m_callbacks.push_back(cb);
}

//private
void Transform::reset()
{
#ifdef USE_PARALLEL_PROCESSING
    std::scoped_lock l(m_mutex);
#endif
    m_origin = glm::vec3(0.f, 0.f, 0.f);
    m_position = glm::vec3(0.f, 0.f, 0.f);
    m_scale = glm::vec3(1.f, 1.f, 1.f);
    m_rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
    m_transform = glm::mat4(1.f);
    m_parent = nullptr;
    m_dirtyFlags = 0;
    m_depth = 0;
    m_attachmentTransform = glm::mat4(1.f);

    m_callbacks.clear();
    m_children.clear();
}

void Transform::doCallbacks() const
{
    for (auto& c : m_callbacks)
    {
        c();
    }

    for (auto c : m_children)
    {
        c->doCallbacks();
    }
}

void Transform::increaseDepth()
{
#ifdef USE_PARALLEL_PROCESSING
    std::scoped_lock l(m_mutex);
#endif
    m_depth++;
    for (auto& c : m_children)
    {
        c->increaseDepth();
    }
}

void Transform::decreaseDepth()
{
#ifdef USE_PARALLEL_PROCESSING
    std::scoped_lock l(m_mutex);
#endif
    if (m_depth > 0)
    {
        m_depth--; //this is a hack, we should never call this if we're at 0 already
    }

    for (auto& c : m_children)
    {
        c->decreaseDepth();
    }
}