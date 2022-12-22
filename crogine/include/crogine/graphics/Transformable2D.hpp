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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/glm/mat4x4.hpp>

namespace cro
{
    /*!
    \brief Base class for transformable 2D objects such as SimpleQuad and SimpleText
    */
    class CRO_EXPORT_API Transformable2D
    {
    public:
        Transformable2D();
        virtual ~Transformable2D() = default;

        /*!
        \brief Sets the position of the transform
        */
        void setPosition(glm::vec2 position);

        /*!
        \brief Returns the current position of the transform
        */
        glm::vec2 getPosition() const { return m_position; }

        /*!
        \brief Moves the transform by the given amount
        */
        void move(glm::vec2 amount);

        /*!
        \brief Set the rotation in degrees
        This rotates about the origin point at the bottom left
        */
        void setRotation(float rotation);

        /*!
        \brief Returns the current rotation in degrees
        */
        float getRotation() const;

        /*!
        \brief Rotates the transform by the given amount, in degrees
        */
        void rotate(float rotation);

        /*!
        \brief Set the scale of the transform
        \param scale A vec2 containing the scale in the x and y axis
        */
        void setScale(glm::vec2 scale);

        /*!
        \brief Returns the current scale
        */
        glm::vec2 getScale() const { return m_scale; }

        /*!
        \brief Scales the transform in the x and y axis by the
        given vector
        */
        void scale(glm::vec2 amount);

        /*!
        \brief Returns the combined transform
        */
        const glm::mat4& getTransform() const;

    private:
        glm::vec2 m_position;
        float m_rotation;
        glm::vec2 m_scale;
        mutable glm::mat4 m_modelMatrix;

        mutable bool m_dirty;

        void updateTransform() const;
    };
}