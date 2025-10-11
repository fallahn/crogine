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

#include <crogine/graphics/CircleMeshBuilder.hpp>
#include <crogine/util/Constants.hpp>

using namespace cro;

CircleMeshBuilder::CircleMeshBuilder(float radius, std::uint32_t pointCount)
    : m_radius(radius),
    m_pointCount(pointCount)
{
    CRO_ASSERT(radius > 0, "Must have a positive radius");
    CRO_ASSERT(pointCount > 2, "Must have at least 3 points");
}

//private
Mesh::Data CircleMeshBuilder::build() const
{
    const float step = cro::Util::Const::TAU / m_pointCount;
    std::vector<glm::vec2> points;

    for (auto i = 0u; i < m_pointCount; ++i)
    {
        points.emplace_back(std::sin(i * step), std::cos(i * step)) *= m_radius;
    }

    //pos, normal, tan, bit, uv
    std::vector<float> verts;
    for (auto p : points)
    {
        verts.emplace_back(p.x);
        verts.emplace_back(p.y);
        verts.emplace_back(0.f);

        verts.emplace_back(0.f);
        verts.emplace_back(0.f);
        verts.emplace_back(1.f);

        verts.emplace_back(1.f);
        verts.emplace_back(0.f);
        verts.emplace_back(0.f);

        verts.emplace_back(0.f);
        verts.emplace_back(1.f);
        verts.emplace_back(0.f);

        auto u = p.x + m_radius;
        u /= (m_radius * 2.f);

        auto v = p.y + m_radius;
        v /= (m_radius * 2.f);

        verts.emplace_back(u);
        verts.emplace_back(v);
    }
    

    std::vector<std::uint32_t> indices;
    for (auto i = 0u; i < ((m_pointCount + 1) / 2); ++i)
    {
        indices.emplace_back(i);

        auto next = (m_pointCount - 1) - i;
        if (next > 0)
        {
            indices.emplace_back(next);
        }
    }


    Mesh::Data meshData;
    meshData.attributes[Mesh::Attribute::Position].componentCount = 3;
    meshData.attributes[Mesh::Attribute::Normal].componentCount = 3;
    meshData.attributes[Mesh::Attribute::Tangent].componentCount = 3;
    meshData.attributes[Mesh::Attribute::Bitangent].componentCount = 3;
    meshData.attributes[Mesh::Attribute::UV0].componentCount = 2;
    meshData.attributeFlags = (VertexProperty::Position | VertexProperty::Normal | VertexProperty::Tangent | VertexProperty::Bitangent | VertexProperty::UV0);
    meshData.primitiveType = GL_TRIANGLE_STRIP;
    meshData.vertexCount = verts.size() / 14;
    meshData.vertexSize = getVertexSize(meshData.attributes);
    createVBO(meshData, verts);

    meshData.submeshCount = 1;
    meshData.indexData[0].format = GL_UNSIGNED_INT;
    meshData.indexData[0].primitiveType = meshData.primitiveType;
    meshData.indexData[0].indexCount = static_cast<std::uint32_t>(indices.size());
    createIBO(meshData, indices.data(), 0, sizeof(std::uint32_t));

    //spatial bounds
    meshData.boundingBox[0] = { -m_radius, -m_radius, -0.01f };
    meshData.boundingBox[1] = { m_radius, m_radius, 0.01f };
    meshData.boundingSphere.centre = glm::vec3(0.f);
    meshData.boundingSphere.radius = m_radius;

    return meshData;
}