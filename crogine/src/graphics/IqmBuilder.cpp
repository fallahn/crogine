/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2022
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

#include <crogine/graphics/IqmBuilder.hpp>
#include "../detail/GLCheck.hpp"

#include <crogine/detail/glm/mat4x4.hpp>
#include <crogine/detail/glm/gtc/quaternion.hpp>
#include <crogine/detail/glm/gtx/matrix_decompose.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Matrix.hpp>

#include <cstring>
#include <array>
#include <limits>
#include <functional>

using namespace cro;

namespace
{
    namespace Iqm
    {
#include "Iqm.inl"
    }

    glm::mat4 rootTransform;
}

void loadVertexData(const Iqm::Header& header, char* data, const std::string& strings, cro::Mesh::Data& out);
void loadAnimationData(const Iqm::Header& header, char* data, const std::string& strings, cro::Skeleton& out);

IqmBuilder::IqmBuilder(const std::string& path)
    : m_path    (FileSystem::getResourcePath() + path),
    m_uid       (0),
    m_file      (nullptr)
{
    std::hash<std::string> hashAttack;
    m_uid = hashAttack(path);

    rootTransform = glm::toMat4(glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), -90.f * Util::Const::degToRad, glm::vec3(1.f, 0.f, 0.f)));
}

IqmBuilder::~IqmBuilder()
{
    if (m_file)
    {
        SDL_RWclose(m_file);
    }
}

//public
std::size_t IqmBuilder::getUID() const
{
    return m_uid;
}

//private
cro::Mesh::Data IqmBuilder::build() const
{
    cro::Mesh::Data returnData;
    returnData.primitiveType = GL_TRIANGLES;
    
    m_file = SDL_RWFromFile(m_path.c_str(), "rb");
    if (m_file)
    {
        //do some file checks
        auto fileSize = SDL_RWsize(m_file);
        if (fileSize < static_cast<std::int32_t>(sizeof(Iqm::Header)))
        {
            Logger::log(m_path + ": Invalid file size", Logger::Type::Error);
            SDL_RWclose(m_file);
            m_file = nullptr;
            return {};
        }

        std::vector<char> fileData(fileSize);
        auto readCount = SDL_RWread(m_file, fileData.data(), fileSize, 1);
        SDL_RWclose(m_file);
        m_file = nullptr;

        if (readCount == 0)
        {
            Logger::log(m_path + ": failed to read data.", Logger::Type::Error);
            return {};
        }
        
        //start parsing - begin by checking header
        char* filePtr = fileData.data();
        Iqm::Header header;
        Iqm::readHeader(filePtr, header);

        if (std::string(header.magic) != Iqm::IQM_MAGIC)
        {
            Logger::log("Not an IQM file, header returned " + std::string(header.magic) + " instead of " + Iqm::IQM_MAGIC, Logger::Type::Error);
            return {};
        }

        if (header.version != Iqm::IQM_VERSION)
        {
            Logger::log("Wrong IQM version, found " + std::to_string(header.version) + " expected " + std::to_string(Iqm::IQM_VERSION), Logger::Type::Error);
            return {};
        }

        if (header.filesize > (16 << 20))
        {
            Logger::log("IQM file greater than 16mb, file not loaded", Logger::Type::Error);
            return {};
        }

        //read string data
        std::string strings;
        strings.resize(header.textCount);
        std::memcpy(&strings[0], fileData.data() + header.textOffset, header.textCount);

        //check for vertex data
        if (header.vertexCount != 0)
        {
            loadVertexData(header, fileData.data(), strings, returnData);
        }
        else
        {
            Logger::log("No vertex data was found in " + m_path + ", this file may contain only animation data", Logger::Type::Warning);
        }

        if (header.jointCount != 0)
        {
            loadAnimationData(header, fileData.data(), strings, m_skeleton);
        }

        //update the bounding box to match the rootTransform.
        returnData.boundingSphere.centre = glm::vec3(rootTransform * glm::vec4(returnData.boundingSphere.centre, 1.f));
        returnData.boundingBox = rootTransform * returnData.boundingBox;
    }
    else
    {
        Logger::log("Failed opening " + m_path, Logger::Type::Error);
        return {};
    }
    return returnData;
}

