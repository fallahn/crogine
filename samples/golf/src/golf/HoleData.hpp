/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2024
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include "Path.hpp"

#include <crogine/ecs/Entity.hpp>
#include <crogine/util/Spline.hpp>
#include <crogine/detail/glm/vec3.hpp>
#include <crogine/graphics/Colour.hpp>

#include <array>
#include <string>

static inline const std::array<std::string, 12u> CourseNames =
{
    "course_01",
    "course_02",
    "course_03",
    "course_04",
    "course_05",
    "course_06",
    "course_07",
    "course_08",
    "course_09",
    "course_10",
    "course_11",
    "course_12",
};

static inline std::int32_t getCourseIndex(const std::string& name)
{
    if (auto result = std::find(CourseNames.begin(), CourseNames.end(), name); result != CourseNames.end())
    {
        return static_cast<std::int32_t>(std::distance(CourseNames.begin(), result));
    }
    return -1;
}

struct LightData final
{
    float radius = 0.f;
    glm::vec3 position = glm::vec3(0.f);
    cro::Colour colour = cro::Colour::White;
    std::string animation;
};

struct HoleData final
{
    glm::vec3 tee = glm::vec3(0.f);
    glm::vec3 target = glm::vec3(1.f);
    glm::vec3 subtarget = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 pin = glm::vec3(0.f);
    float distanceToPin = 0.f;
    std::int32_t par = 0;
    bool puttFromTee = false;
    std::string mapPath;
    std::string modelPath;
    cro::Entity modelEntity;
    std::vector<LightData> lightData;
    std::vector<cro::Entity> lights;
    std::vector<cro::Entity> propEntities;
    std::vector<cro::Entity> particleEntities;
    std::vector<cro::Entity> audioEntities;
    std::vector<glm::mat4> crowdPositions;
    std::vector<Path> crowdCurves;
    std::vector<Path> propCurves;
};

static inline constexpr std::size_t MaxHoles = 18;