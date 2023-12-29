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

#include <crogine/graphics/BoundingBox.hpp>

#include <crogine/detail/glm/vec4.hpp>

#include <array>

namespace cro
{
    using Plane = glm::vec4;
    using Frustum = std::array<Plane, 6u>;
    struct CRO_EXPORT_API Sphere final
    {
        float radius = 0.f;
        glm::vec3 centre = glm::vec3(0.f);

        Sphere();
        Sphere(float radius, glm::vec3 centre = glm::vec3(0.f));
        explicit Sphere(const Box&);
        Sphere& operator = (const Box&);

        bool contains(glm::vec3);
    };

    enum class Planar
    {
        Intersection, Front, Back
    };

    namespace Spatial
    {
        /*!
        \brief Returns the distance between a plane and a point
        */
        float CRO_EXPORT_API distance(Plane plane, glm::vec3 point);
        /*!
        \brief Checks for planar intersection with a sphere.
        \returns Planar::Front if the sphere is in front of the plane (positive to plane normal),
        Planar::Intersects when intersecting or Planar::Back if it behind the plane.
        */
        Planar CRO_EXPORT_API intersects(Plane plane, Sphere sphere);
        /*!
        \brief Checks for planar intersection with a bounding box.
        \returns Planar::Front if the box is on the positive side (pointed to by plane
        normal), Planar::Intersects when intersecting or Planar::Back
        */
        Planar CRO_EXPORT_API intersects(Plane plane, Box box);

        /*!
        \brief Updates the given frustum based on the given viewProjection matrix
        \param frustum An array of 6 Planes which make up the frustum
        \param viewProj A matrix containing the view-projection transform to apply to the frustum
        \return cro::Box containing the AABB of the newly updated frustum
        */
        cro::Box CRO_EXPORT_API updateFrustum(std::array<Plane, 6u>& frustum, glm::mat4 viewProj);
    }
}