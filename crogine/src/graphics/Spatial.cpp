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

#include <crogine/core/Log.hpp>

#include <crogine/graphics/Spatial.hpp>

#include <crogine/detail/glm/vec3.hpp>
#include <crogine/detail/glm/geometric.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

using namespace cro;

Sphere::Sphere()
    : radius(0.f),
    centre  (0.f) {}

Sphere::Sphere(float r, glm::vec3 c)
    : radius(r),
    centre  (c) {}

Sphere::Sphere(const Box& box)
{
    auto rad = (box[1] - box[0]) / 2.f;
    centre = box[0] + rad;

    radius = 0.f;
    for (auto i = 0; i < 3; ++i)
    {
        auto l = std::abs(rad[i]);
        if (l > radius)
        {
            radius = l;
        }
    }

    //LogI << glm::length(rad) / radius << std::endl;
}

Sphere& Sphere::operator=(const Box& box)
{
    auto rad = (box[1] - box[0]) / 2.f;
    centre = box[0] + rad;

    radius = 0.f;
    for (auto i = 0; i < 3; ++i)
    {
        auto l = std::abs(rad[i]);
        if (l > radius)
        {
            radius = l;
        }
    }

    //LogI << glm::length(rad) / radius << std::endl;

    return *this;
}

bool Sphere::contains(glm::vec3 point)
{
    auto pointLen = glm::length2(point - centre);
    return pointLen < (radius * radius);
}

namespace
{
    //used to calculate the AABB for a frustum.
    constexpr std::array ClipPoints =
    {
        //far
        glm::vec4(-1.f, -1.f, -1.f, 1.f),
        glm::vec4(1.f, -1.f, -1.f, 1.f),
        glm::vec4(1.f,  1.f, -1.f, 1.f),
        glm::vec4(-1.f,  1.f, -1.f, 1.f),
        //near
        glm::vec4(-1.f, -1.f, 1.f, 1.f),
        glm::vec4(1.f, -1.f, 1.f, 1.f),
        glm::vec4(1.f,  1.f, 1.f, 1.f),
        glm::vec4(-1.f,  1.f, 1.f, 1.f)
    };
}

float Spatial::distance(Plane plane, glm::vec3 point)
{
    return glm::dot({ plane.x, plane.y, plane.z }, point) + plane.w;
}

Planar Spatial::intersects(Plane plane, Sphere sphere)
{
    const float dist = distance(plane, sphere.centre);
    
    if (std::abs(dist) <= sphere.radius)
    {
        return Planar::Intersection;
    }
    else if (dist > 0.f)
    {
        return Planar::Front;
    }
    return Planar::Back;
}

Planar Spatial::intersects(Plane plane, Box box)
{
    glm::vec3 extents = (box[1] - box[0]) / 2.f;
    glm::vec3 centre = box[0] + extents;

    extents *= glm::vec3(plane.x, plane.y, plane.z);
    const float dist = distance(plane, centre);

    if (std::abs(dist) <= (std::abs(extents.x) + std::abs(extents.y) + std::abs(extents.z)))
    {
        return Planar::Intersection;
    }
    return (dist > 0) ? Planar::Front : Planar::Back;
}

Box Spatial::updateFrustum(std::array<Plane, 6u>& frustum, glm::mat4 viewProj)
{
    frustum =
    {
        { Plane //left
        (
            viewProj[0][3] + viewProj[0][0],
            viewProj[1][3] + viewProj[1][0],
            viewProj[2][3] + viewProj[2][0],
            viewProj[3][3] + viewProj[3][0]
        ),
        Plane //right
        (
            viewProj[0][3] - viewProj[0][0],
            viewProj[1][3] - viewProj[1][0],
            viewProj[2][3] - viewProj[2][0],
            viewProj[3][3] - viewProj[3][0]
        ),
        Plane //bottom
        (
            viewProj[0][3] + viewProj[0][1],
            viewProj[1][3] + viewProj[1][1],
            viewProj[2][3] + viewProj[2][1],
            viewProj[3][3] + viewProj[3][1]
        ),
        Plane //top
        (
            viewProj[0][3] - viewProj[0][1],
            viewProj[1][3] - viewProj[1][1],
            viewProj[2][3] - viewProj[2][1],
            viewProj[3][3] - viewProj[3][1]
        ),
        Plane //near
        (
            viewProj[0][3] + viewProj[0][2],
            viewProj[1][3] + viewProj[1][2],
            viewProj[2][3] + viewProj[2][2],
            viewProj[3][3] + viewProj[3][2]
        ),
        Plane //far
        (
            viewProj[0][3] - viewProj[0][2],
            viewProj[1][3] - viewProj[1][2],
            viewProj[2][3] - viewProj[2][2],
            viewProj[3][3] - viewProj[3][2]
        ) }
    };

    //normalise the planes
    for (auto& p : frustum)
    {
        const float factor = 1.f / std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
        p.x *= factor;
        p.y *= factor;
        p.z *= factor;
        p.w *= factor;
    }

    //calc the AABB
    auto invViewProjection = glm::inverse(viewProj);
    std::array points =
    {
        invViewProjection * ClipPoints[0],
        invViewProjection * ClipPoints[1],
        invViewProjection * ClipPoints[2],
        invViewProjection * ClipPoints[3],
        invViewProjection * ClipPoints[4],
        invViewProjection * ClipPoints[5],
        invViewProjection * ClipPoints[6],
        invViewProjection * ClipPoints[7]
    };
    for (auto& p : points)
    {
        p /= p.w;
    }

    auto [minX, maxX] = std::minmax_element(points.begin(), points.end(),
        [](const glm::vec4& lhs, const glm::vec4& rhs)
        {
            return lhs.x < rhs.x;
        });

    auto [minY, maxY] = std::minmax_element(points.begin(), points.end(),
        [](const glm::vec4& lhs, const glm::vec4& rhs)
        {
            return lhs.y < rhs.y;
        });

    auto [minZ, maxZ] = std::minmax_element(points.begin(), points.end(),
        [](const glm::vec4& lhs, const glm::vec4& rhs)
        {
            return lhs.z < rhs.z;
        });

    return { glm::vec3(minX->x, minY->y, minZ->z), glm::vec3(maxX->x, maxY->y, maxZ->z) };
}