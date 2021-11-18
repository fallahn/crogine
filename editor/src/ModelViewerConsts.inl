/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2021
http://trederia.blogspot.com

crogine editor - Zlib license.

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

#include <crogine/detail/glm/vec3.hpp>

#include <crogine/util/Constants.hpp>

#include <cstdint>

static const glm::vec3 DefaultCameraPosition({ 0.f, 0.2f, 5.f });
static const glm::vec3 DefaultArcballPosition({ 0.f, 0.75f, 0.f });

static constexpr float DefaultFOV = 35.f * cro::Util::Const::degToRad;
static constexpr float MaxFOV = 120.f * cro::Util::Const::degToRad;
static constexpr float MinFOV = 5.f * cro::Util::Const::degToRad;
static constexpr float DefaultFarPlane = 30.f;

struct CameraSettings final
{
    float FOV = DefaultFOV;
    float FarPlane = DefaultFarPlane;
    cro::Entity camera;
};

struct CameraID final
{
    enum
    {
        Default,
        FreeLook,

        Count
    };
};

static constexpr std::uint32_t LightmapSize = 1024;

static constexpr std::uint8_t MaxSubMeshes = 8; //for imported models. Can be made bigger but this is generally a waste of memory

float updateView(cro::Entity entity, float farPlane, float fov);