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

#include <crogine/util/Random.hpp>

#include <array>

struct CPUStat final
{
    enum
    {
        Skill, //0 - 2 for club range
        WindAccuracy, //rand values +/- this in lookup table
        PowerAccuracy,
        StrokeAccuracy,
        MistakeLikelyhood, //this in 10 chance of a mistake
        MistakeAccuracy, //+/- rand this in lookup table
        PerfectionLikelyhood, //this in 100 chance of possible perfection

        Count
    };
};

static constexpr std::array<std::array<std::int32_t, CPUStat::Count>, 28> CPUStats =
{
    {
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 0, 0},
    {2, 0, 0, 0, 0, 0, 0},
    {2, 0, 0, 0, 0, 0, 0},
    {2, 0, 0, 0, 0, 0, 0},
    {2, 0, 0, 0, 0, 0, 0},
    }
};

//when applying wind multiply its effect by this
static constexpr std::array<float, 11> WindOffsets =
{
    -0.5f, -0.4f, -0.3f, -0.2f, -0.1f, 0.01f, 0.1f , 0.2f, 0.3f, 0.4f, 0.5f
};

//when calculating power offset, multiply this by club power
static constexpr std::array<float, 7> PowerOffsets =
{
    -3.f, -2.f, -1.f, -0.04f, 1.f, 2.f, 3.f
};

static constexpr std::array<float, 9> AccuracyOffsets =
{
    -8.f, -4.f, -2.f, -1.f, 0.f, 1.f, 2.f, 4.f, 8.f
};

template <std::size_t S>
float getOffset(const std::array<float, S>& vals, std::int32_t accuracy)
{
    CRO_ASSERT(accuracy != 0, "");
    auto idx = (S / 2) + cro::Util::Random(-accuracy, accuracy);

    CRO_ASSERT(idx < S, "");
    return vals[idx];
}