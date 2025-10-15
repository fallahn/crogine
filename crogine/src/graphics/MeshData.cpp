/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
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

#include <crogine/detail/glm/gtc/packing.hpp>
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
    /*
    As this func unpacks the vert data to float we return the size of the unpacked vertex in bytes
    */
    template <typename T>
    std::uint32_t read(const Data& meshData, std::vector<float>& destVerts, std::vector<std::vector<T>>& destIndices)
    {
        static_assert(std::is_same<T, std::uint8_t>::value
            || std::is_same<T, std::uint16_t>::value
            || std::is_same<T, std::uint32_t>::value, "must be uint8, uint16 or uint32");

        //TODO this currently undoes any vertex optimisation to return all-float vertex
        //data. This is for backwards compat - but really it should be up to the caller
        //of this func to decide if it should unpack everything into floats.

        std::vector<std::uint8_t> byteData(meshData.vertexCount * meshData.vertexSize);

        glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData.vboAllocation.bufferID));
        glCheck(glGetBufferSubData(GL_ARRAY_BUFFER, meshData.vboAllocation.offset, meshData.vertexCount * meshData.vertexSize, byteData.data()));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

        destVerts.clear();
        //destVerts.resize(meshData.vertexCount * (meshData.vertexSize / sizeof(float)));
        for (auto i = 0u; i < meshData.vertexCount; ++i)
        {
            const auto idx = i * meshData.vertexSize;
            for (auto j = 0u; j < Attribute::Total; ++j)
            {
                if ((meshData.attributeFlags & (1 << j)) != 0)
                {
                    switch (meshData.attributes[j].glType)
                    {
                    default:break;
                    case GL_UNSIGNED_BYTE:
                        //hmm we should really check which attribute we're unpacking here
                        //though for now we'll assume it's colour
                    {
                        std::uint32_t c = 0;
                        std::memcpy(&c, &byteData[idx + meshData.attributes[j].byteOffset], sizeof(c));
                        const auto v = glm::unpackUint4x8(c);
                        for (auto k = 0; k < 4; ++k)
                        {
                            destVerts.push_back(static_cast<float>(v[k]) / 255.f);
                        }
                    }
                        break;
                    case GL_BYTE:
                        
                        break;
                    case GL_UNSIGNED_SHORT:

                        break;
                    case GL_SHORT:

                        break;
                    case GL_HALF_FLOAT:

                        break;
                    case GL_UNSIGNED_INT:

                        break;
                    case GL_INT:

                        break;
                    case GL_UNSIGNED_INT_10_10_10_2:

                        break;
                    case GL_UNSIGNED_INT_2_10_10_10_REV:

                        break;
                    case GL_FLOAT:
                    {
                        std::vector<float> temp(meshData.attributes[j].componentCount);
                        std::memcpy(temp.data(), &byteData[idx + meshData.attributes[j].byteOffset], meshData.attributes[j].componentCount * sizeof(float));
                        destVerts.insert(destVerts.end(), temp.begin(), temp.end());
                    }
                        break;
                    }
                }
            }
        }

        destIndices.clear();
        destIndices.resize(meshData.submeshCount);

        for (auto i = 0u; i < meshData.submeshCount; ++i)
        {
            destIndices[i].resize(meshData.indexData[i].indexCount);
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[i].iboAllocation.bufferID));
            glCheck(glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[i].iboAllocation.offset, meshData.indexData[i].indexCount * sizeof(T), destIndices[i].data()));
        }
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
        
        std::uint32_t unpackedSize = 0;
        for (auto i = 0u; i < Attribute::Total; ++i)
        {
            if ((meshData.attributeFlags & (1 << i)) != 0)
            {
                switch (i)
                {
                default: break;
                case Attribute::Position:
                case Attribute::Normal:
                case Attribute::Tangent:
                case Attribute::Bitangent: 
                    unpackedSize += 3 * sizeof(float);
                    break;
                case Attribute::UV0:
                case Attribute::UV1: 
                    unpackedSize += 2 * sizeof(float);
                    break;
                case Attribute::Colour:
                case Attribute::BlendIndices:
                case Attribute::BlendWeights:
                    unpackedSize += 4 * sizeof(float);
                    break;
                }
            }
        }

        return unpackedSize;
    }
}

std::uint32_t Attribute::getSize() const
{
    switch (glType)
    {
    default:
        LogW << "getVertexSize(): " << glType << " - unhandled data type" << std::endl;
        return 0;
    case GL_UNSIGNED_BYTE:
    case GL_BYTE:
        return componentCount;
    case GL_UNSIGNED_SHORT:
    case GL_SHORT:
    case GL_HALF_FLOAT:
        return componentCount * 2;
    case GL_UNSIGNED_INT:
    case GL_INT:
    case GL_UNSIGNED_INT_10_10_10_2:
    case GL_UNSIGNED_INT_2_10_10_10_REV:
    case GL_FLOAT:
        return componentCount * 4;
    }
}

//NOTE!!! The size of the vertex in the returned data may be LARGER than stated in the Data struct
//as all vertex attributes are currently unpacked to float!!! So we return the unpacked size (in bytes) here
std::uint32_t cro::Mesh::readVertexData(const Data& meshData, std::vector<float>& destVerts, std::vector<std::vector<std::uint8_t>>& destIndices)
{
    return read(meshData, destVerts, destIndices);
}
//NOTE!!! The size of the vertex in the returned data may be LARGER than stated in the Data struct
//as all vertex attributes are currently unpacked to float!!! So we return the unpacked size (in bytes) here
std::uint32_t cro::Mesh::readVertexData(const Data& meshData, std::vector<float>& destVerts, std::vector<std::vector<std::uint16_t>>& destIndices)
{
    return read(meshData, destVerts, destIndices);
}
//NOTE!!! The size of the vertex in the returned data may be LARGER than stated in the Data struct
//as all vertex attributes are currently unpacked to float!!! So we return the unpacked size (in bytes) here
std::uint32_t cro::Mesh::readVertexData(const Data& meshData, std::vector<float>& destVerts, std::vector<std::vector<std::uint32_t>>& destIndices)
{
    return read(meshData, destVerts, destIndices);
}