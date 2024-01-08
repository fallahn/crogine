/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
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

#include <crogine/ecs/Entity.hpp>

#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>

//used for though/chat bubble animation
struct CogitationData final
{
    std::int32_t direction = 1;
    float currentTime = 0.f;
    std::int32_t spriteID = 0;
};

struct CogitationCallback final
{
    cro::Entity nameEnt;

    CogitationCallback(cro::Entity e) : nameEnt(e) {}

    void operator () (cro::Entity e, float dt)
    {
        auto end = nameEnt.getComponent<cro::Drawable2D>().getLocalBounds().width;
        e.getComponent<cro::Transform>().setPosition({ end + 6.f, 4.f });

        float scale = 0.f;
        auto& [direction, currTime, _] = e.getComponent<cro::Callback>().getUserData<CogitationData>();
        if (direction == 0)
        {
            //grow
            currTime = std::min(1.f, currTime + (dt * 3.f));
            scale = cro::Util::Easing::easeOutQuint(currTime);
        }
        else
        {
            //shrink
            currTime = std::max(0.f, currTime - (dt * 3.f));
            if (currTime == 0)
            {
                e.getComponent<cro::Callback>().active = false;
            }

            scale = cro::Util::Easing::easeInQuint(currTime);
        }

        e.getComponent<cro::Transform>().setScale({ scale, scale });
    }
};




//used to move the flag as the player approaches
struct FlagCallbackData final
{
    static constexpr float MaxHeight = 1.f;
    float targetHeight = 0.f;
    float currentHeight = 0.f;
};
using WindEffectData = std::pair<float, float>;

//bar to show the current wind effect on the ball
struct WindEffectCallback final
{
    void operator () (cro::Entity e, float dt)
    {
        const float Speed = dt / 4.f;
        auto& [target, current] = e.getComponent<cro::Callback>().getUserData<WindEffectData>();
        if (target < current)
        {
            current = std::max(target, current - Speed);
        }
        else
        {
            current = std::min(target, current + Speed);
        }
        cro::FloatRect crop(0.f, 0.f, 2.f, 32.f * current); //TODO constify this once we settle on size
        e.getComponent<cro::Drawable2D>().setCroppingArea(crop);

        if (current > 0)
        {
            auto colour = glm::mix(static_cast<glm::vec4>(TextGoldColour), static_cast<glm::vec4>(TextHighlightColour), current);
            auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
            for (auto& v : verts)
            {
                v.colour = colour;
            }
        }
    };
};


//used for drone targeting
struct DroneCallbackData final
{
    float rotation = 0.f;
    float acceleration = 0.f;
    cro::Entity target;
    glm::vec3 resetPosition = glm::vec3(160.f,10.f,-100.f);
};

//used for expanding the detection radius of the sky cam
struct CamCallbackData final
{
    float progress = 0.f;
};

//used by pop-up messages
struct MessageAnim final
{
    enum
    {
        Delay, Open, Hold, Close
    }state = Delay;
    float currentTime = 0.5f;
};

//used by fastforward/skip message
struct SkipCallbackData final
{
    float progress = 0.f;
    std::uint32_t direction = 1;
    std::uint32_t buttonIndex = 0; //sets the button animation to xbox or PS
};

//used by avatar preview animation
struct AvatarAnimCallbackData final
{
    std::int32_t direction = 0; //in/out
    float progress = 0.f;
};

struct AnimDirection final
{
    enum
    {
        Grow, Hold, Shrink, Destroy
    };
};
using WindHideData = AvatarAnimCallbackData;
using BullsEyeData = AvatarAnimCallbackData;