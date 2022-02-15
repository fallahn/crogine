/*-----------------------------------------------------------------------

Matt Marchant 2022
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include "Consts.hpp"
#include "VoxelData.hpp"

std::uint32_t Voxel::Mesh::addVertex(const pv::MarchingCubesVertex<Voxel::Data>& vertex)
{
    auto& vert = m_vertices.emplace_back();
    vert.position = vertex.position;// pv::decodePosition(vertex.encodedPosition);
    vert.colour = TerrainColours[vertex.data.terrain];
    vert.normal = vertex.normal;// pv::decodeNormal(vertex.encodedNormal);

    m_data.emplace_back(vertex.data);

    return static_cast<std::uint32_t>(m_vertices.size() - 1);
}

void Voxel::Mesh::addTriangle(std::uint32_t a, std::uint32_t b, std::uint32_t c)
{
    m_indices.push_back(a);
    m_indices.push_back(b);
    m_indices.push_back(c);
}

void Voxel::Mesh::clear()
{
    m_data.clear();
    m_vertices.clear();
    m_indices.clear();
}