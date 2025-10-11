/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2021
http://trederia.blogspot.com

crogine editor - Zlib license.

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

#include "NormalVisMeshBuilder.hpp"

#include <crogine/detail/OpenGL.hpp>

NormalVisMeshBuilder::NormalVisMeshBuilder(cro::Mesh::Data data, const std::vector<float>& vbo)
    : m_sourceData(data), m_sourceBuffer(vbo){}

cro::Mesh::Data NormalVisMeshBuilder::build() const
{
    auto vertexSize = m_sourceData.vertexSize / sizeof(float);

    std::size_t normalOffset = 0;
    std::size_t tanOffset = 0;
    std::size_t bitanOffset = 0;

    //calculate the offset index into a single vertex which points
    //to any normal / tan / bitan values
    if (m_sourceData.attributes[cro::Mesh::Attribute::Normal].size != 0)
    {
        for (auto i = 0; i < cro::Mesh::Attribute::Normal; ++i)
        {
            normalOffset += m_sourceData.attributes[i].size;
        }
    }

    if (m_sourceData.attributes[cro::Mesh::Attribute::Tangent].size != 0)
    {
        for (auto i = 0; i < cro::Mesh::Attribute::Tangent; ++i)
        {
            tanOffset += m_sourceData.attributes[i].size;
        }
    }

    if (m_sourceData.attributes[cro::Mesh::Attribute::Bitangent].size != 0)
    {
        for (auto i = 0; i < cro::Mesh::Attribute::Bitangent; ++i)
        {
            bitanOffset += m_sourceData.attributes[i].size;
        }
    }


    std::vector<float> vboData;
    auto addVertex = [&](glm::vec3 position, glm::vec3 colour)
    {
        vboData.push_back(position.x);
        vboData.push_back(position.y);
        vboData.push_back(position.z);

        vboData.push_back(colour.r);
        vboData.push_back(colour.g);
        vboData.push_back(colour.b);
    };

    auto getPosition = [&](std::size_t idx)->glm::vec3
    {
        return glm::vec3(m_sourceBuffer[idx], m_sourceBuffer[idx + 1], m_sourceBuffer[idx + 2]);
    };

    const float visScale = 0.3f;
    for (std::size_t i = 0; i < m_sourceBuffer.size(); i += vertexSize)
    {
        auto vertPos = getPosition(i);
        
        auto idx = normalOffset + i;
        auto normalPos = vertPos + (getPosition(idx) * visScale);

        addVertex(vertPos, glm::vec3(1.f, 0.f, 0.f));
        addVertex(normalPos, glm::vec3(1.f, 0.f, 0.f));

        if (tanOffset != 0)
        {
            idx = tanOffset + i;
            auto tanPos = vertPos + (getPosition(idx) * visScale);

            addVertex(vertPos, glm::vec3(0.f, 1.f, 0.f));
            addVertex(tanPos, glm::vec3(0.f, 1.f, 0.f));
        }

        if (bitanOffset != 0)
        {
            idx = bitanOffset + i;
            auto bitanPos = vertPos + (getPosition(idx) * visScale);

            addVertex(vertPos, glm::vec3(0.f, 0.f, 1.f));
            addVertex(bitanPos, glm::vec3(0.f, 0.f, 1.f));
        }
    }

    cro::Mesh::Data meshData;
    meshData.attributes[cro::Mesh::Attribute::Position].size = 3;
    meshData.attributes[cro::Mesh::Attribute::Colour].size = 3;
    meshData.attributeFlags = cro::VertexProperty::Position | cro::VertexProperty::Colour;
    meshData.primitiveType = GL_LINES;
    meshData.vertexSize = getVertexSize(meshData.attributes);
    meshData.vertexCount = vboData.size() / (meshData.vertexSize / sizeof(float));
    createVBO(meshData, vboData);

    std::vector<std::uint16_t> indices;
    for (auto i = 0u; i < meshData.vertexCount; ++i)
    {
        indices.push_back(i);
    }
    meshData.submeshCount = 1;
    meshData.indexData[0].format = GL_UNSIGNED_SHORT;
    meshData.indexData[0].primitiveType = GL_LINES;
    meshData.indexData[0].indexCount = static_cast<std::uint32_t>(indices.size());
    createIBO(meshData, indices.data(), 0, sizeof(std::uint16_t));

    meshData.boundingBox[0] = glm::vec3(1.f);
    meshData.boundingBox[1] = glm::vec3(-1.f);
    
    auto rad = 0.6f;
    meshData.boundingSphere.centre= glm::vec3(rad);
    meshData.boundingSphere.radius = rad;

    return meshData;
}