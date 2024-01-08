/*-----------------------------------------------------------------------

Matt Marchant 2023
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

#include <array>
#include <string>

struct XPStringID final
{
    enum
    {
        NiceOn,
        NearMiss,
        GreatAccuracy,
        OnTheFairway,
        DroneHit,
        CourseComplete,
        LongPutt,
        TopChip,
        NiceChip,
        BackSpinSkill,
        TopSpinSkill,
        FirstPlace,
        SecondPlace,
        ThirdPlace,

        HIO,
        Albatross,
        AlbatrossAssist,
        Eagle,
        EagleAssist,
        Birdie,
        BirdieAssist,
        Par,
        ParAssist,

        ChallengeComplete,
        BullsEyeHit,
        Survivor,

        Count
    };
};

//note that final message appends a space and the XP value.
static inline const std::array<std::string, XPStringID::Count> XPStrings =
{
    std::string("Nice On!"),
    "Near Miss",
    "Great Accuracy",
    "Fairway",
    "Drone Hit!",
    "Course Complete",
    "Long Putt",
    "Top Chip!",
    "So Close!",
    "Backspin Skill!",
    "Topspin Skill!",
    "First Place!",
    "Second Place",
    "Third Place",

    "Hole In One!!",
    "Albatross (No Assist)",
    "Albatross!",
    "Eagle (No Assist)",
    "Eagle!",
    "Birdie (No Assist)",
    "Birdie",
    "Par (No Assist)",
    "Par",

    "Challenge Completed",
    "Target Hit",
    "Survivor"
};