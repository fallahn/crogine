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
    std::array<std::int32_t, AnimationID::Count> animationIDs = {};
    cro::Entity ballModel;

    //hackery to allow different attachment offsets for different animations
    static constexpr glm::vec3 DriveOffset = glm::vec3(0.f, -1.f, 1.f);
    //static constexpr glm::vec3 ChipOffset = glm::vec3(-1.f, -7.f, 3.f); //V1
    static constexpr glm::vec3 ChipOffset = glm::vec3(0.f, 0.f, 1.f);

    static constexpr glm::quat DriveRotation = glm::quat(0.0628276f, 0.952123f, -0.284735f, -0.0918745f);
    //static constexpr glm::quat ChipRotation = glm::quat(0.154042f, 0.839663f, -0.266642f, -0.447369f); //V1
    static constexpr glm::quat ChipRotation = glm::quat(-0.0210049f, 0.907963f, -0.0598632f, -0.414221f); //V2

    //glm::quat ChipRotation;

    std::uint32_t clubModelID = 0;
    bool flipped = false;

    //Avatar()
    //{
    //    ChipRotation = cro::Util::Vector::eulerToQuat(glm::vec3(179.f, 49.f, -8.f) * cro::Util::Const::degToRad);
    //    LogI << ChipRotation << std::endl;
    //}

    float progress = 0.f;
    float direction = -1.f;
    static constexpr float toStep = 1.f / 8.25f; //hacky way to do this but the logical way is... bumpy
    static constexpr float fromStep = 1.f / 3.f;

    void applyAttachment() 
    {
        if (hands)
        {
            if (model.getComponent<cro::Skeleton>().getState() == cro::Skeleton::Playing)
            {
                const auto animID = model.getComponent<cro::Skeleton>().getCurrentAnimation();
                //if (animID == animationIDs[AnimationID::ChipIdle])
                //{
                //    /*hands->setPosition(ChipOffset);
                //    hands->setRotation(ChipRotation);*/

                //    progress = std::clamp(progress + (toStep * direction), 0.f, 1.f);
                //    hands->setPosition(glm::mix(DriveOffset, ChipOffset, progress));
                //    hands->setRotation(glm::slerp(DriveRotation, ChipRotation, progress));
                //}
                //else 
                if (animID == animationIDs[AnimationID::ToChip]
                    || animID == animationIDs[AnimationID::ChipIdle])
                {
                    /*const auto p = model.getComponent<cro::Skeleton>().getAnimationProgress();
                    hands->setPosition(glm::mix(DriveOffset, ChipOffset, p));
                    hands->setRotation(glm::slerp(DriveRotation, ChipRotation, p));*/
                    direction = 1.f;

                    progress = std::clamp(progress + (toStep * direction), 0.f, 1.f);
                    hands->setPosition(glm::mix(DriveOffset, ChipOffset, progress));
                    hands->setRotation(glm::slerp(DriveRotation, ChipRotation, progress));
                }
                else if (animID == animationIDs[AnimationID::FromChip]
                    || animID == animationIDs[AnimationID::Idle])
                {
                    /*const auto p = model.getComponent<cro::Skeleton>().getAnimationProgress();
                    hands->setPosition(glm::mix(ChipOffset, DriveOffset, p));
                    hands->setRotation(glm::slerp(ChipRotation, DriveRotation, p));*/
                    direction = -1.f;

                    progress = std::clamp(progress + (fromStep * direction), 0.f, 1.f);
                    hands->setPosition(glm::mix(DriveOffset, ChipOffset, progress));
                    hands->setRotation(glm::slerp(DriveRotation, ChipRotation, progress));

                    /*progress = 0.f;
                    hands->setPosition(DriveOffset);
                    hands->setRotation(DriveRotation);*/
                }
                //else if (animID == animationIDs[AnimationID::Idle])
                //{
                //    /*hands->setPosition(DriveOffset);
                //    hands->setRotation(DriveRotation);*/

                //    progress = std::clamp(progress + (fromStep * direction), 0.f, 1.f);
                //    hands->setPosition(glm::mix(DriveOffset, ChipOffset, progress));
                //    hands->setRotation(glm::slerp(DriveRotation, ChipRotation, progress));
                //}
            }
        }
    }
};
