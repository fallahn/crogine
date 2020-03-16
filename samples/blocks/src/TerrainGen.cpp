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

#include <algorithm>

#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/Colour.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/util/Random.hpp>

#include <cmath>

using fn = FastNoiseSIMD;

namespace
{
    float rounded(glm::vec2 coord)
    {
        auto bump = [](float t) {return std::max(0.f, 1.f - std::pow(t, 4.f)); };
        auto b = bump(coord.x) * bump(coord.y);
        return std::min((b * 0.9f) * 1.25f, 1.f);
    }

    const float MaxTerrainHeight = 80.f;

    float noiseOneFreq = 0.004f;
    float noiseTwoFreq = 0.008f;
    float minHeight = 0.399f;

    std::int32_t seed = 1234567;
}

TerrainGenerator::TerrainGenerator(bool debugWindow)
    : m_noise(nullptr),
    m_lastHeightmapSize(0)
{
    m_noise = fn::NewFastNoiseSIMD();

    if (debugWindow)
    {
        static const std::int32_t chunkCount = 16;

        auto w = chunkCount * ChunkSize;
        for (auto& t : m_debugTextures)
        {
            t.create(w, w, cro::ImageFormat::A);
        }
        m_previewTexture.create(w, w);

        registerWindow([&]()
            {
                ImGui::SetNextWindowSize({ 560.f, 540.f });
                if (ImGui::Begin("Terrain"))
                {
                    ImGui::SliderFloat("Noise One Freq", &noiseOneFreq, 0.001f, 0.09f);
                    ImGui::SliderFloat("Noise Two Freq", &noiseTwoFreq, 0.001f, 0.09f);
                    ImGui::SliderFloat("MinHeight", &minHeight, 0.1f, 0.6f);

                    if (ImGui::Button("Random Seed"))
                    {
                        seed = cro::Util::Random::value(12564, 984343);
                    }
                    ImGui::SameLine();
                    ImGui::Text("%d", seed);

                    for (const auto& t : m_debugTextures)
                    {
                        auto h = t.getGLHandle();
                        ImGui::Image((ImTextureID)(intptr_t)h, { 128.f, 128.f });
                        ImGui::SameLine();
                    }
                    ImGui::NewLine();

                    ImGui::Image((ImTextureID)(intptr_t)m_previewTexture.getGLHandle(), { 256.f, 256.f });
                    ImGui::SameLine();
                    if (ImGui::Button("Render"))
                    {
                        
                        for (auto z = 0; z < chunkCount; ++z)
                        {
                            for (auto x = 0; x < chunkCount; ++x)
                            {
                                createChunkHeightmap({ x, 0, z }, chunkCount, seed);
                            }
                        }
                        renderHeightmaps();
                    }
                }

                ImGui::End();
            });
    }
}

TerrainGenerator::~TerrainGenerator()
{
    if (m_noise)
    {
        
    }
}

