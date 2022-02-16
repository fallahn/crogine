/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/glm/vec4.hpp>

#include <crogine/graphics/Colour.hpp>

#include <array>

struct Layer final
{
    enum
    {
        Water, Terrain, Voxel,

        Count
    };
};

namespace Voxel
{
    constexpr glm::vec2 MapSize(320.f, 200.f);
    constexpr glm::ivec3 ChunkSize(16, 8, 16);
    constexpr glm::ivec3 ChunkCount(19, 1, 11);
    constexpr glm::ivec3 IslandSize = glm::ivec3(ChunkSize * ChunkCount);
    constexpr float WaterLevel = -0.02f;
    constexpr float TerrainLevel = WaterLevel - 0.03f;
    constexpr float MaxTerrainHeight = 4.5f; //must match value in golf terrain generator

    constexpr float CursorRadius = 1.f;
    constexpr float MinCursorScale = 1.f;
    constexpr float MaxCursorScale = 15.f;
    constexpr float CursorScaleStep = 0.5f;

    constexpr float BrushMaxStrength = 1.f;
    constexpr float BrushMinStrength = 0.05f;
    constexpr float BrushMaxFeather = 2.f;
    constexpr float BrushMinFeather = 0.f;

    //colour to set the cursor to depending on active layer
    const std::array<cro::Colour, Layer::Count> LayerColours =
    {
        cro::Colour::Transparent,
        cro::Colour::Magenta,
        cro::Colour::Cyan
    };
}

struct TerrainID final
{
    enum Enum
    {
        Rough, Fairway, Green, Bunker, Water, Scrub, Stone,

        Unused,
        Count
    };
};

static const std::array<std::string, TerrainID::Count - 1> TerrainStrings =
{
    "Rough", "Fairway", "Green", "Bunker", "Water", "Scrub", "Stone"
};

static const std::array TerrainColours =
{
    glm::vec4(0.157f,0.306f,0.263f,1.f),
    glm::vec4(0.275f,0.494f,0.243f,1.f),
    glm::vec4(0.576f,0.671f,0.322f,1.f),
    glm::vec4(0.949f,0.812f,0.361f,1.f),
    glm::vec4(0.f,0.f,1.f,1.f),
    glm::vec4(0.102f,0.115f,0.176f,1.f),
    glm::vec4(0.5f,0.5f,0.5f,1.f),

    glm::vec4(1.f,0.f,0.f,1.f)
};

static const std::array CollisionColours =
{
    glm::vec4(glm::vec3(0.02f), 1.f),
    glm::vec4(glm::vec3(0.059f), 1.f),
    glm::vec4(glm::vec3(0.098f), 1.f),
    glm::vec4(glm::vec3(0.137f), 1.f),
    glm::vec4(glm::vec3(0.176f), 1.f),
    glm::vec4(glm::vec3(0.216f), 1.f),
    glm::vec4(glm::vec3(0.255f), 1.f)
};

struct Shader
{
    enum
    {
        Water,
        Terrain,
        Voxel,

        Count
    };
};

using Material = Shader;