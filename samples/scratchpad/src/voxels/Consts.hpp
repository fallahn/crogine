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
    constexpr glm::ivec3 IslandSize(310, 4, 190);
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