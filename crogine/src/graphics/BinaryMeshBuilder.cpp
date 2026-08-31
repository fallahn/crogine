/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2026
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

#include <crogine/core/FileSystem.hpp>
#include <crogine/detail/AllocationResource.hpp>
#include <crogine/detail/glm/gtc/type_ptr.hpp>
#include <crogine/graphics/BinaryMeshBuilder.hpp>
#include <crogine/util/Maths.hpp>

#include "../detail/GLCheck.hpp"

using namespace cro;

BinaryMeshBuilder::BinaryMeshBuilder(const std::string& path, bool optimiseOnLoad)
    : m_path            (path),
    m_optimiseOnLoad    (optimiseOnLoad),
    m_uid               (0)
{
    FS_ASSERT; //convert path to std::filesystem
#ifdef __APPLE__
    if (!FileSystem::fileExists(m_path))
    {
        m_path = (cro::FileSystem::getResourcePath() / path).string();
    }

#endif

    if (FileSystem::fileExists(m_path))
    {
        //calc a UID from the file path
        std::hash<std::string> hashAttack;
        m_uid = hashAttack(path);
    }
    else
    {
        LogE << "Could not find model binary " << m_path << std::endl;
        m_path = "";
    }
}

//public
std::size_t BinaryMeshBuilder::getUID() const
{
    return m_uid;
}

Skeleton BinaryMeshBuilder::getSkeleton() const
{
    return m_skeleton;
}

Mesh::Data BinaryMeshBuilder::build(AllocationResource* allocationResource) const
{
    if (m_optimiseOnLoad)
    {
        return buildOptimised(allocationResource);
    }
    return buildDefault();
}

