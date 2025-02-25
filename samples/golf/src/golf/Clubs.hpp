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

#include <Social.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Random.hpp>

#include <string>
#include <array>
#include <cstdint>

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
        (1<<Driver),     (1<<ThreeWood), (1<<FiveWood),
        (1<<FourIron),   (1<<FiveIron),  (1<<SixIron),
        (1<<SevenIron),  (1<<EightIron), (1<<NineIron),
        (1<<PitchWedge), (1<<GapWedge),  (1<<SandWedge),
        (1<<Putter)
    };

    static constexpr std::array<std::int32_t, 5u> LockedSet =
    {
        FiveWood, FourIron, SixIron, SevenIron, NineIron
    };

    static inline std::int32_t getUnlockLevel(std::int32_t id)
    {
        for (auto i = 0u; i < LockedSet.size(); ++i)
        {
            if (LockedSet[i] == id)
            {
                return (i + 1) * Social::ClubStepLevel;
            }
        }

        return 0;
    }

    static constexpr std::int32_t DefaultSet =
        Flags[Driver]    | Flags[ThreeWood]  | Flags[FiveIron] |
        Flags[EightIron] | Flags[PitchWedge] | Flags[GapWedge] |
        Flags[SandWedge] | Flags[Putter];

    static constexpr std::int32_t FullSet = 0x1FFF;

    static inline std::int32_t getRandomSet()
    {
        std::int32_t retVal = /*Flags[Driver] | */Flags[Putter];
        retVal |= (1 << cro::Util::Random::value(ThreeWood, FiveWood));
        retVal |= (1 << cro::Util::Random::value(FourIron, NineIron));
        retVal |= (1 << cro::Util::Random::value(PitchWedge, SandWedge));

        return retVal;
    }
};

class Club final
{
public:

    Club(std::int32_t id, const std::string& name, float angle, float sidespin, float topspin);

    std::string getName(bool imperial, float distanceToPin) const;

    std::string getLabel() const { return m_name; } //doesn't include distance

    std::string getDistanceLabel(bool imperial, std::int32_t level) const;

    float getPower(float distanceToPin, bool imperial) const;

    float getAngle() const;

    //note distanceToPin only affects the putter so this can be 0
    //for any of the other clubs
    float getTarget(float distanceToPin) const;

    float getBaseTarget() const;
    float getDefaultTarget() const; //target with no level-shift applied
    float getTargetAtLevel(std::int32_t level) const;

    float getSideSpinMultiplier() const;
    float getTopSpinMultiplier() const;
    float getSpinInfluence() const { return (m_topspin + m_sidespin) / 2.f; } //an average just used for printing stats

    static std::int32_t getClubLevel(); //0-2 for range
    static void setClubLevel(std::int32_t);

    static std::int32_t getModifierIndex(); //default, punch, flop
    static void setModifierIndex(std::int32_t);

    static void setFixedPuttingDistance(bool b) { m_fixedPuttingDistance = b; }

private:
    const std::int32_t m_id = -1;
    std::string m_name; //displayed in UI
    float m_angle = 0.f; //pitch of shot (should be positive) - not a clubstat as it remains constant with range
    float m_sidespin = 1.f; //multiplier 0-1
    float m_topspin = 1.f; //multiplier 0-1

    //hack to fix the putter distance at 10m if set in the options
    static bool m_fixedPuttingDistance;

    //putter below this is rescaled
    static constexpr float ShortRange = 1.f / 3.f;
    static constexpr float ShortRangeThreshold = ShortRange * 0.8f;
    static constexpr float TinyRange = 1.f / 10.f;
    static constexpr float TinyRangeThreshold = TinyRange * 0.5f;

    float getScaledValue(float v, float dist) const;
};

struct ShotType final
{
    enum
    {
        Regular = (1<<0),
        Punch   = (1<<1),
        Flop    = (1<<2),
    };
};

static constexpr std::array<std::int32_t, ClubID::Count> ClubShot =
{
    ShotType::Regular, //driver
    ShotType::Regular, //wood
    ShotType::Regular,
    ShotType::Regular | ShotType::Punch, //iron
    ShotType::Regular | ShotType::Punch,
    ShotType::Regular | ShotType::Punch,
    ShotType::Regular | ShotType::Punch,
    ShotType::Regular | ShotType::Punch,
    ShotType::Regular | ShotType::Punch | ShotType::Flop,
    ShotType::Regular | ShotType::Flop, //wedge
    ShotType::Regular | ShotType::Flop,
    ShotType::Regular | ShotType::Flop,
    ShotType::Regular, //putter
};

//static inline const std::array<Club, ClubID::Count> Clubs =
//{
//    Club(ClubID::Driver,    "Driver ", 45.f, 0.3f,   0.5f),  //default set
//    Club(ClubID::ThreeWood, "3 Wood ", 45.f, 0.35f,  0.55f), //default set
//    Club(ClubID::FiveWood,  "5 Wood ", 45.f, 0.45f,  0.55f), //Level 5
//    
//    
//    Club(ClubID::FourIron,  "4 Iron ", 40.f, 0.45f, 0.78f), //Level 10
//    Club(ClubID::FiveIron,  "5 Iron ", 40.f, 0.5f,  0.78f), //default set
//    Club(ClubID::SixIron,   "6 Iron ", 40.f, 0.55f, 0.8f),  //Level 15
//    Club(ClubID::SevenIron, "7 Iron ", 40.f, 0.6f,  0.8f),  //Level 20
//    Club(ClubID::EightIron, "8 Iron ", 40.f, 0.75f, 0.85f), //default set
//    Club(ClubID::NineIron,  "9 Iron ", 40.f, 0.8f,  0.85f),  //Level 25
//
//    
//    Club(ClubID::PitchWedge, "Pitch Wedge ", 52.f, 0.05f, 0.9f),  //default set
//    Club(ClubID::GapWedge,   "Gap Wedge ",   60.f, 0.05f, 0.93f), //default set
//    Club(ClubID::SandWedge,  "Sand Wedge ",  60.f, 0.05f, 0.95f), //default set
//    Club(ClubID::Putter,     "Putter ",      0.f,  0.f,   0.f)    //default set
//};

//remember if using these to also set the club stats in Clubs.cpp
static inline const std::array<Club, ClubID::Count> Clubs =
{
    Club(ClubID::Driver,    "Driver ", 28.992f, 0.300f, 0.500f),
    Club(ClubID::ThreeWood, "3 Wood ", 32.315f, 0.350f, 0.550f),
    Club(ClubID::FiveWood,  "5 Wood ", 34.721f, 0.450f, 0.550f),
    Club(ClubID::FourIron,  "4 Iron ", 37.586f, 0.450f, 0.780f),
    Club(ClubID::FiveIron,  "5 Iron ", 37.128f, 0.500f, 0.780f),
    Club(ClubID::SixIron,   "6 Iron ", 36.326f, 0.550f, 0.800f),
    Club(ClubID::SevenIron, "7 Iron ", 35.924f, 0.600f, 0.800f),
    Club(ClubID::EightIron, "8 Iron ", 35.924f, 0.750f, 0.850f),
    Club(ClubID::NineIron,  "9 Iron ", 35.523f, 0.800f, 0.850f),
    Club(ClubID::PitchWedge, "Pitch Wedge ", 56.895f, 0.050f, 0.900f),
    Club(ClubID::GapWedge,   "Gap Wedge ",   62.452f, 0.050f, 0.930f),
    Club(ClubID::SandWedge,  "Sand Wedge ",  60.000f, 0.050f, 0.950f),
    Club(ClubID::Putter,     "Putter ",      0.000f, 0.000f, 0.000f),
};