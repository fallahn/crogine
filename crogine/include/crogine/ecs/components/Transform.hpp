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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/ecs/Entity.hpp>

#include <crogine/detail/glm/vec3.hpp>
#include <crogine/detail/glm/mat4x4.hpp>
#include <crogine/detail/glm/gtc/quaternion.hpp>

#include <vector>
#include <functional>

#ifdef USE_PARALLEL_PROCESSING
#include <mutex>
#endif

namespace cro
{
    class Entity;
    class SkeletalAnimator;
    struct Attachment;

    /*!
    \brief A three dimensional transform component
    */
    class CRO_EXPORT_API Transform final
    {
    public:
        enum
        {
            MaxChildren = 320
        };


        Transform();
        Transform(const Transform&) = delete;
        //moving takes on all the properties of
        //the existing transform including the
        //parental hierarchy, and resets the existing
        //transform to an identity.
        Transform(Transform&&) noexcept;

        Transform& operator = (const Transform&) = delete;
        Transform& operator = (Transform&&) noexcept;

        ~Transform();

        /*!
        \brief Sets the origin of the transform
        */
        void setOrigin(glm::vec3);
        /*!
        \brief Sets the origin of the x and y axis.
        Useful shortcut when working with 2D objects
        such as text or sprites
        */
        void setOrigin(glm::vec2);

        /*!
        \brief Sets the transform position
        */
        void setPosition(glm::vec3);

        /*!
        \brief Sets a 2D position by leaving the z-coord in tact.
        */
        void setPosition(glm::vec2);

        /*!
        \brief Sets the rotation around the given axis.
        \param axis A 3 dimensional axis around which the rotation is set
        \param angle The angle in radians to rotate
        */
        void setRotation(glm::vec3 axis, float angle);

        /*!
        \brief Sets the Euler rotation around the z axis, in radians.
        Useful shortcut when using 2D objects such as sprites or text
        */
        void setRotation(float);

        /*!
        \brief Sets the rotation with the given quaternion
        */
        void setRotation(glm::quat);

        /*!
        \brief Sets the rotation with the given matrix.
        \param rotation A matrix containing the rotation to set
        */
        void setRotation(glm::mat4);

        /*!
        \brief Prevent implicit conversion of vec3 to quat
        */
        void setRotation(glm::vec3) = delete;

        /*
        \brief Sets the transform scale for each axis
        */
        void setScale(glm::vec3);

        /*!
        \brief Sets the transform scale on the x/y axis.
        Useful for 2D objects such as text or sprites. Z is fixed at 1
        */
        void setScale(glm::vec2);

        /*
        \brief Moves the transform.
        The given parameter is added to the transform's existing position
        */
        void move(glm::vec3);

        /*!
        \brief Moves the transform on the x and y axis only.
        The given parameter is added to the transform's current position.
        Useful for working with 2D objects such as sprites or text
        */
        void move(glm::vec2);

        /*!
        \brief Rotates the transform.
        \param axis A vector representing the axis around which to rotate the transform
        \param amount The amount of rotation in radians
        */
        void rotate(glm::vec3 axis, float amount);

        /*!
        \brief Rotates the transform around its z axis by the given radians.
        Useful shortcut for working with 2D items such as sprites or text
        */
        void rotate(float amount);

        /*!
        \brief Rotates the transform by the given quaternion
        */
        void rotate(glm::quat rotation);

        /*!
        \brief Rotates the transform by the given matrix
        */
        void rotate(glm::mat4 rotation);

        /*!
        \brief Prevent implicit conversion from vec3
        */
        void rotate(glm::vec3) = delete;

        /*!
        \brief Scales the transform.
        The existing scale on the x, y, and z axis is multiplied by the given values
        */
        void scale(glm::vec3);

        /*!
        \brief Scales the transform along the x and y axis.
        Useful shortcut for scaling 2D items such as text or sprites
        */
        void scale(glm::vec2);

        /*!
        \brief Returns the current origin of the transform
        */
        glm::vec3 getOrigin() const;

        /*
        \brief Returns the local position of the transform
        */
        glm::vec3 getPosition() const;

