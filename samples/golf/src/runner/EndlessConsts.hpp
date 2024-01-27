/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#include "../Colordome-32.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/detail/glm/vec2.hpp>

#include <cstdint>

static inline constexpr float BeefStickTime = 5.f;
static inline constexpr float BallTime = 0.13f;

static inline constexpr std::uint32_t RenderScale = 2;
//static inline constexpr glm::uvec2 RenderSize = glm::uvec2(320, 224) * RenderScale;
static inline constexpr glm::uvec2 RenderSize = glm::uvec2(320, 180) * RenderScale;
static inline constexpr glm::vec2 RenderSizeFloat = glm::vec2(RenderSize);
static inline constexpr float ScreenHalfWidth = RenderSizeFloat.x / 2.f;
static inline constexpr float PlayerWidth = 74.f * RenderScale;
static inline constexpr float PlayerHeight = 84.f * RenderScale;
static inline constexpr cro::FloatRect PlayerBounds((RenderSizeFloat.x - PlayerWidth) / 2.f, 0.f, PlayerWidth, PlayerHeight);

static inline constexpr float FogDensity = 15.f;
static inline float expFog(float distance, float density)
{
    float fogAmount = 1.f / (std::pow(cro::Util::Const::E, (distance * distance * density)));
    fogAmount = (0.5f * (1.f - fogAmount));

    fogAmount = std::round(fogAmount * 25.f);
    fogAmount /= 25.f;

    return fogAmount;
}
static inline const cro::Colour GrassFogColour = CD32::Colours[CD32::GreenDark];
static inline const cro::Colour RoadFogColour = CD32::Colours[CD32::BlueDark];

static inline float getViewScale(glm::vec2 windowSize)
{
    //return std::max(1.f, std::floor(windowSize.y / RenderSize.y));

    //float scale = std::floor(windowSize.x / RenderSize.x);
    float scale = std::floor(windowSize.y / RenderSize.y);

    //if we have a bigger ratio than 16:9 we're probably ultra-wide so zoom out
    if ((windowSize.x / windowSize.y) > (16.f / 9.f))
    {
        scale = std::max(1.f, scale - 1.f);
    }
    return scale;
}

struct CommandID final
{
    enum
    {
        UIElement = 0x1
    };
};

struct TextFlashCallback final
{
    static constexpr float FlashTime = 1.f;
    float currTime = FlashTime;

    void operator() (cro::Entity e, float dt)
    {
        currTime -= dt;

        if (currTime < 0)
        {
            currTime += 1.f;
            auto scale = e.getComponent<cro::Transform>().getScale().x;
            scale = scale == 1 ? 0.f : 1.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        }
    }
};