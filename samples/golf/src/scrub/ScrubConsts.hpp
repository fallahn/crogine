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

#include <crogine/graphics/Colour.hpp>
#include <crogine/detail/glm/vec2.hpp>

#include <cstdint>

//#define HIDE_BACKGROUND

static constexpr inline float MaxSpriteScale = 4.f; //when view scale returns this (~2kHD) sprites on the HUD are 1:1
static constexpr cro::Colour SoapMeterColour = cro::Colour(0xadd9b7ff);

namespace sc
{
    struct ShaderID final
    {
        enum
        {
            LevelMeter,
            Fire,

            Count
        };
    };

    struct FontID final
    {
        enum
        {
            Title, Body,

            Count
        };
    };

    static inline constexpr std::uint32_t SmallTextSize = 12;
    static inline constexpr std::uint32_t MediumTextSize = 24;
    static inline constexpr std::uint32_t LargeTextSize = 72;

    static inline const float TextDepth = 1.f;
    static inline const float UIBackgroundDepth = -1.f;

    //hmm we need to make sure to set these with the scene scale too...
    static inline constexpr glm::vec2 SmallTextOffset = glm::vec2(1.f);
    static inline constexpr glm::vec2 MediumTextOffset = glm::vec2(2.f);
    static inline constexpr glm::vec2 LargeTextOffset = glm::vec2(6.f);
}