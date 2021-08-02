/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#pragma once

#include "Terrain.hpp"

#include <crogine/graphics/Colour.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/util/Constants.hpp>

#include <cstdint>

static constexpr float CameraStrokeHeight = 3.2f;// 2.6f;
static constexpr float CameraPuttHeight = 0.3f;
static constexpr float CameraStrokeOffset = 7.f;// 5.1f;
static constexpr float CameraPuttOffset = 0.8f;
static constexpr float FOV = 60.f * cro::Util::Const::degToRad;

static constexpr float BallPointSize = 1.4f;

static constexpr float MaxHook = -0.25f;
static constexpr float KnotsPerMetre = 1.94384f;
static constexpr float HoleRadius = 0.053f;

static constexpr float WaterLevel = -0.02f;
static constexpr float TerrainLevel = WaterLevel - 0.03f;
static constexpr float MaxTerrainHeight = 3.5f;

struct ShaderID final
{
    enum
    {
        Water = 100,
        Terrain,
        Cel
    };
};

static inline std::pair<std::uint8_t, float> readMap(const cro::Image& img, float px, float py)
{
    auto size = glm::vec2(img.getSize());
    //I forget why our coords a re float - but this makes for horrible casts :(
    std::uint32_t x = static_cast<std::uint32_t>(std::min(size.x, std::max(0.f, std::floor(px))));
    std::uint32_t y = static_cast<std::uint32_t>(std::min(size.y, std::max(0.f, std::floor(py))));

    std::uint32_t stride = 4;
    //TODO we should have already asserted the format is RGBA elsewhere...
    if (img.getFormat() == cro::ImageFormat::RGB)
    {
        stride = 3;
    }

    auto index = (y * static_cast<std::uint32_t>(size.x) + x) * stride;

    std::uint8_t terrain = img.getPixelData()[index] / 10;
    terrain = std::min(static_cast<std::uint8_t>(TerrainID::Scrub), terrain);

    auto height = static_cast<float>(img.getPixelData()[index + 1]) / 255.f;
    height *= MaxTerrainHeight;

    switch (terrain)
    {
    default:
        return { terrain, height };
    case TerrainID::Scrub:
        return height > -(TerrainLevel - WaterLevel) ? std::make_pair(terrain, height) : std::make_pair(TerrainID::Water, height);
    }
}

//TODO use this for interpolating slop height on a height map
static inline float readHeightmap(glm::vec3 position, const std::vector<float>& heightmap)
{
    auto lerp = [](float a, float b, float t) constexpr
    {
        return a + t * (b - a);
    };

    //const auto getHeightAt = [&](std::int32_t x, std::int32_t y)
    //{
    //    //heightmap is flipped relative to the world innit
    //    x = std::min(static_cast<std::int32_t>(IslandTileCount), std::max(0, x));
    //    y = std::min(static_cast<std::int32_t>(IslandTileCount), std::max(0, y));
    //    return heightmap[(IslandTileCount - y) * IslandTileCount + x];
    //};

    //float posX = position.x / TileSize;
    //float posY = position.z / TileSize;

    //float intpart = 0.f;
    //auto modX = std::modf(posX, &intpart) / TileSize;
    //auto modY = std::modf(posY, &intpart) / TileSize; //normalise this for lerpitude

    //std::int32_t x = static_cast<std::int32_t>(posX);
    //std::int32_t y = static_cast<std::int32_t>(posY);

    //float topLine = lerp(getHeightAt(x, y), getHeightAt(x + 1, y), modX);
    //float botLine = lerp(getHeightAt(x, y + 1), getHeightAt(x + 1, y + 1), modX);
    //return lerp(topLine, botLine, modY) * IslandHeight;
}