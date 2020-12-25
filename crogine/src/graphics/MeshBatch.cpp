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

#include <crogine/detail/Assert.hpp>

#include <crogine/graphics/MeshBatch.hpp>
#include <crogine/graphics/MeshData.hpp>

using namespace cro;

MeshBatch::MeshBatch(std::uint32_t flags)
    : m_flags(flags)
{

}

//public
bool MeshBatch::addMesh(const std::string& path, const std::vector<glm::mat4>& transforms)
{
    //validate path

    //open/validate data

    //validate mesh flags


    for (const auto& tx : transforms)
    {
        //translate/rotate/scale position

        //rotate normals/tangents
    }

    return false;
}

void MeshBatch::updateMeshData(Mesh::Data& data) const
{
    CRO_ASSERT(data.attributeFlags == m_flags, "Flags do not match!");
    CRO_ASSERT(data.vbo != 0, "Not a valid vertex buffer. Must be created with a MeshResource first");

    if (data.attributeFlags == m_flags)
    {

    }
}