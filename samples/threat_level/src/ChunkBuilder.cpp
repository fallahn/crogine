/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2025
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#include "ChunkBuilder.hpp"
#include "TerrainChunk.hpp"
#include "ErrorCheck.hpp"


//private
cro::Mesh::Data ChunkBuilder::build() const
{
    cro::Mesh::Data data;

    data.attributes[cro::Mesh::Attribute::Position].componentCount = 2;
    data.attributes[cro::Mesh::Attribute::Colour].componentCount = 3;
    data.attributeFlags |= (cro::VertexProperty::Position | cro::VertexProperty::Colour);

    data.primitiveType = GL_TRIANGLE_STRIP;
    data.submeshCount = 2;
    data.vertexCount = TerrainChunk::PointCount * 2;
    data.vertexSize = (data.attributes[cro::Mesh::Attribute::Position].componentCount 
        + data.attributes[cro::Mesh::Attribute::Colour].componentCount 
        + data.attributes[cro::Mesh::Attribute::Normal].componentCount) * sizeof(float);
    //fill with empty data so we don't accidentally render any garbage
    std::vector<char> vertData(data.vertexCount * data.vertexSize);
    glCheck(glGenBuffers(1, &data.vboAllocation.vboID));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, data.vboAllocation.vboID));
    glCheck(glBufferData(GL_ARRAY_BUFFER, data.vertexCount * data.vertexSize, vertData.data(), GL_DYNAMIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
    
    //index array for top and bottom parts
    //these should remain the same only the positions get updated
    constexpr std::size_t indexCount = TerrainChunk::PointCount;
    data.indexData[0].format = GL_UNSIGNED_SHORT;
    data.indexData[0].indexCount = indexCount;
    data.indexData[0].primitiveType = data.primitiveType;

    std::array<std::uint16_t, indexCount> indices;
    for (auto i = 0u; i < indexCount; ++i)
    {
        indices[i] = i;
    }
    glCheck(glGenBuffers(1, &data.indexData[0].ibo));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.indexData[0].ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(std::uint16_t), indices.data(), GL_DYNAMIC_DRAW));
    
    data.indexData[1].format = GL_UNSIGNED_SHORT;
    data.indexData[1].indexCount = indexCount;
    data.indexData[1].primitiveType = data.primitiveType;

    for (auto i = 0u; i < indexCount; ++i)
    {
        indices[i] += indexCount;
    }
    glCheck(glGenBuffers(1, &data.indexData[1].ibo));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.indexData[1].ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(std::uint16_t), indices.data(), GL_DYNAMIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    return data;
}