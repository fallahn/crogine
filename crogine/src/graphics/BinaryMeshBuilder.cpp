/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

#include <crogine/graphics/BinaryMeshBuilder.hpp>
#include <crogine/detail/ModelBinary.hpp>
#include <crogine/detail/glm/gtc/type_ptr.hpp>
#include <crogine/core/FileSystem.hpp>

#include "../detail/GLCheck.hpp"

using namespace cro;

BinaryMeshBuilder::BinaryMeshBuilder(const std::string& path)
    : m_path    (cro::FileSystem::getResourcePath() + path),
    m_uid       (0)
{
    if (FileSystem::fileExists(m_path))
    {
        //calc a UID from the file path
        std::hash<std::string> hashAttack;
        m_uid = hashAttack(path);
    }
    else
    {
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

Mesh::Data BinaryMeshBuilder::build() const
{
    Mesh::Data meshData;

    RaiiRWops file;
    file.file = SDL_RWFromFile(m_path.c_str(), "rb");
    if (file.file)    
    {
        Detail::ModelBinary::Header header;
        auto len = SDL_RWseek(file.file, 0, RW_SEEK_END);
        if (len < sizeof(header))
        {
            LogE << "Unable to open " << m_path << ": invalid file size" << std::endl;
            return {};
        }

        SDL_RWseek(file.file, 0, RW_SEEK_SET);
        SDL_RWread(file.file, &header, sizeof(header), 1);

        if (header.magic != Detail::ModelBinary::MAGIC
            && header.magic != Detail::ModelBinary::MAGIC_V1)
        {
            LogE << "Invalid header found" << std::endl;
            return {};
        }

        if (header.meshOffset)
        {
            Detail::ModelBinary::MeshHeader meshHeader;
            SDL_RWread(file.file, &meshHeader, sizeof(meshHeader), 1);

            if ((meshHeader.flags & VertexProperty::Position) == 0)
            {
                LogE << "No position data in mesh" << std::endl;
                return {};
            }

            std::vector<float> tempVerts;
            std::vector<std::uint32_t> sizes(meshHeader.indexArrayCount);
            std::vector<std::vector<std::uint32_t>> indexData(meshHeader.indexArrayCount);

            SDL_RWread(file.file, sizes.data(), meshHeader.indexArrayCount * sizeof(std::uint32_t), 1);

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
                        meshData.attributes[i] = 3;
                        break;
                    case Mesh::Attribute::Colour:
                        vertStride += 4;
                        meshData.attributes[i] = 4;
                        break;
                    case Mesh::Attribute::Normal:
                        vertStride += 3;
                        meshData.attributes[i] = 3;
                        break;
                    case Mesh::Attribute::Tangent:
                        meshData.attributes[i] = 3;
                        meshData.attributes[Mesh::Attribute::Bitangent] = 3;
                        vertStride += 4; //we'll be decoding tangents
                        break;
                    case Mesh::Attribute::UV0:
                    case Mesh::Attribute::UV1:
                        vertStride += 2;
                        meshData.attributes[i] = 2;
                        break;
                    case Mesh::Attribute::BlendIndices:
                    case Mesh::Attribute::BlendWeights:
                        vertStride += 4;
                        meshData.attributes[i] = 4;
                        break;
                    }
                }
            }

            auto pos = SDL_RWtell(file.file);
            auto vertSize = meshHeader.indexArrayOffset - pos;
            tempVerts.resize(vertSize / sizeof(float));
            SDL_RWread(file.file, tempVerts.data(), vertSize, 1);
            CRO_ASSERT(tempVerts.size() % vertStride == 0, "");
            
            for (auto i = 0u; i < meshHeader.indexArrayCount; ++i)
            {
                indexData[i].resize(sizes[i]);
                SDL_RWread(file.file, indexData[i].data(), sizes[i] * sizeof(std::uint32_t), 1);
            }

            //process vertex data
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

                createIBO(meshData, indexData[i].data(), i, sizeof(std::uint32_t));
            }

            //boundingbox / sphere
            meshData.boundingBox[0] = glm::vec3(std::numeric_limits<float>::max());
            meshData.boundingBox[1] = glm::vec3(std::numeric_limits<float>::lowest());
            for (std::size_t i = 0; i < vertData.size(); i += (meshData.vertexSize / sizeof(float)))
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
            //radius should fir the mesh as tightly as possible
            for (auto i = 0; i < 3; ++i)
            {
                auto l = std::abs(rad[i]);
                if (l > meshData.boundingSphere.radius)
                {
                    meshData.boundingSphere.radius = l;
                }
            }
        }

        m_skeleton = {};
        if (header.skeletonOffset)
        {
            if (header.version < 2)
            {
                LogW << m_path <<  "\nSkeletal animation requires version 2 or greater. Please re-export the model" << std::endl;
            }

            else if (SDL_RWseek(file.file, header.skeletonOffset, RW_SEEK_SET) > -1)
            {
                Detail::ModelBinary::SkeletonHeaderV2 skelHeader;
                SDL_RWread(file.file, &skelHeader, sizeof(skelHeader), 1);
                m_skeleton.setRootTransform(glm::make_mat4(skelHeader.rootTransform));

                std::vector<Joint> inFrames(skelHeader.frameCount * skelHeader.frameSize);
                std::vector<Detail::ModelBinary::SerialAnimation> inAnims(skelHeader.animationCount);
                std::vector<Detail::ModelBinary::SerialNotification> inNotifications(skelHeader.notificationCount);
                std::vector<Detail::ModelBinary::SerialAttachment> inAttachments(skelHeader.attachmentCount);
                std::vector<float> inverseBindPose(skelHeader.frameSize * 16);

                SDL_RWread(file.file, inFrames.data(), sizeof(Joint), inFrames.size());
                SDL_RWread(file.file, inAnims.data(), sizeof(Detail::ModelBinary::SerialAnimation), inAnims.size());
                SDL_RWread(file.file, inNotifications.data(), sizeof(Detail::ModelBinary::SerialNotification), inNotifications.size());
                SDL_RWread(file.file, inAttachments.data(), sizeof(Detail::ModelBinary::SerialAttachment), inAttachments.size());
                SDL_RWread(file.file, inverseBindPose.data(), sizeof(float), inverseBindPose.size());


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
    else
    {
        LogE << m_path << ": " << SDL_GetError() << std::endl;
    }

    return meshData;
}