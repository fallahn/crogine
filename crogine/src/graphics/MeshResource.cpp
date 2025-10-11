/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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

#include <limits>

using namespace cro;

namespace
{
    std::size_t autoID = std::numeric_limits<std::size_t>::max();
}

MeshResource::MeshResource()
{

}

MeshResource::~MeshResource()
{
    flush();
}

//public
bool MeshResource::loadMesh(std::size_t ID, const MeshBuilder& mb)
{
    //don't forget to check ID doesn't exist
    if (m_meshData.count(ID) != 0)
    {
        Logger::log("Mesh with this ID already exists!", Logger::Type::Error);
        return false;
    }

    auto meshData = mb.build(&m_allocationResource);
    if (meshData.vboAllocation.vboID > 0 && meshData.submeshCount > 0)
    {
        m_meshData.insert(std::make_pair(ID, meshData));

        auto skeleton = mb.getSkeleton();
        if (skeleton)
        {
            m_skeletalData.insert(std::make_pair(ID, skeleton));
        }

        return true;
    }
    LOG("Invalid mesh data was returned from MeshBuilder", Logger::Type::Error);
    return false;
}

std::size_t MeshResource::loadMesh(const MeshBuilder& mb, bool forceReload)
{
    std::size_t nextID = mb.getUID();
    if (nextID == 0)
    {
        nextID = autoID--;
    }

    if (m_meshData.count(nextID) != 0)
    {
        if (forceReload)
        {
            deleteMesh(m_meshData.at(nextID));
            m_meshData.erase(nextID);
            m_skeletalData.erase(nextID);

            if (!loadMesh(nextID, mb))
            {
                return 0;
            }
        }

        return nextID;
    }

    if (loadMesh(nextID, mb))
    {
        return nextID;
    }

    return 0;
}

const Mesh::Data& MeshResource::getMesh(std::size_t id) const
{
    CRO_ASSERT(m_meshData.count(id) != 0, "Mesh not found");
    return m_meshData.at(id);
}

Mesh::Data& MeshResource::getMesh(std::size_t id)
{
    CRO_ASSERT(m_meshData.count(id) != 0, "Mesh not found");
    return m_meshData.at(id);
}

Skeleton MeshResource::getSkeltalAnimation(std::size_t ID) const
{
    if (m_skeletalData.count(ID) != 0)
    {
        return m_skeletalData.at(ID);
    }
    return {};
}

void MeshResource::flush()
{
    //make sure all the meshes are deaded
    for (auto& md : m_meshData)
    {
        deleteMesh(md.second);
    }
    m_meshData.clear();
    m_skeletalData.clear();
    autoID = std::numeric_limits<std::size_t>::max();
}

//private
void MeshResource::deleteMesh(Mesh::Data md)
{
    //delete index buffers
    for (auto& id : md.indexData)
    {
        if (id.ibo)
        {
            glCheck(glDeleteBuffers(1, &id.ibo));
        }

#ifdef PLATFORM_DESKTOP
        for (auto& vao : id.vao)
        {
            if (vao)
            {
                glCheck(glDeleteVertexArrays(1, &vao));
            }
        }
#endif
    }
    //delete vertex buffer
    if (md.vboAllocation.vboID)
    {
        glCheck(glDeleteBuffers(1, &md.vboAllocation.vboID));
    }
}