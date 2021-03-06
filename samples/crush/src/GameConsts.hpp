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
#include <crogine/graphics/BoundingBox.hpp>
#include <crogine/graphics/Colour.hpp>

#include <cstdint>
#include <cmath>
#include <array>

static constexpr std::int32_t CrushScore = 100;
static constexpr std::int32_t SnailScore = 40;
static constexpr std::int32_t RoundScore = 500;
static constexpr std::int32_t BonusScore = 20; //pickups etc
static constexpr std::int32_t DeathPenalty = -20;

struct RoundStats final
{
    bool winner = false;
    std::uint8_t crushCount = 0;
    std::uint8_t snailCount = 0;
    std::uint8_t deathCount = 0;
    std::uint8_t bonusCount = 0;
    std::uint8_t playerID = 0;
};

static constexpr float MapWidth = 36.f;
static constexpr float MapHeight = 18.f;

static constexpr float Gravity = -80.f;
static constexpr float MaxGravity = -30.f;

static constexpr glm::vec3 PlayerSize = glm::vec3(0.6f, 0.8f, 0.6f);
static constexpr cro::FloatRect FootBounds = cro::FloatRect(0.05f - (PlayerSize.x / 2.f), -0.05f, 0.5f, 0.05f);
static const cro::Box PlayerBounds = {glm::vec3(-((PlayerSize.x / 2.f) + 0.5f), 0.f, PlayerSize.z / 2.f), glm::vec3((PlayerSize.x / 2.f) + 0.5f, PlayerSize.y, -PlayerSize.z / 2.f)};

//assumes crate origin is centre
static const cro::Box CrateBounds = { glm::vec3(-0.3f, -0.3f, 0.3f), glm::vec3(0.3f, 0.3f, -0.3f) };
static constexpr cro::FloatRect CrateArea = cro::FloatRect(-0.25f, -0.25f, 0.5f, 0.5f);
static constexpr cro::FloatRect CrateFoot = cro::FloatRect(-0.2f, -0.35f, 0.4f, 0.1f);

static constexpr glm::vec3 CrateCarryOffset = glm::vec3((PlayerSize.x / 2.f) + (CrateArea.width / 2.f) + 0.1f, CrateArea.height * 1.7f, 0.f);

static constexpr float PuntVelocity = 30.f;
static constexpr float CrateFriction = 0.91f; //this is a multiplier

//assumes spawn is centre bottom
static constexpr cro::FloatRect SafeArea = cro::FloatRect(-0.4f, 0.f, 0.8f, 2.f);
static constexpr cro::FloatRect SpawnBase = cro::FloatRect(-0.5f, 0.f, 1.f, 0.2f);

//assumes snail is centre bottom
static constexpr cro::FloatRect SnailArea = cro::FloatRect(-0.3f, 0.f, 0.6f, 0.5f);
static constexpr cro::FloatRect SnailFoot = cro::FloatRect(-0.25f, -0.1f, 0.5f, 0.1f);
static const cro::Box SnailBounds = { glm::vec3(SnailArea.left, SnailArea.bottom, 0.3f), glm::vec3(SnailArea.width / 2.f, SnailArea.height, -0.3f) };

static const cro::Box ExplosionBounds = { glm::vec3(-1.5f), glm::vec3(1.5f) };
static constexpr cro::FloatRect ExplosionArea = cro::FloatRect(-1.5f, -1.5f, 3.f, 3.f);

static constexpr float HoloRotationSpeed = 0.5f;

static constexpr float LayerDepth = 7.5f;
static constexpr float LayerThickness = 0.55f;
static constexpr float SpawnOffset = 10.f;
static constexpr std::array PlayerSpawns =
{
    glm::vec3(-SpawnOffset, 0.f, LayerDepth),
    glm::vec3(SpawnOffset, 0.f, -LayerDepth),
    glm::vec3(-SpawnOffset, 0.f, LayerDepth),
    glm::vec3(SpawnOffset, 0.f, -LayerDepth)
};

static const std::array PlayerColours =
{
    cro::Colour::Red, cro::Colour::Magenta,
    cro::Colour::Green, cro::Colour::Yellow
};

//player view
static constexpr float CameraHeight = 1.5f;
static constexpr float CameraDistance = 12.f;
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