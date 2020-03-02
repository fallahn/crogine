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

#include "TerrainGen.hpp"
#include "Voxel.hpp"
#include "Chunk.hpp"
#include "ChunkManager.hpp"
#include "WorldConsts.hpp"

//TODO also try fastnoise to see how results compare
#include <crogine/detail/glm/gtc/noise.hpp>

#include <cmath>
#include <array>

using namespace WorldConst;
using Heightmap = std::array<std::int32_t, ChunkArea>;

float rounded(glm::vec2 coord)
{
    auto bump = [](float t) {return std::max(0.f, 1.f - std::pow(t, 6.f)); };
    auto b = bump(coord.x) * bump(coord.y);
    return b * 0.9f;
}

struct NoiseOptions final
{
    std::int32_t octaves = 0;
    float amplitude = 0.f;
    float smoothness = 0.f;
    float roughness = 0.f;
    float offset = 0.f;
};

float getNoiseAt(glm::vec2 voxelPos, glm::vec2 chunkPos, const NoiseOptions& options, std::int32_t seed)
{
    auto voxel = voxelPos + chunkPos * static_cast<float>(ChunkSize);

    float value = 0.f;
    float accumulated = 0.f;

    for (auto i = 0; i < options.octaves; ++i)
    {
        float freq = std::pow(2.f, i);
        float amplitude = std::pow(options.roughness, i);

        glm::vec2 coord = voxel * freq / options.smoothness;

        float noise = glm::simplex(glm::vec3(seed + coord.x, seed + coord.y, seed));
        noise = noise + 1.f / 2.f;
        value += noise * amplitude;

        accumulated += amplitude;
    }
    return value / accumulated;
}

Heightmap createChunkHeightmap(glm::ivec3 chunkPos, std::int32_t chunkCount, std::int32_t seed)
{
    const float worldSize = static_cast<float>(chunkCount * ChunkSize);

    NoiseOptions noiseA;
    noiseA.amplitude = 105.f;
    noiseA.octaves = 6;
    noiseA.smoothness = 205.f;
    noiseA.roughness = 0.58f;
    noiseA.offset = 18.f;

    NoiseOptions noiseB;
    noiseB.amplitude = 20.f;
    noiseB.octaves = 4;
    noiseB.smoothness = 200.f;
noiseB.roughness = 0.45f;
noiseB.offset = 0.f;

glm::vec2 chunkXZ(chunkPos.x, chunkPos.z);

Heightmap heightmap = {};
for (auto z = 0u; z < ChunkSize; ++z)
{
    for (auto x = 0; x < ChunkSize; ++x)
    {
        float bx = static_cast<float>(x + chunkPos.x * ChunkSize);
        float bz = static_cast<float>(z + chunkPos.z * ChunkSize);

        glm::vec2 coord((glm::vec2(bx, bz) - worldSize / 2.f) / worldSize * 2.f);

        auto noise0 = getNoiseAt({ x,z }, chunkXZ, noiseA, seed);
        auto noise1 = getNoiseAt({ x,z }, { chunkPos.x, chunkPos.z }, noiseB, seed);

        //round off the edges
        auto island = rounded(coord) * 1.25f;
        auto result = noise0 * noise1;

        heightmap[z * ChunkSize + x] = static_cast<std::int32_t>((result * noiseA.amplitude + noiseA.offset) * island - 5.f);
    }
}

return heightmap;
}

void createTerrain(Chunk& chunk, const Heightmap& heightmap, const vx::DataManager& voxeldata, std::int32_t seed)
{
    for (auto z = 0; z < ChunkSize; ++z)
    {
        for (auto x = 0; x < ChunkSize; ++x)
        {
            auto height = heightmap[z * ChunkSize + x];

            for (auto y = 0; y < ChunkSize; ++y)
            {
                auto voxY = chunk.getPosition().y * ChunkSize + y;
                std::uint8_t voxelID = 0;

                //above the height value we're water or air (air is default)
                if (voxY > height)
                {
                    if (voxY < WaterLevel)
                    {
                        voxelID = voxeldata.getID(vx::Water);
                    }
                }
                //on the top layer, so sand if near water
                //else grass (or whatever a biome map might throw up)
                else if (voxY == height)
                {
                    if (voxY < (WaterLevel + 3))
                    {
                        voxelID = voxeldata.getID(vx::Sand);
                    }
                    else
                    {
                        //if using biome set the top data according
                        //to the current biome

                        voxelID = voxeldata.getID(vx::Grass);
                    }
                }
                //some arbitrary depth of dirt below the surface.
                //again, a biome would influence this
                else if (voxY > (height - 4))
                {
                    //TODO we only want to put this under grass
                    //sand should have more sand underneath it
                    voxelID = voxeldata.getID(vx::Dirt);
                }
                else
                {
                    //we're underground, so make solid
                    voxelID = voxeldata.getID(vx::Stone);
                }


                //set the voxelID at the current chunk position
                if (voxelID > 0)
                {
                    chunk.setVoxelQ({ x,y,z }, voxelID);
                }
            }
        }
    }
}

void generateTerrain(ChunkManager& chunkManager, std::int32_t chunkX, std::int32_t chunkZ,
    const vx::DataManager& voxelData, std::int32_t seed, std::int32_t chunkCount)
{
    glm::ivec3 chunkPos(chunkX, 0, chunkZ);

    auto heightmap = createChunkHeightmap(chunkPos, chunkCount, seed);
    auto maxHeight = *std::max_element(heightmap.begin(), heightmap.end());

    for(auto y = 0; y < std::max(1, maxHeight / ChunkSize + 1); ++y)
    {
        auto& chunk = chunkManager.addChunk({ chunkX, y, chunkZ });
        createTerrain(chunk, heightmap, voxelData, seed);
        chunkManager.ensureNeighbours(chunk.getPosition());
    }
}