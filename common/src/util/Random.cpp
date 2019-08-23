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

/*
Poisson Disc sampling based on article at http://devmag.org.za/2009/05/03/poisson-disk-sampling/
*/

#include <crogine/util/Random.hpp>
#include <crogine/util/Constants.hpp>

#include <crogine/detail/glm/gtx/norm.hpp>

using namespace cro;
using namespace cro::Util::Random;

namespace
{
    const std::size_t maxGridPoints = 3;

    //it's not desirable but the only way I can think to hide this class
    class Grid
    {
    public:
        Grid(const FloatRect& area, std::size_t maxPoints)
            : m_area(area), m_maxPoints(maxPoints)
        {
            CRO_ASSERT(maxPoints < sizeof(std::size_t), "max points must be less than " + std::to_string(sizeof(std::size_t)));
            resize(area, maxPoints);
        }
        ~Grid() = default;

        void addPoint(glm::vec2 point)
        {
            auto x = static_cast<std::size_t>(point.x + m_cellOffset.x) >> m_maxPoints;
            auto y = static_cast<std::size_t>(point.y + m_cellOffset.y) >> m_maxPoints;
            auto idx = y * m_cellCount.x + x;

            if (idx < m_cells.size()) m_cells[idx].push_back(point);
        }

        void resize(std::size_t maxPoints)
        {
            m_maxPoints = maxPoints;
            m_cellSize = 1 << m_maxPoints;
            m_cellOffset = { static_cast<int>(std::abs(m_area.left)), static_cast<int>(std::abs(m_area.bottom)) };
            m_cellCount = { static_cast<int>(std::ceil(m_area.width / static_cast<float>(m_cellSize))), static_cast<int>(std::ceil(m_area.height / static_cast<float>(m_cellSize))) };
            m_cells.clear();
            m_cells.resize(m_cellCount.x * m_cellCount.y);
        }

        void resize(const FloatRect& area, std::size_t maxPoints)
        {
            m_area = area;
            resize(maxPoints);
        }

        bool hasNeighbour(glm::vec2 point, float radius) const
        {
            const float radSqr = radius * radius;

            glm::ivec2 minDist
            (
                static_cast<int>(std::max(std::min(point.x - radius, (m_area.left + m_area.width) - 1.f), m_area.left)),
                static_cast<int>(std::max(std::min(point.y - radius, (m_area.bottom + m_area.height) - 1.f), m_area.bottom))
            );

            glm::ivec2 maxDist
            (
                static_cast<int>(std::max(std::min(point.x + radius, (m_area.left + m_area.width) - 1.f), m_area.left)),
                static_cast<int>(std::max(std::min(point.y + radius, (m_area.bottom + m_area.height) - 1.f), m_area.bottom))
            );

            glm::ivec2 minCell
            (
                (minDist.x + m_cellOffset.x) >> m_maxPoints,
                (minDist.y + m_cellOffset.y) >> m_maxPoints
            );

            glm::ivec2 maxCell
            (
                std::min(1 + ((maxDist.x + m_cellOffset.x) >> m_maxPoints), m_cellCount.x),
                std::min(1 + ((maxDist.y + m_cellOffset.y) >> m_maxPoints), m_cellCount.y)
            );

            for (auto y = minCell.y; y < maxCell.y; ++y)
            {
                for (auto x = minCell.x; x < maxCell.x; ++x)
                {
                    for (const auto& cell : m_cells[y * m_cellCount.x + x])
                    {
                        if (glm::length2(point - cell) < radSqr)
                        {
                            return true;
                        }
                    }
                }
            }

            return false;
        }

    private:
        using Cell = std::vector<glm::vec2>;
        std::vector<Cell> m_cells;
        glm::ivec2 m_cellCount;
        glm::ivec2 m_cellOffset;
        FloatRect m_area;
        std::size_t m_maxPoints;
        std::size_t m_cellSize;
    };
}

std::vector<glm::vec2> cro::Util::Random::poissonDiscDistribution(const FloatRect& area, float minDist, std::size_t maxPoints)
{
    std::vector<glm::vec2> workingPoints;
    std::vector<glm::vec2> retVal;

    Grid grid(area, maxGridPoints);

    auto centre = (glm::vec2(area.width, area.height) / 2.f) + glm::vec2(area.left, area.bottom);
    workingPoints.push_back(centre);
    retVal.push_back(centre);
    grid.addPoint(centre);

    while (!workingPoints.empty())
    {
        std::size_t idx = (workingPoints.size() == 1) ? 0 : value(0, workingPoints.size() - 1);
        centre = workingPoints[idx];

        workingPoints.erase(std::begin(workingPoints) + idx);

        for (auto i = 0u; i < maxPoints; ++i)
        {
            float radius = minDist * (1.f + value(0.f, 1.f));
            float angle = value(-1.f, 1.f) * cro::Util::Const::PI;
            glm::vec2 newPoint = centre + glm::vec2(std::sin(angle), std::cos(angle)) * radius;

            if (area.contains(newPoint) && !grid.hasNeighbour(newPoint, minDist))
            {
                workingPoints.push_back(newPoint);
                retVal.push_back(newPoint);
                grid.addPoint(newPoint);
            }
        }
    }

    return retVal;
}