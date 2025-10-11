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

#include "GLCheck.hpp"

#include <crogine/detail/ModelBinary.hpp>
#include <crogine/graphics/MeshBuilder.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Skeleton.hpp>

using namespace cro;

bool cro::Detail::ModelBinary::write(cro::Entity entity, const std::string& path, bool includeSkeleton)
{
    bool retVal = false;

    Detail::ModelBinary::HeaderV2 header;
    std::uint32_t skelOffset = sizeof(header);

    //if these are not empty after processing
    //then they'll be written to the file
    Detail::ModelBinary::MeshHeader meshHeader;
    std::vector<std::uint32_t> outIndexSizes;
    std::vector<float> outVertexData;
    std::vector<std::uint32_t> outIndexData;

    if (entity.hasComponent<Model>())
    {
        header.meshOffset = sizeof(header);

        const auto& meshData = entity.getComponent<Model>().getMeshData();

        //download the mesh data from vbo/ibo
        std::vector<float> vertexData;
        std::vector<std::vector<std::uint32_t>> indexData;

        vertexData.resize(meshData.vertexCount * (meshData.vertexSize / sizeof(float)));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData.vboAllocation.vboID));
        glCheck(glGetBufferSubData(GL_ARRAY_BUFFER, meshData.vboAllocation.offset, meshData.vertexCount * meshData.vertexSize, vertexData.data()));
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
            if (meshData.attributes[i].size != 0)
            {
                offsets[i] = vertStride;
                meshHeader.flags |= (1 << i);
            }
            vertStride += meshData.attributes[i].size;
        }
        CRO_ASSERT(vertexData.size() % vertStride == 0, "");
        //reset the bitan flag because setting it is misleading
        meshHeader.flags &= ~VertexProperty::Bitangent;
        //skip blending data if not animated
        if (!includeSkeleton)
        {
            meshHeader.flags &= ~(VertexProperty::BlendIndices | VertexProperty::BlendWeights);
        }

        for (auto i = 0ull; i < vertexData.size(); i += vertStride)
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
                        if (meshData.attributes[Mesh::Attribute::Colour].size == 3)
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
                        CRO_ASSERT(meshData.attributes[Mesh::Attribute::Normal].size != 0, "");
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
                    if (meshHeader.flags & (1 << j)
                        && includeSkeleton)
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

        //remove any empty index arrays to prevent creating empty submeshes
        indexData.erase(std::remove_if(indexData.begin(), indexData.end(),
            [](const std::vector<std::uint32_t>& v)
            {
                return v.empty();
            }), indexData.end());

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

    //skeleton output
    Detail::ModelBinary::SkeletonHeaderV2 skelHeader;
    std::vector<Detail::ModelBinary::SerialAnimation> outAnimations;
    std::vector<Detail::ModelBinary::SerialNotification> outNotifications;
    std::vector<Detail::ModelBinary::SerialAttachment> outAttachments;
    std::vector<float> outInverseBindPose;

    if (includeSkeleton &&
        entity.hasComponent<Skeleton>())
    {
        //update the main header with the beginning of the skel data
        header.skeletonOffset = skelOffset;

        const auto& skeleton = entity.getComponent<Skeleton>();
        skelHeader = skeleton;

        for (const auto& anim : skeleton.getAnimations())
        {
            outAnimations.emplace_back(anim);
        }

        const auto& notifications = skeleton.getNotifications();
        for (auto i = 0u; i < notifications.size(); ++i)
        {
            if (!notifications[i].empty())
            {
                for (auto j = 0u; j < notifications[i].size(); ++j)
                {
                    auto& n = outNotifications.emplace_back(i, notifications[i][j].jointID, notifications[i][j].userID);
                    auto len = std::min(Attachment::MaxNameLength, notifications[i][j].name.size());
                    std::memcpy(n.name, notifications[i][j].name.c_str(), len);
                    n.name[len] = 0;
                }
            }
        }

        for (const auto& attachment : skeleton.getAttachments())
        {
            outAttachments.emplace_back(attachment);
        }

        const auto& ibp = skeleton.getInverseBindPose();
        CRO_ASSERT(!ibp.empty(), "inverse bind pose data is missing");
        outInverseBindPose.resize(ibp.size() * 16);
        std::memcpy(outInverseBindPose.data(), ibp.data(), sizeof(float)* outInverseBindPose.size());

        retVal = true;
    }

    //should only be true if one or both component types
    //have been processed
    if (retVal)
    {
        //open the file
        SDL_RWops* file = SDL_RWFromFile(path.c_str(), "wb");

        if (!file)
        {
            LogE << "Failed opening " << path << " for writing" << std::endl;
        }
        else
        {
            //write the header
            SDL_RWwrite(file, &header, sizeof(header), 1);

            if (header.meshOffset)
            {
                //write mesh data
                SDL_RWwrite(file, &meshHeader, sizeof(meshHeader), 1);
                SDL_RWwrite(file, outIndexSizes.data(), sizeof(std::uint32_t), outIndexSizes.size());
                SDL_RWwrite(file, outVertexData.data(), sizeof(float), outVertexData.size());
                SDL_RWwrite(file, outIndexData.data(), sizeof(std::uint32_t), outIndexData.size());
            }

            if (header.skeletonOffset)
            {
                const auto& frames = entity.getComponent<cro::Skeleton>().getFrames();

                //write skel data
                SDL_RWwrite(file, &skelHeader, sizeof(skelHeader), 1);
                SDL_RWwrite(file, frames.data(), sizeof(Joint), frames.size());
                SDL_RWwrite(file, outAnimations.data(), sizeof(SerialAnimation), outAnimations.size());
                SDL_RWwrite(file, outNotifications.data(), sizeof(SerialNotification), outNotifications.size());
                SDL_RWwrite(file, outAttachments.data(), sizeof(SerialAttachment), outAttachments.size());
                SDL_RWwrite(file, outInverseBindPose.data(), sizeof(float), outInverseBindPose.size());
            }

            if (SDL_RWclose(file))
            {
                LogE << "SDL: Failed writing model binary - " << SDL_GetError() << std::endl;
            }
        }
    }

    return retVal;
}

