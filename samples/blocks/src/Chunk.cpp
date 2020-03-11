/*
World Generation code based on Matthew Hopson's Open Builder
https://github.com/Hopson97/open-builder

MIT License

Copyright (c) 2019 Matthew Hopson

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include "Chunk.hpp"
#include "ChunkManager.hpp"
#include "Voxel.hpp"

#include <crogine/detail/Assert.hpp>

namespace
{
    bool voxelPositionOutOfBounds(glm::ivec3 pos)
    {
        return
            (
                pos.x < 0 || pos.x >= WorldConst::ChunkSize
                || pos.y < 0 || pos.y >= WorldConst::ChunkSize
                || pos.z < 0 || pos.z >= WorldConst::ChunkSize
            );
    }
}

Chunk::Chunk(ChunkManager& m, glm::ivec3 pos)
    : m_chunkManager    (m),
    m_position          (pos),
    m_highestPoint      (-1)
{
    //this assume 'air' is ID 0 - ideally we should be checking
    //the ID assigned by the voxel manager when the type is loaded.
    std::fill(m_voxels.begin(), m_voxels.end(), 0);
}

//public
std::uint8_t Chunk::getVoxelQ(glm::ivec3 position) const
{
    CRO_ASSERT(!voxelPositionOutOfBounds(position), "Out of bounds");
    return m_voxels[toLocalVoxelIndex(position)];
}

void Chunk::setVoxelQ(glm::ivec3 position, std::uint8_t id)
{
    CRO_ASSERT(!voxelPositionOutOfBounds(position), "Out of bounds");
    m_voxels[toLocalVoxelIndex(position)] = id;
}

std::uint8_t Chunk::getVoxel(glm::ivec3 position) const
{
    if (voxelPositionOutOfBounds(position))
    {
        return m_chunkManager.getVoxel(toGlobalVoxelPosition(position, m_position));
    }
    return getVoxelQ(position);
}

void Chunk::setVoxel(glm::ivec3 position, std::uint8_t id)
{
    if (voxelPositionOutOfBounds(position))
    {
        m_chunkManager.setVoxel(toGlobalVoxelPosition(position, m_position), id);
        return;
    }
    setVoxelQ(position, id);
}

glm::ivec3 Chunk::getPosition() const
{
    return m_position;
}

ChunkVoxels& Chunk::getVoxels()
{
    return m_voxels;
}

const ChunkVoxels& Chunk::getVoxels() const
{
    return m_voxels;
}

//free funcs
CompressedVoxels compressVoxels(const ChunkVoxels& voxels)
{
    CompressedVoxels compressed;
    std::uint8_t currentVoxel = voxels[0];
    std::uint16_t voxelCount = 1;

    for (auto i = 1u; i < voxels.size(); i++) 
    {
        auto voxel = voxels[i];
        if (voxel == currentVoxel) 
        {
            voxelCount++;
        }
        else
        {
            compressed.emplace_back(currentVoxel, voxelCount);
            currentVoxel = voxels[i];
            voxelCount = 1;
        }
    }
    compressed.emplace_back(currentVoxel, voxelCount);
    return compressed;
}

ChunkVoxels decompressVoxels(const CompressedVoxels& voxels)
{
    ChunkVoxels voxelData = {};
    std::size_t idx = 0;
    for (const auto& [type, count] : voxels)
    {
        for (auto i = 0u; i < count; i++) 
        {
            voxelData[idx++] = type;
        }
    }
    return voxelData;
}