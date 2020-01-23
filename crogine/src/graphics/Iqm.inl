/*-----------------------------------------------------------------------

Matt Marchant 2017
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

//iqm format specific structs
struct Header
{
    char magic[16];
    unsigned int version;
    unsigned int filesize;
    unsigned int flags;
    unsigned int textCount, textOffset;
    unsigned int meshCount, meshOffset;
    unsigned int varrayCount, vertexCount, varrayOffset;
    unsigned int triangleCount, triangleOffset, adjacencyOffset;
    unsigned int jointCount, jointOffset;
    unsigned int poseCount, poseOffset;
    unsigned int animCount, animOffset;
    unsigned int frameCount, frameChannelCount, frameOffset, boundsOffset;
    unsigned int commentCount, commentOffset;
    unsigned int extensionCount, extensionOffset;
};

struct Mesh
{
    unsigned int name;
    unsigned int material;
    unsigned int firstVertex, vertexCount;
    unsigned int firstTriangle, triangleCount;
};

enum
{
    POSITION = 0,
    TEXCOORD = 1,
    NORMAL = 2,
    TANGENT = 3,
    BLENDINDICES = 4,
    BLENDWEIGHTS = 5,
    COLOUR = 6,
    CUSTOM = 0x10
};

enum
{
    BYTE = 0,
    UBYTE = 1,
    SHORT = 2,
    USHORT = 3,
    INT = 4,
    UINT = 5,
    HALF = 6,
    FLOAT = 7,
    DOUBLE = 8
};

struct Triangle
{
    unsigned int vertex[3];
};

struct Adjacency
{
    unsigned int triangle[3];
};

struct Jointv1
{
    unsigned int name;
    int parent;
    float translate[3], rotate[3], scale[3];
};

struct Joint
{
    unsigned int name;
    int parent;
    float translate[3], rotate[4], scale[3];
};

struct Posev1
{
    int parent;
    unsigned int mask;
    float channelOffset[9];
    float channelScale[9];
};

struct Pose
{
    int parent;
    unsigned int mask;
    float channelOffset[10];
    float channelScale[10];
};

struct Anim
{
    unsigned int name;
    unsigned int firstFrame, frameCount;
    float framerate;
    unsigned int flags;
};

enum
{
    IQM_LOOP = 1 << 0
};

struct VertexArray
{
    unsigned int type;
    unsigned int flags;
    unsigned int format;
    unsigned int size;
    unsigned int offset;
};

struct Bounds
{
    float bbmin[3], bbmax[3];
    float xyradius, radius;
};

struct Extension
{
    unsigned int name;
    unsigned int dataCount, dataOffset;
    unsigned int extensionOffset; // pointer to next extension
};


//utility functions when parsing
const std::string IQM_MAGIC = "INTERQUAKEMODEL";
const std::uint8_t IQM_VERSION = 2u;

//data size of vertex properties
const std::uint8_t positionSize = 3u;
const std::uint8_t normalSize = 3u;
const std::uint8_t uvSize = 2u;
const std::uint8_t blendIndexSize = 4u;
const std::uint8_t blendWeightSize = 4u;
const std::uint8_t colourSize = 4u;

char* readHeader(char* headerPtr, Iqm::Header& dest)
{
    std::memcpy(&dest.magic, headerPtr, sizeof(dest.magic));
    headerPtr += sizeof(dest.magic);
    std::memcpy(&dest.version, headerPtr, sizeof(dest.version));
    headerPtr += sizeof(dest.version);
    std::memcpy(&dest.filesize, headerPtr, sizeof(dest.filesize));
    headerPtr += sizeof(dest.filesize);
    std::memcpy(&dest.flags, headerPtr, sizeof(dest.flags));
    headerPtr += sizeof(dest.flags);
    std::memcpy(&dest.textCount, headerPtr, sizeof(dest.textCount));
    headerPtr += sizeof(dest.textCount);
    std::memcpy(&dest.textOffset, headerPtr, sizeof(dest.textOffset));
    headerPtr += sizeof(dest.textOffset);
    std::memcpy(&dest.meshCount, headerPtr, sizeof(dest.meshCount));
    headerPtr += sizeof(dest.meshCount);
    std::memcpy(&dest.meshOffset, headerPtr, sizeof(dest.meshOffset));
    headerPtr += sizeof(dest.meshOffset);
    std::memcpy(&dest.varrayCount, headerPtr, sizeof(dest.varrayCount));
    headerPtr += sizeof(dest.varrayCount);
    std::memcpy(&dest.vertexCount, headerPtr, sizeof(dest.vertexCount));
    headerPtr += sizeof(dest.vertexCount);
    std::memcpy(&dest.varrayOffset, headerPtr, sizeof(dest.varrayOffset));
    headerPtr += sizeof(dest.varrayOffset);
    std::memcpy(&dest.triangleCount, headerPtr, sizeof(dest.triangleCount));
    headerPtr += sizeof(dest.triangleCount);
    std::memcpy(&dest.triangleOffset, headerPtr, sizeof(dest.triangleOffset));
    headerPtr += sizeof(dest.triangleOffset);
    std::memcpy(&dest.adjacencyOffset, headerPtr, sizeof(dest.adjacencyOffset));
    headerPtr += sizeof(dest.adjacencyOffset);
    std::memcpy(&dest.jointCount, headerPtr, sizeof(dest.jointCount));
    headerPtr += sizeof(dest.jointCount);
    std::memcpy(&dest.jointOffset, headerPtr, sizeof(dest.jointOffset));
    headerPtr += sizeof(dest.jointOffset);
    std::memcpy(&dest.poseCount, headerPtr, sizeof(dest.poseCount));
    headerPtr += sizeof(dest.poseCount);
    std::memcpy(&dest.poseOffset, headerPtr, sizeof(dest.poseOffset));
    headerPtr += sizeof(dest.poseOffset);
    std::memcpy(&dest.animCount, headerPtr, sizeof(dest.animCount));
    headerPtr += sizeof(dest.animCount);
    std::memcpy(&dest.animOffset, headerPtr, sizeof(dest.animOffset));
    headerPtr += sizeof(dest.animOffset);
    std::memcpy(&dest.frameCount, headerPtr, sizeof(dest.frameCount));
    headerPtr += sizeof(dest.frameCount);
    std::memcpy(&dest.frameChannelCount, headerPtr, sizeof(dest.frameChannelCount));
    headerPtr += sizeof(dest.frameChannelCount);
    std::memcpy(&dest.frameOffset, headerPtr, sizeof(dest.frameOffset));
    headerPtr += sizeof(dest.frameOffset);
    std::memcpy(&dest.boundsOffset, headerPtr, sizeof(dest.boundsOffset));
    headerPtr += sizeof(dest.boundsOffset);
    std::memcpy(&dest.commentCount, headerPtr, sizeof(dest.commentCount));
    headerPtr += sizeof(dest.commentCount);
    std::memcpy(&dest.commentOffset, headerPtr, sizeof(dest.commentOffset));
    headerPtr += sizeof(dest.commentOffset);
    std::memcpy(&dest.extensionCount, headerPtr, sizeof(dest.extensionCount));
    headerPtr += sizeof(dest.extensionCount);
    std::memcpy(&dest.extensionOffset, headerPtr, sizeof(dest.extensionOffset));
    headerPtr += sizeof(dest.extensionOffset);

    return headerPtr;
}

char* readVertexArray(char* vertArrayPtr, Iqm::VertexArray& dest)
{
    std::memcpy(&dest.type, vertArrayPtr, sizeof(dest.type));
    vertArrayPtr += sizeof(dest.type);
    std::memcpy(&dest.flags, vertArrayPtr, sizeof(dest.flags));
    vertArrayPtr += sizeof(dest.flags);
    std::memcpy(&dest.format, vertArrayPtr, sizeof(dest.format));
    vertArrayPtr += sizeof(dest.format);
    std::memcpy(&dest.size, vertArrayPtr, sizeof(dest.size));
    vertArrayPtr += sizeof(dest.size);
    std::memcpy(&dest.offset, vertArrayPtr, sizeof(dest.offset));
    vertArrayPtr += sizeof(dest.offset);

    return vertArrayPtr;
}

char* readMesh(char* meshArrayPtr, Iqm::Mesh& dest)
{
    std::memcpy(&dest.name, meshArrayPtr, sizeof(dest.name));
    meshArrayPtr += sizeof(dest.name);
    std::memcpy(&dest.material, meshArrayPtr, sizeof(dest.material));
    meshArrayPtr += sizeof(dest.material);
    std::memcpy(&dest.firstVertex, meshArrayPtr, sizeof(dest.firstVertex));
    meshArrayPtr += sizeof(dest.firstVertex);
    std::memcpy(&dest.vertexCount, meshArrayPtr, sizeof(dest.vertexCount));
    meshArrayPtr += sizeof(dest.vertexCount);
    std::memcpy(&dest.firstTriangle, meshArrayPtr, sizeof(dest.firstTriangle));
    meshArrayPtr += sizeof(dest.firstTriangle);
    std::memcpy(&dest.triangleCount, meshArrayPtr, sizeof(dest.triangleCount));
    meshArrayPtr += sizeof(dest.triangleCount);

    return meshArrayPtr;
}

char* readJoint(char* jointArrayPtr, Iqm::Joint& dest)
{
    std::memcpy(&dest.name, jointArrayPtr, sizeof(dest.name));
    jointArrayPtr += sizeof(dest.name);
    std::memcpy(&dest.parent, jointArrayPtr, sizeof(dest.parent));
    jointArrayPtr += sizeof(dest.parent);
    std::memcpy(&dest.translate, jointArrayPtr, sizeof(dest.translate));
    jointArrayPtr += sizeof(dest.translate);
    std::memcpy(&dest.rotate, jointArrayPtr, sizeof(dest.rotate));
    jointArrayPtr += sizeof(dest.rotate);
    std::memcpy(&dest.scale, jointArrayPtr, sizeof(dest.scale));
    jointArrayPtr += sizeof(dest.scale);

    return jointArrayPtr;
}

char* readPose(char* poseArrayPtr, Iqm::Pose& dest)
{
    std::memcpy(&dest.parent, poseArrayPtr, sizeof(dest.parent));
    poseArrayPtr += sizeof(dest.parent);
    std::memcpy(&dest.mask, poseArrayPtr, sizeof(dest.mask));
    poseArrayPtr += sizeof(dest.mask);
    std::memcpy(&dest.channelOffset, poseArrayPtr, sizeof(dest.channelOffset));
    poseArrayPtr += sizeof(dest.channelOffset);
    std::memcpy(&dest.channelScale, poseArrayPtr, sizeof(dest.channelScale));
    poseArrayPtr += sizeof(dest.channelScale);

    return poseArrayPtr;
}

char* readAnim(char* animArrayPtr, Iqm::Anim& dest)
{
    std::memcpy(&dest.name, animArrayPtr, sizeof(dest.name));
    animArrayPtr += sizeof(dest.name);
    std::memcpy(&dest.firstFrame, animArrayPtr, sizeof(dest.firstFrame));
    animArrayPtr += sizeof(dest.firstFrame);
    std::memcpy(&dest.frameCount, animArrayPtr, sizeof(dest.frameCount));
    animArrayPtr += sizeof(dest.frameCount);
    std::memcpy(&dest.framerate, animArrayPtr, sizeof(dest.framerate));
    animArrayPtr += sizeof(dest.framerate);
    std::memcpy(&dest.flags, animArrayPtr, sizeof(dest.flags));
    animArrayPtr += sizeof(dest.flags);

    return animArrayPtr;
}

glm::mat4 createBoneMatrix(const glm::quat& rotation, const glm::vec3& translation, const glm::vec3& scale)
{
    /*glm::mat3 rotMat = glm::inverse(glm::mat3_cast(glm::normalize(rotation)));
    rotMat[0] *= scale;
    rotMat[1] *= scale;
    rotMat[2] *= scale;

    glm::mat4 retVal(rotMat);
    retVal[0].w = translation.x;
    retVal[1].w = translation.y;
    retVal[2].w = translation.z;
    retVal[3].w = 1.0;

    return glm::transpose(retVal);*/

    glm::mat4 t = glm::translate(glm::mat4(1.f), translation);

    glm::mat4 rot = glm::toMat4(rotation);
    rot = glm::scale(rot, scale);
    //rot = glm::translate(rot, -m_origin);

    return t * rot;
}

//used in normal calcs
glm::vec2 vec2FromArray(std::uint32_t index, const std::vector<float>& arr)
{
    auto offset = index * 2u;
    return glm::vec2(arr[offset], arr[offset + 1u]);
}

glm::vec3 vec3FromArray(std::uint32_t index, const std::vector<float>& arr)
{
    auto offset = index * 3u;
    return glm::vec3(arr[offset], arr[offset + 1u], arr[offset + 2u]);
}

void addVecToArray(std::uint32_t index, std::vector<float>& arr, const glm::vec3& v)
{
    auto offset = index * 3u;
    arr[offset] += v.x;
    arr[offset + 1u] += v.y;
    arr[offset + 2u] += v.z;
}

void normaliseVec3Array(std::vector<float>& arr)
{
    for (auto i = 0u; i < arr.size(); i += 3u)
    {
        glm::vec3 vec(arr[i], arr[i + 1u], arr[i + 2u]);
        vec = glm::normalize(vec);
        arr[i] = vec.x;
        arr[i + 1u] = vec.y;
        arr[i + 2u] = vec.z;
    }
}