cro::Mesh::Data cro::Detail::ModelBinary::read(const std::string& binPath, std::vector<float>& dstVert, std::vector<std::vector<std::uint32_t>>& dstIdx)
{
    //make sure everything is empty - who knows what gets passed in ;)
    dstVert.clear();
    dstIdx.clear();

    cro::Mesh::Data meshData;

    cro::RaiiRWops file;
    file.file = SDL_RWFromFile(binPath.c_str(), "rb");
    if (file.file)
    {
        cro::Detail::ModelBinary::Header header;
        auto len = SDL_RWseek(file.file, 0, RW_SEEK_END);
        if (len < sizeof(header))
        {
            LogE << "Unable to open " << binPath << ": invalid file size" << std::endl;
            return {};
        }

        SDL_RWseek(file.file, 0, RW_SEEK_SET);
        SDL_RWread(file.file, &header, sizeof(header), 1);

        if (header.magic != cro::Detail::ModelBinary::MAGIC
            && header.magic != cro::Detail::ModelBinary::MAGIC_V1)
        {
            LogE << "Invalid header found" << std::endl;
            return {};
        }

        if (header.meshOffset)
        {
            cro::Detail::ModelBinary::MeshHeader meshHeader;
            SDL_RWread(file.file, &meshHeader, sizeof(meshHeader), 1);

            if ((meshHeader.flags & cro::VertexProperty::Position) == 0)
            {
                LogE << "No position data in mesh" << std::endl;
                return {};
            }
            std::vector<std::uint32_t> sizes(meshHeader.indexArrayCount);
            dstIdx.resize(meshHeader.indexArrayCount);

            SDL_RWread(file.file, sizes.data(), meshHeader.indexArrayCount * sizeof(std::uint32_t), 1);

            std::uint32_t vertStride = 0;
            for (auto i = 0u; i < cro::Mesh::Attribute::Total; ++i)
            {
                if (meshHeader.flags & (1 << i))
                {
                    switch (i)
                    {
                    default:
                    case cro::Mesh::Attribute::Bitangent:
                        break;
                    case cro::Mesh::Attribute::Position:
                        vertStride += 3;
                        meshData.attributes[i].size = 3;
                        break;
                    case cro::Mesh::Attribute::Colour:
                        vertStride += 4;
                        meshData.attributes[i].size = 4;
                        break;
                    case cro::Mesh::Attribute::Normal:
                        vertStride += 3;
                        meshData.attributes[i].size = 3;
                        break;
                    case cro::Mesh::Attribute::UV0:
                    case cro::Mesh::Attribute::UV1:
                        vertStride += 2;
                        meshData.attributes[i].size = 2;
                        break;
                    case cro::Mesh::Attribute::Tangent:
                    case cro::Mesh::Attribute::BlendIndices:
                    case cro::Mesh::Attribute::BlendWeights:
                        vertStride += 4;
                        meshData.attributes[i].size = 4;
                        break;
                    }
                }
            }

            auto pos = SDL_RWtell(file.file);
            auto vertSize = meshHeader.indexArrayOffset - pos;

            std::vector<float> tempVerts(vertSize / sizeof(float));
            SDL_RWread(file.file, tempVerts.data(), vertSize, 1);
            CRO_ASSERT(tempVerts.size() % vertStride == 0, "");

            for (auto i = 0u; i < meshHeader.indexArrayCount; ++i)
            {
                dstIdx[i].resize(sizes[i]);
                SDL_RWread(file.file, dstIdx[i].data(), sizes[i] * sizeof(std::uint32_t), 1);
            }

            dstVert.swap(tempVerts);
            meshData.attributeFlags = meshHeader.flags;
            meshData.primitiveType = GL_TRIANGLES;

            for (const auto& a : meshData.attributes)
            {
                meshData.vertexSize += a.size;
            }
            meshData.vertexSize *= sizeof(float);
            meshData.vertexCount = dstVert.size() / (meshData.vertexSize / sizeof(float));

            meshData.submeshCount = meshHeader.indexArrayCount;
            for (auto i = 0u; i < meshData.submeshCount; ++i)
            {
                meshData.indexData[i].format = GL_UNSIGNED_INT;
                meshData.indexData[i].primitiveType = meshData.primitiveType;
                meshData.indexData[i].indexCount = static_cast<std::uint32_t>(dstIdx[i].size());
            }
        }


        meshData.boundingBox[0] = glm::vec3(std::numeric_limits<float>::max());
        meshData.boundingBox[1] = glm::vec3(std::numeric_limits<float>::lowest());
        for (std::size_t i = 0; i < dstVert.size(); i += (meshData.vertexSize / sizeof(float)))
        {
            //min point
            if (meshData.boundingBox[0].x > dstVert[i])
            {
                meshData.boundingBox[0].x = dstVert[i];
            }
            if (meshData.boundingBox[0].y > dstVert[i + 1])
            {
                meshData.boundingBox[0].y = dstVert[i + 1];
            }
            if (meshData.boundingBox[0].z > dstVert[i + 2])
            {
                meshData.boundingBox[0].z = dstVert[i + 2];
            }

            //maxpoint
            if (meshData.boundingBox[1].x < dstVert[i])
            {
                meshData.boundingBox[1].x = dstVert[i];
            }
            if (meshData.boundingBox[1].y < dstVert[i + 1])
            {
                meshData.boundingBox[1].y = dstVert[i + 1];
            }
            if (meshData.boundingBox[1].z < dstVert[i + 2])
            {
                meshData.boundingBox[1].z = dstVert[i + 2];
            }
        }
        meshData.boundingSphere = meshData.boundingBox;
    }
    else
    {
        LogE << "SDL: " << binPath << ": " << SDL_GetError() << std::endl;
        return {};
    }
    return meshData;
}