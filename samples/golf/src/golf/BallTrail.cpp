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

#include "BallTrail.hpp"
#include "CommandIDs.hpp"
#include "Terrain.hpp"

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
}

BallTrail::BallTrail()
    : m_meshData    (nullptr),
    m_baseColour    (1.f),
    m_front         (0),
    m_active        (false)
{

}

//public
void BallTrail::create(cro::Scene& scene, cro::ResourceCollection& resources, std::int32_t materialID)
{
    auto meshID = resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_LINE_STRIP));

    auto material = resources.materials.get(materialID);
    material.enableDepthTest = false;
    material.blendMode = cro::Material::BlendMode::Additive;
    auto meshData = resources.meshes.getMesh(meshID);
    meshData.boundingBox = { glm::vec3(0.f), glm::vec3(320.f, 100.f, -200.f) };
    meshData.boundingSphere = meshData.boundingBox;

    auto entity = scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Model>(meshData, material);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::BeaconColour;
    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap));

    m_meshData = &entity.getComponent<cro::Model>().getMeshData();
}

void BallTrail::addPoint(glm::vec3 position)
{
    m_vertexData.emplace_back(position, m_baseColour);

    m_indices.push_back(static_cast<std::uint32_t>(m_indices.size()));

    if (!m_active && m_indices.size() > 120)
    {
        m_active = true;
    }
}

void BallTrail::update()
{
    if (m_active)
    {
        m_front++;
    }

    if (!m_vertexData.empty())
    {
        if (m_front >= m_indices.size())
        {
            m_front = 0;
            m_vertexData.clear();
            m_indices.clear();
            m_active = false;

            auto* submesh = &m_meshData->indexData[0];
            submesh->indexCount = m_indices.size() - m_front;
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
            glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW));
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
        }

        else
        {
            //TODO we could sub buffer this and only add what's new
            m_meshData->vertexCount = m_indices.size();
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_meshData->vbo));
            glCheck(glBufferData(GL_ARRAY_BUFFER, m_meshData->vertexSize * m_meshData->vertexCount, m_vertexData.data(), GL_DYNAMIC_DRAW));
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

            auto* submesh = &m_meshData->indexData[0];
            submesh->indexCount = m_indices.size() - m_front;
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
            glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), m_indices.data() + m_front, GL_DYNAMIC_DRAW));
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
        }
    }
}

void BallTrail::setUseBeaconColour(bool use)
{
    if (use)
    {
        m_baseColour = glm::vec4(0.8f, 0.f, 0.8f, 1.f);
    }
    else
    {
        m_baseColour = glm::vec4(0.08f);
    }
}