        /*!
        \brief Returns the world position of the transform
        */
        glm::vec3 getWorldPosition() const;

        /*!
        \brief Returns the rotation of the transform
        */
        glm::quat getRotation() const;

        /*!
        \brief Returns the rotation, in radians, around the z axis
        */
        float getRotation2D() const;

        /*!
        \brief Returns the sum rotation of this transform
        and all its parents (if any)
        */
        glm::quat getWorldRotation() const;
        
        /*!
        \brief Returns the local scale of the transform
        */
        glm::vec3 getScale() const;

        /*!
        \brief Returns the combined world scale of the transform
        according to its scene graph hierarchy
        */
        glm::vec3 getWorldScale() const;

        /*!
        \brief Returns a matrix representing the complete transform
        in local space.
        */
        glm::mat4 getLocalTransform() const;
        /*!
        \brief Sets the local transform from the given matrix
        */
        void setLocalTransform(glm::mat4 transform);

        /*!
        \brief Returns a matrix representing the world space Transform.
        This is the local transform multiplied by all parenting transforms
        */
        glm::mat4 getWorldTransform() const;

        /*!
        \brief Returns the forward vector of this transform
        This won't be normalised if the scale is anything but 1,1,1
        */
        glm::vec3 getForwardVector() const;

        /*!
        \brief Returns the up vector of this transform
        This won't be normalised if the scale is anything but 1,1,1
        */
        glm::vec3 getUpVector() const;

        /*!
        \brief Returns the right vector of this transform
        This won't be normalised if the scale is anything but 1,1,1
        */
        glm::vec3 getRightVector() const;

        /*!
        \brief Adds the given transform as a child, if there is room
        No more than MaxChildren may be added to any one transform.
        \returns false if child was not successfully added
        */
        bool addChild(Transform& tx);

        /*!
        \brief Removes the given child if it exists.
        */
        void removeChild(Transform& tx);

        /*!
        \brief Returns the depth of this transform in the Scene hierarchy.
        For example if this is a root node it returns 0, or 1 if it has a single parent
        */
        std::size_t getDepth() const { return m_depth; }

        /*!
        \brief Returns true if this transform is marked for an update
        next time getWorldTransform() or getLocalTransform() are called
        */
        bool getDirty() const { return m_dirtyFlags != 0; }

        /*!
        \brief Adds a callback which is executed when the transform is updated.
        Specifically this happens on setLocalTransform() or getLocalTransform()
        if the transform has internally been flagged as dirty. As getWorldTransform()
        also calls getLocalTransform() then this function may also raise callback
        events.
        */
        void addCallback(std::function<void()> callback);

        static constexpr glm::vec3 X_AXIS = glm::vec3(1.f, 0.f, 0.f);
        static constexpr glm::vec3 Y_AXIS = glm::vec3(0.f, 1.f, 0.f);
        static constexpr glm::vec3 Z_AXIS = glm::vec3(0.f, 0.f, 1.f);
        static constexpr glm::quat QUAT_IDENTITY = glm::quat(1.f, 0.f, 0.f, 0.f);

    private:
        glm::vec3 m_origin;
        glm::vec3 m_position;
        glm::vec3 m_scale;
        glm::quat m_rotation;
        mutable glm::mat4 m_transform;

        Transform* m_parent;
        std::vector<Transform*> m_children = {};
        void doCallbacks() const; //actually mutable - called from getTransform()

#ifdef USE_PARALLEL_PROCESSING
        mutable std::mutex m_mutex;
#endif

        std::size_t m_depth;
        void increaseDepth();
        void decreaseDepth();

        enum Flags
        {
            Parent = 0x1,
            Child = 0x2,
            Tx = 0x4,
            All = Parent | Child | Tx
        };
        mutable std::uint8_t m_dirtyFlags;

        std::vector<std::function<void()>> m_callbacks;

        void reset();

        //this is a fudge to allow transforms to read
        //skeletal attachment points
        glm::mat4 m_attachmentTransform;
        friend class SkeletalAnimator;
        friend struct Attachment;
    };
}