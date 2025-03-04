/*-----------------------------------------------------------------------

Matt Marchant 2025
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

#include <cstdint>
#include <array>
#include <string>

namespace inv
{
    enum ItemType
    {
        Driver, FiveW, ThreeW, NineI, EightI, SevenI,
        SixI, FiveI, FourI, PitchWedge, GapWedge,
        SandWedge, Ball,

        Count
    };

    struct Item final
    {
        const std::int32_t type = ItemType::Ball;

        const std::int32_t stat01 = 0; //+/- 10 with 0 as default
        const std::int32_t stat02 = 0;
        const std::int32_t price = 0;

        const std::int32_t manufacturer = 12; //indexes into the names array

        Item() = default;
        Item(ItemType t, std::int32_t s1, std::int32_t s2, std::int32_t p, std::int32_t n)
            : type(t), stat01(s1), stat02(s2), price(p), manufacturer(n) { };
    };

    struct Loadout final
    {
        static constexpr std::uint32_t MaxCharBytes = 32;

        std::array<std::int32_t, ItemType::Count> items = {};
        std::array<std::uint8_t, MaxCharBytes> name = {};

        Loadout()
        {
            std::fill(items.begin(), items.end(), -1);
            std::fill(name.begin(), name.end(), 0);
        }
    };

    //the total item count available in the shop
    static constexpr std::uint32_t MaxItems = 89;
    struct Inventory final
    {
        static constexpr std::uint32_t MaxLoadouts = 6;
        std::uint32_t loadoutCount = 2; //number of unlocked loadouts

        std::array<Loadout, MaxLoadouts> loadouts = {};
        std::array<std::int32_t, MaxItems> inventory = {}; //indices of shop-bought items

        std::int32_t balance = 5000;

        Inventory()
        {
            std::fill(inventory.begin(), inventory.end(), -1);
        }

        bool read() {}
        bool write() {}
    };

    static inline const std::array<std::string, 13u> Manufacturers =
    {
        "Gallawent", "Dong", "FellowCraft", "Akrun", "Dannis", "Clix",
        "BeyTree", "Tunnelrock", "Flaxen", "Headings", "Woodgear",
        "Brilton & Stockley",

        "Default"
    };

    //total items available in the shop
    static inline const std::array<Item, MaxItems> Items =
    {
        //type, stat 1, stat 2, price, manufacturer ID
        
        //driver-wood stat 1 accuracy
        //stat 2 distance
        Item(ItemType::Driver, -4, 4,2300, 0),
        Item(ItemType::Driver, -2, 3,2400, 1),
        Item(ItemType::Driver, -1, 2,2500, 2),
        Item(ItemType::Driver,  1, 0,2550, 3),
        Item(ItemType::Driver,  3,-1,2600, 4),
        Item(ItemType::Driver,  5,-3,2750, 5),
        Item(ItemType::Driver,  8,-6,2800, 6),

        Item(ItemType::ThreeW, -2, 3,2050, 0),
        Item(ItemType::ThreeW, -3, 4,2140, 1),
        Item(ItemType::ThreeW, -1, 2,2200, 2),
        Item(ItemType::ThreeW,  2,-1,2350, 3),
        Item(ItemType::ThreeW,  4,-3,2540, 4),
        Item(ItemType::ThreeW,  3,-4,2600, 5),
        Item(ItemType::ThreeW,  7,-6,2650, 6),

        Item(ItemType::FiveW, -3, 3,2000, 0),
        Item(ItemType::FiveW, -2, 3,2100, 1),
        Item(ItemType::FiveW,  0, 1,2250, 2),
        Item(ItemType::FiveW,  2,-1,2280, 3),
        Item(ItemType::FiveW,  3,-1,2400, 4),
        Item(ItemType::FiveW,  5,-2,2550, 5),
        Item(ItemType::FiveW,  4,-1,2700, 6),
        
        //irons stat1 spin effect - pos more spin,
        //stat2 increased punch distance
        Item(ItemType::NineI, -2, 6,720, 0),
        Item(ItemType::NineI, -2, 5,700, 1),
        Item(ItemType::NineI, -1, 3,810, 2),
        Item(ItemType::NineI,  0, 1,780, 3),
        Item(ItemType::NineI,  1,-1,850, 4),
        Item(ItemType::NineI,  2,-3,990, 5),
        Item(ItemType::NineI,  3,-4,920, 6),
        
        Item(ItemType::EightI, -2, 5,720, 0),
        Item(ItemType::EightI, -2, 5,700, 1),
        Item(ItemType::EightI, -2, 4,810, 2),
        Item(ItemType::EightI, -1, 2,780, 3),
        Item(ItemType::EightI,  1, 0,850, 4),
        Item(ItemType::EightI,  2,-1,990, 5),
        Item(ItemType::EightI,  3,-5,920, 6),
        
        Item(ItemType::SevenI, -2, 5,720, 0),
        Item(ItemType::SevenI, -4, 6,700, 1),
        Item(ItemType::SevenI, -2, 4,810, 2),
        Item(ItemType::SevenI,  0, 1,780, 3),
        Item(ItemType::SevenI,  1, 0,850, 4),
        Item(ItemType::SevenI,  3,-3,990, 5),
        Item(ItemType::SevenI,  5,-6,920, 6),
        
        Item(ItemType::SixI, -3, 6,720, 0),
        Item(ItemType::SixI, -3, 4,700, 1),
        Item(ItemType::SixI, -2, 3,810, 2),
        Item(ItemType::SixI, -2, 1,780, 3),
        Item(ItemType::SixI,  0,-1,850, 4),
        Item(ItemType::SixI,  1,-3,990, 5),
        Item(ItemType::SixI,  4,-4,920, 6),
        
        Item(ItemType::FiveI, -2, 5,720, 0),
        Item(ItemType::FiveI, -2, 3,700, 1),
        Item(ItemType::FiveI, -3, 4,810, 2),
        Item(ItemType::FiveI, -1, 2,780, 3),
        Item(ItemType::FiveI,  0, 1,850, 4),
        Item(ItemType::FiveI,  1,-2,990, 5),
        Item(ItemType::FiveI,  5,-4,920, 6),
        
        Item(ItemType::FourI, -2, 4,720, 0),
        Item(ItemType::FourI, -3, 5,700, 1),
        Item(ItemType::FourI, -1, 2,810, 2),
        Item(ItemType::FourI, -1, 0,780, 3),
        Item(ItemType::FourI,  2,-1,850, 4),
        Item(ItemType::FourI,  3,-3,990, 5),
        Item(ItemType::FourI,  6,-5,920, 6),
        
        //wedges stat1 increase flop height
        //stat2 top and back spin have more effect
        Item(ItemType::PitchWedge, 2,0,400, 0),
        Item(ItemType::PitchWedge, 2,1,510, 1),
        Item(ItemType::PitchWedge, 2,2,580, 2),
        Item(ItemType::PitchWedge, 1,2,610, 3),
        Item(ItemType::PitchWedge, 0,2,705, 4),
        Item(ItemType::PitchWedge, 1,3,800, 5),
        Item(ItemType::PitchWedge, 0,3,890, 6),
        
        Item(ItemType::GapWedge, 2,0,410, 0),
        Item(ItemType::GapWedge, 1,0,510, 1),
        Item(ItemType::GapWedge, 2,1,590, 2),
        Item(ItemType::GapWedge, 1,1,600, 3),
        Item(ItemType::GapWedge, 1,2,710, 4),
        Item(ItemType::GapWedge, 0,2,790, 5),
        Item(ItemType::GapWedge, 0,3,905, 6),
        
        Item(ItemType::SandWedge,  2,0,405, 0),
        Item(ItemType::SandWedge,  1,0,500, 1),
        Item(ItemType::SandWedge,  0,1,590, 2),
        Item(ItemType::SandWedge,  1,1,605, 3),
        Item(ItemType::SandWedge,  1,2,700, 4),
        Item(ItemType::SandWedge,  0,2,800, 5),
        Item(ItemType::SandWedge, -1,1,900, 6),
        
        //reduced effect from hook/slice
        Item(ItemType::Ball, 1,0,200, 7),
        Item(ItemType::Ball, 2,0,220, 8),
        Item(ItemType::Ball, 4,0,350, 9),
        Item(ItemType::Ball, 5,0,400, 10),
        Item(ItemType::Ball, 7,0,490, 11),
    };
}