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

#include <crogine/graphics/MeshBuilder.hpp>

#include "../detail/GLCheck.hpp"

using namespace cro;

std::size_t MeshBuilder::getAttributeSize(const std::array<Mesh::Attribute, Mesh::Attribute::Total>& attrib)
{
    std::size_t size = 0;
    for (const auto& a : attrib)
    {
        size += a.size;
    }
    return size;
}

std::size_t MeshBuilder::getVertexSize(const std::array<Mesh::Attribute, Mesh::Attribute::Total>& attrib)
{
    std::size_t total = 0;
    for (const auto& a : attrib)
    {
        total += a.sizeInBytes();
    }

    return total;
}

void MeshBuilder::createVBO(Mesh::Data& meshData, const std::vector<float>& vertexData)
{
    glCheck(glGenBuffers(1, &meshData.vboAllocation.vboID));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData.vboAllocation.vboID));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData.vertexSize * meshData.vertexCount, vertexData.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
}

void MeshBuilder::createIBO(Mesh::Data& meshData, const void* idxData, std::size_t idx, std::int32_t dataSize)
{
    glCheck(glGenBuffers(1, &meshData.indexData[idx].ibo));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[idx].ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[idx].indexCount * dataSize, idxData, GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
}