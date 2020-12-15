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

#include <cstdint>

static constexpr float SeaRadius = 50.f;
static constexpr float CameraHeight = 3.2f;
static constexpr float CameraDistance = 10.f;
static constexpr std::uint32_t ReflectionMapSize = 512u;

static constexpr float IslandSize = SeaRadius * 2.f;// 98.f; //this saves scaling the depth map in the sea shader.
static constexpr float TileSize = 1.f;
static constexpr std::size_t IslandTileCount = static_cast<std::size_t>(IslandSize / TileSize);
static constexpr std::size_t IslandBorder = 9; //Tile count
static constexpr float IslandLevels = 16.f; //snap height vals to this many levels
static constexpr std::uint32_t IslandFadeSize = 13u;
static constexpr float EdgeVariation = 3.f;
static constexpr float IslandHeight = 4.f;
static constexpr float IslandWorldHeight = -2.02f;
