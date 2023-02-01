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

#include "TerrainChunks.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/gui/Gui.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Matrix.hpp>

namespace
{
    const cro::FloatRect MapSize(0.f, 0.f, 320.f, 200.f);
    const glm::vec2 ChunkSize(MapSize.width / TerrainChunker::ChunkCountX, MapSize.height / TerrainChunker::ChunkCountY);

    bool intersects(glm::vec2 camPos, cro::FloatRect cell, float radius)
    {
        glm::vec2 closest(
            std::clamp(camPos.x, cell.left, cell.left + cell.width),
            std::clamp(camPos.y, cell.bottom, cell.bottom + cell.height));

        glm::vec2 distance = camPos - closest;

        float distanceSquared = glm::dot(distance, distance);
        return distanceSquared < (radius * radius);
    }
}

TerrainChunker::TerrainChunker(const cro::Scene& scene)
    : m_scene   (scene),
    m_chunks    (ChunkCountX * ChunkCountY)
{
    for (auto& lod : m_models)
    {
        std::fill(lod.begin(), lod.end(), nullptr);
    }

#ifdef CRO_DEBUG_
    m_debugTexture.create(320, 200, false);
    m_debugQuadTexture.create(1, 1);
    std::array<std::uint8_t, 4u> c = { 255,255,255,255 };
    m_debugQuadTexture.update(c.data());
    m_debugQuad.setTexture(m_debugQuadTexture);
    m_debugQuad.setTextureRect({ 0.f, 0.f, ChunkSize.x, ChunkSize.y });
    m_debugQuad.setBlendMode(cro::Material::BlendMode::Alpha);

    m_debugCamera.setVertexData(
        {
            cro::Vertex2D(glm::vec2(-30.f, 80.f), cro::Colour::Cyan),
            cro::Vertex2D(glm::vec2(0.f), cro::Colour::Red),
            cro::Vertex2D(glm::vec2(30.f, 80.f), cro::Colour::Cyan)        
        });
    registerWindow([&]()
        {
            if (ImGui::Begin("Terrain Chunks"))
            {
                ImGui::Image(m_debugTexture.getTexture(), { MapSize.width, MapSize.height }, { 0.f, 1.f }, { 1.f, 0.f });
                //ImGui::Text("Draw Count %u", m_visible.size());
            }
            ImGui::End();
        });
#endif
}

