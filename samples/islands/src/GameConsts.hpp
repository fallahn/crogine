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

#include <cstdint>
#include <array>

//player view
static constexpr float SeaRadius = 50.f;
static constexpr float CameraHeight = 3.2f;
static constexpr float CameraDistance = 10.f;
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

struct ClientRequestFlags
{
    enum
    {
        Heightmap = 0x1,
        TreeMap   = 0x2,


        All = 0x1 | 0x2
    };
};
static constexpr std::uint32_t MaxDataRequests = 10;

//day night stuff
static constexpr std::uint32_t DayMinutes = 24 * 60;
static constexpr float RadsPerMinute = cro::Util::Const::TAU / 6.f; //6 minutes per cycle
static constexpr float RadsPerSecond = RadsPerMinute / 60.f;
