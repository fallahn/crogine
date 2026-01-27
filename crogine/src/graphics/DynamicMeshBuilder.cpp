/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2026
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

#include <crogine/detail/AllocationResource.hpp>
#include <crogine/graphics/DynamicMeshBuilder.hpp>

#include "../detail/GLCheck.hpp"

using namespace cro;

namespace
{
    template<typename T>
    void setIndexDataForT(Mesh::Data& dst, const std::vector<DataArray<T>>& indexData)
    {
        //CRO_ASSERT(dst.iboAllocator, "");
        for (auto i = 0u; i < dst.submeshCount; ++i)
        {
            /*if (dst.indexData[i].iboAllocation.blockCount != 0)
            {
                dst.iboAllocator->freeAllocation(dst.indexData[i].iboAllocation);
            }

            dst.indexData[i].iboAllocation = dst.iboAllocator->newAllocation(indexData[i].size);*/

            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dst.indexData[i].iboAllocation.bufferID));
            //glCheck(glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, dst.indexData[i].iboAllocation.offset, indexData[i].size * sizeof(T), indexData[i].data));
            glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexData[i].size * sizeof(T), indexData[i].data, GL_DYNAMIC_DRAW));
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
        }
    }
}

DynamicMeshBuilder::DynamicMeshBuilder(std::uint32_t flags, std::uint8_t submeshCount, std::uint32_t primitiveType, std::uint32_t indexFormat)
    : m_flags       (flags),
    m_submeshCount  (submeshCount),
    m_primitiveType (primitiveType),
    m_indexFormat   (indexFormat)
{
    CRO_ASSERT((flags & VertexProperty::Position) != 0, "must specify at least a position attribute");
    CRO_ASSERT(submeshCount > 0, "must request at least one submesh");
    CRO_ASSERT(indexFormat == GL_UNSIGNED_BYTE
        || indexFormat == GL_UNSIGNED_SHORT
        || indexFormat == GL_UNSIGNED_INT, "Invalid index format");
}

//private
Mesh::Data DynamicMeshBuilder::build(AllocationResource* allocationResource) const
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
    CRO_ASSERT(allocationResource, "");
    glCheck(glGenBuffers(1, &meshData.vboAllocation.bufferID));
    //meshData.vboAllocator = allocationResource->getVBOAllocator(4, meshData.vertexSize);
    //meshData.vboAllocation = meshData.vboAllocator->newAllocation(meshData.vertexCount);

    /*std::uint32_t iboDataSize = sizeof(std::uint32_t);
    switch (m_indexFormat)
    {
    default: break;
    case GL_UNSIGNED_BYTE:
        iboDataSize = sizeof(std::uint8_t);
        break;
    case GL_UNSIGNED_SHORT:
        iboDataSize = sizeof(std::uint16_t);
        break;
    }*/

    meshData.submeshCount = m_submeshCount;
    for (auto i = 0; i < m_submeshCount; ++i)
    {
        meshData.indexData[i].format = m_indexFormat;
        meshData.indexData[i].primitiveType = meshData.primitiveType;
        meshData.indexData[i].indexCount = 0;

        //create IBO
        glCheck(glGenBuffers(1, &meshData.indexData[i].iboAllocation.bufferID));
        //meshData.iboAllocator = allocationResource->getIBOAllocator(3 * iboDataSize, iboDataSize);
        //meshData.indexData[i].iboAllocation = meshData.iboAllocator->newAllocation(0);
    }

    return meshData;
}

void DynamicMeshBuilder::setIndexData(Mesh::Data& dst, const std::vector<DataArray<std::uint8_t>>& indexData)
{
    setIndexDataForT(dst, indexData);
}
void DynamicMeshBuilder::setIndexData(Mesh::Data& dst, const std::vector<DataArray<std::uint16_t>>& indexData)
{
    setIndexDataForT(dst, indexData);
}
void DynamicMeshBuilder::setIndexData(Mesh::Data& dst, const std::vector<DataArray<std::uint32_t>>& indexData)
{
    setIndexDataForT(dst, indexData);
}

//private
void DynamicMeshBuilder::setVertexData(Mesh::Data& dst, /*const DataArray<std::uint8_t>& vertData*/const std::uint8_t* data, std::size_t size)
{
    //CRO_ASSERT(dst.vertexCount != 0, "");
    //CRO_ASSERT(dst.vertexCount <= (vertData.size() / (dst.vertexSize / sizeof(float))), "");
    //CRO_ASSERT(dst.vboAllocator, "Missing allocator");
    //TODO assert the mesh data was actually returned from a DynamicMeshBuilder

    //if (dst.vboAllocation.blockCount != 0)
    //{
    //    dst.vboAllocator->freeAllocation(dst.vboAllocation);
    //}

    //dst.vboAllocation = dst.vboAllocator->newAllocation(dst.vertexCount);
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, dst.vboAllocation.bufferID));
    //glCheck(glBufferSubData(GL_ARRAY_BUFFER, dst.vboAllocation.offset, vertData.size * sizeof(float), vertData.data));
    glCheck(glBufferData(GL_ARRAY_BUFFER, size, data, GL_DYNAMIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
}