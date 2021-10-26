/*-----------------------------------------------------------------------

Matt Marchant 2021
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


This is based on the work of Bruno Opsenica
https://bruop.github.io/improved_frustum_culling/

-----------------------------------------------------------------------*/

#pragma once

#include <crogine/Config.hpp>
#include <crogine/graphics/Spatial.hpp>
#include <crogine/detail/glm/mat4x4.hpp>

namespace cro
{
    /*!
    \brief Contains the Frustum data required for testing.
    Use Camera::getFrustumData() to get this from any given Camera component
    */
    struct CRO_EXPORT_API FrustumData final
    {
        float nearRight = 0.f;
        float nearTop = 0.f;
        float nearPlane = 0.f;
        float farPlane = 0.f;
    };

    namespace Util::Frustum
    {
        /*!
        \brief Test an AABB against a frustum for visibility
        \param data FrustumData of the Camera to test against (retrieved with Camera::getFrustumData())
        \param viewSpaceTransform The AABBs world transform multiplied with the Camera's viewMatrix
        \param aabb The axis aligned bounding box, in local space, to test.

        The given AABB is converted to an OBB in view space, via the given transform, and then tested
        using SAT for intersection with the frustum. See https://bruop.github.io/improved_frustum_culling/
        */

        bool CRO_EXPORT_API visible(FrustumData data, const glm::mat4& viewSpaceTransform, Box aabb);
    }
}