//public
void TerrainGenerator::generateTerrain(ChunkManager& chunkManager, std::int32_t chunkX, std::int32_t chunkZ,
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

void TerrainGenerator::renderHeightmaps()
{
    auto area = m_lastHeightmapSize * m_lastHeightmapSize;

    for (auto& t : m_debugTextures)
    {
        if (t.getGLHandle() == 0
            || t.getSize().x != m_lastHeightmapSize)
        {
            t.create(m_lastHeightmapSize, m_lastHeightmapSize, cro::ImageFormat::A);
        }
    }
    if (m_previewTexture.getGLHandle() == 0
        || m_previewTexture.getSize().x != m_lastHeightmapSize)
    {
        m_previewTexture.create(m_lastHeightmapSize, m_lastHeightmapSize);
    }

    if (m_noiseImageOne.size() == area)
    {
        m_debugTextures[NoiseOne].update(m_noiseImageOne.data());
    }

    if (m_noiseImageTwo.size() == area)
    {
        m_debugTextures[NoiseTwo].update(m_noiseImageTwo.data());
    }

    if (m_falloffImage.size() == area)
    {
        m_debugTextures[Falloff].update(m_falloffImage.data());
    }

    if (m_finalImage.size() == area)
    {
        m_debugTextures[Final].update(m_finalImage.data());

        //create a colour preview
        std::vector<std::uint8_t> bytes;
        for (auto c : m_finalImage)
        {
            std::int32_t height = static_cast<std::int32_t>((static_cast<float>(c) / 255.f)* MaxTerrainHeight);
            if (height < WaterLevel)
            {
                bytes.push_back(0);
                bytes.push_back(0);
                bytes.push_back(255);
                bytes.push_back(255);
            }
            else if (height < WaterLevel + 3)
            {
                bytes.push_back(255);
                bytes.push_back(255);
                bytes.push_back(200);
                bytes.push_back(255);
            }
            else
            {
                bytes.push_back(10);
                bytes.push_back(250);
                bytes.push_back(80);
                bytes.push_back(255);
            }
        }
        m_previewTexture.update(bytes.data());
    }
}

//private
Heightmap TerrainGenerator::createChunkHeightmap(glm::ivec3 chunkPos, std::int32_t chunkCount, std::int32_t seed)
{
    const float worldSize = static_cast<float>(chunkCount * ChunkSize);
    auto chunkWorldPos = chunkPos * ChunkSize;

    m_noise->SetSeed(seed);
    m_noise->SetFrequency(/*0.02f*/noiseOneFreq);
    
    auto* noiseSet0 = m_noise->GetSimplexSet(chunkWorldPos.x, chunkWorldPos.y, chunkWorldPos.z, ChunkSize, 1, ChunkSize);

    m_noise->SetFrequency(/*0.01f*/noiseTwoFreq);

    auto* noiseSet1 = m_noise->GetSimplexSet(chunkWorldPos.x, chunkWorldPos.y, chunkWorldPos.z, ChunkSize, 1, ChunkSize);

    glm::vec2 chunkXZ(chunkPos.x, chunkPos.z);

    //TODO could make this debug only
    std::uint32_t lastHeightmapSize = chunkCount * ChunkSize;
    if (m_lastHeightmapSize != lastHeightmapSize)
    {
        m_lastHeightmapSize = lastHeightmapSize;
        m_noiseImageOne.resize(lastHeightmapSize * lastHeightmapSize);
        m_noiseImageTwo.resize(m_noiseImageOne.size());
        m_falloffImage.resize(m_noiseImageOne.size());
        m_finalImage.resize(m_noiseImageOne.size());
    }

    Heightmap heightmap = {};
    std::int32_t i = 0;
    for (auto x = 0u; x < ChunkSize; ++x)
    {
        for (auto z = 0; z < ChunkSize; ++z)
        {
            //round off the edges
            float bx = static_cast<float>(x + (chunkPos.x * ChunkSize));
            float bz = static_cast<float>(z + (chunkPos.z * ChunkSize));

            glm::vec2 coord((glm::vec2(bx, bz) - worldSize / 2.f) / worldSize * 2.f);
            auto island = rounded(coord);

            auto noise0 = (((noiseSet0[i] + 1.f) / 2.f) * (1.f - minHeight)) + minHeight;
            auto noise1 = (((noiseSet1[i] + 1.f) / 2.f) * (1.f - minHeight)) + minHeight;

            auto result = noise0 * noise1;

            //debug images
            std::int32_t coordX = x + (chunkPos.x * ChunkSize);
            std::int32_t coordY = z + (chunkPos.z * ChunkSize);
            std::size_t idx = coordY *  lastHeightmapSize + coordX;

            std::uint8_t c = static_cast<std::uint8_t>(255.f * noise0);
            m_noiseImageOne[idx] = c;
            
            c = static_cast<std::uint8_t>(255.f * noise1);
            m_noiseImageTwo[idx] = c;

            c = static_cast<std::uint8_t>(255.f * island);
            m_falloffImage[idx] = c;

            c = static_cast<std::uint8_t>(255.f * result * island);
            m_finalImage[idx] = c;

            //output heightmap
            heightmap[z * ChunkSize + x] = static_cast<std::int32_t>((result * MaxTerrainHeight) * island);

            i++;
        }
    }

    fn::FreeNoiseSet(noiseSet0);
    fn::FreeNoiseSet(noiseSet1);

    return heightmap;
}

void TerrainGenerator::createTerrain(Chunk& chunk, const Heightmap& heightmap, const vx::DataManager& voxelData, std::int32_t seed)
{
    std::int8_t highestPoint = -1;

    for (auto z = 0; z < ChunkSize; ++z)
    {
        for (auto x = 0; x < ChunkSize; ++x)
        {
            auto height = heightmap[z * ChunkSize + x];

            for (auto y = 0; y < ChunkSize; ++y)
            {
                auto voxY = chunk.getPosition().y * ChunkSize + y;
                std::uint8_t voxelID = voxelData.getID(vx::Air);

                //above the height value we're water or air (air is default)
                if (voxY > height)
                {
                    if (voxY < WaterLevel)
                    {
                        voxelID = voxelData.getID(vx::Water);
                    }
                    else if (voxY == height + 1)
                    {
                        //random vegetation - TODO create a distribution map/poisson disc distribution
                        if (cro::Util::Random::value(0, 8) == 6)
                        {
                            if (voxY < WaterLevel + 4)
                            {
                                //we must be above sand
                                voxelID = voxelData.getID("sand_grass");
                            }
                            else
                            {
                                //assume grass? might be rock or something
                                voxelID = (cro::Util::Random::value(0, 1) == 0)
                                    ? voxelData.getID("short_grass01")
                                    : voxelData.getID("short_grass02");
                            }
                        }
                    }
                }
                //on the top layer, so sand if near water
                //else grass (or whatever a biome map might throw up)
                else if (voxY == height)
                {
                    if (voxY < (WaterLevel + 3))
                    {
                        voxelID = voxelData.getID(vx::Sand);
                    }
                    else
                    {
                        //if using biome set the top data according
                        //to the current biome

                        voxelID = voxelData.getID(vx::Grass);
                    }
                }
                //some arbitrary depth of dirt below the surface.
                //again, a biome would influence this
                else if (voxY > (height - 4)) //TODO this value should be modulated by a depth map for varitation (as should grass)
                {
                    //we only want to put dirt under grass
                    //sand should have more sand underneath it
                    if (voxY > WaterLevel)
                    {
                        voxelID = voxelData.getID(vx::Dirt);
                    }
                    else
                    {
                        voxelID = voxelData.getID(vx::Sand);
                    }
                }
                else
                {
                    //we're underground, so make solid
                    voxelID = voxelData.getID(vx::Stone);
                }


                //set the voxelID at the current chunk position
                chunk.setVoxelQ({ x,y,z }, voxelID);
                if (voxelID != voxelData.getID(vx::Air))
                {
                    highestPoint = (y > highestPoint) ? y : highestPoint;
                }
            }
        }
    }

    chunk.setHighestPoint(highestPoint);
}