#include <crogine/util/Spline.hpp>
#include <crogine/detail/Assert.hpp>

using namespace cro::Util::Maths;

Spline::Spline()
: m_dt(0.f)
{
}

//public
void Spline::addPoint(const glm::vec3& v)
{
    m_points.push_back(v);
    m_dt = 1.f / static_cast<float>(m_points.size());
}

glm::vec3 Spline::getInterpolatedPoint(float t)
{
    //find out in which interval we are on the spline
    std::int32_t p = static_cast<std::int32_t>(t / m_dt);

    //compute local control point indices
    std::int32_t p0 = std::max(0, std::min(p - 1, static_cast<std::int32_t>(m_points.size() - 1)));
    std::int32_t p1 = std::max(0, std::min(p, static_cast<std::int32_t>(m_points.size() - 1)));
    std::int32_t p2 = std::max(0, std::min(p + 1, static_cast<std::int32_t>(m_points.size() - 1)));
    std::int32_t p3 = std::max(0, std::min(p + 2, static_cast<std::int32_t>(m_points.size() - 1)));
    
    //relative (local) time 
    float lt = (t - m_dt * static_cast<float>(p)) / m_dt;
    
    //interpolate
    return eq(lt, m_points[p0], m_points[p1], m_points[p2], m_points[p3]);
}

std::size_t Spline::getPointCount() const
{
    return m_points.size();
}

glm::vec3 Spline::getPointAt(std::size_t idx) const
{
    CRO_ASSERT(idx < m_points.size(), "Out of bounds");
    return m_points[idx];
}

//private
glm::vec3 Spline::eq(float t, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, const glm::vec3& p4)
{
    float t2 = t * t;
    float t3 = t2 * t;

    float b1 = 0.5f * (-t3 + 2.f * t2 - t);
    float b2 = 0.5f * (3.f * t3 - 5.f * t2 + 2);
    float b3 = 0.5f * (-3.f * t3 + 4.f * t2 + t);
    float b4 = 0.5f * (t3 - t2);

    return (p1 * b1 + p2 * b2 + p3 * b3 + p4 * b4);
}