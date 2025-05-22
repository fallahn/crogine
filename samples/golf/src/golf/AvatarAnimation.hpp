/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
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

#include <crogine/ecs/components/Skeleton.hpp>

struct AnimationID final
{
    enum
    {
        Idle, Swing, ToChip, Chip, ChipIdle, FromChip, Putt,
        Celebrate, Disappoint,
        Impatient, IdleStand,
        Count,

    };
    static constexpr std::size_t Invalid = std::numeric_limits<std::size_t>::max();
};

struct Avatar final
{
    cro::Entity model;
    cro::Attachment* hands = nullptr;
    std::array<std::size_t, AnimationID::Count> animationIDs = {};
    cro::Entity ballModel;

    //hackery to allow different attachment offsets for different animations
    static constexpr glm::vec3 DriveOffset = glm::vec3(0.f, -1.f, 1.f);
    static constexpr glm::vec3 ChipOffset = glm::vec3(-1.f, -7.f, 3.f);

    static constexpr glm::quat DriveRotation = glm::quat(0.0628276f, 0.952123f, -0.284735f, -0.0918745f);
    static constexpr glm::quat ChipRotation = glm::quat(0.154042f, 0.839663f, -0.266642f, -0.447369f);

    std::uint32_t clubModelID = 0;
    bool flipped = false;

    //Avatar()
    //{

    //}

    void applyAttachment() 
    {
        if (hands)
        {
            const auto getProgress =
                [&](std::int32_t animID) 
                {
                    const auto& skel = model.getComponent<cro::Skeleton>();
                    const auto& anim = skel.getAnimations()[animID];
                    const float progress = static_cast<float>(anim.currentFrame - anim.startFrame) / anim.frameCount;
                    const float subFrame = 1.f / anim.frameCount;

                    return (subFrame * skel.getCurrentFrameTime()) + progress;
                };

            if (model.getComponent<cro::Skeleton>().getState() == cro::Skeleton::Playing)
            {
                const auto animID = model.getComponent<cro::Skeleton>().getCurrentAnimation();
                if (animID == animationIDs[AnimationID::Chip]
                    || animID == animationIDs[AnimationID::ChipIdle])
                {
                    hands->setPosition(ChipOffset);
                    hands->setRotation(ChipRotation);
                }
                else if (animID == animationIDs[AnimationID::ToChip])
                {
                    const auto p = getProgress(animID);
                    hands->setPosition(glm::mix(DriveOffset, ChipOffset, p));
                    hands->setRotation(glm::lerp(DriveRotation, ChipRotation, p));
                }
                else if (animID == animationIDs[AnimationID::FromChip])
                {
                    const auto p = getProgress(animID);
                    hands->setPosition(glm::mix(ChipOffset, DriveOffset, p));
                    hands->setRotation(glm::lerp(ChipRotation, DriveRotation, p));
                }
                else
                {
                    hands->setPosition(DriveOffset);
                    hands->setRotation(DriveRotation);
                }
            }
        }
    }
};
