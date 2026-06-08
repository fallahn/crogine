#pragma once

#include <box2d/math_functions.h>

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
//so walls are centre pos + half size
static constexpr float Thickness = 0.05f;
static constexpr std::array<std::pair<glm::vec2, glm::vec2>, 4u> WallPosSize =
{
    std::make_pair(glm::vec2(0.f, -(TableSize.y / 2.f) - Thickness), glm::vec2(TableSize.x / 2.f, Thickness)),
    std::make_pair(glm::vec2(0.f, (TableSize.y / 2.f) + Thickness), glm::vec2(TableSize.x / 2.f, Thickness)),
    std::make_pair(glm::vec2(-(TableSize.x / 2.f) - Thickness, 0.f), glm::vec2(Thickness, TableSize.y / 2.f)),
    std::make_pair(glm::vec2((TableSize.x / 2.f) + Thickness, 0.f), glm::vec2(Thickness, TableSize.y / 2.f)),
};

static constexpr glm::vec2 ChannelWallSize = glm::vec2(0.0065f, 0.375f);
static constexpr glm::vec2 ChannelWallPos = glm::vec2((TableSize.x / 2.f) - (PinballRadius * 2.f) - ChannelWallSize.x - 0.001f, (-TableSize.y / 2.f) + ChannelWallSize.y);

//this is the centre between the left wall and the funnel wall
static constexpr float PlayAreaCentre = ((TableSize.x / 2.f) + (ChannelWallPos.x - ChannelWallSize.x)) / 2.f;

//triangular funnels at the bottom
static constexpr glm::vec2 FunnelSize = glm::vec2(PlayAreaCentre, 0.18f);
static constexpr glm::vec2 FunnelLeftPos = -TableSize / 2.f;
static constexpr glm::vec2 FunnelRightPos = { FunnelLeftPos.x + FunnelSize.x, FunnelLeftPos.y };

static constexpr glm::vec2 ArcSegmentSize = glm::vec2(0.045f, 0.015f);
static constexpr float ArcRadius = (TableSize.x / 2.f) + ArcSegmentSize.y;
static constexpr glm::vec2 ArcCentre = glm::vec2(0.f, (TableSize.y / 2.f) - ArcRadius);

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

//ramp on the left which pushes new ball into play
static constexpr std::array<glm::vec2, 4u> RampPoints =
{
    glm::vec2(-(TableSize.x / 2.f) - 0.01f, -0.25f),
    glm::vec2(-0.2f, -0.15f),
    glm::vec2(-0.19f, 0.f),
    glm::vec2(-(TableSize.x / 2.f) - 0.01f, 0.16f)
};

//triangle bumper above flipper
static constexpr std::array<glm::vec2, 3u> BumperPoints =
{
    glm::vec2(-0.12f, -0.37f),
    glm::vec2(-0.16f, -0.25f),
    glm::vec2(-0.16f, -0.34f)
};
static constexpr float BumperRad = 0.01f;


static constexpr glm::vec2 FlipperChuteSizeLarge = glm::vec2(0.025f, 0.11f);
static constexpr glm::vec2 FlipperChutePosLarge = glm::vec2(-0.16f, -0.416f);
static constexpr float FlipperChuteLargeRotation = 51.7f * cro::Util::Const::degToRad;

static constexpr glm::vec2 FlipperChuteSizeSmall = glm::vec2(0.01f, 0.13f);
static constexpr glm::vec2 FlipperChutePosSmall = glm::vec2(-0.208f, -0.32f);

static constexpr float FlipperLength = 0.065f;
static constexpr float FlipperRadius = 0.01f;
static constexpr float FlipperRestRotation = 40.f * cro::Util::Const::degToRad;
static constexpr float FlipperActiveRotation = 1.2f;


//helper func because b2d has no ctors
static inline b2Vec2 b2Vector(float x, float y)
{
    b2Vec2 r = {};
    r.x = x;
    r.y = y;
    return r;
}

static inline b2Vec2 b2Vector(glm::vec2 v)
{
    b2Vec2 r = {};
    r.x = v.x;
    r.y = v.y;
    return r;
}