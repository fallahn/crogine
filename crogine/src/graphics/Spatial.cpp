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

#include <crogine/graphics/Spatial.hpp>

#include <crogine/detail/glm/vec3.hpp>
#include <crogine/detail/glm/geometric.hpp>

using namespace cro;

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
    glm::vec3 centre = (box[0] + box[1]) / 2.f;
    const float dist = distance(plane, centre);

    glm::vec3 extents = (box[1] - box[0]) / 2.f;
    extents *= glm::vec3(plane.x, plane.y, plane.z);

    if (std::abs(dist) <= (std::abs(extents.x) + std::abs(extents.y) + std::abs(extents.z)))
    {
        return Planar::Intersection;
    }
    return (dist > 0) ? Planar::Front : Planar::Back;
}