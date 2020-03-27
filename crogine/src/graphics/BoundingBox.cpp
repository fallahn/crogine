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

#include <crogine/detail/Assert.hpp>
#include <crogine/graphics/BoundingBox.hpp>

using namespace cro;

Box::Box()
    : m_points({ glm::vec3(0.f), glm::vec3(0.f) })
{

}

Box::Box(glm::vec3 min, glm::vec3 max)
    : m_points({ min, max })
{

}

glm::vec3 Box::getCentre() const
{
    return m_points[0] + ((m_points[1] - m_points[0]) / 2.f);
}

glm::vec3& Box::operator[](std::size_t idx)
{
    CRO_ASSERT(idx < m_points.size(), "Index out of range");
    return m_points[idx];
}

const glm::vec3& Box::operator[](std::size_t idx) const
{
    CRO_ASSERT(idx < m_points.size(), "Index out of range");
    return m_points[idx];
}

bool Box::intersects(const Box& other, Box* overlap)
{
    auto result = (m_points[0].x <= other[1].x && m_points[1].x >= other[0].x) &&
                    (m_points[0].y <= other[1].y && m_points[1].y >= other[0].y) &&
                    (m_points[0].z <= other[1].z && m_points[1].z >= other[0].z);

    if (overlap && result)
    {
        auto& out = *overlap;
        out[0] = { std::max(m_points[0].x, other[0].x), std::max(m_points[0].y, other[0].y), std::max(m_points[0].z, other[0].z) };
        out[1] = { std::min(m_points[1].x, other[1].x), std::min(m_points[1].y, other[1].y), std::min(m_points[1].z, other[1].z) };
    }
    return result;
}