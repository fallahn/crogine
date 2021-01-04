/*-----------------------------------------------------------------------

Matt Marchant 2020
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

#include "IslandGenerator.hpp"
#include "GameConsts.hpp"

#include "PoissonDisk.hpp"
#include "fastnoise/FastNoiseSIMD.h"

#include <crogine/detail/glm/gtx/norm.hpp>

IslandGenerator::IslandGenerator()
    : m_heightmap(IslandTileCount * IslandTileCount, 0.f)
{

}


//public
void IslandGenerator::generate(std::int32_t seed)
{
    createHeightmap(seed);
    createFoliageMaps(seed);
}

//private
void IslandGenerator::createHeightmap(std::int32_t seed)
{
    //height mask
    const float MaxLength = TileSize * IslandFadeSize;
    const std::uint32_t Offset = IslandBorder + IslandFadeSize;

    for (auto y = IslandBorder; y < IslandTileCount - IslandBorder; ++y)
    {
        for (auto x = IslandBorder; x < IslandTileCount - IslandBorder; ++x)
        {
            float val = 0.f;
            float pos = 0.f;

            if (x < IslandTileCount / 2)
            {
                pos = (x - IslandBorder) * TileSize;

            }
            else
            {
                pos = (IslandTileCount - IslandBorder - x) * TileSize;
            }

            val = pos / MaxLength;
            val = std::min(1.f, std::max(0.f, val));

            if (y > Offset && y < IslandTileCount - Offset)
            {
                m_heightmap[y * IslandTileCount + x] = val;
            }

            if (y < IslandTileCount / 2)
            {
                pos = (y - IslandBorder) * TileSize;

            }
            else
            {
                pos = (IslandTileCount - IslandBorder - y) * TileSize;
            }

            val = pos / MaxLength;
            val = std::min(1.f, std::max(0.f, val));

            if (x > Offset && x < IslandTileCount - Offset)
            {
                m_heightmap[y * IslandTileCount + x] = val;
            }
        }
    }

    //mask corners
    auto corner = [&, MaxLength](glm::uvec2 start, glm::uvec2 end, glm::vec2 centre)
    {
        for (auto y = start.t; y < end.t + 1; ++y)
        {
            for (auto x = start.s; x < end.s + 1; ++x)
            {
                glm::vec2 pos = glm::vec2(x, y) * TileSize;
                float val = 1.f - (glm::length(pos - centre) / MaxLength);
                val = std::max(0.f, std::min(1.f, val));
                m_heightmap[y * IslandTileCount + x] = val;
            }
        }
    };
    glm::vec2 centre = glm::vec2(Offset, Offset) * TileSize;
    corner({ IslandBorder, IslandBorder }, { Offset, Offset }, centre);

    centre.x = (IslandTileCount - Offset) * TileSize;
    corner({ IslandTileCount - Offset - 1, IslandBorder }, { IslandTileCount - IslandBorder, Offset }, centre);

    centre.y = (IslandTileCount - Offset) * TileSize;
    corner({ IslandTileCount - Offset - 1, IslandTileCount - Offset - 1 }, { IslandTileCount - IslandBorder, IslandTileCount - IslandBorder }, centre);

    centre.x = Offset * TileSize;
    corner({ IslandBorder, IslandTileCount - Offset - 1 }, { Offset, IslandTileCount - IslandBorder }, centre);


    //fastnoise
    FastNoiseSIMD* myNoise = FastNoiseSIMD::NewFastNoiseSIMD(seed);
    myNoise->SetFrequency(0.05f);
    float* noiseSet = myNoise->GetSimplexFractalSet(0, 0, 0, IslandTileCount * 4, 1, 1);

    //use a 1D noise push the edges of the mask in/out by +/-1
    std::uint32_t i = 0;
    for (auto x = Offset; x < IslandTileCount - Offset; ++x)
    {
        auto val = std::round(noiseSet[i++] * EdgeVariation);
        if (val < 0) //move gradient tiles up one
        {
            for (auto j = 0; j < std::abs(val); ++j)
            {
                //top row
                for (auto y = IslandBorder; y < Offset + 1; ++y)
                {
                    m_heightmap[(y - 1) * IslandTileCount + x] = m_heightmap[y * IslandTileCount + x];
                }

                //bottom row
                for (auto y = IslandTileCount - Offset - 1; y < IslandTileCount - IslandBorder; ++y)
                {
                    m_heightmap[(y - 1) * IslandTileCount + x] = m_heightmap[y * IslandTileCount + x];
                }
            }
        }
        else if (val > 0) //move gradient tiles down one
        {
            for (auto j = 0; j < val; ++j)
            {
                //top row
                for (auto y = Offset; y > IslandBorder; --y)
                {
                    m_heightmap[y * IslandTileCount + x] = m_heightmap[(y - 1) * IslandTileCount + x];
                }

                //bottom row
                for (auto y = IslandTileCount - IslandBorder; y > IslandTileCount - Offset - 1; --y)
                {
                    m_heightmap[y * IslandTileCount + x] = m_heightmap[(y - 1) * IslandTileCount + x];
                }
            }
        }
    }

    for (auto y = Offset; y < IslandTileCount - Offset; ++y)
    {
        auto val = std::round(noiseSet[i++] * EdgeVariation);
        if (val < 0) //move gradient tiles left one
        {
            for (auto j = 0; j < std::abs(val); ++j)
            {
                //left col
                for (auto x = IslandBorder; x < Offset + 1; ++x)
                {
                    m_heightmap[y * IslandTileCount + x] = m_heightmap[y * IslandTileCount + (x + 1)];
                }

                //right col
                for (auto x = IslandTileCount - Offset - 1; x < IslandTileCount - IslandBorder; ++x)
                {
                    m_heightmap[y * IslandTileCount + x] = m_heightmap[y * IslandTileCount + (x + 1)];
                }
            }
        }
        else if (val > 0) //move gradient tiles right one
        {
            for (auto j = 0; j < val; ++j)
            {
                //left col
                for (auto x = Offset; x > IslandBorder; --x)
                {
                    m_heightmap[y * IslandTileCount + (x + 1)] = m_heightmap[y * IslandTileCount + x];
                }

                //right col
                for (auto x = IslandTileCount - IslandBorder; x > IslandTileCount - Offset - 1; --x)
                {
                    m_heightmap[y * IslandTileCount + (x + 1)] = m_heightmap[y * IslandTileCount + x];
                }
            }
        }
    }
    FastNoiseSIMD::FreeNoiseSet(noiseSet);

    //main island noise
    myNoise->SetFrequency(0.01f);
    noiseSet = myNoise->GetSimplexFractalSet(0, 0, 0, IslandTileCount, IslandTileCount, 1);

    myNoise->SetFrequency(0.1f);
    auto* noiseSet2 = myNoise->GetSimplexFractalSet(0, 0, 0, IslandTileCount, IslandTileCount, 1);

    for (auto y = 0u; y < IslandTileCount; ++y)
    {
        for (auto x = 0u; x < IslandTileCount; ++x)
        {
            float val = noiseSet[y * IslandTileCount + x];
            val += 1.f;
            val /= 2.f;

            const float minAmount = IslandLevels * 0.6f;
            float multiplier = IslandLevels - minAmount;

            val *= multiplier;
            val = std::floor(val);
            val += minAmount;
            val /= IslandLevels;

            float valMod = noiseSet2[y * IslandTileCount + x];
            valMod += 1.f;
            valMod /= 2.f;
            valMod = 0.8f + valMod * 0.2f;

            m_heightmap[y * IslandTileCount + x] *= val * valMod;
        }
    }

    FastNoiseSIMD::FreeNoiseSet(noiseSet2);
    FastNoiseSIMD::FreeNoiseSet(noiseSet);
}

void IslandGenerator::createFoliageMaps(std::int32_t seed)
{
    std::array minBounds = { IslandBorder * TileSize * 2.f, IslandBorder * TileSize * 2.f };
    std::array maxBounds = { IslandSize - (IslandBorder * TileSize * 2.f), IslandSize - (IslandBorder * TileSize * 2.f) };

    auto plantMap = thinks::PoissonDiskSampling(BushRadius, minBounds, maxBounds, 30u, seed);
    for (auto [x, y] : plantMap)
    {
        if (readHeightmap({ x, 0.f, y }, m_heightmap) > GrassHeight)
        {
            m_bushmap.emplace_back(x, y);
        }
    }

    plantMap = thinks::PoissonDiskSampling(BushRadius * 3.5f, minBounds, maxBounds, 30u, seed * 2);
    for (auto [x, y] : plantMap)
    {
        if (readHeightmap({ x, 0.f, y }, m_heightmap) > GrassHeight)
        {
            m_treemap.emplace_back(x, y);
        }
    }
}