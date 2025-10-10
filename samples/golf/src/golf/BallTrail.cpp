/*-----------------------------------------------------------------------

Matt Marchant 2023 - 2024
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

#include "BallTrail.hpp"
#include "CommandIDs.hpp"
#include "Terrain.hpp"
#include "GameConsts.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/Scene.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>

#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/DynamicMeshBuilder.hpp>

#include "../ErrorCheck.hpp"

namespace
{
    const std::uint32_t VertexSize = 7; //num floats

    //std::size_t insertedIndex = 0;
    //std::size_t insertCount = 0;

    //std::vector<std::uint32_t> debugPoints;
}

BallTrail::BallTrail()
    : m_bufferIndex (0),
    m_baseColour    (0.f)/*,
    m_insertTime    (0.f),
    m_updateTime    (0.f)*/
{
    //registerWindow([&]() 
    //    {
    //        if (ImGui::Begin("Ball Trail"))
    //        {
    //            ImGui::Text("Buffer Index %lu", m_bufferIndex);

    //            std::string s = m_trails[0].active ? "active" : "inactive";
    //            ImGui::Text("Trail 0 %s", s.c_str());

    //            s = m_trails[1].active ? "active" : "inactive";
    //            ImGui::Text("Trail 1 %s", s.c_str());

    //            ImGui::Text("Update Time %1.8f", m_updateTime);
    //            ImGui::Text("Insert Time %1.8f", m_insertTime);

    //            ImGui::Text("Inserted Count %lu", insertCount);
    //            ImGui::Text("Inserted Index %lu", insertedIndex);

    //            for (auto v : debugPoints)
    //            {
    //                ImGui::Text("%u", v);
    //            }
    //        }
    //        ImGui::End();
    //    });
}

//public
void BallTrail::create(cro::Scene& scene, cro::ResourceCollection& resources, std::int32_t materialID, bool courseSize)
{
    auto material = resources.materials.get(materialID);
    material.enableDepthTest = false;
    material.blendMode = cro::Material::BlendMode::Additive;
    material.setProperty("u_colour", cro::Colour::White);
    

    cro::Box boundingBox;
    if (courseSize)
    {
        glm::vec2 mapSize(MapSize);
        boundingBox = { glm::vec3(0.f), glm::vec3(mapSize.x, 100.f, -mapSize.y) };
    }
    else
    {
        //this is the driving range
        glm::vec2 rangeSize(RangeSize);
        boundingBox = { glm::vec3(-rangeSize.x, 100.f, rangeSize.y), glm::vec3(rangeSize.x, 100.f, -rangeSize.y) };
    }

    for (auto i = 0u; i < BufferCount; ++i)
    {
        auto meshID = resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_LINE_STRIP));
        auto meshData = resources.meshes.getMesh(meshID);
        meshData.boundingBox = boundingBox;
        meshData.boundingSphere = meshData.boundingBox;

        auto entity = scene.createEntity();
        entity.addComponent<cro::Transform>();
        entity.addComponent<cro::Model>(meshData, material);
        entity.addComponent<cro::CommandTarget>().ID = CommandID::BeaconColour;
        entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap | RenderFlags::FlightCam));

        //entity.addComponent<cro::Callback>().active = true;
        //entity.getComponent<cro::Callback>().setUserData<float>(0.f);
        //entity.getComponent<cro::Callback>().function =
        //    [](cro::Entity e, float dt)
        //    {
        //        auto& r = e.getComponent<cro::Callback>().getUserData<float>();
        //        r += dt / 3.f;

        //        e.getComponent<cro::Model>().setMaterialProperty(0, "u_colourRotation", r);
        //    };

        m_trails[i].meshData = &entity.getComponent<cro::Model>().getMeshData();
    }
}

void BallTrail::setNext()
{
    //if we switched before reaching min-index size we need
    //to activate the old buffer so that it tidies itself up
    m_trails[m_bufferIndex].active = !m_trails[m_bufferIndex].vertexData.empty();

    m_bufferIndex = (m_bufferIndex + 1) % BufferCount;
}

void BallTrail::addPoint(glm::vec3 position, std::uint32_t callerIndex)
{
    m_trails[m_bufferIndex].vertexData.emplace_back(position, m_baseColour);

    m_trails[m_bufferIndex].indices.push_back(static_cast<std::uint32_t>(m_trails[m_bufferIndex].indices.size()));

    if (m_trails[m_bufferIndex].indices.size() > 120)
    {
        m_trails[m_bufferIndex].active = true;

        //TODO track parent entity and set to shown
    }

    //insertedIndex = m_bufferIndex;
    //insertCount++;
    //debugPoints.push_back(callerIndex);
    //m_insertTime = m_insertTimer.restart();
}

void BallTrail::update()
{
    for (auto& trail : m_trails)
    {
        if (trail.active)
        {
            trail.front++;
        }

        if (!trail.vertexData.empty())
        {
            if (trail.front >= trail.indices.size())
            {
                trail.front = 0;
                trail.vertexData.clear();
                trail.indices.clear();
                trail.active = false;

                auto* submesh = &trail.meshData->indexData[0];
                submesh->indexCount = static_cast<std::uint32_t>(trail.indices.size() - trail.front);
                glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
                glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW));
                glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

                //TODO track parent entity and set to hidden.
            }

            else
            {
                //fade trail tip
                static constexpr std::size_t FadeLength = 60;
                auto start = trail.front;
                auto end = std::min(trail.front + FadeLength, trail.vertexData.size() - 1);
                for (auto i = 0u; i < (end - start); ++i)
                {
                    float a = static_cast<float>(i) / (end - start);
                    trail.vertexData[start + i].c = m_baseColour * a;
                }


                //TODO we could sub buffer this and only add what's new
                trail.meshData->vertexCount = trail.indices.size();
                glCheck(glBindBuffer(GL_ARRAY_BUFFER, trail.meshData->vboAllocation.vboID));
                glCheck(glBufferData(GL_ARRAY_BUFFER, trail.meshData->vertexSize * trail.meshData->vertexCount, trail.vertexData.data(), GL_DYNAMIC_DRAW));
                glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

                auto* submesh = &trail.meshData->indexData[0];
                submesh->indexCount = static_cast<std::uint32_t>(trail.indices.size() - trail.front);
                glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
                glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), trail.indices.data() + trail.front, GL_DYNAMIC_DRAW));
                glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
            }
        }
    }
    //insertCount = 0;
    //debugPoints.clear();
    //m_updateTime = m_updateTimer.restart();
}

void BallTrail::reset()
{
    //forces the buffer to clear on next update because it thinks it's at the end
    m_trails[m_bufferIndex].front = m_trails[m_bufferIndex].vertexData.size() + 2;
}

void BallTrail::setUseBeaconColour(bool use)
{
    if (use)
    {
        m_baseColour = glm::vec4(0.2f, 0.f, 0.2f, 1.f);
    }
    else
    {
        m_baseColour = glm::vec4(0.08f);
    }
}