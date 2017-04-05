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

#ifndef CRO_PLANE_HPP_
#define CRO_PLANE_HPP_

#include <crogine/Config.hpp>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <array>

namespace cro
{
    using Plane = glm::vec4;
    using Box = std::array<glm::vec3, 2u>;
    struct CRO_EXPORT_API Sphere final
    {
        float radius = 0.f;
        glm::vec3 centre;
    };

    namespace Spatial
    {
        float CRO_EXPORT_API distance(Plane plane, glm::vec3 point);
        
        bool CRO_EXPORT_API intersects(Plane plane, Sphere sphere);
        bool CRO_EXPORT_API intersects(Plane plane, Box box);

    }
}

#endif //CRO_PLANE_HPP_