Mesh::Data BinaryMeshBuilder::buildOptimised(AllocationResource* allocationResource) const
{
    FS_ASSERT;

    Mesh::Data meshData;

    RaiiRWops file;
    file.open(m_path, "rb");
    if (file)    
    {
        Detail::ModelBinary::Header header;
        auto len = SDL_SeekIO(file.filePtr(), 0, SDL_IO_SEEK_END);
        if (len < sizeof(header))
        {
            LogE << "Unable to open " << m_path << ": invalid file size" << std::endl;
            return {};
        }

        SDL_SeekIO(file.filePtr(), 0, SDL_IO_SEEK_SET);
        SDL_ReadIO(file.filePtr(), &header, sizeof(header));

        if (header.magic != Detail::ModelBinary::MAGIC
            && header.magic != Detail::ModelBinary::MAGIC_V1)
        {
            LogE << "Invalid header found" << std::endl;
            return {};
        }

        if (header.meshOffset)
        {
            Detail::ModelBinary::MeshHeader meshHeader;
            SDL_ReadIO(file.filePtr(), &meshHeader, sizeof(meshHeader));

            if ((meshHeader.flags & VertexProperty::Position) == 0)
            {
                LogE << "No position data in mesh" << std::endl;
                return {};
            }

            std::vector<float> tempVerts;
            std::vector<std::uint32_t> sizes(meshHeader.indexArrayCount);
            std::vector<std::vector<std::uint32_t>> indexData(meshHeader.indexArrayCount);

            SDL_ReadIO(file.filePtr(), sizes.data(), meshHeader.indexArrayCount * sizeof(std::uint32_t));

            std::uint32_t vertStride = 0; //uncompressed stride
            std::uint32_t byteOffset = 0;
            for (auto i = 0u; i < Mesh::Attribute::Total; ++i)
            {
                if (meshHeader.flags & (1 << i))
                {
                    switch (i)
                    {
                    default:
                    case Mesh::Attribute::Bitangent:
                        break;
                    case Mesh::Attribute::Position:
                        vertStride += 3;
                        meshData.attributes[i].componentCount = 3;
                        meshData.attributes[i].byteOffset = byteOffset;
                        byteOffset += 3 * sizeof(float);
                        break;
                    case Mesh::Attribute::Colour:
                        vertStride += 4;
                        meshData.attributes[i].componentCount = 4;
                        meshData.attributes[i].glNormalised = GL_TRUE;
                        meshData.attributes[i].glType = GL_UNSIGNED_BYTE;
                        meshData.attributes[i].byteOffset = byteOffset;
                        byteOffset += 4;
                        break;
                    case Mesh::Attribute::Normal:
                        vertStride += 3;
                        meshData.attributes[i].componentCount = 3;
                        meshData.attributes[i].byteOffset = byteOffset;
                        byteOffset += 3 * sizeof(float);
                        //meshData.attributes[i].glType = GL_HALF_FLOAT;// GL_INT_2_10_10_10_REV;
                        //byteOffset += 4 * sizeof(std::uint16_t);
                        break;
                    case Mesh::Attribute::Tangent:
                        meshData.attributes[i].componentCount = 3;
                        meshData.attributes[Mesh::Attribute::Bitangent].componentCount = 3;
                        vertStride += 4; //we'll be decoding tangents, so include sign float

                        meshData.attributes[i].byteOffset = byteOffset;
                        byteOffset += 6 * sizeof(float); //this is the final offset including bitan
                        break;
                    case Mesh::Attribute::UV0:
                    case Mesh::Attribute::UV1:
                        vertStride += 2;
                        meshData.attributes[i].componentCount = 2;
                        meshData.attributes[i].glType = GL_HALF_FLOAT;
                        meshData.attributes[i].byteOffset = byteOffset;
                        byteOffset += 2 * sizeof(std::uint16_t);
                        break;
                    case Mesh::Attribute::BlendIndices:
                        vertStride += 4;
                        meshData.attributes[i].componentCount = 4;
                        meshData.attributes[i].byteOffset = byteOffset;
                        meshData.attributes[i].glType = GL_UNSIGNED_BYTE;
                        byteOffset += 4;
                        break;
                    case Mesh::Attribute::BlendWeights:
                        vertStride += 4;
                        meshData.attributes[i].componentCount = 4;
                        meshData.attributes[i].byteOffset = byteOffset;
                        //byteOffset += 4 * sizeof(float);

                        meshData.attributes[i].glNormalised = GL_TRUE;
                        meshData.attributes[i].glType = GL_UNSIGNED_BYTE;
                        byteOffset += 4;

                        break;
                    }
                }
            }

            auto pos = SDL_TellIO(file.filePtr());
            auto vertSize = meshHeader.indexArrayOffset - pos;
            tempVerts.resize(vertSize / sizeof(float));
            SDL_ReadIO(file.filePtr(), tempVerts.data(), vertSize);
            CRO_ASSERT(tempVerts.size() % vertStride == 0, "");
            
            for (auto i = 0u; i < meshHeader.indexArrayCount; ++i)
            {
                indexData[i].resize(sizes[i]);
                SDL_ReadIO(file.filePtr(), indexData[i].data(), sizes[i] * sizeof(std::uint32_t));
            }

            std::vector<float> positions; //used to calculate bounds

            //process vertex data - we separate it out to optimise its
            //size first, then re-interleave. Should be stored in the model
            //file like this really, but we haven't updated the exporter yet :)

            //interleave the attributes
            std::vector<std::uint8_t> interleavedData;
            const auto insertData =
                [&](const void* data, std::size_t size)
                {
                    std::vector<std::uint8_t> temp(size);
                    std::memcpy(temp.data(), data, temp.size());
                    interleavedData.insert(interleavedData.end(), temp.begin(), temp.end());
                };
            for (auto i = 0u; i < tempVerts.size(); i += vertStride)
            {
                std::uint32_t offset = 0;
                glm::vec3 normal = glm::vec3(0.f);
                for (auto j = 0u; j < Mesh::Attribute::Total; ++j)
                {
                    if (meshHeader.flags & (1 << j))
                    {
                        switch (j)
                        {
                        default:
                        case Mesh::Attribute::Bitangent:
                            break;
                        case Mesh::Attribute::Position:
                        {
                            std::vector<float> p;
                            p.push_back(tempVerts[i + offset]);
                            p.push_back(tempVerts[i + offset + 1]);
                            p.push_back(tempVerts[i + offset + 2]);
                            insertData(p.data(), meshData.attributes[j].getSize());

                            positions.push_back(tempVerts[i + offset]);
                            positions.push_back(tempVerts[i + offset + 1]);
                            positions.push_back(tempVerts[i + offset + 2]);
                        }
                            offset += 3;
                            break;
                        case Mesh::Attribute::Colour:
                        {
                            const glm::vec4 colour =
                            {
                                tempVerts[i + offset],
                                tempVerts[i + offset + 1],
                                tempVerts[i + offset + 2],
                                tempVerts[i + offset + 3]
                            };
                            const auto c = glm::packUnorm4x8(colour);
                            insertData(&c, meshData.attributes[j].getSize());
                            offset += 4;
                        }
                            break;
                        case Mesh::Attribute::Normal:
                        {
                            normal =
                            {
                                tempVerts[i + offset],
                                tempVerts[i + offset + 1],
                                tempVerts[i + offset + 2],
                            };
                            insertData(&normal, meshData.attributes[j].getSize());

                            //hmm the 2x10x10x10 format seems to be a dark art no one wants
                            //to talk about anf half precision normals affect the height map too much
                            //const auto n = Detail::packInt2x10x10x10xRev(glm::vec4(normal, 0.f));
                            /*const std::array<std::uint32_t, 2u> n =
                            {
                                glm::packHalf2x16({normal.x, normal.y}),
                                glm::packHalf2x16({normal.z, 0.f})
                            };
                            insertData(&n, meshData.attributes[j].getSize());*/
                        }
                            offset += 3;
                            break;
                        case Mesh::Attribute::Tangent:
                        {
                            //TODO we should be sending this as a vec4
                            //and calculating bitan on the GPU
                            const glm::vec3 tangent =
                            {
                                tempVerts[i + offset],
                                tempVerts[i + offset + 1],
                                tempVerts[i + offset + 2]
                            };

                            const auto sign = (tempVerts[i + offset + 3]);
                            CRO_ASSERT(glm::length2(normal) != 0, "");

                            const auto bitan = glm::cross(normal, tangent) * sign;

                            insertData(&tangent, meshData.attributes[j].getSize());
                            insertData(&bitan, meshData.attributes[j].getSize());
                        }
                            offset += 4;
                            break;
                        case Mesh::Attribute::UV0:
                        case Mesh::Attribute::UV1:
                        {
                            const glm::vec2 uv =
                            {
                                tempVerts[i + offset],
                                tempVerts[i + offset + 1]
                            };

                            //const auto uvPacked = glm::packSnorm2x16(uv);
                            const auto uvPacked = glm::packHalf2x16(uv);
                            insertData(&uvPacked, meshData.attributes[j].getSize());
                        }
                            offset += 2;
                            break;
                        case Mesh::Attribute::BlendIndices:
                        {
                            std::vector<float> blendIndex;
                            blendIndex.push_back(tempVerts[i + offset]);
                            blendIndex.push_back(tempVerts[i + offset + 1]);
                            blendIndex.push_back(tempVerts[i + offset + 2]);
                            blendIndex.push_back(tempVerts[i + offset + 3]);

                            //ennnnnndianness
                            const std::uint32_t indices =
                                std::uint8_t(blendIndex[3]) << 24 |
                                std::uint8_t(blendIndex[2]) << 16 |
                                std::uint8_t(blendIndex[1]) << 8 |
                                std::uint8_t(blendIndex[0]);
                            insertData(&indices, meshData.attributes[j].getSize());
                        }
                            offset += 4;
                            break;
                        case Mesh::Attribute::BlendWeights:
                        {
                            const glm::vec4 weights =
                            {
                                tempVerts[i + offset],
                                tempVerts[i + offset + 1],
                                tempVerts[i + offset + 2],
                                tempVerts[i + offset + 3]
                            };
                            const auto c = glm::packUnorm4x8(weights);
                            insertData(&c, meshData.attributes[j].getSize());
                            offset += 4;
                        }
                            break;
                        }
                    }
                }
            }
            

            //set the vertex data
            meshData.attributeFlags = meshHeader.flags;
            meshData.primitiveType = GL_TRIANGLES;
            meshData.vertexSize = getVertexSize(meshData.attributes);
            meshData.vertexCount = interleavedData.size() / meshData.vertexSize;
            
            meshData.vboAllocator = allocationResource->getVBOAllocator(4, meshData.vertexSize);
            meshData.vboAllocation = meshData.vboAllocator->newAllocation(meshData.vertexCount);
            
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData.vboAllocation.bufferID));
            glCheck(glBufferSubData(GL_ARRAY_BUFFER, meshData.vboAllocation.offset, interleavedData.size(), interleavedData.data()));
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

            //generate the index arrays
            meshData.submeshCount = meshHeader.indexArrayCount;
            for (auto i = 0u; i < meshData.submeshCount; ++i)
            {
                meshData.indexData[i].format = GL_UNSIGNED_INT;
                meshData.indexData[i].primitiveType = meshData.primitiveType;
                meshData.indexData[i].indexCount = static_cast<std::uint32_t>(indexData[i].size());

                if (meshData.vertexCount < std::numeric_limits<std::uint8_t>::max())
                {
                    //we can use bytes for indexing
                    meshData.indexData[i].format = GL_UNSIGNED_BYTE;
                    std::vector<std::uint8_t> temp(indexData[i].size());
                    for (auto j = 0u; j < temp.size(); ++j)
                    {
                        temp[j] = indexData[i][j];
                    }

                    //this is a bit pointless doing it every submesh as it should always return the same one
                    meshData.iboAllocator = allocationResource->getIBOAllocator(3, sizeof(std::uint8_t));
                    meshData.indexData[i].iboAllocation = meshData.iboAllocator->newAllocation(indexData[i].size());
                    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[i].iboAllocation.bufferID));
                    glCheck(glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[i].iboAllocation.offset,
                        temp.size(), temp.data()));
                }
                else 
                    if (meshData.vertexCount < std::numeric_limits<std::uint16_t>::max())
                {
                    //use shorts
                    meshData.indexData[i].format = GL_UNSIGNED_SHORT;
                    std::vector<std::uint16_t> temp(indexData[i].size());
                    for (auto j = 0u; j < temp.size(); ++j)
                    {
                        temp[j] = indexData[i][j];
                    }
                    meshData.iboAllocator = allocationResource->getIBOAllocator(3, sizeof(std::uint16_t));
                    meshData.indexData[i].iboAllocation = meshData.iboAllocator->newAllocation(indexData[i].size());
                    
                    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[i].iboAllocation.bufferID));
                    glCheck(glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[i].iboAllocation.offset,
                        temp.size() * sizeof(std::uint16_t), temp.data()));
                }
                else
                {
                    meshData.iboAllocator = allocationResource->getIBOAllocator(3, sizeof(std::uint32_t));
                    meshData.indexData[i].iboAllocation = meshData.iboAllocator->newAllocation(indexData[i].size());
                    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[i].iboAllocation.bufferID));
                    glCheck(glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[i].iboAllocation.offset,
                        indexData[i].size() * sizeof(std::uint32_t), indexData[i].data()));
                }
            }
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

            //boundingbox / sphere
            calcBounds(meshData, positions);
        }

        m_skeleton = {};
        parseSkeleton(file, header);

        //LogI << FILE_LINE << "REVERT THIS" << std::endl;
    }
    else
    {
        LogE << "SDL: " << m_path << ": " << SDL_GetError() << std::endl;
    }

    return meshData;
}

