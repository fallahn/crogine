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

#include <crogine/graphics/QuadBuilder.hpp>
#include <crogine/graphics/CubeBuilder.hpp>
#include <crogine/graphics/SphereBuilder.hpp>

#include <crogine/detail/Assert.hpp>

#include <crogine/detail/glm/vec3.hpp>
#include <crogine/detail/glm/gtc/packing.hpp>

#include "../detail/GLCheck.hpp"

#include <functional>

using namespace cro;

namespace
{
    static const float PI = 3.1412f;
    static const float degToRad = PI / 180.f;
}

QuadBuilder::QuadBuilder(glm::vec2 size, FloatRect textureRect)
    : m_size        (size),
    m_textureRect   (textureRect),
    m_uid           (0)
{
    CRO_ASSERT(size.x > 0 && size.y > 0, "Invalid size");

    std::hash<std::string> hashIt;
    //this isn't flawless, for example if dimensions are swapped....
    m_uid = hashIt(std::to_string(size.x) + std::to_string(size.y) + std::to_string(textureRect.left) + std::to_string(textureRect.height));
}

Mesh::Data QuadBuilder::build() const
{
    Mesh::Data meshData;

    const float halfSizeX = m_size.x / 2.f;
    const float halfSizeY = m_size.y / 2.f;
    std::vector<float> vertexData =
    {
        -halfSizeX, halfSizeY, 0.f,   0.f,0.f,1.f,   1.f,0.f,0.f,   0.f,1.f,0.f,     m_textureRect.left, m_textureRect.bottom + m_textureRect.height,
        -halfSizeX, -halfSizeY, 0.f,  0.f,0.f,1.f,   1.f,0.f,0.f,   0.f,1.f,0.f,     m_textureRect.left, m_textureRect.bottom,
        halfSizeX, halfSizeY, 0.f,    0.f,0.f,1.f,   1.f,0.f,0.f,   0.f,1.f,0.f,     m_textureRect.left + m_textureRect.width, m_textureRect.bottom + m_textureRect.height,
        halfSizeX, -halfSizeY, 0.f,   0.f,0.f,1.f,   1.f,0.f,0.f,   0.f,1.f,0.f,     m_textureRect.left + m_textureRect.width, m_textureRect.bottom
    };

    meshData.attributes[Mesh::Attribute::Position].size = 3;
    meshData.attributes[Mesh::Attribute::Normal].size = 3;
    meshData.attributes[Mesh::Attribute::Tangent].size = 3;
    meshData.attributes[Mesh::Attribute::Bitangent].size = 3;
    meshData.attributes[Mesh::Attribute::UV0].size = 2;
    meshData.attributeFlags = (VertexProperty::Position | VertexProperty::Normal | VertexProperty::Tangent | VertexProperty::Bitangent | VertexProperty::UV0);
    meshData.primitiveType = GL_TRIANGLE_STRIP;
    meshData.vertexCount = 4;
    meshData.vertexSize = getVertexSize(meshData.attributes);
    createVBO(meshData, vertexData);


    //index arrays
    std::array<std::uint8_t, 4u> idxData = { { 0, 1, 2, 3 } };
    meshData.submeshCount = 1;
    meshData.indexData[0].format = GL_UNSIGNED_BYTE;
    meshData.indexData[0].primitiveType = meshData.primitiveType;
    meshData.indexData[0].indexCount = static_cast<std::uint32_t>(idxData.size());

    createIBO(meshData, idxData.data(), 0, sizeof(std::uint8_t));

    //spatial bounds
    meshData.boundingBox[0] = { -halfSizeX, -halfSizeY, -0.01f };
    meshData.boundingBox[1] = { halfSizeX, halfSizeX, 0.01f };
    meshData.boundingSphere.radius = glm::length(meshData.boundingBox[0]);

    return meshData;
}

//------------------------------------

CubeBuilder::CubeBuilder(glm::vec3 dimensions)
    : m_uid     (0),
    m_dimensions(dimensions) 
{
    CRO_ASSERT(dimensions.x > 0 && dimensions.y > 0 && dimensions.z > 0, "Must have non-negative size");

    std::size_t x = static_cast<std::size_t>(dimensions.x * 1000.f);
    std::size_t y = static_cast<std::size_t>(dimensions.y * 1000.f);
    std::size_t z = static_cast<std::size_t>(dimensions.z * 1000.f);

    m_uid = x << 32 | y << 16 | z;
}

