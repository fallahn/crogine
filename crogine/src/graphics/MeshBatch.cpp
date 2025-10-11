/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2025
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
#include "../detail/GLCheck.hpp"

#include <crogine/detail/Assert.hpp>
#include <crogine/detail/glm/gtc/matrix_inverse.hpp>

#include <crogine/core/FileSystem.hpp>

#include <crogine/graphics/MeshBatch.hpp>
#include <crogine/graphics/MeshData.hpp>
#include <crogine/graphics/MeshBuilder.hpp>

using namespace cro;

MeshBatch::MeshBatch(std::uint32_t flags)
    : m_flags(flags)
{
    CRO_ASSERT((flags & VertexProperty::Position), "Requires at least a position attribute");
}

//public
bool MeshBatch::addMesh(const std::string& path, const std::vector<glm::mat4>& transforms)
{
    if ((m_flags & VertexProperty::Position) == 0)
    {
        LogE << "Invalid flags specified, requires at least psoition attribute" << std::endl;
        return false;
    }

    //validate path
    if (!FileSystem::fileExists(path)
        || FileSystem::getFileExtension(path) != ".cmf")
    {
        LogE << "Failed opening " << path << ": file not found or not correct format" << std::endl;
        return false;
    }

    //open/validate data
    Detail::MeshFile meshFile;
    if (!Detail::readCMF(path, meshFile))
    {
        LogE << "Failed to open " << path << std::endl;
        return false;
    }

    //validate mesh flags
    if (meshFile.flags != m_flags)
    {
        LogE << path << ": mesh flags do not match" << std::endl;
        return false;
    }

    //check the vertex size
    std::uint32_t stride = 0;
    for (auto i = 0u; i < Mesh::Attribute::UV0; ++i)
    {
        if (meshFile.flags & (1 << i))
        {
            stride += 3;
        }
    }
    for (auto i = static_cast<std::int32_t>(Mesh::Attribute::UV0); i < Mesh::Attribute::UV1; ++i)
    {
        if (meshFile.flags & (1 << i))
        {
            stride += 2;
        }
    }

    CRO_ASSERT(meshFile.vboData.size() % stride == 0, "Incorrect stride");

    //...aaand, transform
    std::uint32_t indexOffset = static_cast<std::uint32_t>(m_vertexData.size()) / stride;
    
    for (const auto& tx : transforms)
    {
        for (auto i = 0u; i < meshFile.vboData.size(); i += stride)
        {
            //translate/rotate/scale position
            glm::vec4 position = { meshFile.vboData[i], meshFile.vboData[i+1], meshFile.vboData[i+2], 1.f };
            position = tx * position;

            m_vertexData.push_back(position.x);
            m_vertexData.push_back(position.y);
            m_vertexData.push_back(position.z);

            auto offset = i + 3;
            if (meshFile.flags & VertexProperty::Colour)
            {
                m_vertexData.push_back(meshFile.vboData[offset]);
                m_vertexData.push_back(meshFile.vboData[offset+1]);
                m_vertexData.push_back(meshFile.vboData[offset+2]);
                offset += 3;
            }

            if (meshFile.flags & VertexProperty::Normal)
            {
                //rotate normals/tangents
                auto normalMatrix = glm::inverseTranspose(glm::mat3(tx));

                glm::vec3 normal = { meshFile.vboData[offset], meshFile.vboData[offset+1], meshFile.vboData[offset+2] };
                normal = normalMatrix * normal;

                m_vertexData.push_back(normal.x);
                m_vertexData.push_back(normal.y);
                m_vertexData.push_back(normal.z);
                offset += 3;

                if (meshFile.flags & VertexProperty::Tangent)
                {
                    //assumes we always have bitan too (as per mesh spec)
                    for (auto j = 0u; j < 2; ++j)
                    {
                        normal = { meshFile.vboData[offset], meshFile.vboData[offset + 1], meshFile.vboData[offset + 2] };
                        normal = normalMatrix * normal;

                        m_vertexData.push_back(normal.x);
                        m_vertexData.push_back(normal.y);
                        m_vertexData.push_back(normal.z);
                        offset += 3;
                    }
                }
            }

            if (meshFile.flags & VertexProperty::UV0)
            {
                m_vertexData.push_back(meshFile.vboData[offset]);
                m_vertexData.push_back(meshFile.vboData[offset + 1]);
                offset += 2;
            }

            if (meshFile.flags & VertexProperty::UV1)
            {
                m_vertexData.push_back(meshFile.vboData[offset]);
                m_vertexData.push_back(meshFile.vboData[offset + 1]);
                offset += 2;
            }

            CRO_ASSERT(offset % stride == 0, "Missed some data somewhere...");
        }

        //update the index arrays.
        for (auto i = 0u; i < meshFile.arrayCount; ++i)
        {
            for (auto idx : meshFile.indexArrays[i])
            {
                m_indexData[i].push_back(indexOffset + idx);
            }
        }

        indexOffset = static_cast<std::uint32_t>(m_vertexData.size()) / stride;
    }

    return true;
}

void MeshBatch::updateMeshData(Mesh::Data& data) const
{
    CRO_ASSERT(data.attributeFlags == m_flags, "Flags do not match!");
    CRO_ASSERT(data.vboAllocation.vboID != 0, "Not a valid vertex buffer. Must be created with a MeshResource first");
    CRO_ASSERT(data.submeshCount > 0, "Not a valid mesh");

    if (data.attributeFlags == m_flags)
    {
        //upload to vbo/ibo
        data.vertexCount = m_vertexData.size() / (data.vertexSize / sizeof(float));
        
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, data.vboAllocation.vboID));
        glCheck(glBufferData(GL_ARRAY_BUFFER, data.vertexSize * data.vertexCount, m_vertexData.data(), GL_STATIC_DRAW));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

        for (auto i = 0u; i < data.submeshCount; ++i)
        {
            data.indexData[i].indexCount = static_cast<std::uint32_t>(m_indexData[i].size());
            
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.indexData[i].ibo));
            glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, data.indexData[i].indexCount * sizeof(std::uint32_t), m_indexData[i].data(), GL_STATIC_DRAW));
        }
        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));



        //boundingbox / sphere
        data.boundingBox[0] = glm::vec3(std::numeric_limits<float>::max());
        data.boundingBox[1] = glm::vec3(std::numeric_limits<float>::min());
        for (std::size_t i = 0; i < m_vertexData.size(); i += (data.vertexSize / sizeof(float)))
        {
            //min point
            if (data.boundingBox[0].x > m_vertexData[i])
            {
                data.boundingBox[0].x = m_vertexData[i];
            }
            if (data.boundingBox[0].y > m_vertexData[i + 1])
            {
                data.boundingBox[0].y = m_vertexData[i + 1];
            }
            if (data.boundingBox[0].z > m_vertexData[i + 2])
            {
                data.boundingBox[0].z = m_vertexData[i + 2];
            }

            //maxpoint
            if (data.boundingBox[1].x < m_vertexData[i])
            {
                data.boundingBox[1].x = m_vertexData[i];
            }
            if (data.boundingBox[1].y < m_vertexData[i + 1])
            {
                data.boundingBox[1].y = m_vertexData[i + 1];
            }
            if (data.boundingBox[1].z < m_vertexData[i + 2])
            {
                data.boundingBox[1].z = m_vertexData[i + 2];
            }
        }
        auto rad = (data.boundingBox[1] - data.boundingBox[0]) / 2.f;
        data.boundingSphere.centre = data.boundingBox[0] + rad;
        data.boundingSphere.radius = glm::length(rad);
    }
}