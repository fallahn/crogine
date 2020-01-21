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

#include <array>
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

        /*!
        \brief Sets the origin of the transform
        */
        void setOrigin(glm::vec3);

        /*!
        \brief Sets the transform position
        */
        void setPosition(glm::vec3);
        /*!
        \brief Sets the euler rotation in radians for the x, y and z axis
        */
        void setRotation(glm::vec3);
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
        \brief Sets the parent entity if this node in the scene graph.
        Parents must have a Transform component
        */
        void setParent(Entity);

        /*!
        \brief Removes the parent entity if it exists so that this
        and all child nodes are orphaned
        */
        void removeParent();

        /*!
        \brief Returns the ID of the entity to which this transform's
        Entity is parented.
        */
        int32 getParentID() const { return m_parent; }

        /*!
        \brief Returns a read-only list of child IDs
        */
        const std::array<int32, MaxChildren>& getChildIDs() const { return m_children; }

    private:
        glm::vec3 m_origin;
        glm::vec3 m_position;
        glm::vec3 m_scale;
        glm::quat m_rotation;
        mutable glm::mat4 m_transform;
        mutable glm::mat4 m_worldTransform;

        int32 m_parent;
        int32 m_lastParent;
        int32 m_id;
        std::array<int32, MaxChildren> m_children = {};
        std::vector<int32> m_removedChildren;

        enum Flags
        {
            Parent = 0x1,
            Child = 0x2,
            Tx = 0x4,
            All = Parent | Child | Tx
        };
        mutable uint8 m_dirtyFlags;

        friend class SceneGraph;

        /*!
        \brief Sets the child node at the given index.
        \param id ID of the entity to add as a child.
        No more than MaxChildren may be added to any one transform.
        \returns false if child was not successfully added
        */
        bool addChild(uint32 id);

        /*!
        \brief Removes a child with the given entity ID if it exists.
        */
        void removeChild(uint32 id);
    };
}