Mesh::Data BinaryMeshBuilder::buildDefault() const
{
    FS_ASSERT;
    Mesh::Data meshData;

    RaiiRWops file;
    file.open(m_path, "rb");
    if (file)
    {
        Detail::ModelBinary::Header header;
        auto len = SDL_SeekIO(file.filePtr(), 0, SDL_IO_SEEK_END);
        if (len < sizeof(header))
        {
            LogE << "Unable to open " << m_path << ": invalid file size" << std::endl;
            return {};
        }

        SDL_SeekIO(file.filePtr(), 0, SDL_IO_SEEK_SET);
        SDL_ReadIO(file.filePtr(), &header, sizeof(header));

        if (header.magic != Detail::ModelBinary::MAGIC
            && header.magic != Detail::ModelBinary::MAGIC_V1)
        {
            LogE << "Invalid header found" << std::endl;
            return {};
        }

        if (header.meshOffset)
        {
            Detail::ModelBinary::MeshHeader meshHeader;
            SDL_ReadIO(file.filePtr(), &meshHeader, sizeof(meshHeader));

            if ((meshHeader.flags & VertexProperty::Position) == 0)
            {
                LogE << "No position data in mesh" << std::endl;
                return {};
            }

            std::vector<float> tempVerts;
            std::vector<std::uint32_t> sizes(meshHeader.indexArrayCount);
            std::vector<std::vector<std::uint32_t>> indexData(meshHeader.indexArrayCount);

            SDL_ReadIO(file.filePtr(), sizes.data(), meshHeader.indexArrayCount * sizeof(std::uint32_t));

            std::uint32_t vertStride = 0;
            for (auto i = 0u; i < Mesh::Attribute::Total; ++i)
            {
                if (meshHeader.flags & (1 << i))
                {
                    switch (i)
                    {
                    default:
                    case Mesh::Attribute::Bitangent:
                        break;
                    case Mesh::Attribute::Position:
                        vertStride += 3;
                        meshData.attributes[i].componentCount = 3;
                        break;
                    case Mesh::Attribute::Colour:
                        vertStride += 4;
                        meshData.attributes[i].componentCount = 4;
                        break;
                    case Mesh::Attribute::Normal:
                        vertStride += 3;
                        meshData.attributes[i].componentCount = 3;
                        break;
                    case Mesh::Attribute::Tangent:
                        meshData.attributes[i].componentCount = 3;
                        meshData.attributes[Mesh::Attribute::Bitangent].componentCount = 3;
                        vertStride += 4; //we'll be decoding tangents
                        break;
                    case Mesh::Attribute::UV0:
                    case Mesh::Attribute::UV1:
                        vertStride += 2;
                        meshData.attributes[i].componentCount = 2;
                        break;
                    case Mesh::Attribute::BlendIndices:
                    case Mesh::Attribute::BlendWeights:
                        vertStride += 4;
                        meshData.attributes[i].componentCount = 4;
                        break;
                    }
                }
            }

            auto pos = SDL_TellIO(file.filePtr());
            auto vertSize = meshHeader.indexArrayOffset - pos;
            tempVerts.resize(vertSize / sizeof(float));
            SDL_ReadIO(file.filePtr(), tempVerts.data(), vertSize);
            CRO_ASSERT(tempVerts.size() % vertStride == 0, "");

            for (auto i = 0u; i < meshHeader.indexArrayCount; ++i)
            {
                indexData[i].resize(sizes[i]);
                SDL_ReadIO(file.filePtr(), indexData[i].data(), sizes[i] * sizeof(std::uint32_t));
            }

            //process vertex data
            std::vector<float> positions; //used to calculate bounds
            std::vector<float> vertData;
            for (auto i = 0u; i < tempVerts.size(); i += vertStride)
            {
                std::uint32_t offset = 0;
                glm::vec3 normal = glm::vec3(0.f);
                for (auto j = 0u; j < Mesh::Attribute::Total; ++j)
                {
                    if (meshHeader.flags & (1 << j))
                    {
                        switch (j)
                        {
                        default:
                        case Mesh::Attribute::Bitangent:
                            break;
                        case Mesh::Attribute::Position:
                            vertData.push_back(tempVerts[i + offset]);
                            vertData.push_back(tempVerts[i + offset + 1]);
                            vertData.push_back(tempVerts[i + offset + 2]);

                            positions.push_back(tempVerts[i + offset]);
                            positions.push_back(tempVerts[i + offset + 1]);
                            positions.push_back(tempVerts[i + offset + 2]);

                            offset += 3;
                            break;
                        case Mesh::Attribute::Colour:
                            vertData.push_back(tempVerts[i + offset]);
                            vertData.push_back(tempVerts[i + offset + 1]);
                            vertData.push_back(tempVerts[i + offset + 2]);
                            vertData.push_back(tempVerts[i + offset + 3]);

                            offset += 4;
                            break;
                        case Mesh::Attribute::Normal:
                            vertData.push_back(tempVerts[i + offset]);
                            vertData.push_back(tempVerts[i + offset + 1]);
                            vertData.push_back(tempVerts[i + offset + 2]);

                            normal =
                            {
                                tempVerts[i + offset],
                                tempVerts[i + offset + 1],
                                tempVerts[i + offset + 2],
                            };

                            offset += 3;
                            break;
                        case Mesh::Attribute::Tangent:
                        {
                            glm::vec3 tan =
                            {
                                (tempVerts[i + offset]),
                                (tempVerts[i + offset + 1]),
                                (tempVerts[i + offset + 2])
                            };

                            auto sign = (tempVerts[i + offset + 3]);
                            CRO_ASSERT(glm::length2(normal) != 0, "");

                            auto bitan = glm::cross(normal, tan) * sign;

                            vertData.push_back(tan.x);
                            vertData.push_back(tan.y);
                            vertData.push_back(tan.z);

                            vertData.push_back(bitan.x);
                            vertData.push_back(bitan.y);
                            vertData.push_back(bitan.z);
                        }
                        offset += 4;
                        break;
                        case Mesh::Attribute::UV0:
                        case Mesh::Attribute::UV1:
                            vertData.push_back(tempVerts[i + offset]);
                            vertData.push_back(tempVerts[i + offset + 1]);

                            offset += 2;
                            break;
                        case Mesh::Attribute::BlendIndices:
                        case Mesh::Attribute::BlendWeights:
                            vertData.push_back(tempVerts[i + offset]);
                            vertData.push_back(tempVerts[i + offset + 1]);
                            vertData.push_back(tempVerts[i + offset + 2]);
                            vertData.push_back(tempVerts[i + offset + 3]);

                            offset += 4;
                            break;
                        }
                    }
                }
            }

            meshData.attributeFlags = meshHeader.flags;
            meshData.primitiveType = GL_TRIANGLES;
            meshData.vertexSize = getVertexSize(meshData.attributes);
            meshData.vertexCount = vertData.size() / (meshData.vertexSize / sizeof(float));

            createVBO(meshData, vertData);

            meshData.submeshCount = meshHeader.indexArrayCount;
            for (auto i = 0u; i < meshData.submeshCount; ++i)
            {
                meshData.indexData[i].format = GL_UNSIGNED_INT;
                meshData.indexData[i].primitiveType = meshData.primitiveType;
                meshData.indexData[i].indexCount = static_cast<std::uint32_t>(indexData[i].size());

                if (meshData.vertexCount < std::numeric_limits<std::uint8_t>::max())
                {
                    //LogI << "Optimising for byte size indices" << std::endl;

                    //we can use bytes for indexing
                    meshData.indexData[i].format = GL_UNSIGNED_BYTE;
                    std::vector<std::uint8_t> temp(indexData[i].size());
                    for (auto j = 0u; j < temp.size(); ++j)
                    {
                        temp[j] = indexData[i][j];
                    }

                    createIBO(meshData, temp.data(), i, sizeof(std::uint8_t));
                }
                else if (meshData.vertexCount < std::numeric_limits<std::uint16_t>::max())
                {
                    //use shorts
                    //LogI << "Optimising for short size indices" << std::endl;

                    meshData.indexData[i].format = GL_UNSIGNED_SHORT;
                    std::vector<std::uint16_t> temp(indexData[i].size());
                    for (auto j = 0u; j < temp.size(); ++j)
                    {
                        temp[j] = indexData[i][j];
                    }
                    createIBO(meshData, temp.data(), i, sizeof(std::uint16_t));
                }
                else
                {
                    //LogI << "Using default size indices" << std::endl;
                    createIBO(meshData, indexData[i].data(), i, sizeof(std::uint32_t));
                }
            }
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

            //boundingbox / sphere
            calcBounds(meshData, positions);
        }

        m_skeleton = {};
        parseSkeleton(file, header);
    }
    else
    {
        LogE << "SDL: " << m_path << ": " << SDL_GetError() << std::endl;
    }

    return meshData;
}

