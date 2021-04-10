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

#include <crogine/util/Constants.hpp>
#include <crogine/detail/glm/vec3.hpp>

#include <cstdint>
#include <cmath>
#include <array>

static const glm::vec3 PlayerSize = glm::vec3(0.6f, 0.8f, 0.6f);

static constexpr float LayerDepth = 7.5f;
static constexpr float LayerThickness = 0.55f;
static constexpr float SpawnOffset = 10.f;
static const std::array PlayerSpawns =
{
    glm::vec3(-SpawnOffset, 0.f, LayerDepth),
    glm::vec3(SpawnOffset, 0.f, LayerDepth),
    glm::vec3(-SpawnOffset, 0.f, -LayerDepth),
    glm::vec3(SpawnOffset, 0.f, -LayerDepth)
};

//player view
static constexpr float CameraHeight = 4.5f;
static constexpr float CameraDistance = 15.f;
static constexpr std::uint32_t ReflectionMapSize = 512u;

static constexpr float SunOffset = 153.f;

struct ClientRequestFlags
{
    enum
    {
        MapName = 0x1,
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