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
#include <crogine/detail/glm/vec3.hpp>
#include <crogine/detail/glm/matrix.hpp>

#include <array>

namespace cro
{
    /*!
    \brief Bounding box created from 2 3D points
    */
    class CRO_EXPORT_API Box final
    {
    public:
        /*!
        \brief Constructs a box with no size
        */
        Box();

        /*!
        \brief Creates a box with the given min/max values
        \param min The minimum extent
        \param max The maximum extent
        */
        Box(glm::vec3 min, glm::vec3 max);

        /*!
        \brief Returns the centre of the box in local coordinates
        */
        glm::vec3 getCentre() const;

        /*!
        \brief Overload allows accessing the min and max values as an array
        */
        glm::vec3& operator [](std::size_t);
        const glm::vec3& operator [](std::size_t) const;

        /*!
        \brief Returns true if the box intersects with the give box
        \param box Another box to test the intersection with
        \param overlap A pointer to a box which will be filled to
        create a box representing the intersection of the two boxes
        that are currently being tested.
        */
        bool intersects(const Box& box, Box* overlap = nullptr);

        /*!
        \brief Returns the size of the box as width/height/depth
        */
        glm::vec3 getSize() const { return m_points[1] - m_points[0]; }

        /*!
        \brief Adds the given position to the box
        */
        void operator += (glm::vec3 position) { m_points[0] += position; m_points[1] += position; }

    private:
        std::array<glm::vec3, 2u> m_points;
    };

    /*!
    \brief Creates a new Box with the given position added to the given box
    */
    Box operator + (const Box& box, glm::vec3 position)
    {
        Box retval = box;
        retval += position;
        return retval;
    }

    /*!
    \brief Transforms the min/max values of the box by the given matrix
    */
    Box operator * (const glm::mat4& mat, const Box& box)
    {
        return { glm::vec3(mat * glm::vec4(box[0], 1.f)), glm::vec3(mat * glm::vec4(box[1], 1.f)) };
    }
}