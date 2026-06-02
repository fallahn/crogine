/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2026
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

#include "GameConsts.hpp"
#include "CameraFollowSystem.hpp"

#include <crogine/core/String.hpp>
#include <crogine/core/Clock.hpp>

#include <crogine/ecs/Entity.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Transform.hpp>

#include <crogine/detail/glm/vec3.hpp>

//*sigh* multiple structs with the same name and different defs...
using WindCallbackData = std::pair<float, float>;

static constexpr cro::Time IdleTime = cro::seconds(30.f);
static constexpr glm::vec3 PlayerPosition(0.f, 2.5f, 121.f);
static constexpr glm::vec3 CameraPosition = PlayerPosition + glm::vec3(0.f, CameraStrokeHeight, CameraStrokeOffset);

static constexpr glm::vec2 BillboardChunk(40.f, 50.f);
static constexpr std::size_t ChunkCount = 5;


//callback data for anim/self destruction
//of messages / options window
struct PopupAnim final
{
    enum
    {
        Delay, Open, Hold, Close,
        Abort //used to remove open messages when forcefully restarting
    }state = Delay;
    float currentTime = 0.5f;
};

struct MiniTrailData final
{
    enum
    {
        Reset, Follow, Idle
    }state = Reset;

    float progress = 1.f;
    float height = 0.f; //normalised
};

struct FanData final
{
    std::int32_t dir = 1;
    float progress = 0.f;
};

struct FoliageCallback final
{
    FoliageCallback(float d = 0.f) : delay(d + 8.f) {} //magic number is some delay before effect starts
    float delay = 0.f;
    float progress = 0.f;
    static constexpr float Distance = 14.f;

    void operator() (cro::Entity e, float dt)
    {
        delay -= (dt * 1.6f);

        if (delay < 0)
        {
            progress = std::min(1.f, progress + dt);

            auto pos = e.getComponent<cro::Transform>().getPosition();
            pos.y = (cro::Util::Easing::easeInOutQuint(progress) - 1.f) * Distance;
            e.getComponent<cro::Transform>().setPosition(pos);

            if (progress == 1)
            {
                e.getComponent<cro::Callback>().active = false;
            }
        }
    }
};

struct FlagPosCallbackData final
{
    float progress = 1.f;
    enum
    {
        Out, In
    }state = Out;
    glm::vec3 startPos = glm::vec3(0.f);
    glm::vec3 targetPos = glm::vec3(0.f);
    static constexpr float MaxDepth = 3.f;
};

static inline float getMaxShadowDistance(std::int32_t camID, bool hq)
{
    if (hq)
    {
        switch (camID)
        {
        default: return 80.f;
        case CameraID::Player: return 40.f;
        case CameraID::Idle: return 40.f;
        case CameraID::Green: return 55.f; //50.f
        }
    }
    else
    {
        switch (camID)
        {
        default: return 80.f;
        case CameraID::Player: return 20.f;
        case CameraID::Idle: return 15.f;
        case CameraID::Green: return 15.f;
        }
    }
    return 80.f;
}

static inline std::uint32_t getShadowMapSize(std::int32_t q)
{
    switch (q)
    {
    default:
    case 0:
        return ShadowMapLowest;
    case 1:
        return ShadowMapLow;
    case 2:
    case 3:
        return ShadowMapHigh;
    }
}

static inline const std::array BannerStrings =
{
    cro::String("Missed Me!!"),
    cro::String("Buy Pentworth's\nIndispensible Lube"),
    cro::String("Also Available In Chartreuse"),
    cro::String("Honk if you love cilantro"),
    cro::String("Brilton & Stockley"),
    cro::String("Space For Rent"),
    cro::String("Strike it with a Dong"),
    cro::String("Dannis Always Chips In")
};