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

#ifndef CRO_TRANSFORM_HPP_
#define CRO_TRANSFORM_HPP_

#include <crogine/Config.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/ecs/Entity.hpp>

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>

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
        using ParentID = int32;

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
        \brief Returns the position of the transform
        */
        glm::vec3 getPosition() const;
        /*!
        \brief Returns the euler rotation of the transform
        */
        glm::vec3 getRotation() const;
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
        glm::mat4 getWorldTransform(std::vector<Entity>&) const;

    private:
        glm::vec3 m_origin;
        glm::vec3 m_position;
        glm::vec3 m_scale;
        glm::quat m_rotation;
        mutable glm::mat4 m_transform;

        mutable bool m_dirty;
    };
}


#endif //CRO_TRANSFORM_HPP_