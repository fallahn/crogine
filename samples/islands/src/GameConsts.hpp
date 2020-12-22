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

#pragma once

#include <crogine/util/Constants.hpp>
#include <crogine/detail/glm/vec3.hpp>

#include <cstdint>
#include <cmath>
#include <array>

static constexpr float SpawnOffset = 30.f;
static const std::array PlayerSpawns =
{
    glm::vec3(-SpawnOffset, 0.f, -SpawnOffset),
    glm::vec3(SpawnOffset, 0.f, -SpawnOffset),
    glm::vec3(-SpawnOffset, 0.f, SpawnOffset),
    glm::vec3(SpawnOffset, 0.f, SpawnOffset)
};

static constexpr float BoatOffset = 2.f;
static const std::array BoatSpawns =
{
    PlayerSpawns[0] + glm::vec3(-BoatOffset, 0.f, -BoatOffset),
    PlayerSpawns[1] + glm::vec3(BoatOffset, 0.f, -BoatOffset),
    PlayerSpawns[2] + glm::vec3(-BoatOffset, 0.f, BoatOffset),
    PlayerSpawns[3] + glm::vec3(BoatOffset, 0.f, BoatOffset)
};

//player view
static constexpr float SeaRadius = 50.f;
static constexpr float CameraHeight = 4.5f;
static constexpr float CameraDistance = 15.f;
static constexpr std::uint32_t ReflectionMapSize = 512u;

//island generation
static constexpr float IslandSize = SeaRadius * 2.f; //this saves scaling the depth map in the sea shader.
static constexpr float TileSize = 0.8f;// 1.f;
static constexpr std::size_t IslandTileCount = static_cast<std::size_t>(IslandSize / TileSize);
static constexpr std::size_t IslandBorder = 9; //Tile count
static constexpr float IslandLevels = 16.f; //snap height vals to this many levels
static constexpr std::uint32_t IslandFadeSize = 13u;
static constexpr float EdgeVariation = 3.f;
static constexpr float IslandHeight = 4.2f; //these must be replicated in the sea water shader
static constexpr float IslandWorldHeight = -2.02f;

static constexpr float SunOffset = IslandSize * 1.35f;
static constexpr float GrassHeight = 3.1f;
static constexpr float BushRadius = 0.8f;

struct ClientRequestFlags
{
    enum
    {
        Heightmap = 0x1,
        TreeMap   = 0x2,
        BushMap   = 0x4,

        All = 0x1 | 0x2 | 0x4
    };
};
static constexpr std::uint32_t MaxDataRequests = 10;

//day night stuff
static constexpr std::uint32_t DayMinutes = 24 * 60;
static constexpr float RadsPerMinute = cro::Util::Const::TAU / 6.f; //6 minutes per cycle
static constexpr float RadsPerSecond = RadsPerMinute / 60.f;

static inline float readHeightmap(glm::vec3 position, const std::vector<float>& heightmap)
{
    auto lerp = [](float a, float b, float t) constexpr
    {
        return a + t * (b - a);
    };

    const auto getHeightAt = [&](std::int32_t x, std::int32_t y)
    {
        //heightmap is flipped relative to the world innit
        x = std::min(static_cast<std::int32_t>(IslandTileCount), std::max(0, x));
        y = std::min(static_cast<std::int32_t>(IslandTileCount), std::max(0, y));
        return heightmap[(IslandTileCount - y) * IslandTileCount + x];
    };

    float posX = position.x / TileSize;
    float posY = position.z / TileSize;

    float intpart = 0.f;
    auto modX = std::modf(posX, &intpart) / TileSize;
    auto modY = std::modf(posY, &intpart) / TileSize; //normalise this for lerpitude

    std::int32_t x = static_cast<std::int32_t>(posX);
    std::int32_t y = static_cast<std::int32_t>(posY);

    float topLine = lerp(getHeightAt(x, y), getHeightAt(x + 1, y), modX);
    float botLine = lerp(getHeightAt(x, y + 1), getHeightAt(x + 1, y + 1), modX);
    return lerp(topLine, botLine, modY) * IslandHeight;
}