void BinaryMeshBuilder::calcBounds(Mesh::Data& meshData, const std::vector<float>& vertData) const
{
    meshData.boundingBox[0] = glm::vec3(std::numeric_limits<float>::max());
    meshData.boundingBox[1] = glm::vec3(std::numeric_limits<float>::lowest());
    for (std::size_t i = 0u; i < vertData.size(); i += 3u /*(meshData.vertexSize / sizeof(float))*/)
    {
        //min point
        if (meshData.boundingBox[0].x > vertData[i])
        {
            meshData.boundingBox[0].x = vertData[i];
        }
        if (meshData.boundingBox[0].y > vertData[i + 1])
        {
            meshData.boundingBox[0].y = vertData[i + 1];
        }
        if (meshData.boundingBox[0].z > vertData[i + 2])
        {
            meshData.boundingBox[0].z = vertData[i + 2];
        }

        //maxpoint
        if (meshData.boundingBox[1].x < vertData[i])
        {
            meshData.boundingBox[1].x = vertData[i];
        }
        if (meshData.boundingBox[1].y < vertData[i + 1])
        {
            meshData.boundingBox[1].y = vertData[i + 1];
        }
        if (meshData.boundingBox[1].z < vertData[i + 2])
        {
            meshData.boundingBox[1].z = vertData[i + 2];
        }
    }
    const auto rad = (meshData.boundingBox[1] - meshData.boundingBox[0]) / 2.f;
    meshData.boundingSphere.centre = meshData.boundingBox[0] + rad;
    //radius should fit the mesh as tightly as possible
    for (auto i = 0; i < 3; ++i)
    {
        auto l = std::abs(rad[i]);
        if (l > meshData.boundingSphere.radius)
        {
            meshData.boundingSphere.radius = l;
        }
    }
}

