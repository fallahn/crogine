/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#include <crogine/graphics/MeshData.hpp>

#include "../detail/GLCheck.hpp"

#include <type_traits>

/*
OK So this is basically esoteric template specialisation
but complicated include order has forced my hand.
*/

using namespace cro::Mesh;

namespace
{
    template <typename T>
    void read(const Data& meshData, std::vector<float>& destVerts, std::vector<std::vector<T>>& destIndices)
    {
        static_assert(std::is_same<T, std::uint8_t>::value
            || std::is_same<T, std::uint16_t>::value
            || std::is_same<T, std::uint32_t>::value, "must be uint8, uint16 or uint32");

        destVerts.clear();
        destVerts.resize(meshData.vertexCount * (meshData.vertexSize / sizeof(float)));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData.vboAllocation.vboID));
        glCheck(glGetBufferSubData(GL_ARRAY_BUFFER, meshData.vboAllocation.offset, meshData.vertexCount * meshData.vertexSize, destVerts.data()));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

        destIndices.clear();
        destIndices.resize(meshData.submeshCount);

        for (auto i = 0u; i < meshData.submeshCount; ++i)
        {
            destIndices[i].resize(meshData.indexData[i].indexCount);
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[i].ibo));
            glCheck(glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, meshData.indexData[i].indexCount * sizeof(T), destIndices[i].data()));
        }
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
    }
}


void cro::Mesh::readVertexData(const Data& meshData, std::vector<float>& destVerts, std::vector<std::vector<std::uint8_t>>& destIndices)
{
    read(meshData, destVerts, destIndices);
}

void cro::Mesh::readVertexData(const Data& meshData, std::vector<float>& destVerts, std::vector<std::vector<std::uint16_t>>& destIndices)
{
    read(meshData, destVerts, destIndices);
}

void cro::Mesh::readVertexData(const Data& meshData, std::vector<float>& destVerts, std::vector<std::vector<std::uint32_t>>& destIndices)
{
    read(meshData, destVerts, destIndices);
}