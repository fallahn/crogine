/*-----------------------------------------------------------------------

Matt Marchant 2021
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include <cstdint>
#include <array>

struct ColourPair final
{
    std::uint32_t light = 0x000000FF;
    std::uint32_t dark = 0x000000FF;

    constexpr ColourPair(std::uint32_t l, std::uint32_t d)
        : light(l), dark(d) {}
};

namespace pc
{
    struct ColourID final
    {
        enum
        {
            White,
            Taupe,
            Grey,
            Blue,
            Green,
            Yellow,
            Red,
            Beige,
            Tan,
            Brown,
            Olive,

            Count
        };
    };

    static constexpr std::array<ColourPair, ColourID::Count> Palette =
    {
        ColourPair(0xfff8e1ff, 0xc8b89fff), //white
        ColourPair(0x987a68ff, 0x674949ff), //taupe
        ColourPair(0xadb9b8ff, 0x6b6f72ff), //grey
        ColourPair(0x6eb39dff, 0x30555bff), //blue
        ColourPair(0x6ebe70ff, 0x467e3eff), //green
        ColourPair(0xf2cf5cff, 0xec773dff), //yellow
        ColourPair(0xc85287ff, 0xb83530ff), //red
        ColourPair(0xdbaf77ff, 0xb77854ff), //beige
        ColourPair(0xb77854ff, 0x833e35ff), //tan
        ColourPair(0x833e35ff, 0x50285fff), //brown
        ColourPair(0x7e6d37ff, 0x65432fff)  //olive
    };

    struct ColourKey final
    {
        enum
        {
            Bottom, Top,
            Skin, Hair,

            Count
        };
    };

    static constexpr std::array<ColourPair, ColourKey::Count> Keys =
    {
        ColourPair(0x0a0a0aff, 0x000000ff),
        ColourPair(0x282828ff, 0x1e1e1eff),
        ColourPair(0x464646ff, 0x3c3c3cff),
        ColourPair(0x646464ff, 0x5a5a5aff)
    };
}
