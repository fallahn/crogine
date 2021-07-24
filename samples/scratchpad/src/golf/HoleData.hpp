/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#pragma once

#include <crogine/ecs/Entity.hpp>
#include <crogine/graphics/Image.hpp>

#include <crogine/detail/glm/vec3.hpp>

struct HoleData final
{
    glm::vec3 tee = glm::vec3(0.f);
    glm::vec3 pin = glm::vec3(0.f);
    std::int32_t par = 0;
    cro::Image map;
    std::vector<glm::vec3> normalMap;
    cro::Entity modelEntity;
};