Mesh::Data CubeBuilder::build() const
{
    Mesh::Data meshData;

    auto dim = m_dimensions / 2.f;

    std::vector<float> vertexData =
    {
        //front
        -dim.x, dim.y, dim.z,   0.f,0.f,1.f,   1.f,0.f,0.f,   0.f,1.f,0.f,     0.f, 1.f,
        -dim.x, -dim.y, dim.z,  0.f,0.f,1.f,   1.f,0.f,0.f,   0.f,1.f,0.f,     0.f, 0.f,
        dim.x, dim.y, dim.z,    0.f,0.f,1.f,   1.f,0.f,0.f,   0.f,1.f,0.f,     1.f, 1.f,
        dim.x, -dim.y, dim.z,   0.f,0.f,1.f,   1.f,0.f,0.f,   0.f,1.f,0.f,     1.f, 0.f,
        //back
        dim.x, dim.y, -dim.z,   0.f,0.f,-1.f,   -1.f,0.f,0.f,   0.f,1.f,0.f,     0.f, 1.f,
        dim.x, -dim.y, -dim.z,  0.f,0.f,-1.f,   -1.f,0.f,0.f,   0.f,1.f,0.f,     0.f, 0.f,
        -dim.x, dim.y, -dim.z,    0.f,0.f,-1.f,   -1.f,0.f,0.f,   0.f,1.f,0.f,     1.f, 1.f,
        -dim.x, -dim.y, -dim.z,   0.f,0.f,-1.f,   -1.f,0.f,0.f,   0.f,1.f,0.f,     1.f, 0.f,
        //left
        -dim.x, dim.y, -dim.z,   -1.f,0.f,0.f,   0.f,0.f,1.f,   0.f,1.f,0.f,     0.f, 1.f,
        -dim.x, -dim.y, -dim.z,  -1.f,0.f,0.f,   0.f,0.f,1.f,   0.f,1.f,0.f,     0.f, 0.f,
        -dim.x, dim.y, dim.z,    -1.f,0.f,0.f,   0.f,0.f,1.f,   0.f,1.f,0.f,     1.f, 1.f,
        -dim.x, -dim.y, dim.z,   -1.f,0.f,0.f,   0.f,0.f,1.f,   0.f,1.f,0.f,     1.f, 0.f,
        //right
        dim.x, dim.y, dim.z,   1.f,0.f,0.f,   0.f,0.f,-1.f,   0.f,1.f,0.f,     0.f, 1.f,
        dim.x, -dim.y, dim.z,  1.f,0.f,0.f,   0.f,0.f,-1.f,   0.f,1.f,0.f,     0.f, 0.f,
        dim.x, dim.y, -dim.z,    1.f,0.f,0.f,   0.f,0.f,-1.f,   0.f,1.f,0.f,     1.f, 1.f,
        dim.x, -dim.y, -dim.z,   1.f,0.f,0.f,   0.f,0.f,-1.f,   0.f,1.f,0.f,     1.f, 0.f,
        //top
        -dim.x, dim.y, -dim.z,   0.f,1.f,0.f,   1.f,0.f,0.f,   0.f,0.f,-1.f,     0.f, 1.f,
        -dim.x, dim.y, dim.z,  0.f,1.f,0.f,   1.f,0.f,0.f,   0.f,0.f,-1.f,     0.f, 0.f,
        dim.x, dim.y, -dim.z,    0.f,1.f,0.f,   1.f,0.f,0.f,   0.f,0.f,-1.f,     1.f, 1.f,
        dim.x, dim.y, dim.z,   0.f,1.f,0.f,   1.f,0.f,0.f,   0.f,0.f,-1.f,     1.f, 0.f,
        //bottom
        -dim.x, -dim.y, dim.z,   0.f,-1.f,0.f,   -1.f,0.f,0.f,   0.f,0.f,-1.f,     0.f, 1.f,
        -dim.x, -dim.y, -dim.z,  0.f,-1.f,0.f,   -1.f,0.f,0.f,   0.f,0.f,-1.f,     0.f, 0.f,
        dim.x, -dim.y, dim.z,    0.f,-1.f,0.f,   -1.f,0.f,0.f,   0.f,0.f,-1.f,     1.f, 1.f,
        dim.x, -dim.y, -dim.z,   0.f,-1.f,0.f,   -1.f,0.f,0.f,   0.f,0.f,-1.f,     1.f, 0.f
    };

    meshData.attributes[Mesh::Attribute::Position].size = 3;
    meshData.attributes[Mesh::Attribute::Normal].size = 3;
    meshData.attributes[Mesh::Attribute::Tangent].size = 3;
    meshData.attributes[Mesh::Attribute::Bitangent].size = 3;
    meshData.attributes[Mesh::Attribute::UV0].size = 2;
    meshData.attributeFlags = (VertexProperty::Position | VertexProperty::Normal | VertexProperty::Tangent | VertexProperty::Bitangent | VertexProperty::UV0);
    meshData.primitiveType = GL_TRIANGLES;
    meshData.vertexCount = 24;
    meshData.vertexSize = getVertexSize(meshData.attributes);
    createVBO(meshData, vertexData);


    //index arrays
    std::array<std::uint16_t, 36u> idxData =
    { {
            0, 1, 2, 1, 3, 2,
            4, 5, 6, 5, 7, 6,
            8, 9, 10, 9, 11, 10,
            12, 13, 14, 13, 15, 14,
            16, 17, 18, 17, 19 ,18,
            20, 21, 22, 21, 23, 22
        } };
    meshData.submeshCount = 1;
    meshData.indexData[0].format = GL_UNSIGNED_SHORT;
    meshData.indexData[0].primitiveType = meshData.primitiveType;
    meshData.indexData[0].indexCount = static_cast<std::uint32_t>(idxData.size());

    createIBO(meshData, idxData.data(), 0, sizeof(std::uint16_t));
    
    meshData.boundingBox[0] = -dim;
    meshData.boundingBox[1] = dim;
    meshData.boundingSphere.radius = glm::length(meshData.boundingBox[0]);

    return meshData;
}

