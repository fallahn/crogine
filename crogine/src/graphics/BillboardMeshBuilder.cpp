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

#include <crogine/graphics/BillboardMeshBuilder.hpp>
#include <crogine/graphics/Colour.hpp>

#include "../detail/GLCheck.hpp"

using namespace cro;

namespace
{
    //TODO update the shader to match? Rename Normal to Root?
    //struct VertexLayout final
    //{
    //    glm::vec3 pos = glm::vec3(0.f);
    //    Detail::ColourLowP colour;
    //    glm::vec3 rootPos = glm::vec3(0.f);
    //    std::uint32_t uvCoords = 0; //pack with glm::packSnorm2x16()
    //    //glm::vec2 size = glm::vec2(0.f); //TODO think about how we can pack this into un-normalised shorts
    //    std::uint32_t size = 0;
    //};

    struct VertexLayout final
    {
        glm::vec3 pos = glm::vec3(0.f);
        cro::Colour colour = cro::Colour::White;
        glm::vec3 rootPos = glm::vec3(0.f);
        glm::vec2 uvCoords = glm::vec2(0.f);
        glm::vec2 size = glm::vec2(0.f);
    };
}

BillboardMeshBuilder::BillboardMeshBuilder()
{

}

//private
Mesh::Data BillboardMeshBuilder::build() const
{
    Mesh::Data meshData;
    meshData.attributeFlags = VertexProperty::Position | VertexProperty::Normal | VertexProperty::Colour | VertexProperty::UV0 | VertexProperty::UV1;

    meshData.attributes[Mesh::Attribute::Position].size = 3;
    meshData.attributes[Mesh::Attribute::Colour].size = 4;
    meshData.attributes[Mesh::Attribute::Normal].size = 3;
    meshData.attributes[Mesh::Attribute::UV0].size = 2;
    meshData.attributes[Mesh::Attribute::UV1].size = 2;

    meshData.primitiveType = GL_TRIANGLES;
    meshData.vertexSize = getVertexSize(meshData.attributes);
    meshData.vertexCount = 0;

    //create vbo
    glCheck(glGenBuffers(1, &meshData.vboAllocation.vboID));

    meshData.submeshCount = 1;
    for (auto i = 0; i < 1; ++i)
    {
        meshData.indexData[i].format = GL_UNSIGNED_INT; //TODO 16 bit
        meshData.indexData[i].primitiveType = meshData.primitiveType;
        meshData.indexData[i].indexCount = 0;

        //create IBO
        glCheck(glGenBuffers(1, &meshData.indexData[i].ibo));
    }

    return meshData;
}