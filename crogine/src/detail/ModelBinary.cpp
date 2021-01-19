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

#include "GLCheck.hpp"

#include <crogine/detail/ModelBinary.hpp>
#include <crogine/graphics/MeshBuilder.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Skeleton.hpp>

using namespace cro;

bool cro::Detail::ModelBinary::write(cro::Entity entity, const std::string& path)
{
    bool retVal = false;

    Detail::ModelBinary::Header header;
    std::uint32_t skelOffset = sizeof(header);

    //if these are not empty after processing
    //then they'll be written to the file
    Detail::ModelBinary::MeshHeader meshHeader;
    std::vector<std::uint32_t> outIndexSizes;
    std::vector<float> outVertexData;
    std::vector<std::uint32_t> outIndexData;

    //TODO buffers for skeleton output
    Detail::ModelBinary::SkeletonHeader skelHeader;

    if (entity.hasComponent<Model>())
    {
        header.meshOffset = sizeof(header);

        const auto& meshData = entity.getComponent<Model>().getMeshData();

        //download the mesh data from vbo/ibo
        std::vector<float> vertexData;
        std::vector<std::vector<std::uint32_t>> indexData;

        vertexData.resize(meshData.vertexCount * (meshData.vertexSize / sizeof(float)));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData.vbo));
        glCheck(glGetBufferSubData(GL_ARRAY_BUFFER, 0, meshData.vertexCount * meshData.vertexSize, vertexData.data()));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

        indexData.resize(meshData.submeshCount);

        for (auto i = 0u; i < meshData.submeshCount; ++i)
        {
            indexData[i].resize(meshData.indexData[i].indexCount);
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[i].ibo));

            //fudgy kludge for different index types
            switch (meshData.indexData[i].format)
            {
            default: break;
            case GL_UNSIGNED_BYTE:
            {
                std::vector<std::uint8_t> temp(meshData.indexData[i].indexCount);
                glCheck(glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, meshData.indexData[i].indexCount, temp.data()));
                for (auto j = 0u; j < meshData.indexData[i].indexCount; ++j)
                {
                    indexData[i][j] = temp[j];
                }
            }
            break;
            case GL_UNSIGNED_SHORT:
            {
                std::vector<std::uint16_t> temp(meshData.indexData[i].indexCount);
                glCheck(glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, meshData.indexData[i].indexCount * sizeof(std::uint16_t), temp.data()));
                for (auto j = 0u; j < meshData.indexData[i].indexCount; ++j)
                {
                    indexData[i][j] = temp[j];
                }
            }
            break;
            case GL_UNSIGNED_INT:
                glCheck(glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, meshData.indexData[i].indexCount * sizeof(std::uint32_t), indexData[i].data()));
                break;
            }
        }
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

        //parse the vertex data and correct the colour for missing
        //alpha channel, and setup the tangent value to compensate
        //for missing bitan
        std::size_t vertStride = 0;
        std::array<std::size_t, Mesh::Attribute::Total> offsets;
        std::fill(offsets.begin(), offsets.end(), 0);

        for (auto i = 0u; i < meshData.attributes.size(); ++i)
        {
            if (meshData.attributes[i] != 0)
            {
                offsets[i] = vertStride;
                meshHeader.flags |= (1 << i);
            }
            vertStride += meshData.attributes[i];
        }
        CRO_ASSERT(vertexData.size() % vertStride == 0, "");
        //reset the bitan flag because setting it is misleading
        meshHeader.flags &= ~VertexProperty::Bitangent;

        for (auto i = 0u; i < vertexData.size(); i += vertStride)
        {
            for (auto j = 0u; j < meshData.attributes.size(); ++j)
            {
                switch (j)
                {
                case Mesh::Attribute::Bitangent:
                default: break;
                case Mesh::Attribute::Position:
                case Mesh::Attribute::Normal:
                    if (meshHeader.flags & (1 << j))
                    {
                        outVertexData.push_back(vertexData[i + offsets[j]]);
                        outVertexData.push_back(vertexData[i + offsets[j] + 1]);
                        outVertexData.push_back(vertexData[i + offsets[j] + 2]);
                    }
                    break;
                case Mesh::Attribute::Colour:
                    if (meshHeader.flags & (1 << j))
                    {
                        outVertexData.push_back(vertexData[i + offsets[j]]);
                        outVertexData.push_back(vertexData[i + offsets[j] + 1]);
                        outVertexData.push_back(vertexData[i + offsets[j] + 2]);
                        if (meshData.attributes[Mesh::Attribute::Colour] == 3)
                        {
                            //set alpha to one
                            outVertexData.push_back(1.f);
                        }
                        else
                        {
                            outVertexData.push_back(vertexData[i + offsets[j] + 3]);
                        }
                    }
                    break;
                case Mesh::Attribute::Tangent:
                    if (meshHeader.flags & (1 << j))
                    {
                        CRO_ASSERT(meshData.attributes[Mesh::Attribute::Normal] != 0, "");
                        glm::vec3 normal =
                        {
                            vertexData[i + offsets[Mesh::Attribute::Normal]],
                            vertexData[i + offsets[Mesh::Attribute::Normal] + 1],
                            vertexData[i + offsets[Mesh::Attribute::Normal] + 2]
                        };

                        glm::vec3 tangent =
                        {
                            vertexData[i + offsets[Mesh::Attribute::Tangent]],
                            vertexData[i + offsets[Mesh::Attribute::Tangent] + 1],
                            vertexData[i + offsets[Mesh::Attribute::Tangent] + 2]
                        };

                        glm::vec3 bitan =
                        {
                            vertexData[i + offsets[Mesh::Attribute::Bitangent]],
                            vertexData[i + offsets[Mesh::Attribute::Bitangent] + 1],
                            vertexData[i + offsets[Mesh::Attribute::Bitangent] + 2]
                        };

                        auto temp = glm::normalize(glm::cross(normal, tangent));
                        float sign = glm::dot(temp, bitan);
                        //sign = std::round(sign);

                        //TODO this *should* just work with the dot product
                        //... but it doesn't. So imma just paper over this bug for now
                        sign = (sign > 0) ? 1.f : -1.f;

                        CRO_ASSERT(std::abs(sign) == 1, "");
                        outVertexData.push_back(tangent.x);
                        outVertexData.push_back(tangent.y);
                        outVertexData.push_back(tangent.z);
                        outVertexData.push_back(sign);
                    }
                    break;
                case Mesh::Attribute::UV0:
                case Mesh::Attribute::UV1:
                    if (meshHeader.flags & (1 << j))
                    {
                        outVertexData.push_back(vertexData[i + offsets[j]]);
                        outVertexData.push_back(vertexData[i + offsets[j] + 1]);
                    }
                    break;
                case Mesh::Attribute::BlendIndices:
                case Mesh::Attribute::BlendWeights:
                    if (meshHeader.flags & (1 << j))
                    {
                        outVertexData.push_back(vertexData[i + offsets[j]]);
                        outVertexData.push_back(vertexData[i + offsets[j] + 1]);
                        outVertexData.push_back(vertexData[i + offsets[j] + 2]);
                        outVertexData.push_back(vertexData[i + offsets[j] + 3]);
                    }
                    break;
                }
            }
        }



        //copy index data
        for (const auto& data : indexData)
        {
            outIndexSizes.push_back(static_cast<std::uint32_t>(data.size()));
            for (auto d : data)
            {
                outIndexData.push_back(d);
            }
        }


        //update the header with relevant detail
        meshHeader.indexArrayCount = static_cast<std::uint16_t>(indexData.size());
        meshHeader.indexArrayOffset = header.meshOffset
            + static_cast<std::uint32_t>(sizeof(meshHeader)
            + (outIndexSizes.size() * sizeof(std::uint32_t))
            + (outVertexData.size() * sizeof(float)));

        //update the skeleton offset with the size of the mesh data
        skelOffset = meshHeader.indexArrayOffset
            + static_cast<std::uint32_t>(outIndexData.size() * sizeof(std::uint32_t));

        retVal = true;
    }

    if (entity.hasComponent<Skeleton>())
    {
        //update the main header with the beginning of the skel data
        header.skeletonOffset = skelOffset;




        //retVal = true;
    }

    //should only be true if one or both component types
    //have been processed
    if (retVal)
    {
        //open the file
        RaiiRWops file;
        file.file = SDL_RWFromFile(path.c_str(), "wb");

        if (!file.file)
        {
            LogE << "Failed opening " << path << " for writing" << std::endl;
        }

        //write the header
        SDL_RWwrite(file.file, &header, sizeof(header), 1);

        if (header.meshOffset)
        {
            //write mesh data
            SDL_RWwrite(file.file, &meshHeader, sizeof(meshHeader), 1);
            SDL_RWwrite(file.file, outIndexSizes.data(), sizeof(std::uint32_t) * outIndexSizes.size(), 1);
            SDL_RWwrite(file.file, outVertexData.data(), sizeof(float) * outVertexData.size(), 1);
            SDL_RWwrite(file.file, outIndexData.data(), sizeof(std::uint32_t) * outIndexData.size(), 1);
        }

        if (header.skeletonOffset)
        {
            //write skel data
        }
        SDL_RWclose(file.file);
    }

    return retVal;
}