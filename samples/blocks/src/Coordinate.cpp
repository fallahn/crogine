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

#include "Coordinate.hpp"
#include "WorldConsts.hpp"

#include <cmath>

using namespace WorldConst;

std::int32_t toLocalVoxelIndex(glm::ivec3 position)
{
    return position.y * ChunkArea + position.z * ChunkSize + position.x;
}

glm::ivec3 worldToChunkPosition(glm::vec3 position)
{
    return toChunkPosition(toVoxelPosition(position));
}

glm::ivec3 toChunkPosition(glm::ivec3 position)
{
    return
    {
        position.x < 0 ? ((position.x - ChunkSize) / ChunkSize) : (position.x / ChunkSize),
        position.y < 0 ? ((position.y - ChunkSize) / ChunkSize) : (position.y / ChunkSize),
        position.z < 0 ? ((position.z - ChunkSize) / ChunkSize) : (position.z / ChunkSize)
    };
}

glm::ivec3 toLocalVoxelPosition(glm::ivec3 position)
{
    return
    {
        (ChunkSize + (position.x % ChunkSize)) % ChunkSize,
        (ChunkSize + (position.y % ChunkSize)) % ChunkSize,
        (ChunkSize + (position.z % ChunkSize)) % ChunkSize
    };
}

glm::ivec3 toGlobalVoxelPosition(glm::ivec3 vxPos, glm::ivec3 ckPos)
{
    return
    {
        ckPos.x * ChunkSize + vxPos.x,
        ckPos.y * ChunkSize + vxPos.y,
        ckPos.z * ChunkSize + vxPos.z
    };
}

glm::ivec3 toVoxelPosition(glm::vec3 pos)
{
    return
    {
        std::floor(pos.x),
        std::floor(pos.y),
        std::floor(pos.z)
    };
}