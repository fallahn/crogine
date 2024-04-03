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

#pragma once

#include <crogine/graphics/Colour.hpp>

#include <cstdint>
#include <array>

namespace pc
{
    static inline constexpr std::array<std::array<std::uint32_t, 2u>, 12u> KeyMap =
    {
        std::array<std::uint32_t, 2u>({8u, 7u}),
        std::array<std::uint32_t, 2u>({6u, 5u}),
        std::array<std::uint32_t, 2u>({20u,21u}),
        std::array<std::uint32_t, 2u>({0u, 1u}),
        std::array<std::uint32_t, 2u>({1u, 2u}),
        std::array<std::uint32_t, 2u>({2u, 3u}),
        std::array<std::uint32_t, 2u>({9u, 4u}),
        std::array<std::uint32_t, 2u>({11u,12u}),
        std::array<std::uint32_t, 2u>({17u,15u}),
        std::array<std::uint32_t, 2u>({38u,39u}),
        std::array<std::uint32_t, 2u>({35u,34u}),
        std::array<std::uint32_t, 2u>({23u,19u})
    };

    static inline constexpr std::array<cro::Colour, 40u> Palette =
    {
        cro::Colour(0xdbaf77ff),
        cro::Colour(0xb77854ff),
        cro::Colour(0x833e35ff),
        cro::Colour(0x50282fff),
        cro::Colour(0x65432fff),
        cro::Colour(0x674949ff),
        cro::Colour(0x987a68ff),
        cro::Colour(0xc8b89fff),
        cro::Colour(0xfff8e1ff),
        cro::Colour(0x7e6d37ff),
        cro::Colour(0xadd9b7ff),
        cro::Colour(0x6eb39dff),
        cro::Colour(0x30555bff),
        cro::Colour(0x1a1e2dff),
        cro::Colour(0x284e43ff),
        cro::Colour(0x467e3eff),
        cro::Colour(0x93ab52ff),
        cro::Colour(0x6ebe70ff),
        cro::Colour(0x207262ff),
        cro::Colour(0xd0b0dff),
        cro::Colour(0xadb9b8ff),
        cro::Colour(0x6b6f72ff),
        cro::Colour(0x3a3941ff),
        cro::Colour(0x281721ff),
        cro::Colour(0x6a52abff),
        cro::Colour(0x5c7ff2ff),
        cro::Colour(0x3db1ecff),
        cro::Colour(0x4dc6d5ff),
        cro::Colour(0x30b3b8ff),
        cro::Colour(0x3493b7ff),
        cro::Colour(0xbe6ebcff),
        cro::Colour(0x763e7eff),
        cro::Colour(0x6d2944ff),
        cro::Colour(0x722030ff),
        cro::Colour(0xb83530ff),
        cro::Colour(0xc85257ff),
        cro::Colour(0xd55c4dff),
        cro::Colour(0xec9983ff),
        cro::Colour(0xf2cf5cff),
        cro::Colour(0xec773dff)
    };

    struct ColourKey final
    {
        enum Index
        {
            Hair, Skin,
            TopLight, TopDark,
            BottomLight, BottomDark,

            Count
        };
    };

    static inline const std::array<std::size_t, ColourKey::Count> PairCounts =
    {
        Palette.size(),
        16,
        Palette.size(),
        Palette.size(),
        Palette.size(),
        Palette.size()
    };

    static constexpr std::array<std::uint32_t, ColourKey::Count> Keys =
    {
        (0x64), //hair
        (0x46), //skin

        (0x28), //top light
        (0x1e), //top dark

        (0x0a), //bottom light
        (0x00), //bottom dark
    };
}
