/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2022
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

#include <crogine/graphics/Rectangle.hpp>

#include <array>

namespace cro
{
    struct Sphere;
    /*!
    \brief Bounding box created from 2 3D points
    */
    class CRO_EXPORT_API Box final
    {
    public:
        /*!
        \brief Constructs a box with no size
        */
        constexpr Box()
            : m_points({ glm::vec3(0.f), glm::vec3(0.f) }) {}

        /*!
        \brief Creates a box with the given min/max values
        \param min The minimum extent
        \param max The maximum extent
        */
        constexpr Box(glm::vec3 min, glm::vec3 max)
            : m_points({ min, max }){}

        /*!
        \brief Constructs a Box from a 2D FloatRect with a given thickness
        \param rect FloatRect to take x and y coordinates from
        \param thickness Thickness for the box to create. The created box is
        centred about this value.
        */
        Box(FloatRect rect, float thickness = 1.f);

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
        \brief Returns true if the box intersects with the given box
        \param box Another box to test the intersection with
        \param overlap A pointer to a box which will be filled to
        create a box representing the intersection of the two boxes
        that are currently being tested.
        */
        bool intersects(const Box& box, Box* overlap = nullptr) const;

        /*!
        \brief Returns true if the box intersects with the given sphere
        */
        bool intersects(Sphere sphere) const;

        /*!
        \brief Returns true if this box fully contains the given box
        */
        bool contains(const Box& box) const;

        /*!
        \brief Returns true if this box contains the given point
        */
        bool contains(glm::vec3 point) const;

        /*!
        \brief Returns the size of the box as width/height/depth
        */
        glm::vec3 constexpr getSize() const { return m_points[1] - m_points[0]; }

        /*!
        \brief Returns the sum of all 6 sides of the box
        */
        float getPerimeter() const;

        /*!
        \brief Returns the cubic volume of the box
        */
        float getVolume() const;

        /*!
        \brief Adds the given position to the box
        */
        void operator += (glm::vec3 position) { m_points[0] += position; m_points[1] += position; }

        bool operator == (const Box& other) const
        {
            return (&other == this) || (m_points[0] == other.m_points[0] && m_points[1] == other.m_points[1]);
        }

        bool operator != (const Box& other) const
        {
            return !(*this == other);
        }

        /*!
        \brief Returns a new box created by merging the two given boxes
        into a new encompassing box
        */
        static Box merge(Box, Box);

    private:
        std::array<glm::vec3, 2u> m_points;

        std::array<glm::vec3, 8> getCorners() const;

        glm::vec3 closest(glm::vec3) const;

        friend Box operator * (const glm::mat4&, const Box&);
    };

    /*!
    \brief Creates a new Box with the given position added to the given box
    */
    inline Box operator + (const Box& box, glm::vec3 position)
    {
        Box retval = box;
        retval += position;
        return retval;
    }

    /*!
    \brief Transforms a box by the given matrix
    */
    inline Box operator * (const glm::mat4& mat, const Box& box)
    {
        auto corners = box.getCorners();

        auto newMin = glm::vec3(mat * glm::vec4(corners[0], 1.f));
        auto newMax = newMin;

        auto updateMinMax = [&newMin, &newMax](glm::vec3 point)
        {
            if (point.x < newMin.x)
            {
                newMin.x = point.x;
            }
            if (point.x > newMax.x)
            {
                newMax.x = point.x;
            }
            if (point.y < newMin.y)
            {
                newMin.y = point.y;
            }
            if (point.y > newMax.y)
            {
                newMax.y = point.y;
            }
            if (point.z < newMin.z)
            {
                newMin.z = point.z;
            }
            if (point.z > newMax.z)
            {
                newMax.z = point.z;
            }
        };

        for (auto i = 1u; i < 8u; ++i)
        {
            auto point = glm::vec3(mat * glm::vec4(corners[i], 1.f));
            updateMinMax(point);
        }

        return Box(newMin, newMax);
    }
}
