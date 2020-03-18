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

#pragma once

#include "Chunk.hpp"
#include "Coordinate.hpp"

class ChunkManager final
{
public:
    ChunkManager();

    Chunk& addChunk(glm::ivec3);

    const Chunk& getChunk(glm::ivec3) const;

    std::uint8_t getVoxel(glm::ivec3) const;

    void setVoxel(glm::ivec3, std::uint8_t);

    //returns true if there is a chunk at this position
    bool hasChunk(glm::ivec3) const;

    //returns true if all 6 neighbours exist
    bool hasNeighbours(glm::ivec3) const;

    void ensureNeighbours(glm::ivec3);

    const PositionMap<Chunk>& getChunks() const;

    std::vector<glm::ivec3> getChunkPositions() const { return m_chunkPositions; }

private:
    PositionMap<Chunk> m_chunks;
    Chunk m_errorChunk;

    std::vector<glm::ivec3> m_chunkPositions;
};
