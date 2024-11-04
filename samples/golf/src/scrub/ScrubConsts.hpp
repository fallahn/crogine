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

#include "../golf/MessageIDs.hpp"

#include <crogine/graphics/Colour.hpp>
#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/glm/vec3.hpp>

#include <cstdint>

//#define HIDE_BACKGROUND

static constexpr inline float BucketOffset = 10.f;
static constexpr inline glm::vec3 BucketSpawnPosition = glm::vec3(BucketOffset, 7.f, 0.f);

static constexpr inline float MaxSpriteScale = 4.f; //when view scale returns this (~2kHD) sprites on the HUD are 1:1
static constexpr inline cro::Colour SoapMeterColour = cro::Colour(0x0207ffff);// cro::Colour(0xadd9b7ff);

//xbox
static constexpr inline std::uint32_t ButtonLT = 0x2196;
static constexpr inline std::uint32_t ButtonRT = 0x2197;
static constexpr inline std::uint32_t ButtonLB = 0x2198;
static constexpr inline std::uint32_t ButtonRB = 0x2199;
static constexpr inline std::uint32_t ButtonX  = 0x21D0;
static constexpr inline std::uint32_t ButtonY  = 0x21D1;
static constexpr inline std::uint32_t ButtonB  = 0x21D2;
static constexpr inline std::uint32_t ButtonA  = 0x21D3;


//ps
static constexpr inline std::uint32_t ButtonL1       = 0x21B0;
static constexpr inline std::uint32_t ButtonR1       = 0x21B1;
static constexpr inline std::uint32_t ButtonL2       = 0x21B2;
static constexpr inline std::uint32_t ButtonR2       = 0x21B3;
static constexpr inline std::uint32_t ButtonSquare   = 0x21E0;
static constexpr inline std::uint32_t ButtonTriangle = 0x21E1;
static constexpr inline std::uint32_t ButtonCircle   = 0x21E2;
static constexpr inline std::uint32_t ButtonCross    = 0x21E3;


static constexpr inline std::uint32_t RightStick     = 0x21C6;
static constexpr inline std::uint32_t Warning        = 0x26A0;
static constexpr inline std::uint32_t EmojiTerminate = 0xFE0F;

static constexpr inline glm::uvec2 BucketTextureSize = glm::uvec2(468, 1280);
static constexpr inline glm::uvec2 SoapTextureSize = glm::uvec2(80, 1440);

//size of the level meters/bars
static constexpr inline float BarHeight = 406.f;
static constexpr inline float BarWidth = 80.f;

//how much to shift the pitch by when time is low
static inline constexpr float PitchChangeA = 1.12f;
static inline constexpr float PitchChangeB = 1.19f;

namespace sc
{
    struct ShaderID final
    {
        enum
        {
            LevelMeter,
            Fire,
            Soap,

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

    struct CollisionID final
    {
        enum
        {
            Ball, Bucket,

            Count
        };
    };

    struct ParticleID final
    {
        enum
        {
            Bubbles, Splash,

            Count
        };
    };

    struct MessageID final
    {
        enum
        {
            ParticleMessage = cl::MessageID::Count * 200
        };
    };

    struct ParticleEvent final
    {
        std::int32_t particleID = 0;
        float soapLevel = 0.f;
        glm::vec3 position = glm::vec3(0.f);
    };

    static inline constexpr std::uint32_t SmallTextSize = 12;
    static inline constexpr std::uint32_t MediumTextSize = 24;
    static inline constexpr std::uint32_t LargeTextSize = 72;

    static inline const float TextDepth = 1.f;
    static inline const float UIBackgroundDepth = -1.f;
    static inline const float UIMeterDepth = -2.f;

    //hmm we need to make sure to set these with the scene scale too...
    static inline constexpr glm::vec2 SmallTextOffset = glm::vec2(1.f);
    static inline constexpr glm::vec2 MediumTextOffset = glm::vec2(2.f);
    static inline constexpr glm::vec2 LargeTextOffset = glm::vec2(6.f);
}