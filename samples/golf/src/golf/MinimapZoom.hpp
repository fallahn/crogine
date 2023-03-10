/*-----------------------------------------------------------------------

Matt Marchant 2023
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/glm/mat4x4.hpp>

#include <cstdint>

struct MinimapZoom final
{
    glm::vec2 pan = glm::vec2(0.f);
    float tilt = 0.f;
    float zoom = 1.f;

    glm::mat4 invTx = glm::mat4(1.f);
    std::uint32_t shaderID = 0u;
    std::int32_t matrixUniformID = -1;
    std::int32_t featherUniformID = -1;

    glm::vec2 mapScale = glm::vec2(0.f); //this is the size of the texture used in relation to world map, ie pixels per metre
    glm::vec2 textureSize = glm::vec2(1.f);

    void updateShader();
    glm::vec2 toMapCoords(glm::vec3 worldPos) const;
};
