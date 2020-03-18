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

#include "ChunkManager.hpp"
#include "Voxel.hpp"

ChunkManager::ChunkManager()
    : m_errorChunk(*this, glm::ivec3(0))
{

}

//public
Chunk& ChunkManager::addChunk(glm::ivec3 position)
{
    auto result = m_chunks.find(position);
    if (result == m_chunks.end())
    {
        m_chunkPositions.push_back(position);

        return m_chunks.emplace(
            std::piecewise_construct, std::forward_as_tuple(position), std::forward_as_tuple(*this, position)).first->second;
    }
    return result->second;
}

const Chunk& ChunkManager::getChunk(glm::ivec3 position) const
{
    auto result = m_chunks.find(position);
    if (result == m_chunks.end())
    {
        return m_errorChunk;
    }
    return result->second;
}

std::uint8_t ChunkManager::getVoxel(glm::ivec3 position) const
{
    auto chunkPos = toChunkPosition(position);
    auto result = m_chunks.find(chunkPos);

    if (result == m_chunks.end())
    {
        return vx::CommonType::OutOfBounds;
    }
    return result->second.getVoxelQ(toLocalVoxelPosition(position));
}

void ChunkManager::setVoxel(glm::ivec3 position, std::uint8_t id)
{
    auto chunkPos = toChunkPosition(position);
    auto result = m_chunks.find(chunkPos);
    auto local = toLocalVoxelPosition(position);

    if (result != m_chunks.end())
    {
        result->second.setVoxelQ(local, id);
    }
    else
    {
        auto& chunk = addChunk(chunkPos);
        chunk.setVoxelQ(local, id);
    }

    ensureNeighbours(chunkPos);
}

bool ChunkManager::hasChunk(glm::ivec3 position) const
{
    return (m_chunks.find(position) != m_chunks.end());
}

bool ChunkManager::hasNeighbours(glm::ivec3 position) const
{
    return hasChunk(position) 
        && hasChunk({ position.x, position.y + 1, position.z }) //top 
        && hasChunk({ position.x, position.y - 1, position.z }) //bottom
        && hasChunk({ position.x - 1, position.y, position.z }) //left
        && hasChunk({ position.x + 1, position.y, position.z }) //right
        && hasChunk({ position.x, position.y, position.z - 1 }) //front
        && hasChunk({ position.x, position.y, position.z + 1 }); //back
}

void ChunkManager::ensureNeighbours(glm::ivec3 pos)
{
    addChunk(pos);
    addChunk({ pos.x, pos.y + 1, pos.z });
    addChunk({ pos.x, pos.y - 1, pos.z });
    addChunk({ pos.x - 1, pos.y, pos.z });
    addChunk({ pos.x + 1, pos.y, pos.z });
    addChunk({ pos.x, pos.y, pos.z - 1 });
    addChunk({ pos.x, pos.y, pos.z + 1 });
}

const PositionMap<Chunk>& ChunkManager::getChunks() const
{
    return m_chunks;
}