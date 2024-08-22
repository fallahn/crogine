#pragma once

#include <crogine/core/App.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/detail/glm/vec2.hpp>

static inline constexpr glm::uvec2 SceneSize(1920u, 1080u);
static inline constexpr glm::vec2 SceneSizeFloat(SceneSize);

static inline constexpr float MinLightPos = -30.f;
static inline constexpr float MaxLightPos = SceneSizeFloat.x - MinLightPos;
static inline constexpr float LightFullBright = 150.f;

static inline constexpr float BackgroundDepth = -10.f;
static inline constexpr float LightRayDepth = -9.f;
static inline constexpr float ParticleDepth = -8.f;
static inline constexpr float BallDepth = 0.f;
static inline constexpr float ScreenFadeDepth = 1.f;

struct ShaderID final
{
    enum
    {
        LightRay,


        Count
    };
};

static inline void cameraCallback(cro::Camera& cam)
{
    const auto windowSize = glm::vec2(cro::App::getWindow().getSize());
    const float windowRatio = windowSize.x / windowSize.y;

    constexpr float sceneRatio = SceneSizeFloat.x / SceneSizeFloat.y;
    const float height = windowRatio / sceneRatio;
    const float bottom = (1.f - height) / 2.f;

    cam.viewport = { 0.f, bottom, 1.f, height };
    cam.setOrthographic(0.f, SceneSizeFloat.x, 0.f, SceneSizeFloat.y, -10.f, 20.f);
}
