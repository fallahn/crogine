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

#include "ChunkMeshBuilder.hpp"
#include "ErrorCheck.hpp"
#include "WorldConsts.hpp"

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/geometric.hpp>

std::size_t ChunkMeshBuilder::m_componentCount = 0;

cro::Mesh::Data ChunkMeshBuilder::build() const
{
    cro::Mesh::Data data;

    data.attributes[cro::Mesh::Attribute::Position].componentCount = 3;
    data.attributes[cro::Mesh::Attribute::Colour].componentCount = 4;
    data.attributes[cro::Mesh::Attribute::Normal].componentCount = 3;
    data.attributes[cro::Mesh::Attribute::UV0].componentCount = 2;
    data.attributeFlags = (cro::VertexProperty::Position | cro::VertexProperty::Colour | cro::VertexProperty::Normal | cro::VertexProperty::UV0);

    m_componentCount = 12; //this is important! update this if modifying above

    data.primitiveType = GL_TRIANGLES;
    data.vertexSize = getVertexSize(data.attributes);
    data.vertexCount = 0;
    
    glCheck(glGenBuffers(1, &data.vboAllocation.vboID));
    /*glCheck(glBindBuffer(GL_ARRAY_BUFFER, data.vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));*/

    //first submesh is all solid geometry
    data.submeshCount = SubMeshID::Count;
    data.indexData[SubMeshID::Solid].format = GL_UNSIGNED_INT;
    data.indexData[SubMeshID::Solid].primitiveType = GL_TRIANGLES;
    data.indexData[SubMeshID::Solid].indexCount = 0;

    glCheck(glGenBuffers(1, &data.indexData[SubMeshID::Solid].ibo));
    /*glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.indexData[SubMeshID::Solid].ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));*/

    //second submesh is foliage
    data.indexData[SubMeshID::Foliage].format = GL_UNSIGNED_INT;
    data.indexData[SubMeshID::Foliage].primitiveType = GL_TRIANGLES;
    data.indexData[SubMeshID::Foliage].indexCount = 0;

    glCheck(glGenBuffers(1, &data.indexData[SubMeshID::Foliage].ibo));
    /*glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.indexData[SubMeshID::Foliage].ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));*/

    //third submesh is used for water/fluid
    data.indexData[SubMeshID::Water].format = GL_UNSIGNED_INT;
    data.indexData[SubMeshID::Water].primitiveType = GL_TRIANGLES;
    data.indexData[SubMeshID::Water].indexCount = 0;

    glCheck(glGenBuffers(1, &data.indexData[SubMeshID::Water].ibo));
    /*glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.indexData[SubMeshID::Water].ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));*/

    //this needs to be set to be culled properly
    data.boundingBox[0] = glm::vec3(0.f);
    data.boundingBox[1] = glm::vec3(WorldConst::ChunkSize);

    //sphere has to ENCASE AABB, not fit in it
    data.boundingSphere.centre = glm::vec3(WorldConst::ChunkSize / 2);
    data.boundingSphere.radius = glm::length(data.boundingBox[1]) / 2.f;

    return data;
}