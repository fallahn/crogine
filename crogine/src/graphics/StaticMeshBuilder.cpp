/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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

#include "../detail/StaticMeshFile.hpp"

#include <crogine/graphics/StaticMeshBuilder.hpp>
#include <crogine/detail/OpenGL.hpp>

#include <crogine/detail/glm/geometric.hpp>

#include <limits>

using namespace cro;

StaticMeshBuilder::StaticMeshBuilder(const std::string& path)
    : m_path    (FileSystem::getResourcePath() + path),
    m_uid       (0)
{
    std::hash<std::string> hashAttack;
    m_uid = hashAttack(path);
}


//private
Mesh::Data StaticMeshBuilder::build() const
{
    Detail::MeshFile meshFile;
    if (Detail::readCMF(m_path, meshFile))
    {
        CRO_ASSERT(meshFile.flags && (meshFile.flags & VertexProperty::Position), "Invalid flag value");

        Mesh::Data meshData;
        meshData.attributes[Mesh::Position] = 3;
        meshData.attributeFlags = VertexProperty::Position;
        if (meshFile.flags & VertexProperty::Colour)
        {
            meshData.attributes[Mesh::Colour] = 3;
            meshData.attributeFlags |= VertexProperty::Colour;
        }

        if (meshFile.flags & VertexProperty::Normal)
        {
            meshData.attributes[Mesh::Normal] = 3;
            meshData.attributeFlags |= VertexProperty::Normal;
        }

        if (meshFile.flags & (VertexProperty::Tangent | VertexProperty::Bitangent))
        {
            meshData.attributes[Mesh::Tangent] = 3;
            meshData.attributes[Mesh::Bitangent] = 3;
            meshData.attributeFlags |= VertexProperty::Tangent | VertexProperty::Bitangent;
        }

        if (meshFile.flags & VertexProperty::UV0)
        {
            meshData.attributes[Mesh::UV0] = 2;
            meshData.attributeFlags |= VertexProperty::UV0;
        }
        if (meshFile.flags & VertexProperty::UV1)
        {
            meshData.attributes[Mesh::UV1] = 2;
            meshData.attributeFlags |= VertexProperty::UV1;
        }

        meshData.primitiveType = GL_TRIANGLES;       
        meshData.vertexSize = getVertexSize(meshData.attributes);
        meshData.vertexCount = meshFile.vboData.size() / (meshData.vertexSize / sizeof(float));
        createVBO(meshData, meshFile.vboData);

        meshData.submeshCount = meshFile.arrayCount;
        for (auto i = 0; i < meshFile.arrayCount; ++i)
        {
            meshData.indexData[i].format = GL_UNSIGNED_INT;
            meshData.indexData[i].primitiveType = meshData.primitiveType;
            meshData.indexData[i].indexCount = static_cast<std::uint32_t>(meshFile.indexArrays[i].size());

            createIBO(meshData, meshFile.indexArrays[i].data(), i, sizeof(std::uint32_t));
        }

        //boundingbox / sphere
        meshData.boundingBox[0] = glm::vec3(std::numeric_limits<float>::max());
        meshData.boundingBox[1] = glm::vec3(std::numeric_limits<float>::lowest());
        for (std::size_t i = 0; i < meshFile.vboData.size(); i += (meshData.vertexSize / sizeof(float)))
        {
            //min point
            if (meshData.boundingBox[0].x > meshFile.vboData[i])
            {
                meshData.boundingBox[0].x = meshFile.vboData[i];
            }
            if (meshData.boundingBox[0].y > meshFile.vboData[i+1])
            {
                meshData.boundingBox[0].y = meshFile.vboData[i+1];
            }
            if (meshData.boundingBox[0].z > meshFile.vboData[i+2])
            {
                meshData.boundingBox[0].z = meshFile.vboData[i+2];
            }

            //maxpoint
            if (meshData.boundingBox[1].x < meshFile.vboData[i])
            {
                meshData.boundingBox[1].x = meshFile.vboData[i];
            }
            if (meshData.boundingBox[1].y < meshFile.vboData[i + 1])
            {
                meshData.boundingBox[1].y = meshFile.vboData[i + 1];
            }
            if (meshData.boundingBox[1].z < meshFile.vboData[i + 2])
            {
                meshData.boundingBox[1].z = meshFile.vboData[i + 2];
            }
        }
        auto rad = (meshData.boundingBox[1] - meshData.boundingBox[0]) / 2.f;
        meshData.boundingSphere.centre = meshData.boundingBox[0] + rad;
        
        for (auto i = 0; i < 3; ++i)
        {
            auto l = std::abs(rad[i]);
            if (l > meshData.boundingSphere.radius)
            {
                meshData.boundingSphere.radius = l;
            }
        }

        return meshData;
    }
   
    return {};
}