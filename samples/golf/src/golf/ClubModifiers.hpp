/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2024
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

#include "Clubs.hpp"

struct ShotModifier final
{
    float angle = 0.f;
    float powerMultiplier = 1.f;
    float targetMultiplier = 1.f;
    ShotModifier() = default;
    constexpr ShotModifier(float a, float p, float t)
        : angle(a), powerMultiplier(p), targetMultiplier(t){}
};

struct ModifierGroup final
{
    static constexpr std::array<ShotModifier, ClubID::Count> DefaultModifier = {}; //leaves the default shot
    std::array<ShotModifier, ClubID::Count> punchModifier = {};
    std::array<ShotModifier, ClubID::Count> flopModifier = {};

    const std::array<ShotModifier, ClubID::Count>& operator [](std::size_t i) const
    {
        switch (i)
        {
        default:assert(false); break;
        case 0: return DefaultModifier;
        case 1: return punchModifier;
        case 2: return flopModifier;
        }
        return DefaultModifier;
    }

    ModifierGroup() = default;
    constexpr ModifierGroup(const std::array<ShotModifier, ClubID::Count>& p, const std::array<ShotModifier, ClubID::Count>& f)
        : punchModifier(p), flopModifier(f) {}
};

static constexpr std::array<ShotModifier, ClubID::Count> pA =
{
    ShotModifier(-0.287f, 1.405f, 1.001f),
    ShotModifier(-0.259f, 1.286f, 1.061f),
    ShotModifier(-0.297f, 1.307f, 1.068f),
    ShotModifier(-0.358f, 1.058f, 0.660f),
    ShotModifier(-0.351f, 1.061f, 0.660f),
    ShotModifier(-0.343f, 1.061f, 0.666f),
    ShotModifier(-0.282f, 0.987f, 0.660f),
    ShotModifier(-0.290f, 0.993f, 0.660f),
    ShotModifier(-0.290f, 1.007f, 0.667f),
    ShotModifier(-0.503f, 1.129f, 1.143f),
    ShotModifier(-0.686f, 1.217f, 1.333f),
    ShotModifier(-0.686f, 1.306f, 1.500f),
    ShotModifier(0.000f, 1.000f, 1.000f),
};

static constexpr std::array<ShotModifier, ClubID::Count> fA =
{
    ShotModifier(0.058f, 0.920f, 0.910f),
    ShotModifier(0.061f, 0.925f, 0.891f),
    ShotModifier(0.053f, 0.911f, 0.871f),
    ShotModifier(0.190f, 0.877f, 0.789f),
    ShotModifier(0.245f, 0.878f, 0.776f),
    ShotModifier(0.282f, 0.864f, 0.755f),
    ShotModifier(0.328f, 0.857f, 0.725f),
    ShotModifier(0.343f, 0.850f, 0.701f),
    ShotModifier(0.373f, 0.837f, 0.674f),
    ShotModifier(0.298f, 0.871f, 0.429f),
    ShotModifier(0.252f, 0.966f, 0.504f),
    ShotModifier(0.313f, 1.000f, 0.511f),
    ShotModifier(0.000f, 1.000f, 1.000f),
};

static constexpr std::array<ShotModifier, ClubID::Count> pB =
{
    ShotModifier(-0.245f, 1.299f, 1.000f),
    ShotModifier(-0.229f, 1.238f, 1.054f),
    ShotModifier(-0.305f, 1.313f, 1.068f),
    ShotModifier(-0.267f, 0.953f, 0.667f),
    ShotModifier(-0.305f, 0.993f, 0.660f),
    ShotModifier(-0.275f, 0.972f, 0.660f),
    ShotModifier(-0.328f, 1.048f, 0.667f),
    ShotModifier(-0.290f, 1.000f, 0.660f),
    ShotModifier(-0.305f, 1.027f, 0.660f),
    ShotModifier(-0.412f, 1.054f, 1.136f),
    ShotModifier(-0.496f, 1.075f, 1.313f),
    ShotModifier(-0.686f, 1.306f, 1.500f),
    ShotModifier(0.000f, 1.000f, 1.000f),
};

static constexpr std::array<ShotModifier, ClubID::Count> fB =
{
    ShotModifier(0.030f, 0.939f, 0.918f),
    ShotModifier(0.076f, 0.926f, 0.905f),
    ShotModifier(0.100f, 0.905f, 0.877f),
    ShotModifier(0.160f, 0.885f, 0.803f),
    ShotModifier(0.183f, 0.871f, 0.789f),
    ShotModifier(0.206f, 0.864f, 0.776f),
    ShotModifier(0.229f, 0.850f, 0.752f),
    ShotModifier(0.274f, 0.850f, 0.728f),
    ShotModifier(0.328f, 0.844f, 0.708f),
    ShotModifier(0.274f, 0.864f, 0.470f),
    ShotModifier(0.236f, 0.933f, 0.504f),
    ShotModifier(0.313f, 1.000f, 0.511f),
    ShotModifier(0.000f, 1.000f, 1.000f),
};

static constexpr std::array<ShotModifier, ClubID::Count> pC =
{
    ShotModifier(-0.236f, 1.279f, 1.000f),
    ShotModifier(-0.274f, 1.306f, 1.041f),
    ShotModifier(-0.274f, 1.265f, 1.061f),
    ShotModifier(-0.298f, 0.986f, 0.667f),
    ShotModifier(-0.351f, 1.068f, 0.667f),
    ShotModifier(-0.343f, 1.061f, 0.667f),
    ShotModifier(-0.328f, 1.047f, 0.660f),
    ShotModifier(-0.336f, 1.061f, 0.667f),
    ShotModifier(-0.289f, 1.000f, 0.660f),
    ShotModifier(-0.419f, 1.061f, 1.136f),
    ShotModifier(-0.511f, 1.061f, 1.285f),
    ShotModifier(-0.686f, 1.306f, 1.500f),
    ShotModifier(0.000f, 1.000f, 1.000f),
};

static constexpr std::array<ShotModifier, ClubID::Count> fC =
{
    ShotModifier(0.054f, 0.932f, 0.925f),
    ShotModifier(0.061f, 0.932f, 0.912f),
    ShotModifier(0.069f, 0.925f, 0.891f),
    ShotModifier(0.137f, 0.885f, 0.817f),
    ShotModifier(0.167f, 0.878f, 0.803f),
    ShotModifier(0.213f, 0.871f, 0.789f),
    ShotModifier(0.244f, 0.864f, 0.776f),
    ShotModifier(0.251f, 0.857f, 0.755f),
    ShotModifier(0.290f, 0.843f, 0.728f),
    ShotModifier(0.275f, 0.898f, 0.504f),
    ShotModifier(0.244f, 0.939f, 0.490f),
    ShotModifier(0.313f, 1.000f, 0.511f),
    ShotModifier(0.000f, 1.000f, 1.000f),
};

static constexpr std::array<ModifierGroup, 3u> LevelModifiers =
{
    ModifierGroup(pA, fA),
    ModifierGroup(pB, fB),
    ModifierGroup(pC, fC)
};