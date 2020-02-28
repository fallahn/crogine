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

#include <crogine/detail/glm/vec3.hpp>

#include <cstdint>
#include <unordered_map>

struct PositionHash
{
    //http://www.beosil.com/download/CollisionDetectionHashing_VMV03.pdf
    std::size_t operator()(glm::ivec3 position) const
    {
        return (position.x * 88339) ^ (position.z * 91967) ^ (position.z * 126323);
    }
};

template <typename T>
using PositionMap = std::unordered_map<glm::ivec3, T, PositionHash>;

//voxel position to index in voxel array
std::int32_t toLocalVoxelIndex(glm::ivec3);

//world coords to chunk coords
glm::ivec3 worldToChunkPosition(glm::vec3);

//voxel position to chunk position (world to chunk)
glm::ivec3 toChunkPosition(glm::ivec3);

//voxel world position to voxel chunk position
glm::ivec3 toLocalVoxelPosition(glm::ivec3);

//convert local voxel and chunk position to world position
glm::ivec3 toGlobalVoxelPosition(glm::ivec3 vx, glm::ivec3 ck);

//converts world position to world voxel position
glm::ivec3 toVoxelPosition(glm::vec3);