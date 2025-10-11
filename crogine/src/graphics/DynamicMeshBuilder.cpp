/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2025
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

#include <crogine/graphics/DynamicMeshBuilder.hpp>

#include "../detail/GLCheck.hpp"

using namespace cro;

DynamicMeshBuilder::DynamicMeshBuilder(std::uint32_t flags, std::uint8_t submeshCount, std::uint32_t primitiveType)
    : m_flags       (flags),
    m_submeshCount  (submeshCount),
    m_primitiveType (primitiveType)
{
    CRO_ASSERT((flags & VertexProperty::Position) != 0, "must specify at least a position attribute");
    CRO_ASSERT(submeshCount > 0, "must request at least one submesh");
}

//private
Mesh::Data DynamicMeshBuilder::build() const
{
    Mesh::Data meshData;
    meshData.attributeFlags = m_flags;

    meshData.attributes[Mesh::Attribute::Position].componentCount = 3;
    if (m_flags & VertexProperty::Colour)
    {
        meshData.attributes[Mesh::Attribute::Colour].componentCount = 4;
    }

    if (m_flags & VertexProperty::Normal)
    {
        meshData.attributes[Mesh::Attribute::Normal].componentCount = 3;
    }

    if (m_flags & (VertexProperty::Tangent | VertexProperty::Bitangent))
    {
        meshData.attributes[Mesh::Attribute::Tangent].componentCount = 3;
        meshData.attributes[Mesh::Attribute::Bitangent].componentCount = 3;
    }

    if (m_flags & VertexProperty::UV0)
    {
        meshData.attributes[Mesh::Attribute::UV0].componentCount = 2;
    }
    if (m_flags & VertexProperty::UV1)
    {
        meshData.attributes[Mesh::Attribute::UV1].componentCount = 2;
    }

    if (m_flags & VertexProperty::BlendIndices)
    {
        meshData.attributes[Mesh::Attribute::BlendIndices].componentCount = 4;
    }
    if (m_flags & VertexProperty::BlendWeights)
    {
        meshData.attributes[Mesh::Attribute::BlendWeights].componentCount = 4;
    }

    meshData.primitiveType = m_primitiveType;
    meshData.vertexSize = getVertexSize(meshData.attributes);
    meshData.vertexCount = 0;

    //create vbo
    glCheck(glGenBuffers(1, &meshData.vboAllocation.vboID));

    meshData.submeshCount = m_submeshCount;
    for (auto i = 0; i < m_submeshCount; ++i)
    {
        meshData.indexData[i].format = GL_UNSIGNED_INT; //TODO we could make this an option when constructing
        meshData.indexData[i].primitiveType = meshData.primitiveType;
        meshData.indexData[i].indexCount = 0;

        //create IBO
        glCheck(glGenBuffers(1, &meshData.indexData[i].ibo));
    }

    return meshData;
}