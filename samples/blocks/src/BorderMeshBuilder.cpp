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

#include "BorderMeshBuilder.hpp"
#include "WorldConsts.hpp"

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/packing.hpp>

using namespace WorldConst;

cro::Mesh::Data BorderMeshBuilder::build() const
{

    std::vector<float> verts =
    {
        0.f, 0.f, 0.f,   1.f, 0.f, 1.f,
        1.f, 0.f, 0.f,   1.f, 0.f, 1.f,
        1.f, 0.f, 1.f,   1.f, 0.f, 1.f,
        0.f, 0.f, 1.f,   1.f, 0.f, 1.f,

        0.f, 1.f, 0.f,   1.f, 0.f, 1.f,
        1.f, 1.f, 0.f,   1.f, 0.f, 1.f,
        1.f, 1.f, 1.f,   1.f, 0.f, 1.f,
        0.f, 1.f, 1.f,   1.f, 0.f, 1.f
    };

    std::vector<std::uint16_t> indices =
    {
        0, 1,  1, 2,  2, 3,  3, 0,

        4, 5,  5, 6,  6, 7,  7, 4,

        0, 4,  1, 5,  2, 6,  3, 7
    };

    cro::Mesh::Data data;

    data.attributes[cro::Mesh::Attribute::Position].componentCount = 3;
    data.attributes[cro::Mesh::Attribute::Colour].componentCount = 3;
    data.attributeFlags |= (cro::VertexProperty::Position | cro::VertexProperty::Colour);

    data.primitiveType = GL_LINES;
    data.vertexSize = getVertexSize(data.attributes);
    data.vertexCount = 8;
    createVBO(data, verts);

    data.submeshCount = 1;
    data.indexData[0].format = GL_UNSIGNED_SHORT;
    data.indexData[0].primitiveType = data.primitiveType;
    data.indexData[0].indexCount = static_cast<std::uint32_t>(indices.size());

    createIBO(data, indices.data(), 0, sizeof(std::uint16_t) * indices.size());

    data.boundingBox[0] = glm::vec3(0.f);
    data.boundingBox[1] = glm::vec3(ChunkSize);
    data.boundingSphere.radius = glm::length(data.boundingBox[1] / 2.f);
    data.boundingSphere.centre = data.boundingBox[1] / 2.f;

    return data;
}