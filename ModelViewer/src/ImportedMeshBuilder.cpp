/*-----------------------------------------------------------------------

Matt Marchant 2020
http://trederia.blogspot.com

crogine model viewer/importer - Zlib license.

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

#include "ImportedMeshBuilder.hpp"

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/geometric.hpp>

#include <crogine/graphics/StaticMeshBuilder.hpp>

ImportedMeshBuilder::ImportedMeshBuilder(const CMFHeader& header, const std::vector<float>& vertexData, const std::vector<std::vector<std::uint32_t>>& indices, std::uint8_t flags)
    : m_header      (header),
    m_vboData       (vertexData),
    m_indexArrays   (indices),
    m_flags         (flags)
{

}

//private
cro::Mesh::Data ImportedMeshBuilder::build() const
{
    cro::Mesh::Data meshData;
    if (m_flags & cro::VertexProperty::Position) meshData.attributes[cro::Mesh::Position] = 3;
    if (m_flags & cro::VertexProperty::Colour) meshData.attributes[cro::Mesh::Colour] = 3;
    if (m_flags & cro::VertexProperty::Normal) meshData.attributes[cro::Mesh::Normal] = 3;
    if (m_flags & cro::VertexProperty::Tangent) meshData.attributes[cro::Mesh::Tangent] = 3;
    if (m_flags & cro::VertexProperty::Bitangent) meshData.attributes[cro::Mesh::Bitangent] = 3;
    if (m_flags & cro::VertexProperty::UV0) meshData.attributes[cro::Mesh::UV0] = 2;
    if (m_flags & cro::VertexProperty::UV1) meshData.attributes[cro::Mesh::UV1] = 2;

    meshData.primitiveType = GL_TRIANGLES;
    meshData.vertexSize = getVertexSize(meshData.attributes);
    meshData.vertexCount = m_vboData.size() / (meshData.vertexSize / sizeof(float));
    createVBO(meshData, m_vboData);

    meshData.submeshCount = m_indexArrays.size();
    for (auto i = 0u; i < m_indexArrays.size(); ++i)
    {
        CRO_ASSERT(i < 32, "Max Arrays already!");
        meshData.indexData[i].format = GL_UNSIGNED_INT;
        meshData.indexData[i].primitiveType = meshData.primitiveType;
        meshData.indexData[i].indexCount = static_cast<std::uint32_t>(m_indexArrays[i].size());

        createIBO(meshData, m_indexArrays[i].data(), i, sizeof(std::uint32_t));
    }

    //bounding box
    meshData.boundingBox[0] = glm::vec3(std::numeric_limits<float>::max());
    meshData.boundingBox[1] = glm::vec3(-std::numeric_limits<float>::max());
    for (std::size_t i = 0; i < m_vboData.size(); i += meshData.vertexSize)
    {
        //min point
        if (meshData.boundingBox[0].x > m_vboData[i])
        {
            meshData.boundingBox[0].x = m_vboData[i];
        }
        if (meshData.boundingBox[0].y > m_vboData[i + 1])
        {
            meshData.boundingBox[0].y = m_vboData[i + 1];
        }
        if (meshData.boundingBox[0].z > m_vboData[i + 2])
        {
            meshData.boundingBox[0].z = m_vboData[i + 2];
        }

        //maxpoint
        if (meshData.boundingBox[1].x < m_vboData[i])
        {
            meshData.boundingBox[1].x = m_vboData[i];
        }
        if (meshData.boundingBox[1].y < m_vboData[i + 1])
        {
            meshData.boundingBox[1].y = m_vboData[i + 1];
        }
        if (meshData.boundingBox[1].z < m_vboData[i + 2])
        {
            meshData.boundingBox[1].z = m_vboData[i + 2];
        }
    }

    //bounding sphere
    auto rad = (meshData.boundingBox[1] - meshData.boundingBox[0]) / 2.f;
    meshData.boundingSphere.centre = meshData.boundingBox[0] + rad;
    meshData.boundingSphere.radius = glm::length(rad);

    return meshData;
}