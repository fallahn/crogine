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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/ecs/Entity.hpp>

#include <crogine/detail/glm/vec3.hpp>
#include <crogine/detail/glm/mat4x4.hpp>
#include <crogine/detail/glm/gtc/quaternion.hpp>

#include <vector>

namespace cro
{
    class Entity;
    /*!
    \brief A three dimensional transform component
    */
    class CRO_EXPORT_API Transform final
    {
    public:
        enum
        {
            MaxChildren = 16
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
        \brief Sets the transform position
        */
        void setPosition(glm::vec3);

        /*!
        \brief Sets a 2D position by automatically setting
        the Z value to zero
        */
        void setPosition(glm::vec2);

        /*!
        \brief Sets the euler rotation in radians for the x, y and z axis
        */
        void setRotation(glm::vec3);
        /*!
        \brief Sets the rotation with the given quaternion
        */
        void setRotation(glm::quat);
        /*
        \brief Sets the transform scale for each axis
        */
        void setScale(glm::vec3);

        /*
        \brief Moves the transform.
        The given paramter is added to the transform's existing position
        */
        void move(glm::vec3);
        /*!
        \brief Rotates the transform.
        \param axis A vector representing the axis around which to rotate the transform
        \param amount The amount of rotation in radians
        */
        void rotate(glm::vec3 axis, float amount);
        /*!
        \brief Scales the transform.
        The existing scale on the x, y, and z axis is multiplied by the given values
        */
        void scale(glm::vec3);

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
        \brief Returns the euler rotation of the transform
        */
        glm::vec3 getRotation() const;
        /*!
        \brief Returns the rotation as a quaternion
        */
        glm::quat getRotationQuat() const;
        /*!
        \brief Returns the current scale of the transform
        */
        glm::vec3 getScale() const;

        /*!
        \brief Returns a matrix representing the complete transform
        in local space.
        */
        const glm::mat4& getLocalTransform() const;
        /*!
        \brief Returns a matrix representing the world space Transform.
        This is the local transform multiplied by all parenting transforms
        */
        glm::mat4 getWorldTransform() const;

        /*!
        \brief Sets the translation relative to the camera
        In cases where an object, usually the camera itself, needs
        to be transformed around a point, set this to true. For
        example this would be set if creating a 3rd person camera
        targeted on a player. Set to false by default.
        */
        void setRelativeToCamera(bool r) { m_relativeToCamera = r; }

        /*!
        \brief Returns whether or not this transform is relative to 
        world coordinates, or the camera.
        \see setRelativeToCamera()
        */
        bool getRelativeToCamera() const { return m_relativeToCamera; }

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


        static constexpr glm::vec3 X_AXIS = glm::vec3(1.f, 0.f, 0.f);
        static constexpr glm::vec3 Y_AXIS = glm::vec3(0.f, 1.f, 0.f);
        static constexpr glm::vec3 Z_AXIS = glm::vec3(0.f, 0.f, 1.f);

    private:
        glm::vec3 m_origin;
        glm::vec3 m_position;
        glm::vec3 m_scale;
        glm::quat m_rotation;
        mutable glm::mat4 m_transform;

        bool m_relativeToCamera;

        Transform* m_parent;
        std::vector<Transform*> m_children = {};


        enum Flags
        {
            Parent = 0x1,
            Child = 0x2,
            Tx = 0x4,
            All = Parent | Child | Tx
        };
        mutable uint8 m_dirtyFlags;

        void reset();
    };
}