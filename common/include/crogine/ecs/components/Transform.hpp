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
    class Transform final
    {
    public:
        using ParentID = int32;

        Transform() = default;

        void setPosition(glm::vec3);
        void setRotation(glm::vec3);
        void setScale(glm::vec3);

        void move(glm::vec3);
        void rotate(glm::vec3, float);
        void scale(glm::vec3);

        glm::vec3 getPosition() const;
        glm::vec3 getRotation() const;
        glm::vec3 getScale() const;

        const glm::mat4& getLocalTransform() const;
        glm::mat4& getWorldTransform(const std::vector<Entity>&) const;

    private:
        glm::vec3 m_position;
        glm::vec3 m_scale;
        glm::quat m_rotation;
        glm::mat4 m_transform;

        ParentID m_parent = -1;
    };
}


#endif //CRO_TRANSFORM_HPP_