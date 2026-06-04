#pragma once

#include <crogine/detail/glm/vec2.hpp>
#include <crogine/util/Constants.hpp>

#include <array>
#include <cmath>
#include <vector>

//gravity is reduced by the dot prod of the
//angle of the table relative to straight down
static constexpr float TableAngle = 6.5f;
static constexpr glm::vec2 Gravity = glm::vec2(0.f, -9.95f * (TableAngle / 90.f));

//units are in metres / kg
static constexpr glm::vec2 TableSize = glm::vec2(0.56f, 1.2f);
static constexpr float PinballRadius = 0.0135f;
static constexpr glm::vec2 PinballSpawn = glm::vec2((TableSize.x / 2.f) - PinballRadius, -((TableSize.y / 2.f) - PinballRadius));

//b2d automatically creates box shapes about the centre
static constexpr float Thickness = 0.05f;
static constexpr std::array<std::pair<glm::vec2, glm::vec2>, 4u> WallPosSize =
{
    std::make_pair(glm::vec2(0.f, -(TableSize.y / 2.f) - Thickness), glm::vec2(TableSize.x / 2.f, Thickness)),
    std::make_pair(glm::vec2(0.f, (TableSize.y / 2.f) + Thickness), glm::vec2(TableSize.x / 2.f, Thickness)),
    std::make_pair(glm::vec2(-(TableSize.x / 2.f) - Thickness, 0.f), glm::vec2(Thickness, TableSize.y / 2.f)),
    std::make_pair(glm::vec2((TableSize.x / 2.f) + Thickness, 0.f), glm::vec2(Thickness, TableSize.y / 2.f)),
};


constexpr glm::vec2 ArcSegmentSize = glm::vec2(0.045f, 0.015f);
constexpr float ArcRadius = (TableSize.x / 2.f) + ArcSegmentSize.y;
constexpr glm::vec2 ArcCentre = glm::vec2(0.f, (TableSize.y / 2.f) - ArcRadius);

static inline std::vector<glm::vec2> getArc()
{
    std::vector<glm::vec2> verts;
    constexpr auto segment = cro::Util::Const::PI / 12.f;
    for (auto i = 0; i < 13; ++i)
    {
        const float theta = ((i * segment) - (cro::Util::Const::PI / 2.f));
        const glm::vec2 p = { std::sin(theta), std::cos(theta) };
        verts.emplace_back((p * ArcRadius) + ArcCentre);
    }

    return verts;
}