#pragma once

#include <crogine/detail/glm/vec3.hpp>

#include <crogine/util/Constants.hpp>

#include <cstdint>

static const glm::vec3 DefaultCameraPosition({ 0.f, 0.25f, 5.f });
static const glm::vec3 DefaultArcballPosition({ 0.f, 0.75f, 0.f });

static constexpr float DefaultFOV = 35.f * cro::Util::Const::degToRad;
static constexpr float MaxFOV = 120.f * cro::Util::Const::degToRad;
static constexpr float MinFOV = 5.f * cro::Util::Const::degToRad;
static constexpr float DefaultFarPlane = 30.f;

static constexpr std::uint32_t LightmapSize = 1024;

static constexpr std::uint8_t MaxSubMeshes = 8; //for imported models. Can be made bigger but this is generally a waste of memory

float updateView(cro::Entity entity, float farPlane, float fov);