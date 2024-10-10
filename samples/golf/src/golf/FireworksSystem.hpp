/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#include <crogine/ecs/System.hpp>
#include <crogine/graphics/MeshData.hpp>
#include <crogine/graphics/MaterialData.hpp>
#include <crogine/detail/glm/vec3.hpp>
#include <crogine/core/Clock.hpp>

#include <vector>

struct Firework final
{
    float progress = 0.f;
    float prevProgress = -1.f;
    float maxRadius = 1.f;

    cro::Material::Property* progressProperty = nullptr;
};

class FireworksSystem final : public cro::System
{
public:
    FireworksSystem(cro::MessageBus&, const cro::Mesh::Data&, const cro::Material::Data&);

    void process(float) override;

    void addPosition(glm::vec3 p) { m_positions.push_back(p); }

private:
    const cro::Mesh::Data& m_meshData;
    const cro::Material::Data m_materialData;

    std::vector<glm::vec3> m_positions;
    std::size_t m_spawnIndex;
    std::size_t m_spawnCount;

    cro::Clock m_spawnClock;
    float m_spawnTime;
};