//public
void TerrainChunker::update()
{
    //0------
    //
    //
    //------1
    auto camBox = m_scene.getActiveCamera().getComponent<cro::Camera>().getActivePass().getAABB();
    camBox[0].x = std::clamp(camBox[0].x, 0.f, MapSize.width);
    camBox[1].x = std::clamp(camBox[1].x, 0.f, MapSize.width);
    camBox[0].z = std::clamp(camBox[0].z, -MapSize.height, 0.f);
    camBox[1].z = std::clamp(camBox[1].z, -MapSize.height, 0.f);
    cro::FloatRect camAABB(camBox[0].x, -camBox[1].z, camBox[1].x - camBox[0].x, -(camBox[0].z - camBox[1].z));

    auto camWorldPos = m_scene.getActiveCamera().getComponent<cro::Transform>().getWorldPosition();
    auto camPos = glm::vec2(camWorldPos.x, -camWorldPos.z);

    std::uint32_t previousFlags = 0u;

    m_previouslyVisible.swap(m_visible);
    for (auto chunkIndex : m_previouslyVisible)
    {
        //update visibility
#ifdef CRO_DEBUG_
        m_chunks[chunkIndex].lod0 = 0;
        m_chunks[chunkIndex].lod1 = 0;
#endif // CRO_DEBUG_

        m_chunks[chunkIndex].lodData[0].visible = false;
        m_chunks[chunkIndex].lodData[1].visible = false;

        previousFlags |= (1 << chunkIndex);
    }

    m_visible.clear();
    //create x/y coords of intersection
    std::int32_t startX = std::clamp(static_cast<std::int32_t>(camAABB.left / ChunkSize.x), 0, ChunkCountX - 1);
    std::int32_t countX = std::clamp(static_cast<std::int32_t>(camAABB.width / ChunkSize.x) + 1, 1, ChunkCountX);

    std::int32_t startY = std::clamp(static_cast<std::int32_t>(camAABB.bottom / ChunkSize.y), 0, ChunkCountY - 1);
    std::int32_t countY = std::clamp(static_cast<std::int32_t>(camAABB.height / ChunkSize.y) + 1, 1, ChunkCountY);

    std::uint32_t currentFlags = 0u;
    for (auto y = startY; y < startY + countY; ++y)
    {
        for (auto x = startX; x < startX + countX; ++x)
        {
            auto idx = y * ChunkCountX + x;

            if (m_chunks[idx].itemCount != 0)
            {
                //update visibility
                cro::FloatRect cell({ x * ChunkSize.x, y * ChunkSize.y, ChunkSize.x, ChunkSize.y });
                
                if (intersects(camPos, cell, 30.f))
                {
#ifdef CRO_DEBUG_
                    m_chunks[idx].lod0 = 255;
#endif
                    m_chunks[idx].lodData[0].visible = true;
                }
                //if (!intersects(camPos, cell, 20.f))
                else
                {
#ifdef CRO_DEBUG_
                    m_chunks[idx].lod1 = 255;
#endif
                    m_chunks[idx].lodData[1].visible = true;
                }


                //and insert into visible list
                m_visible.push_back(idx);
                currentFlags |= (1 << idx);
            }
        }
    }

    if (previousFlags != currentFlags)
    {
        std::array<std::array<std::uint32_t, 4u>, 2u> counts;
        std::fill(counts[0].begin(), counts[0].end(), 0);
        std::fill(counts[1].begin(), counts[1].end(), 0);

        const auto updateLOD = [&](const TerrainChunk& chunk, std::int32_t idx)
        {
            auto& instanceCounts = counts[idx];

            if (chunk.lodData[idx].visible)
            {
                for (auto i = 0u; i < m_models[idx].size(); ++i)
                {
                    if (m_models[idx][i])
                    {
                        const auto& lodData = chunk.lodData[idx].models[i];
                        if (!lodData.transforms.empty())
                        {
                            m_models[idx][i]->updateInstanceTransforms(lodData.transforms, lodData.normalMatrices, instanceCounts[i]);
                            instanceCounts[i] += static_cast<std::uint32_t>(lodData.transforms.size());
                        }

                        m_models[0][i]->setInstanceCount(instanceCounts[i]);
                    }
                }
            }
        };


        //only update VBO if needed
        //hmmmmm this is very not optimal.
        for (auto idx : m_visible)
        {
            updateLOD(m_chunks[idx], 0);
            updateLOD(m_chunks[idx], 1);
        }
    }


#ifdef CRO_DEBUG_
    auto forward = cro::Util::Matrix::getForwardVector(m_scene.getActiveCamera().getComponent<cro::Transform>().getWorldTransform());
    //float camRot = glm::eulerAngles(m_scene.getActiveCamera().getComponent<cro::Transform>().getWorldRotation()).y;

    m_debugTexture.clear(cro::Colour::Magenta);
    for (auto y = 0u; y < ChunkCountY; ++y)
    {
        for (auto x = 0; x < ChunkCountX; ++x)
        {
            auto idx = y * ChunkCountX + x;

            m_debugQuad.setPosition({ x * ChunkSize.x, y * ChunkSize.y });
            m_debugQuad.setColour(cro::Colour(m_chunks[idx].lod0, m_chunks[idx].lod1, 0));
            m_debugQuad.draw();
        }
    }

    //m_debugQuad.setPosition({ camAABB.left, camAABB.bottom });
    //m_debugQuad.setColour(cro::Colour(1.f, 0.f, 1.f, 0.4f));
    //m_debugQuad.setTextureRect({ 0.f, 0.f, camAABB.width, camAABB.height });
    //m_debugQuad.draw();
    //m_debugQuad.setTextureRect({ 0.f, 0.f, ChunkSize.x, ChunkSize.y });

    //m_debugCamera.setPosition(camPos);
    ////buh why did I use degrees for this??
    //m_debugCamera.setRotation(camRot * cro::Util::Const::radToDeg);
    //m_debugCamera.draw();

    m_debugTexture.display();
#endif
}

void TerrainChunker::setChunks(std::vector<TerrainChunk>& chunks)
{
    CRO_ASSERT(chunks.size() == ChunkCountX * ChunkCountY, "");
    m_chunks.swap(chunks);

    //TODO we might need to force an update here if the visibility flags don't change
}

void TerrainChunker::addModel(cro::Model* model, std::uint32_t lod, std::uint32_t idx)
{
    m_models[lod][idx] = model;
}

//private