void loadVertexData(const Iqm::Header& header, char* data, const std::string& strings, cro::Mesh::Data& out)
{
    const auto dataStart = data;

    data += header.varrayOffset;
    //iqm files keep each attribute in their own array
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> tangents; //tans are 4 component - the w contains the sign of the cross product for the bitan
    std::vector<float> texCoords;
    std::vector<std::uint8_t> blendIndices;
    std::vector<std::uint8_t> blendWeights;
    std::vector<std::uint8_t> colours;
    std::size_t vertexSize = 0;

    for (auto i = 0u; i < header.varrayCount; ++i)
    {
        Iqm::VertexArray vertArray;
        data = Iqm::readVertexArray(data, vertArray);

        switch (vertArray.type)
        {
        default: break;
        case Iqm::POSITION:
            positions.resize(header.vertexCount * Iqm::positionSize);
            std::memcpy(positions.data(), dataStart + vertArray.offset, sizeof(float) * positions.size());
            out.attributes[Mesh::Position] = Iqm::positionSize;
            out.attributeFlags |= VertexProperty::Position;
            vertexSize += Iqm::positionSize;
            break;
        case Iqm::NORMAL:
            normals.resize(header.vertexCount * Iqm::normalSize);
            std::memcpy(normals.data(), dataStart + vertArray.offset, sizeof(float) * normals.size());
            out.attributes[Mesh::Normal] = Iqm::normalSize;
            out.attributeFlags |= VertexProperty::Normal;
            vertexSize += Iqm::normalSize;
            break;
        case Iqm::TANGENT:
            tangents.resize(header.vertexCount * (Iqm::normalSize + 1));
            std::memcpy(tangents.data(), dataStart + vertArray.offset, sizeof(float) * tangents.size());
            out.attributes[Mesh::Tangent] = Iqm::normalSize;
            out.attributes[Mesh::Bitangent] = Iqm::normalSize;
            out.attributeFlags |= (VertexProperty::Tangent | VertexProperty::Bitangent);
            vertexSize += (Iqm::normalSize * 2);
            break;
        case Iqm::TEXCOORD:
            texCoords.resize(header.vertexCount * Iqm::uvSize);
            std::memcpy(texCoords.data(), dataStart + vertArray.offset, sizeof(float) * texCoords.size());
            out.attributes[Mesh::UV0] = Iqm::uvSize;
            out.attributeFlags |= VertexProperty::UV0;
            vertexSize += Iqm::uvSize;
            break;
        case Iqm::BLENDINDICES:
            blendIndices.resize(header.vertexCount * Iqm::blendIndexSize);
            std::memcpy(blendIndices.data(), dataStart + vertArray.offset, blendIndices.size());
            out.attributes[Mesh::BlendIndices] = Iqm::blendIndexSize;
            out.attributeFlags |= VertexProperty::BlendIndices;
            vertexSize += Iqm::blendIndexSize;
            break;
        case Iqm::BLENDWEIGHTS:
            blendWeights.resize(header.vertexCount * Iqm::blendWeightSize);
            std::memcpy(blendWeights.data(), dataStart + vertArray.offset, blendWeights.size());
            out.attributes[Mesh::BlendWeights] = Iqm::blendWeightSize;
            out.attributeFlags |= VertexProperty::BlendWeights;
            vertexSize += Iqm::blendWeightSize;
            break;
        case Iqm::COLOUR:
            colours.resize(header.vertexCount * Iqm::colourSize);
            std::memcpy(colours.data(), dataStart + vertArray.offset, colours.size());
            out.attributes[Mesh::Colour] = Iqm::colourSize;
            out.attributeFlags |= VertexProperty::Colour;
            vertexSize += Iqm::colourSize;
            break;
        }
    }

    //parse the index arrays. Do this first in case
    //we decide to calculate our own tangent data
    //which will be interleaved in  the VBO
    std::vector<Iqm::Triangle> triangles;
    bool buildTangents = tangents.empty();
    char* meshData = dataStart + header.meshOffset;
    for (auto i = 0u; i < header.meshCount; ++i)
    {
        Iqm::Mesh mesh;
        meshData = Iqm::readMesh(meshData, mesh);

        //TODO optionally read material data from string

        Iqm::Triangle triangle;
        char* triangleData = dataStart + header.triangleOffset + (mesh.firstTriangle * sizeof(triangle.vertex));
        std::vector<std::uint16_t> indices;

        for (auto j = 0u; j < mesh.triangleCount; ++j)
        {
            std::memcpy(triangle.vertex, triangleData, sizeof(triangle.vertex));
            triangleData += sizeof(triangle.vertex);
            //IQM triangles are wound clockwise - this is a kludge to make things draw but it makes models appear 'inside out'!
            indices.push_back(triangle.vertex[2]);
            indices.push_back(triangle.vertex[1]);
            indices.push_back(triangle.vertex[0]);

            //cache the triangle if we need to build tangents
            if (buildTangents)
            {
                triangles.push_back(triangle);
            }
        }

        out.indexData[i].format = GL_UNSIGNED_SHORT;
        out.indexData[i].primitiveType = out.primitiveType;
        out.indexData[i].indexCount = static_cast<std::uint32_t>(indices.size());
        out.submeshCount++;

        glCheck(glGenBuffers(1, &out.indexData[i].ibo));
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.indexData[i].ibo));
        glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, out.indexData[i].indexCount * sizeof(std::uint16_t), indices.data(), GL_STATIC_DRAW));
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
    }


    //calc bitangents
    std::vector<float> pureTangents; //4th component removed
    std::vector<float> bitangents;
    if (!buildTangents)
    {
        //use cross product of model data
        for (auto i = 0u, j = 0u; i < normals.size(); i += Iqm::normalSize, j += (Iqm::normalSize + 1))
        {
            glm::vec3 normal(normals[i], normals[i + 1], normals[i + 2]);
            glm::vec3 tangent(tangents[j], tangents[j + 1], tangents[j + 2]);
            glm::vec3 bitan = glm::cross(normal, tangent) * -tangents[j + 3];

            pureTangents.push_back(tangent.x);
            pureTangents.push_back(tangent.y);
            pureTangents.push_back(tangent.z);

            bitangents.push_back(bitan.x);
            bitangents.push_back(bitan.y);
            bitangents.push_back(bitan.z);
        }
    }
    else
    {
        out.attributes[Mesh::Tangent] = Iqm::normalSize;
        out.attributes[Mesh::Bitangent] = Iqm::normalSize;
        vertexSize += (Iqm::normalSize * 2);

        //calc from UV coords and face normals
        LOG("Calculating missing bitangents", Logger::Type::Info);
        pureTangents.resize(normals.size());
        std::memset(pureTangents.data(), 0, pureTangents.size() * sizeof(float));
        bitangents.resize(normals.size());
        std::memset(bitangents.data(), 0, bitangents.size() * sizeof(float));

        std::memset(normals.data(), 0, normals.size() * sizeof(float));

        for (const auto& t : triangles)
        {
            //calc face normal
            std::array<glm::vec3, 3u> face =
            {
                {Iqm::vec3FromArray(t.vertex[0], positions),
                Iqm::vec3FromArray(t.vertex[1], positions),
                Iqm::vec3FromArray(t.vertex[2], positions)}
            };
            glm::vec3 deltaPos1 = face[1] - face[0];
            glm::vec3 deltaPos2 = face[2] - face[0];
            glm::vec3 faceNormal = -glm::cross(deltaPos1, deltaPos2);

            Iqm::addVecToArray(t.vertex[0], normals, faceNormal);
            Iqm::addVecToArray(t.vertex[1], normals, faceNormal);
            Iqm::addVecToArray(t.vertex[2], normals, faceNormal);

            std::array<glm::vec2, 3u> faceCoords =
            {
                {Iqm::vec2FromArray(t.vertex[0], texCoords),
                Iqm::vec2FromArray(t.vertex[1], texCoords),
                Iqm::vec2FromArray(t.vertex[2], texCoords)}
            };
            glm::vec2 deltaUV1 = faceCoords[1] - faceCoords[0];
            glm::vec2 deltaUV2 = faceCoords[2] - faceCoords[0];

            float sign = 1.f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
            glm::vec3 faceTan = (deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * sign;
            glm::vec3 faceBitan = (deltaPos2 * deltaUV1.x - deltaPos1 * deltaUV2.x) * sign;

            Iqm::addVecToArray(t.vertex[0], pureTangents, faceTan);
            Iqm::addVecToArray(t.vertex[1], pureTangents, faceTan);
            Iqm::addVecToArray(t.vertex[2], pureTangents, faceTan);

            Iqm::addVecToArray(t.vertex[0], bitangents, faceBitan);
            Iqm::addVecToArray(t.vertex[1], bitangents, faceBitan);
            Iqm::addVecToArray(t.vertex[2], bitangents, faceBitan);
        }

        Iqm::normaliseVec3Array(normals);
        Iqm::normaliseVec3Array(pureTangents);
        Iqm::normaliseVec3Array(bitangents);
    }


    //interleave data and create VBO
    //also measure bounds if bounds data missing
    std::vector<float> vertexData;
    vertexData.reserve(header.vertexCount * vertexSize);

    std::size_t posIndex = 0;
    std::size_t normalIndex = 0;
    std::size_t tanIndex = 0;
    std::size_t bitanIndex = 0;
    std::size_t uvIndex = 0;
    std::size_t blendIndex = 0;
    std::size_t blendWeightIndex = 0;
    std::size_t colourIndex = 0;

    glm::mat4 yUpMatrix(1.f);
    if (header.animCount == 0)
    {
        yUpMatrix = rootTransform;
    }

    glm::vec3 boundsMin(std::numeric_limits<float>::max());
    glm::vec3 boundsMax(0.f);
    //NOTE these have to match attribute order of Mesh::Data
    for (auto i = 0u; i < header.vertexCount; ++i)
    {
        if (!positions.empty())
        {
            glm::vec3 position = yUpMatrix * glm::vec4(positions[posIndex], positions[posIndex+1], positions[posIndex+2], 1.f);

            posIndex += Iqm::positionSize;

            vertexData.push_back(position.x);
            vertexData.push_back(position.y);
            vertexData.push_back(position.z);

            //guestimate bounds
            if (position.x < boundsMin.x) boundsMin.x = position.x;
            else if (position.x > boundsMax.x) boundsMax.x = position.x;
            if (position.y < boundsMin.y) boundsMin.y = position.y;
            else if (position.y > boundsMax.y) boundsMax.y = position.y;
            if (position.z < boundsMin.z) boundsMin.z = position.z;
            else if (position.z > boundsMax.z) boundsMax.z = position.z;
        }

        for (auto j = 0u; j < Iqm::colourSize && !colours.empty(); ++j)
        {
            vertexData.push_back(static_cast<float>(colours[colourIndex++]) / 255.f);
        }

        if (!normals.empty())
        {
            glm::vec3 normal = yUpMatrix * glm::vec4(normals[normalIndex], normals[normalIndex+1], normals[normalIndex+2], 1.f);

            normalIndex += Iqm::normalSize;

            vertexData.push_back(normal.x);
            vertexData.push_back(normal.y);
            vertexData.push_back(normal.z);
        }

        if (!pureTangents.empty())
        {
            glm::vec3 tan = yUpMatrix * glm::vec4(pureTangents[tanIndex], pureTangents[tanIndex+1], pureTangents[tanIndex+2], 1.f);

            tanIndex += Iqm::normalSize;

            vertexData.push_back(tan.x);
            vertexData.push_back(tan.y);
            vertexData.push_back(tan.z);
        }

        if (!bitangents.empty())
        {
            glm::vec3 bitan = yUpMatrix * glm::vec4(bitangents[bitanIndex], bitangents[bitanIndex+1], bitangents[bitanIndex+2], 1.f);

            bitanIndex += Iqm::normalSize;

            vertexData.push_back(bitan.x);
            vertexData.push_back(bitan.y);
            vertexData.push_back(bitan.z);
        }

        for (auto j = 0u; j < Iqm::uvSize && !texCoords.empty(); ++j)
        {
            if (j % 2 == 0)
            {
                vertexData.push_back(texCoords[uvIndex++]);
            }
            else //flip coords vertically
            {
                vertexData.push_back(1.f - texCoords[uvIndex++]);
            }
        }

        for (auto j = 0u; j < Iqm::blendIndexSize && !blendIndices.empty(); ++j)
        {
            vertexData.push_back(static_cast<float>(blendIndices[blendIndex++]));
        }

        for (auto j = 0u; j < Iqm::blendWeightSize && !blendWeights.empty(); ++j)
        {
            //blend weights need to be normalised
            vertexData.push_back(static_cast<float>(blendWeights[blendWeightIndex++]) / 255.f);
        }        
    }
    

    //use bounds data from file if available, else use the calculated
    //bounds fro mthe vertex data. Note this only creates the mesh's
    //AABB, the skin/animated AABBs are generated by the SkeletalAnimator
    //at runtime, if any animations exist.
    if (header.boundsOffset > 0)
    {
        Iqm::Bounds bounds;
        std::memcpy(&bounds, dataStart + header.boundsOffset, sizeof(Iqm::Bounds));
        out.boundingBox[0] = { bounds.bbmin[0], bounds.bbmin[1], bounds.bbmin[2] };
        out.boundingBox[1] = { bounds.bbmax[0], bounds.bbmax[1], bounds.bbmax[2] };
        out.boundingSphere.radius = bounds.radius;
    }
    else
    {
        out.boundingBox[0] = boundsMin;
        out.boundingBox[1] = boundsMax;
        out.boundingSphere.radius = glm::length(boundsMax - boundsMin) / 2.f;
    }

    out.boundingSphere.centre = out.boundingBox[0] + ((out.boundingBox[1] - out.boundingBox[0]) / 2.f);

    //finally create the VBO
    out.vertexCount = header.vertexCount;
    out.vertexSize = vertexSize * sizeof(float);
    
    glCheck(glGenBuffers(1, &out.vboAllocation.vboID));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, out.vboAllocation.vboID));
    glCheck(glBufferData(GL_ARRAY_BUFFER, out.vertexSize * out.vertexCount, vertexData.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
}

void loadAnimationData(const Iqm::Header& header, char* data, const std::string& strings, cro::Skeleton& out)
{
    //load joints into bind pose
    char* jointIter = data + header.jointOffset;
    std::vector<glm::mat4> inverseBindPose(header.jointCount);

    //warn if bone count > 64 as this is the limit on mobile devices (or even lower!)
    if (header.jointCount > 64)
    {
        Logger::log("IQM joint count greater than 64 - this may exceed limits on mobile devices", Logger::Type::Warning);
    }

    for (auto i = 0u; i < header.jointCount; ++i)
    {
        Iqm::Joint joint;
        jointIter = readJoint(jointIter, joint);

        glm::quat rotation(1.f, 0.f, 0.f, 0.f);
        rotation.w = joint.rotate[3];
        rotation.x = joint.rotate[0];
        rotation.y = joint.rotate[1];
        rotation.z = joint.rotate[2];

        glm::vec3 translation(joint.translate[0], joint.translate[1], joint.translate[2]);
        glm::vec3 scale(joint.scale[0], joint.scale[1], joint.scale[2]);

        inverseBindPose[i] = glm::inverse(Iqm::createBoneMatrix(rotation, translation, scale));

        if (joint.parent != -1)
        {
            //multiply by parent's transform
            inverseBindPose[i] *= inverseBindPose[joint.parent];
        }
    }
    out.setInverseBindPose(inverseBindPose);

    //corrects for y-up
    out.setRootTransform(rootTransform);

    //load keyframes - a 'pose' is a single posed joint, and a set of poses makes up one frame equivalent to a posed skeleton
    if (header.frameCount > 0)
    {
        std::uint32_t frameSize = header.frameCount * header.jointCount;
        std::uint16_t* frameIter = (std::uint16_t*)(data + header.frameOffset);
        if (inverseBindPose.size() > 0)
        {
            std::vector<Joint> tempFrame;

            for (auto frameIndex = 0u; frameIndex < frameSize; frameIndex += header.poseCount)
            {
                tempFrame.clear();
                tempFrame.resize(header.poseCount);

                char* poseIter = data + header.poseOffset;
                for (auto poseIndex = 0u; poseIndex < header.poseCount; ++poseIndex)
                {
                    Iqm::Pose pose;
                    poseIter = readPose(poseIter, pose);

                    glm::quat rotation(1.f, 0.f, 0.f, 0.f);
                    glm::vec3 translation(0.f);
                    glm::vec3 scale(1.f);

                    translation.x = pose.channelOffset[0];
                    if (pose.mask & 0x01) translation.x += *frameIter++ * pose.channelScale[0];
                    translation.y = pose.channelOffset[1];
                    if (pose.mask & 0x02) translation.y += *frameIter++ * pose.channelScale[1];
                    translation.z = pose.channelOffset[2];
                    if (pose.mask & 0x04) translation.z += *frameIter++ * pose.channelScale[2];

                    rotation.x = pose.channelOffset[3];
                    if (pose.mask & 0x08) rotation.x += *frameIter++ * pose.channelScale[3];
                    rotation.y = pose.channelOffset[4];
                    if (pose.mask & 0x10) rotation.y += *frameIter++ * pose.channelScale[4];
                    rotation.z = pose.channelOffset[5];
                    if (pose.mask & 0x20) rotation.z += *frameIter++ * pose.channelScale[5];
                    rotation.w = pose.channelOffset[6];
                    if (pose.mask & 0x40) rotation.w += *frameIter++ * pose.channelScale[6];

                    scale.x = pose.channelOffset[7];
                    if (pose.mask & 0x80) scale.x += *frameIter++ * pose.channelScale[7];
                    scale.y = pose.channelOffset[8];
                    if (pose.mask & 0x100) scale.y += *frameIter++ * pose.channelScale[8];
                    scale.z = pose.channelOffset[9];
                    if (pose.mask & 0x200) scale.z += *frameIter++ * pose.channelScale[9];

                    glm::mat4 mat = Iqm::createBoneMatrix(rotation, translation, scale);
                    tempFrame[poseIndex] = Joint(translation, rotation, scale);
                    tempFrame[poseIndex].parent = pose.parent;
                    tempFrame[poseIndex].worldMatrix = mat;
                }

                //each joint pose is stored in a temp frame. For the final output
                //we walk the node tree multiplying each joint by its parent, creating
                //a world matrix for every joint in the frame. TRS remains local for
                //interpolation. (temp joint actually contains local matrix in worldMatrix field)

                std::vector<cro::Joint> frame;
                for (auto i = 0u; i < tempFrame.size(); ++i)
                {
                    auto& tempJoint = tempFrame[i];

                    auto result = tempJoint.worldMatrix;
                    auto currentParent = tempJoint.parent;

                    while (currentParent != -1)
                    {
                        const auto& j = tempFrame[currentParent];

                        result = j.worldMatrix * result;
                        currentParent = j.parent;
                    }

                    auto& newJoint = frame.emplace_back(tempJoint);
                    newJoint.worldMatrix = /*rootTransform * */result;
                }
                out.addFrame(frame);
            }
        }
    }
    else
    {
        std::vector<cro::Joint> frame(inverseBindPose.size());
        out.addFrame(frame); //use an empty frame in case we haven't loaded any animations
    }


    //parse animations
    char*  animIter = data + header.animOffset;
    for (auto animIndex = 0u; animIndex < header.animCount; ++animIndex)
    {
        Iqm::Anim anim;
        animIter = readAnim(animIter, anim);

        SkeletalAnim skAnim;
        skAnim.frameCount = anim.frameCount;
        skAnim.frameRate = anim.framerate;
        skAnim.looped = ((anim.flags & Iqm::IQM_LOOP) != 0);
        skAnim.name =  &strings[anim.name];
        skAnim.startFrame = anim.firstFrame;   
        out.addAnimation(skAnim);
    }
}