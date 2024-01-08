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

#include <crogine/detail/Assert.hpp>
#include <crogine/graphics/BoundingBox.hpp>
#include <crogine/graphics/Spatial.hpp>

#include <crogine/detail/glm/gtx/norm.hpp>

using namespace cro;

template <>
FloatRect::Rectangle(const Box& box)
    : left  (box[0].x),
    bottom  (box[0].y),
    width   (box[1].x - box[0].x),
    height  (box[1].y - box[0].y)
{

}

template <>
FloatRect& FloatRect::operator=(const Box& box)
{
    left = box[0].x;
    bottom = box[0].y;
    width = box[1].x - box[0].x;
    height = box[1].y - box[0].y;

    return *this;
}

Box::Box(FloatRect rect, float thickness)
    : m_points({ glm::vec3(rect.left, rect.bottom, -(thickness / 2.f)), glm::vec3(rect.left + rect.width, rect.bottom + rect.height, thickness / 2.f) })
{
    //hmmmm do we want a constexpr ctor or trade it for the ability to assert?

    CRO_ASSERT(thickness > 0, "");
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

bool Box::intersects(const Box& other, Box* overlap) const
{
    //auto result = ((m_points[0].x >= other[0].x && m_points[0].x <= other[1].x) || (other[0].x >= m_points[0].x && other[0].x <= m_points[1].x)) &&
    //            ((m_points[0].y >= other[0].y && m_points[0].y <= other[1].y) || (other[0].y >= m_points[0].y && other[0].y <= m_points[1].y)) &&
    //            ((m_points[0].z >= other[0].z && m_points[0].z <= other[1].z) || (other[0].z >= m_points[0].z && other[0].z <= m_points[1].z));

    auto result = (m_points[0].x <= other.m_points[1].x && m_points[1].x >= other.m_points[0].x) &&
                    (m_points[0].y <= other.m_points[1].y && m_points[1].y >= other.m_points[0].y) &&
                    (m_points[0].z <= other.m_points[1].z && m_points[1].z >= other.m_points[0].z);

    if (overlap && result)
    {
        auto& out = *overlap;
        out[0] = { std::max(m_points[0].x, other.m_points[0].x), std::max(m_points[0].y, other.m_points[0].y), std::max(m_points[0].z, other.m_points[0].z) };
        out[1] = { std::min(m_points[1].x, other.m_points[1].x), std::min(m_points[1].y, other.m_points[1].y), std::min(m_points[1].z, other.m_points[1].z) };
    }
    return result;
}

bool Box::intersects(Sphere sphere) const
{
    if (!contains(sphere.centre))
    {
        auto dist = glm::length2(sphere.centre - closest(sphere.centre));
        return dist < (sphere.radius* sphere.radius);
    }
    return true;
}

bool Box::contains(const Box& box) const
{
    if (box.m_points[0].x < m_points[0].x) return false;
    if (box.m_points[0].y < m_points[0].y) return false;
    if (box.m_points[0].z < m_points[0].z) return false;
    
    if (box.m_points[1].x > m_points[1].x) return false;
    if (box.m_points[1].y > m_points[1].y) return false;
    if (box.m_points[1].z > m_points[1].z) return false;

    return true;
}

bool Box::contains(glm::vec3 point) const
{
    if (point.x < m_points[0].x) return false;
    if (point.y < m_points[0].y) return false;
    if (point.z < m_points[0].z) return false;

    if (point.x > m_points[1].x) return false;
    if (point.y > m_points[1].y) return false;
    if (point.z > m_points[1].z) return false;

    return true;
}

float Box::getPerimeter() const
{
    auto size = m_points[1] - m_points[0];
    return (size.x + size.y + size.z) * 2.f;
}

float Box::getVolume() const
{
    auto size = m_points[1] - m_points[0];
    return std::abs(size.x * size.y * size.z);
}

Box Box::merge(Box a, Box b)
{
    Box retVal;

    retVal[0].x = std::min(a[0].x, b[0].x);
    retVal[0].y = std::min(a[0].y, b[0].y);
    retVal[0].z = std::min(a[0].z, b[0].z);

    retVal[1].x = std::max(a[1].x, b[1].x);
    retVal[1].y = std::max(a[1].y, b[1].y);
    retVal[1].z = std::max(a[1].z, b[1].z);

    return retVal;
}

//private
std::array<glm::vec3, 8> Box::getCorners() const
{
    std::array<glm::vec3, 8> retVal = {};

    auto min = m_points[0];
    auto max = m_points[1];

    //near face, specified anti-clockwise looking towards the origin from the positive z-axis.
    //left-top-front.
    retVal[0] = { min.x, max.y, max.z };
    //left-bottom-front.
    retVal[1] = { min.x, min.y, max.z };
    //right-bottom-front.
    retVal[2] = { max.x, min.y, max.z };
    //right-top-front.
    retVal[3] = { max.x, max.y, max.z };

    //far face, specified anti-clockwise looking towards the origin from the negative z-axis.
    //right-top-back.
    retVal[4] = { max.x, max.y, min.z };
    //right-bottom-back.
    retVal[5] = { max.x, min.y, min.z };
    //left-bottom-back.
    retVal[6] = { min.x, min.y, min.z };
    //left-top-back.
    retVal[7] = { min.x, max.y, min.z };

    return retVal;
}

glm::vec3 Box::closest(glm::vec3 point) const
{
    return
    {
        std::max(m_points[0].x, std::min(point.x, m_points[1].x)),
        std::max(m_points[0].y, std::min(point.y, m_points[1].y)),
        std::max(m_points[0].z, std::min(point.z, m_points[1].z)),
    };
}