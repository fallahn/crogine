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

#include "../detail/GLCheck.hpp"

#include <crogine/graphics/GridMeshBuilder.hpp>

using namespace cro;

GridMeshBuilder::GridMeshBuilder(glm::vec2 size, std::uint32_t subDivisions, glm::vec2 multiplier, bool colouredVerts)
    : m_size        (size),
    m_subDivisions  (subDivisions),
    m_uvMultiplier  (multiplier),
    m_colouredVerts (colouredVerts)
{
    CRO_ASSERT(subDivisions > 0, "Must be non-zero");
    CRO_ASSERT(multiplier.x > 0 && multiplier.y > 0, "Must be greater than zero");
}

//private
Mesh::Data GridMeshBuilder::build() const
{
    glm::vec2 stride = m_size / static_cast<float>(m_subDivisions);

    //pos, normal, tan, bit, uv
    std::vector<float> verts;

    auto xCount = m_subDivisions + 1;
    auto yCount = m_subDivisions + 1;

    for (auto y = 0u; y < yCount; ++y)
    {
        for (auto x = 0u; x < xCount; ++x)
        {
            verts.push_back(x * stride.x);
            verts.push_back(y * stride.y);
            verts.push_back(0.f);

            if (m_colouredVerts)
            {
                verts.push_back(1.f);
                verts.push_back(1.f);
                verts.push_back(1.f);
                verts.push_back(1.f);
            }

            verts.push_back(0.f);
            verts.push_back(0.f);
            verts.push_back(1.f);

            verts.push_back(1.f);
            verts.push_back(0.f);
            verts.push_back(0.f);

            verts.push_back(0.f);
            verts.push_back(1.f);
            verts.push_back(0.f);

            verts.push_back(((x * stride.x) / m_size.x) * m_uvMultiplier.x);
            verts.push_back(((y * stride.y) / m_size.y) * m_uvMultiplier.y);
        }
    }

    std::vector<std::uint32_t> indices;
    for (auto y = 0u; y < yCount - 1; y++)
    {
        if (y > 0) 
        {
            indices.push_back(y * xCount);
        }

        for (auto x = 0u; x < xCount; x++)
        {
            indices.push_back(((y + 1) * xCount) + x);
            indices.push_back((y * xCount) + x);
            
        }

        if (y < yCount - 2)
        {
            indices.push_back(((y + 1) * xCount) + (xCount - 1));
        }
    }


    Mesh::Data meshData;
    meshData.attributes[Mesh::Attribute::Position].size = 3;

    if (m_colouredVerts)
    {
        meshData.attributes[Mesh::Attribute::Colour].size = 4;
        meshData.attributeFlags = VertexProperty::Colour;
    }

    meshData.attributes[Mesh::Attribute::Normal].size = 3;
    meshData.attributes[Mesh::Attribute::Tangent].size = 3;
    meshData.attributes[Mesh::Attribute::Bitangent].size = 3;
    meshData.attributes[Mesh::Attribute::UV0].size = 2;
    meshData.attributeFlags |= (VertexProperty::Position | VertexProperty::Normal | VertexProperty::Tangent | VertexProperty::Bitangent | VertexProperty::UV0);

    meshData.primitiveType = GL_TRIANGLE_STRIP;
    meshData.vertexCount = verts.size() / getAttributeSize(meshData.attributes);
    meshData.vertexSize = getVertexSize(meshData.attributes);
    createVBO(meshData, verts);

    meshData.submeshCount = 1;
    meshData.indexData[0].format = GL_UNSIGNED_INT;
    meshData.indexData[0].primitiveType = meshData.primitiveType;
    meshData.indexData[0].indexCount = static_cast<std::uint32_t>(indices.size());
    createIBO(meshData, indices.data(), 0, sizeof(std::uint32_t));

    //spatial bounds
    meshData.boundingBox[0] = { 0.f, 0.f, -0.01f };
    meshData.boundingBox[1] = { m_size.x, m_size.y, 0.01f };
    meshData.boundingSphere.centre = meshData.boundingBox[1] / 2.f;
    meshData.boundingSphere.radius = glm::length(meshData.boundingSphere.centre);

    return meshData;
}