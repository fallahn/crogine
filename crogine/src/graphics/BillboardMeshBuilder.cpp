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

#include "../detail/GLCheck.hpp"

using namespace cro;

namespace
{

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

    meshData.indexData[0].format = GL_UNSIGNED_SHORT;
    meshData.indexData[0].primitiveType = meshData.primitiveType;
    meshData.indexData[0].indexCount = 0;

    //create IBO
    glCheck(glGenBuffers(1, &meshData.indexData[0].ibo));

    return meshData;
}