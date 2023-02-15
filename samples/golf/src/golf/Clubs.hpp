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

#include <crogine/util/Constants.hpp>

#include <string>
#include <array>

struct ClubID final
{
    enum
    {
        Driver, ThreeWood, FiveWood,
        FourIron, FiveIron, SixIron,
        SevenIron, EightIron, NineIron,
        PitchWedge, GapWedge, SandWedge,
        Putter,

        Count
    };
    
    static constexpr std::array<std::int32_t, ClubID::Count> Flags =
    {
        (1<<0), (1<<1), (1<<2),
        (1<<3), (1<<4), (1<<5),
        (1<<6), (1<<7), (1<<8),
        (1<<9), (1<<10), (1<<11)
    };

    static constexpr std::int32_t DefaultSet =
        Flags[Driver] | Flags[ThreeWood] | Flags[FiveIron] |
        Flags[EightIron] | Flags[PitchWedge] | Flags[GapWedge] |
        Flags[SandWedge] | Flags[Putter];

    static constexpr std::int32_t FullSet = 0x1FFF;
};

class Club final
{
public:

    Club(std::int32_t id, const std::string& name, float power, float angle, float target);

    std::string getName(bool imperial, float distanceToPin) const;

    float getPower(float distanceToPin) const;

    float getAngle() const { return m_angle; }

    float getTarget(float distanceToPin) const;

    float getBaseTarget() const { return m_target; }
private:
    const std::int32_t m_id = -1;
    std::string m_name; //displayed in UI
    float m_power = 0.f; //magnitude of impulse applied to reach target
    float m_angle = 0.f; //pitch of shot (should be positive)
    const float m_target = 0.f; //the max (approx) distance when hit with 100% power

    //putter below this is rescaled
    static constexpr float ShortRange = 1.f / 3.f;
    static constexpr float ShortRangeThreshold = ShortRange * 0.65f;
    static constexpr float TinyRange = 1.f / 10.f;
    static constexpr float TinyRangeThreshold = TinyRange * 0.5f;

    float getScaledValue(float v, float dist) const;
};

static const std::array<Club, ClubID::Count> Clubs =
{
    Club(ClubID::Driver, "Driver ", 44.f, 45.f, 220.f),
    Club(ClubID::ThreeWood, "3 Wood ", 39.5f, 45.f, 180.f),
    Club(ClubID::ThreeWood, "5 Wood ", 37.9f, 45.f, 150.f),//////
    
    
    
    Club(ClubID::FourIron, "4 Iron ", 36.f, 40.f, 160.f),//////
    Club(ClubID::FiveIron, "5 Iron ", 35.f, 40.f, 140.f),
    Club(ClubID::SixIron, "6 Iron ", 34.f, 40.f, 120.f),////////
    Club(ClubID::SevenIron, "7 Iron ", 32.8f, 40.f, 110.f),///////
    Club(ClubID::EightIron, "8 Iron ", 30.1f, 40.f, 100.f),   
    Club(ClubID::NineIron, "9 Iron ", 28.f, 40.f, 90.f),///////

    
    
    Club(ClubID::PitchWedge, "Pitch Wedge ", 25.2f, 52.f, 70.f),
    Club(ClubID::GapWedge, "Gap Wedge ", 17.4f, 60.f, 30.f),
    Club(ClubID::SandWedge, "Sand Wedge ", 10.3f, 60.f, 10.f),
    Club(ClubID::Putter, "Putter ", 9.11f, 0.f, 10.f)
};