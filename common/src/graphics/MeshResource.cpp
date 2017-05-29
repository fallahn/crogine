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
#include <crogine/graphics/MeshBuilder.hpp>

#include "../detail/GLCheck.hpp"

using namespace cro;

namespace
{

}

MeshResource::MeshResource()
{

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
bool MeshResource::loadMesh(int32 ID, const MeshBuilder& mb)
{
    //don't forget to check ID doesn't exist
    if (m_meshData.count(ID) != 0)
    {
        Logger::log("Mesh with this ID already exists!", Logger::Type::Error);
        return false;
    }

    auto meshData = mb.build();
    if (meshData.vbo > 0 && meshData.submeshCount > 0)
    {
        m_meshData.insert(std::make_pair(ID, meshData));
        return true;
    }
    LOG("Invalid mesh data was returned from MeshBuilder", Logger::Type::Error);
    return false;
}

const Mesh::Data MeshResource::getMesh(int32 id) const
{
    CRO_ASSERT(m_meshData.count(id) != 0, "Mesh not found");
    return m_meshData.find(id)->second;
}