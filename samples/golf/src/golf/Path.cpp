/*-----------------------------------------------------------------------

Matt Marchant 2022
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include "Path.hpp"

#include <crogine/detail/Assert.hpp>

Path::Path(bool looped)
    : m_length  (0.f),
    m_looped    (looped)
{

}

//public
void Path::addPoint(glm::vec3 point)
{
    if (!m_points.empty()
        && m_points.back() == point)
    {
        return;
    }

    m_points.push_back(point);

    if (m_points.size() > 1)
    {
        m_length += glm::length(point - m_points[m_points.size() - 2]);

        m_speedMultipliers.clear();
        for (auto i = 0; i < m_points.size() - 1; ++i)
        {
            float edgeLength = glm::length(m_points[i] - m_points[i + 1]);
            m_speedMultipliers.push_back(1.f - (edgeLength / m_length));
        }
    }
}

glm::vec3 Path::getPoint(std::size_t index) const
{
    CRO_ASSERT(index < m_points.size(), "Index out of range");
    return m_points[index];
}

float Path::getSpeedMultiplier(std::size_t segmentIndex) const
{
    CRO_ASSERT(segmentIndex < m_speedMultipliers.size(), "Index out of range");
    return m_speedMultipliers[segmentIndex];
}