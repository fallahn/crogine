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

std::uint32_t Voxel::PreviewMesh::addVertex(const pv::MarchingCubesVertex<Voxel::Data>& vertex)
{
    auto& vert = m_vertices.emplace_back();
    vert.position = vertex.position;// pv::decodePosition(vertex.encodedPosition);
    vert.colour = TerrainColours[vertex.data.terrain];
    vert.normal = vertex.normal;// pv::decodeNormal(vertex.encodedNormal);

    return static_cast<std::uint32_t>(m_vertices.size() - 1);
}

void Voxel::PreviewMesh::addTriangle(std::uint32_t a, std::uint32_t b, std::uint32_t c)
{
    m_indices.push_back(a);
    m_indices.push_back(b);
    m_indices.push_back(c);
}

void Voxel::PreviewMesh::clear()
{
    m_vertices.clear();
    m_indices.clear();
}

//export mesh
namespace
{
    constexpr float TextureRepeatY = 4.f;
    constexpr float RepeatRatio = static_cast<float>(Voxel::IslandSize.x) / Voxel::IslandSize.z;
    constexpr glm::vec2 UVMultiplier =
    {
        TextureRepeatY * RepeatRatio,
        TextureRepeatY
    };
    constexpr glm::vec2 MaxUVSize =
    {
        Voxel::IslandSize.x,
        Voxel::IslandSize.z
    };
}

Voxel::ExportMesh::ExportMesh(glm::vec3 positionOffset, float  scale)
    : m_positionOffset  (positionOffset.x, positionOffset.y, positionOffset.z),
    m_scale             (scale)
{

}

std::uint32_t Voxel::ExportMesh::addVertex(const pv::MarchingCubesVertex<Voxel::Data>& vertex)
{
    //TODO apply UV based on x/z coords

    auto& vert = m_vertices.emplace_back();
    vert.position = (vertex.position * m_scale) + m_positionOffset;
    vert.colour = CollisionColours[vertex.data.terrain];
    vert.normal = vertex.normal;
    vert.uv = (glm::vec2(vertex.position.getX(), vertex.position.getZ()) / MaxUVSize) * UVMultiplier;

    //stash this so we can look up which terrain a face belongs to
    m_terrainData.emplace_back(glm::vec3(vert.position.getX(), vert.position.getY(), vert.position.getZ()), vertex.data.terrain);

    return static_cast<std::uint32_t>(m_vertices.size() - 1);
}

void Voxel::ExportMesh::addTriangle(std::uint32_t a, std::uint32_t b, std::uint32_t c)
{
    //TODO can we safely erase the unused vertices too? Would require re-indexing :S

    //calculate face normal and discard if downward facing

    //auto faceNormal = m_terrainData[a].normal + m_terrainData[b].normal + m_terrainData[c].normal;
    //faceNormal /= 3.f;

    auto faceNormal = glm::normalize(glm::cross(m_terrainData[b].position - m_terrainData[a].position, m_terrainData[c].position - m_terrainData[a].position));
    if (glm::dot(glm::vec3(0.f, 1.f, 0.f), faceNormal) < -0.35f)
    {
        return;
    }

    //if all three belong to the same material, that's easy
    //else we use the material with the most verts
    std::int32_t terrainA = m_terrainData[a].terrain;
    std::int32_t terrainCountA = 1;

    std::int32_t terrainB = TerrainID::Unused;
    std::int32_t terrainCountB = 0;

    if (m_terrainData[b].terrain == terrainA)
    {
        terrainCountA++;
    }
    else
    {
        terrainB = m_terrainData[b].terrain;
        terrainCountB++;
    }

    if (m_terrainData[c].terrain == terrainA)
    {
        terrainCountA++;
    }
    else
    {
        if (terrainB == TerrainID::Unused)
        {
            terrainB = m_terrainData[c].terrain;
        }
        terrainCountB++;
    }

    if (terrainCountA == 3)
    {
        m_indices[terrainA].push_back(a);
        m_indices[terrainA].push_back(b);
        m_indices[terrainA].push_back(c);
    }
    else
    {
        if (terrainCountA < terrainCountB)
        {
            //terrain B
            m_indices[terrainB].push_back(a);
            m_indices[terrainB].push_back(b);
            m_indices[terrainB].push_back(c);
        }
        else
        {
            //terrain A
            m_indices[terrainA].push_back(a);
            m_indices[terrainA].push_back(b);
            m_indices[terrainA].push_back(c);
        }
    }
    
}

void Voxel::ExportMesh::clear()
{
    m_vertices.clear();
    for (auto& v : m_indices)
    {
        v.clear();
    }
    m_terrainData.clear();
}