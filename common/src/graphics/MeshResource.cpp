/*-----------------------------------------------------------------------

Matt Marchant 2017
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

#include <crogine/graphics/MeshResource.hpp>
#include <crogine/detail/Assert.hpp>

#include "../glad/glad.h"
#include "../glad/GLCheck.hpp"

#include <vector>

using namespace cro;

namespace
{
    std::size_t getVertexSize(const std::array<std::size_t, Mesh::Attribute::Total>& attrib)
    {
        std::size_t size = 0;
        for (const auto& a : attrib)
        {
            size += a;
        }
        return size * sizeof(float);
    }
}

MeshResource::MeshResource()
{
    //create primitives - TODO only create these on first call?
    loadQuad();
    //loadCube();
    //loadSphere();
}

MeshResource::~MeshResource()
{
    //make sure all the meshes are deaded
    for (auto& md : m_meshData)
    {
        //delete index buffers
        for (auto& id : md.second.indexData)
        {
            if (id.ibo)
            {
                glCheck(glDeleteBuffers(1, &id.ibo));
            }
        }
        //delete vertex buffer
        if (md.second.vbo)
        {
            glCheck(glDeleteBuffers(1, &md.second.vbo));
        }
    }
}

//public
bool MeshResource::loadMesh(int32) { return false; } //TODO don't forget to check ID doesn't exist

const Mesh::Data MeshResource::getMesh(int32 id) const
{
    CRO_ASSERT(m_meshData.count(id) != 0, "Mesh not found");
    return m_meshData.find(id)->second;
}

//private
void MeshResource::loadQuad()
{
    //TODO refactor all this so we can submit data arrays to single mesh building function.

    auto pair = std::make_pair(0, Mesh::Data());
    auto& meshData = pair.second;

    std::vector<float> vertexData =
    {
        -0.5f, 0.5f, 0.f, 0.f, 1.f,
        -0.5f, -0.5f, 0.f, 0.f, 0.f,
        0.5f, 0.5f, 0.f, 1.f, 1.f,
        0.5f, -0.5f, 0.f, 1.f, 0.f
    };

    meshData.attributes[Mesh::Position] = 3;
    meshData.attributes[Mesh::UV0] = 2;
    meshData.primitiveType = GL_TRIANGLE_STRIP;
    meshData.submeshCount = 1;
    meshData.vertexCount = 4;

    glCheck(glGenBuffers(1, &meshData.vbo));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData.vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, getVertexSize(meshData.attributes) * meshData.vertexCount, vertexData.data(), GL_STATIC_DRAW));

    //index arrays
    std::array<uint8, 4u> idxData = { {0, 1, 2 ,3} };
    meshData.submeshCount = 1;
    meshData.indexData[0].format = GL_UNSIGNED_BYTE;
    meshData.indexData[0].primitiveType = meshData.primitiveType;
    
    glCheck(glGenBuffers(1, &meshData.indexData[0].ibo));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[0].ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, 4, idxData.data(), GL_STATIC_DRAW));

    if (meshData.vbo && meshData.indexData[0].ibo)
    {
        m_meshData.insert(pair);
    }
}

void MeshResource::loadCube()
{
    //TODO pending above refactor
}

void MeshResource::loadSphere()
{
    //TODO pending above refactor
}