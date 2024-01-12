#pragma once

#include <crogine/graphics/Colour.hpp>

#include <array>

namespace CD32
{
    static inline constexpr std::array<cro::Colour, 32u> Colours =
    {
        cro::Colour(0xd0b0dff),
        cro::Colour(0xfff8e1ff),
        cro::Colour(0xc8b89fff),
        cro::Colour(0x987a68ff),
        cro::Colour(0x674949ff),
        cro::Colour(0x3a3941ff),
        cro::Colour(0x6b6f72ff),
        cro::Colour(0xadb9b8ff),
        cro::Colour(0xadd9b7ff),
        cro::Colour(0x6eb39dff),
        cro::Colour(0x30555bff),
        cro::Colour(0x1a1e2dff),
        cro::Colour(0x284e43ff),
        cro::Colour(0x467e3eff),
        cro::Colour(0x93ab52ff),
        cro::Colour(0xf2cf5cff),
        cro::Colour(0xec773dff),
        cro::Colour(0xb83530ff),
        cro::Colour(0x722030ff),
        cro::Colour(0x281721ff),
        cro::Colour(0x6d2944ff),
        cro::Colour(0xc85257ff),
        cro::Colour(0xec9983ff),
        cro::Colour(0xdbaf77ff),
        cro::Colour(0xb77854ff),
        cro::Colour(0x833e35ff),
        cro::Colour(0x50282fff),
        cro::Colour(0x65432fff),
        cro::Colour(0x7e6d37ff),
        cro::Colour(0x6ebe70ff),
        cro::Colour(0xb75834ff),
        cro::Colour(0xd55c4dff),
    };

    enum
    {
        Black, BeigeLight, BeigeMid, BeigeDark, BeigeDarkest,
        GreyDark, GreyMid, GreyLight, BlueLight, BlueMid, BlueDark,
        BlueDarkest, GreenDark, GreenMid, GreenLight, Yellow, Orange,
        Red, RedDark, MauveDark, Mauve, Pink, PinkLight, TanLight,
        TanMid, TanDark, TanDarkest, Brown, Olive, Cyan, OrangeDirt,
        PinkDirt,

        Count
    };
}