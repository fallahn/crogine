/*-----------------------------------------------------------------------

Matt Marchant 2025
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

#include <crogine/detail/AllocationResource.hpp>
#include <crogine/graphics/SpriteMeshBuilder.hpp>

#include "../detail/GLCheck.hpp"

using namespace cro;

namespace
{

}

SpriteMeshBuilder::SpriteMeshBuilder()
{

}

//private
Mesh::Data SpriteMeshBuilder::build(AllocationResource* resource) const
{
    Mesh::Data meshData;
    meshData.attributeFlags = VertexProperty::Position | VertexProperty::Colour | VertexProperty::UV0;

    meshData.attributes[Mesh::Attribute::Position].componentCount = 3;

    meshData.attributes[Mesh::Attribute::Colour].componentCount = 4;
    meshData.attributes[Mesh::Attribute::Colour].glType = GL_UNSIGNED_BYTE;
    meshData.attributes[Mesh::Attribute::Colour].glNormalised = GL_TRUE;

    meshData.attributes[Mesh::Attribute::UV0].componentCount = 2;
    meshData.attributes[Mesh::Attribute::UV0].glType = GL_SHORT;
    meshData.attributes[Mesh::Attribute::UV0].glNormalised = GL_TRUE;

    meshData.primitiveType = GL_TRIANGLES;
    meshData.vertexSize = getVertexSize(meshData.attributes);
    meshData.vertexCount = 0;

    //create vbo
    auto* vboAllocator = resource->getVBOAllocator(4, sizeof(VertexLayout));
    meshData.vboAllocator = vboAllocator;
    meshData.vboAllocation = vboAllocator->newAllocation(0);

    meshData.submeshCount = 1;

    meshData.indexData[0].format = GL_UNSIGNED_BYTE; //we'll always have 4 verts
    meshData.indexData[0].primitiveType = meshData.primitiveType;
    meshData.indexData[0].indexCount = 0;

    //create IBO
    auto* iboAllocator = resource->getIBOAllocator(3, sizeof(std::uint8_t));
    meshData.iboAllocator = iboAllocator;
    meshData.indexData[0].iboAllocation = iboAllocator->newAllocation(0);

    return meshData;
}