//------------------------------------

SphereBuilder::SphereBuilder(float radius, std::size_t resolution)
    :m_radius       (radius),
    m_resolution    (resolution),
    m_uid           (0)
{
    CRO_ASSERT(radius > 0, "Invalid radius");
    CRO_ASSERT(resolution > 0, "Invalid face resolution");

    std::hash<std::string> hashIt;
    m_uid = hashIt(std::to_string(radius) + std::to_string(resolution));
}

Mesh::Data SphereBuilder::build() const
{
    Mesh::Data meshData;
    std::vector<float> vertexData;
    std::vector<std::uint16_t> indices;

    //generate vertex layout
    std::size_t offset = 0; //offset into index array as we add faces
    std::function<void(const glm::quat&, const glm::vec2&)> buildFace = [&, this](const glm::quat& rotation, const glm::vec2& uvOffset)
    {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec2> uvs;
        const float spacing = 1.f / m_resolution;
        const glm::vec2 uvSpacing((1.f / 2.f) / static_cast<float>(m_resolution), (1.f / 3.f) / static_cast<float>(m_resolution));
        for (auto y = 0u; y <= m_resolution; ++y)
        {
            for (auto x = 0u; x <= m_resolution; ++x)
            {
                positions.emplace_back((x * spacing) - 0.5f, (y * spacing) - 0.5f, 0.5f);
                uvs.emplace_back(uvOffset.x + (uvSpacing.x * x), uvOffset.y + (uvSpacing.y * y));
            }
        }

        for (auto i = 0u; i < positions.size(); ++i)
        {
            auto vert = glm::normalize(positions[i]) * rotation;
            vertexData.emplace_back(vert.x * m_radius);
            vertexData.emplace_back(vert.y * m_radius);
            vertexData.emplace_back(vert.z * m_radius);
            //normals
            vertexData.emplace_back(vert.x);
            vertexData.emplace_back(vert.y);
            vertexData.emplace_back(vert.z);

            //because we're on a grid the tan points to
            //the next vertex position - unless we're
            //at the end of a line
            glm::vec3 tan = glm::vec3(0.f);
            if (i % (m_resolution + 1) == (m_resolution))
            {
                //end of the line
                glm::vec3 a = glm::normalize(positions[i - 1]);
                glm::vec3 b = glm::normalize(positions[i]);

                tan = glm::normalize(a - b) * rotation;
                tan = glm::reflect(tan, vert);
            }
            else
            {
                glm::vec3 a = glm::normalize(positions[i + 1]);
                glm::vec3 b = glm::normalize(positions[i]);
                tan = glm::normalize(a - b) * rotation;
            }
            glm::vec3 bitan = glm::cross(tan, vert);

            vertexData.emplace_back(tan.x);
            vertexData.emplace_back(tan.y);
            vertexData.emplace_back(tan.z);

            vertexData.emplace_back(bitan.x);
            vertexData.emplace_back(bitan.y);
            vertexData.emplace_back(bitan.z);

            //UVs
            vertexData.emplace_back(uvs[i].x);
            vertexData.emplace_back(uvs[i].y);
        }

        //update indices
        for (auto y = 0u; y <= m_resolution; ++y)
        {
            auto start = y * m_resolution;
            for (auto x = 0u; x <= m_resolution; ++x)
            {
                indices.push_back(static_cast<std::uint16_t>(offset + start + m_resolution + x));
                indices.push_back(static_cast<std::uint16_t>(offset + start + x));
            }

            //add a degenerate at the end of the row bar last
            if (y < m_resolution - 1)
            {
                indices.push_back(static_cast<std::uint16_t>(offset + ((y + 1) * m_resolution + (m_resolution - 1))));
                indices.push_back(static_cast<std::uint16_t>(offset + ((y + 1) * m_resolution)));
            }
        }
        offset += positions.size();
    };

    //build each face rotating as we go
    //mapping to 2x3 texture atlas
    const float u = 1.f / 2.f;
    const float v = 1.f / 3.f;
    buildFace(glm::quat(1.f, 0.f, 0.f, 0.f), { 0.f, v * 2.f }); //Z+
    glm::quat rotation = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), degToRad * 90.f, glm::vec3(0.f, 1.f, 0.f));
    buildFace(rotation, { u, 0.f }); //X-

    rotation = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), degToRad * 180.f, glm::vec3(0.f, 1.f, 0.f));
    buildFace(rotation, { u, v * 2.f }); //Z-

    rotation = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), degToRad * 270.f, glm::vec3(0.f, 1.f, 0.f));
    buildFace(rotation, {}); //X+

    rotation = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), degToRad * 90.f, glm::vec3(1.f, 0.f, 0.f));
    buildFace(rotation, { u, v }); //Y-

    rotation = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), degToRad * -90.f, glm::vec3(1.f, 0.f, 0.f));
    buildFace(rotation, { 0.f, v }); //Y+


    meshData.attributes[Mesh::Attribute::Position].size = 3;
    meshData.attributes[Mesh::Attribute::Normal].size = 3;
    meshData.attributes[Mesh::Attribute::Tangent].size = 3;
    meshData.attributes[Mesh::Attribute::Bitangent].size = 3;
    meshData.attributes[Mesh::Attribute::UV0].size = 2;
    meshData.attributeFlags = (VertexProperty::Position | VertexProperty::Normal | VertexProperty::Tangent | VertexProperty::Bitangent | VertexProperty::UV0);
    meshData.primitiveType = GL_TRIANGLE_STRIP;
    
    meshData.vertexSize = getVertexSize(meshData.attributes);
    meshData.vertexCount = vertexData.size() / (meshData.vertexSize / sizeof(float));
    createVBO(meshData, vertexData);

    meshData.submeshCount = 1;
    meshData.indexData[0].format = GL_UNSIGNED_SHORT;
    meshData.indexData[0].primitiveType = meshData.primitiveType;
    meshData.indexData[0].indexCount = static_cast<std::uint32_t>(indices.size());

    createIBO(meshData, indices.data(), 0, sizeof(std::uint16_t));

    meshData.boundingBox[0] = { -m_radius, -m_radius, -m_radius };
    meshData.boundingBox[1] = { m_radius, m_radius, m_radius };
    meshData.boundingSphere.radius = m_radius;

    return meshData;
}