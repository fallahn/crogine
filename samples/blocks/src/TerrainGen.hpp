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

#include "fastnoise/FastNoiseSIMD.h"
#include "WorldConsts.hpp"

#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/glm/vec3.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/Texture.hpp>

#include <cstdint>
#include <array>
#include <vector>

class Chunk;
class ChunkManager;
namespace vx
{
    class DataManager;
}

using namespace WorldConst;
using Heightmap = std::array<std::int32_t, ChunkArea>;

class TerrainGenerator final : public cro::GuiClient
{
public:
    TerrainGenerator(bool debugWindow = false);
    ~TerrainGenerator();

    TerrainGenerator(const TerrainGenerator&) = delete;
    TerrainGenerator(TerrainGenerator&&) = delete;

    TerrainGenerator& operator = (const TerrainGenerator&) = delete;
    TerrainGenerator& operator = (TerrainGenerator&&) = delete;

    void generateTerrain(ChunkManager&, std::int32_t x, std::int32_t z, const vx::DataManager&, std::int32_t seed, std::int32_t chunksPerSide);

    void renderHeightmaps();

    enum
    {
        NoiseOne, NoiseTwo,
        Falloff, Final,

        Count
    };

private:
    FastNoiseSIMD* m_noise;

    std::vector<std::uint8_t> m_noiseImageOne;
    std::vector<std::uint8_t> m_noiseImageTwo;
    std::vector<std::uint8_t> m_falloffImage;
    std::vector<std::uint8_t> m_finalImage;
    std::uint32_t m_lastHeightmapSize;

    std::array<cro::Texture, Count> m_debugTextures;
    cro::Texture m_previewTexture;

    Heightmap createChunkHeightmap(glm::ivec3 chunkPos, std::int32_t chunkCount, std::int32_t seed);
    void createTerrain(Chunk& chunk, const Heightmap& heightmap, const vx::DataManager& voxeldata, std::int32_t seed);
};