void BinaryMeshBuilder::parseSkeleton(RaiiRWops& file, const Detail::ModelBinary::Header& header) const
{
    if (header.skeletonOffset)
    {
        if (header.version < 2)
        {
            LogW << m_path << "\nSkeletal animation requires version 2 or greater. Please re-export the model" << std::endl;
        }

        else if (SDL_SeekIO(file.filePtr(), header.skeletonOffset, SDL_IO_SEEK_SET) > -1)
        {
            Detail::ModelBinary::SkeletonHeaderV2 skelHeader;
            SDL_ReadIO(file.filePtr(), &skelHeader, sizeof(skelHeader));
            m_skeleton.setRootTransform(glm::make_mat4(skelHeader.rootTransform));

            std::vector<Joint> inFrames(skelHeader.frameCount * skelHeader.frameSize);
            std::vector<Detail::ModelBinary::SerialAnimation> inAnims(skelHeader.animationCount);
            std::vector<Detail::ModelBinary::SerialNotification> inNotifications(skelHeader.notificationCount);
            std::vector<Detail::ModelBinary::SerialAttachment> inAttachments(skelHeader.attachmentCount);
            std::vector<float> inverseBindPose(skelHeader.frameSize * 16);

            SDL_ReadIO(file.filePtr(), inFrames.data(), sizeof(Joint) * inFrames.size());
            SDL_ReadIO(file.filePtr(), inAnims.data(), sizeof(Detail::ModelBinary::SerialAnimation) * inAnims.size());
            SDL_ReadIO(file.filePtr(), inNotifications.data(), sizeof(Detail::ModelBinary::SerialNotification) * inNotifications.size());
            SDL_ReadIO(file.filePtr(), inAttachments.data(), sizeof(Detail::ModelBinary::SerialAttachment) * inAttachments.size());
            SDL_ReadIO(file.filePtr(), inverseBindPose.data(), sizeof(float) * inverseBindPose.size());


            CRO_ASSERT(inFrames.size() % skelHeader.frameSize == 0, "");
            for (auto i = 0u; i < skelHeader.frameCount; ++i)
            {
                std::vector<Joint> frame;
                for (auto j = i * skelHeader.frameSize; j < (i * skelHeader.frameSize) + skelHeader.frameSize; ++j)
                {
                    frame.push_back(inFrames[j]);
                }

                m_skeleton.addFrame(frame);
            }

            for (const auto& inAnim : inAnims)
            {
                SkeletalAnim anim;
                anim.frameCount = inAnim.frameCount;
                anim.frameRate = inAnim.frameRate;
                anim.startFrame = inAnim.startFrame;
                anim.name = inAnim.name;
                anim.looped = inAnim.looped != 0;

                m_skeleton.addAnimation(anim);
            }

            for (auto [frameID, jointID, userID, name] : inNotifications)
            {
                m_skeleton.addNotification(frameID, { jointID, userID, name });
            }

            for (const auto& [rotation, translation, scale, parent, name] : inAttachments)
            {
                Attachment ap;
                ap.setParent(parent);
                ap.setPosition(translation);
                ap.setRotation(rotation);
                ap.setScale(scale);
                ap.setName(name);

                m_skeleton.addAttachment(ap);
            }

            std::vector<glm::mat4> invBindMatrices(skelHeader.frameSize);
            for (auto i = 0u; i < inverseBindPose.size(); i += 16)
            {
                std::memcpy(&invBindMatrices[i / 16][0][0], inverseBindPose.data() + i, sizeof(glm::mat4));
            }
            m_skeleton.setInverseBindPose(invBindMatrices);
        }
        else
        {
            LogE << "Failed to seek to skeleton offset, incorrect value provided" << std::endl;
        }
    }
}