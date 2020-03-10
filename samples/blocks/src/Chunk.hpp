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

#include "WorldConsts.hpp"

#include <crogine/detail/glm/vec3.hpp>

#include <cstdint>
#include <vector>
#include <array>

//using a struct rather than std::pair makes it easier to serialise into packet.
struct RLEPair final
{
    RLEPair(std::uint8_t i = 0, std::uint16_t c = 0) : id(i), count(c) {}
    std::uint8_t id = 0;
    std::uint16_t count = 0;
};

//holds a collection of voxel IDs compressed with run length encoding: pair<ID, Count>
using CompressedVoxels = std::vector<RLEPair>;
using ChunkVoxels = std::array<std::uint8_t, WorldConst::ChunkVolume>;

class ChunkManager;
class Chunk final
{
public:
    Chunk(ChunkManager&, glm::ivec3 position);

    //quick getter/setter that doesn't do bounds check
    std::uint8_t getVoxelQ(glm::ivec3) const;

    void setVoxelQ(glm::ivec3, std::uint8_t);

    //safe version which returns an ID from neighbouring
    //chunk if the position is out of bounds
    std::uint8_t getVoxel(glm::ivec3) const;

    void setVoxel(glm::ivec3, std::uint8_t);

    glm::ivec3 getPosition() const;

    ChunkVoxels& getVoxels();
    const ChunkVoxels& getVoxels() const;

    void setHighestPoint(std::int8_t p) { m_highestPoint = p; }
    std::int8_t getHighestPoint() const { return m_highestPoint; }

private:

    ChunkManager& m_chunkManager;
    glm::ivec3 m_position;
    ChunkVoxels m_voxels = {};

    std::int8_t m_highestPoint;
};

//compress / decompress voxels with RLE for network transport
CompressedVoxels compressVoxels(const ChunkVoxels&);

ChunkVoxels decompressVoxels(const CompressedVoxels&);