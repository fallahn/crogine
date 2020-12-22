/*-----------------------------------------------------------------------

Matt Marchant 2020
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

#include <crogine/detail/glm/vec2.hpp>

#include <vector>

class IslandGenerator final
{
public:
    IslandGenerator();

    void generate(std::int32_t seed = 1234);

    const std::vector<float>& getHeightmap() const { return m_heightmap; }

    const std::vector<glm::vec2>& getBushmap() const { return m_bushmap; }

    const std::vector<glm::vec2>& getTreemap() const { return m_treemap; }

private:
    std::vector<float> m_heightmap;
    std::vector<glm::vec2> m_bushmap;
    std::vector<glm::vec2> m_treemap;

    void createHeightmap(std::int32_t seed);
    void createFoliageMaps(std::int32_t seed);
};
