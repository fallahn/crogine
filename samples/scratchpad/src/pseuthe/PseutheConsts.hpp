#pragma once

#include <crogine/core/App.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/detail/glm/vec2.hpp>
#include <crogine/graphics/Colour.hpp>

static inline constexpr glm::uvec2 SceneSize(1920u, 1088u); //this allows dividing cells into 64px
static inline constexpr glm::vec2 SceneSizeFloat(SceneSize);

static inline constexpr std::uint32_t CellSize = 64u;
static inline constexpr auto CellCountX = static_cast<std::int32_t>(SceneSize.x / CellSize);
static inline constexpr auto CellCountY = static_cast<std::int32_t>(SceneSize.y / CellSize);
static inline constexpr glm::vec2 CellCentre = glm::vec2(CellSize / 2);

static inline constexpr float MinLightPos = -30.f;
static inline constexpr float MaxLightPos = SceneSizeFloat.x - MinLightPos;
static inline constexpr float LightFullBright = 150.f;

static inline constexpr float BackgroundDepth = -10.f;
static inline constexpr float LightRayDepth = -9.f;
static inline constexpr float ParticleDepth = -8.f;
static inline constexpr float BallDepth = 0.f;
static inline constexpr float ScreenFadeDepth = 6.f;

static inline constexpr std::size_t BallCount = 19;
static inline constexpr std::int32_t MinBallSize = 40;
static inline constexpr std::int32_t MaxBallSize = 92;
static inline constexpr float BallSize = 128.f; //this is the sprite size

//constexpr cro::Colour PlayerColour(std::uint8_t(200u), 200u, 230u, 180u);
static inline constexpr cro::Colour PlayerColour(std::uint8_t(140u), 140u, 161u, 180u);

struct ShaderID final
{
    enum
    {
        LightRay, Ball,

        BlurH, BlurV,
        Count
    };
};

static inline constexpr float NearPlane = -10.f;
static inline constexpr float FarPlane = 20.f;
static inline void cameraCallback(cro::Camera& cam)
{
    const auto windowSize = glm::vec2(cro::App::getWindow().getSize());
    const float windowRatio = windowSize.x / windowSize.y;

    constexpr float sceneRatio = SceneSizeFloat.x / SceneSizeFloat.y;
    const float height = windowRatio / sceneRatio;
    const float bottom = (1.f - height) / 2.f;

    cam.viewport = { 0.f, bottom, 1.f, height };
    cam.setOrthographic(0.f, SceneSizeFloat.x, 0.f, SceneSizeFloat.y, NearPlane, FarPlane);
}

static inline glm::ivec2 getGridPosition(glm::vec2 worldPosition)
{
    return glm::ivec2(std::floor(worldPosition.x / CellSize), std::floor(worldPosition.y / CellSize));
}

static inline glm::vec2 getWorldPosition(glm::ivec2 gridPosition)
{
    return glm::vec2(gridPosition.x * CellSize, gridPosition.y * CellSize);
}