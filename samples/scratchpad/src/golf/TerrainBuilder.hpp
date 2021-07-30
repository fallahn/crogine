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

#include "HoleData.hpp"

#include <crogine/gui/GuiClient.hpp>
#include <crogine/ecs/Entity.hpp>
#include <crogine/ecs/components/BillboardCollection.hpp>

#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <array>

namespace cro
{
    struct ResourceCollection;
    class Scene;
    class Image;
}

class TerrainBuilder final : public cro::GuiClient
{
public:
    explicit TerrainBuilder(const std::vector<HoleData>&);
    ~TerrainBuilder();

    TerrainBuilder(const TerrainBuilder&) = delete;
    TerrainBuilder(TerrainBuilder&&) = delete;
    TerrainBuilder& operator = (const TerrainBuilder&) = delete;
    TerrainBuilder& operator = (TerrainBuilder&&) = delete;


    void create(cro::ResourceCollection&, cro::Scene&); //initial creation

    void update(std::size_t); //loads the configured data into the existing scene and signals the thread to queue upcoming data

private:

    const std::vector<HoleData>& m_holeData;
    std::size_t m_currentHole;

    struct BillboardID final
    {
        enum
        {
            Grass01,
            Pine,
            Birch,
            Willow,

            Count
        };
    };
    std::array<cro::Billboard, BillboardID::Count> m_billboardTemplates = {};

    std::vector<cro::Billboard> m_billboardBuffer;
    std::array<cro::Entity, 2u> m_billboardEntities = {};

    struct Vertex final
    {
        glm::vec3 position = glm::vec3(0.f);
        glm::vec4 colour = glm::vec4(1.f);
        glm::vec3 normal = glm::vec3(0.f, 1.f, 0.f);

        //these actually get attached to tan/bitan attribs in the shader
        //but we'll use them as morph targets
        glm::vec3 targetPosition = glm::vec3(0.f);
        glm::vec3 targetNormal = glm::vec3(0.f, 1.f, 0.f);
    };
    std::vector<Vertex> m_terrainBuffer;
    std::uint32_t m_terrainVBO;

    std::atomic_bool m_threadRunning;
    std::atomic_bool m_wantsUpdate;
    std::unique_ptr<std::thread> m_thread;

    void threadFunc();
    std::pair<std::uint8_t, float> readMap(const cro::Image&, float x, float y) const;
};