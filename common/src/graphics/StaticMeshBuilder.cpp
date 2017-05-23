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

#include <crogine/graphics/StaticMeshBuilder.hpp>
#include <crogine/detail/OpenGL.hpp>

#include <glm/geometric.hpp>


using namespace cro;

StaticMeshBuilder::StaticMeshBuilder(const std::string& path)
    : m_path    (path),
    m_file      (nullptr)
{


}

StaticMeshBuilder::~StaticMeshBuilder()
{
    if (m_file)
    {
        SDL_RWclose(m_file);
    }
}

//private
Mesh::Data StaticMeshBuilder::build() const
{
    m_file = SDL_RWFromFile(m_path.c_str(), "rb");
    if (m_file)
    {
        uint8 flags, arrayCount;
        auto readCount = SDL_RWread(m_file,&flags, sizeof(uint8), 1);
        if (checkError(readCount)) return {};
        readCount = SDL_RWread(m_file, &arrayCount, sizeof(uint8), 1);
        if (checkError(readCount)) return {};

        std::vector<int32> indexOffsets(arrayCount);
        std::vector<int32> indexSizes(arrayCount);
        readCount = SDL_RWread(m_file, indexOffsets.data(), sizeof(int32), arrayCount);
        if (checkError(readCount)) return {};
        readCount = SDL_RWread(m_file, indexSizes.data(), sizeof(int32), arrayCount);
        if (checkError(readCount)) return {};

        std::size_t headerSize = sizeof(flags) + sizeof(arrayCount) +
            ((sizeof(int32) * arrayCount) * 2);

        std::size_t vboSize = (indexOffsets[0] - headerSize) / sizeof(float);
        std::vector<float> vboData(vboSize);
        readCount = SDL_RWread(m_file, vboData.data(), sizeof(float), vboSize);
        if (checkError(readCount)) return {};

        std::vector<std::vector<uint32>> indexArrays(arrayCount);
        for (auto i = 0; i < arrayCount; ++i)
        {
            indexArrays[i].resize(indexSizes[i] / sizeof(uint32));
            readCount = SDL_RWread(m_file, indexArrays[i].data(), sizeof(uint32), indexSizes[i] / sizeof(uint32));
            if (checkError(readCount)) return {};
        }

        SDL_RWclose(m_file);
        m_file = nullptr;

        CRO_ASSERT(flags && (flags & (1 << Mesh::Position)), "Invalid flag value");

        Mesh::Data meshData;
        meshData.attributes[Mesh::Position] = 3;
        if (flags & (1 << Mesh::Colour))
        {
            meshData.attributes[Mesh::Colour] = 3;
        }

        meshData.attributes[Mesh::Normal] = 3;

        if (flags & ((1 << Mesh::Tangent) | (1 << Mesh::Bitangent)))
        {
            meshData.attributes[Mesh::Tangent] = 3;
            meshData.attributes[Mesh::Bitangent] = 3;
        }

        if (flags & (1 << Mesh::UV0))
        {
            meshData.attributes[Mesh::UV0] = 2;
        }
        if (flags & (1 << Mesh::UV1))
        {
            meshData.attributes[Mesh::UV1] = 2;
        }

        meshData.primitiveType = GL_TRIANGLES;       
        meshData.vertexSize = getVertexSize(meshData.attributes);
        meshData.vertexCount = vboData.size() / (meshData.vertexSize / sizeof(float));
        createVBO(meshData, vboData);

        meshData.submeshCount = arrayCount;
        for (auto i = 0; i < arrayCount; ++i)
        {
            meshData.indexData[i].format = GL_UNSIGNED_INT;
            meshData.indexData[i].primitiveType = meshData.primitiveType;
            meshData.indexData[i].indexCount = static_cast<uint32>(indexArrays[i].size());

            createIBO(meshData, indexArrays[i].data(), i, sizeof(uint32));
        }

        //boundingbox / sphere
        for (std::size_t i = 0; i < vboData.size(); i += meshData.vertexSize)
        {
            //min point
            if (meshData.boundingBox[0].x > vboData[i])
            {
                meshData.boundingBox[0].x = vboData[i];
            }
            if (meshData.boundingBox[0].y > vboData[i+1])
            {
                meshData.boundingBox[0].y = vboData[i+1];
            }
            if (meshData.boundingBox[0].z > vboData[i+2])
            {
                meshData.boundingBox[0].z = vboData[i+2];
            }

            //maxpoint
            if (meshData.boundingBox[1].x < vboData[i])
            {
                meshData.boundingBox[1].x = vboData[i];
            }
            if (meshData.boundingBox[1].y < vboData[i + 1])
            {
                meshData.boundingBox[1].y = vboData[i + 1];
            }
            if (meshData.boundingBox[1].z < vboData[i + 2])
            {
                meshData.boundingBox[1].z = vboData[i + 2];
            }
        }
        auto rad = (meshData.boundingBox[1] - meshData.boundingBox[0]) / 2.f;
        meshData.boundingSphere.centre = meshData.boundingBox[0] + rad;
        meshData.boundingSphere.radius = glm::length(rad);

        return meshData;
    }
   
    return {};
}

bool StaticMeshBuilder::checkError(std::size_t readCount) const
{
    if (readCount == 0)
    {
        Logger::log(m_path + ": Unexpected End of File", Logger::Type::Error);
        SDL_RWclose(m_file);
        m_file = nullptr;
        return true;
    }
    //else if (ferror(m_file))
    //{
    //    Logger::log(m_path + ": Error Reading File", Logger::Type::Error);
    //    fclose(m_file);
    //    m_file = nullptr;
    //    return true;
    //}
    return false;
}