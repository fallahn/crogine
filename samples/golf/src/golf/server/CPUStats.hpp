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
        WindAccuracy, //rand values +/- this in lookup table max 5
        PowerAccuracy, //max 3
        StrokeAccuracy, //max 4
        MistakeLikelyhood, //this in 10 chance of a mistake
        MistakeAccuracy, //+/- rand this in lookup table max 3 (could be power, stroke or both)
        PerfectionLikelyhood, //this in 100 chance of possible perfection

        Count
    };
};

static constexpr std::array<std::array<std::int32_t, CPUStat::Count>, 28> CPUStats =
{
    {
    {2, 1, 1, 2, 2, 3, 82},
    {2, 2, 2, 1, 3, 2, 76},
    {2, 3, 1, 2, 4, 2, 64},
    {2, 4, 3, 3, 3, 3, 70},

    {1, 2, 3, 2, 2, 2, 71},
    {1, 1, 2, 1, 2, 2, 46},
    {1, 2, 1, 2, 2, 2, 63},
    {1, 3, 2, 2, 3, 3, 52},
    {1, 2, 2, 3, 5, 3, 50},
    {1, 3, 1, 3, 4, 3, 67},
    {1, 4, 3, 3, 5, 2, 41},
    {1, 3, 3, 4, 6, 3, 39},

    {0, 3, 2, 1, 3, 2, 40},
    {0, 2, 1, 4, 6, 2, 26},
    {0, 2, 2, 2, 3, 2, 29},
    {0, 1, 3, 5, 2, 2, 32},
    {0, 2, 1, 2, 2, 3, 44},
    {0, 2, 1, 2, 2, 2, 27},
    {0, 3, 2, 1, 3, 2, 30},
    {0, 3, 2, 2, 3, 3, 35},
    {0, 2, 1, 3, 4, 3, 23},
    {0, 3, 2, 2, 5, 3, 20},
    {0, 5, 2, 3, 7, 3, 19},
    {0, 4, 3, 3, 6, 2, 12},
    {0, 4, 1, 4, 6, 3, 24},
    {0, 5, 3, 3, 6, 3, 16},
    {0, 4, 3, 4, 7, 3, 13},
    {0, 5, 3, 4, 8, 3, 9},
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