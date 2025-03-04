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
        std::int32_t type = ItemType::Ball;

        std::int32_t stat01 = 0; //+/- 10 with 0 as default
        std::int32_t stat02 = 0;
        std::int32_t price = 0;

        std::int32_t manufacturer = 0; //indexes into the names array

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
        static constexpr std::uint32_t MaxLoadouts = 12;
        std::uint32_t loadoutCount = 4; //number of unlocked loadouts

        std::array<Loadout, MaxLoadouts> loadouts = {};
        std::array<std::int32_t, MaxItems> inventory = {}; //uids of shop-bought items

        std::int32_t balance = 100;

        Inventory()
        {
            std::fill(inventory.begin(), inventory.end(), -1);
        }

        bool read() {}
        bool write() {}
    };

    static inline const std::array<std::string, 12> Manufacturers =
    {
        "Gallawent", "Dong", "FellowCraft", "Akrun", "Dannis", "Clix",
        "BeyTree", "Tunnelrock", "Flaxen", "Headings", "Woodgear",
        "Brilton & Stockley"
    };

    //total items available in the shop - TODO set stats/price
    static inline const std::array<Item, MaxItems> Items =
    {
        Item(ItemType::Driver, 0,0,0, 0),
        Item(ItemType::Driver, 0,0,0, 1),
        Item(ItemType::Driver, 0,0,0, 2),
        Item(ItemType::Driver, 0,0,0, 3),
        Item(ItemType::Driver, 0,0,0, 4),
        Item(ItemType::Driver, 0,0,0, 5),
        Item(ItemType::Driver, 0,0,0, 6),
        
        Item(ItemType::FiveW, 0,0,0, 0),
        Item(ItemType::FiveW, 0,0,0, 1),
        Item(ItemType::FiveW, 0,0,0, 2),
        Item(ItemType::FiveW, 0,0,0, 3),
        Item(ItemType::FiveW, 0,0,0, 4),
        Item(ItemType::FiveW, 0,0,0, 5),
        Item(ItemType::FiveW, 0,0,0, 6),
        
        Item(ItemType::ThreeW, 0,0,0, 0),
        Item(ItemType::ThreeW, 0,0,0, 1),
        Item(ItemType::ThreeW, 0,0,0, 2),
        Item(ItemType::ThreeW, 0,0,0, 3),
        Item(ItemType::ThreeW, 0,0,0, 4),
        Item(ItemType::ThreeW, 0,0,0, 5),
        Item(ItemType::ThreeW, 0,0,0, 6),
        
        Item(ItemType::NineI, 0,0,0, 0),
        Item(ItemType::NineI, 0,0,0, 1),
        Item(ItemType::NineI, 0,0,0, 2),
        Item(ItemType::NineI, 0,0,0, 3),
        Item(ItemType::NineI, 0,0,0, 4),
        Item(ItemType::NineI, 0,0,0, 5),
        Item(ItemType::NineI, 0,0,0, 6),
        
        Item(ItemType::EightI, 0,0,0, 0),
        Item(ItemType::EightI, 0,0,0, 1),
        Item(ItemType::EightI, 0,0,0, 2),
        Item(ItemType::EightI, 0,0,0, 3),
        Item(ItemType::EightI, 0,0,0, 4),
        Item(ItemType::EightI, 0,0,0, 5),
        Item(ItemType::EightI, 0,0,0, 6),
        
        Item(ItemType::SevenI, 0,0,0, 0),
        Item(ItemType::SevenI, 0,0,0, 1),
        Item(ItemType::SevenI, 0,0,0, 2),
        Item(ItemType::SevenI, 0,0,0, 3),
        Item(ItemType::SevenI, 0,0,0, 4),
        Item(ItemType::SevenI, 0,0,0, 5),
        Item(ItemType::SevenI, 0,0,0, 6),
        
        Item(ItemType::SixI, 0,0,0, 0),
        Item(ItemType::SixI, 0,0,0, 1),
        Item(ItemType::SixI, 0,0,0, 2),
        Item(ItemType::SixI, 0,0,0, 3),
        Item(ItemType::SixI, 0,0,0, 4),
        Item(ItemType::SixI, 0,0,0, 5),
        Item(ItemType::SixI, 0,0,0, 6),
        
        Item(ItemType::FiveI, 0,0,0, 0),
        Item(ItemType::FiveI, 0,0,0, 1),
        Item(ItemType::FiveI, 0,0,0, 2),
        Item(ItemType::FiveI, 0,0,0, 3),
        Item(ItemType::FiveI, 0,0,0, 4),
        Item(ItemType::FiveI, 0,0,0, 5),
        Item(ItemType::FiveI, 0,0,0, 6),
        
        Item(ItemType::FourI, 0,0,0, 0),
        Item(ItemType::FourI, 0,0,0, 1),
        Item(ItemType::FourI, 0,0,0, 2),
        Item(ItemType::FourI, 0,0,0, 3),
        Item(ItemType::FourI, 0,0,0, 4),
        Item(ItemType::FourI, 0,0,0, 5),
        Item(ItemType::FourI, 0,0,0, 6),
        
        Item(ItemType::PitchWedge, 0,0,0, 0),
        Item(ItemType::PitchWedge, 0,0,0, 1),
        Item(ItemType::PitchWedge, 0,0,0, 2),
        Item(ItemType::PitchWedge, 0,0,0, 3),
        Item(ItemType::PitchWedge, 0,0,0, 4),
        Item(ItemType::PitchWedge, 0,0,0, 5),
        Item(ItemType::PitchWedge, 0,0,0, 6),
        
        Item(ItemType::GapWedge, 0,0,0, 0),
        Item(ItemType::GapWedge, 0,0,0, 1),
        Item(ItemType::GapWedge, 0,0,0, 2),
        Item(ItemType::GapWedge, 0,0,0, 3),
        Item(ItemType::GapWedge, 0,0,0, 4),
        Item(ItemType::GapWedge, 0,0,0, 5),
        Item(ItemType::GapWedge, 0,0,0, 6),
        
        Item(ItemType::SandWedge, 0,0,0, 0),
        Item(ItemType::SandWedge, 0,0,0, 1),
        Item(ItemType::SandWedge, 0,0,0, 2),
        Item(ItemType::SandWedge, 0,0,0, 3),
        Item(ItemType::SandWedge, 0,0,0, 4),
        Item(ItemType::SandWedge, 0,0,0, 5),
        Item(ItemType::SandWedge, 0,0,0, 6),
        
        Item(ItemType::Ball, 0,0,0, 7),
        Item(ItemType::Ball, 0,0,0, 8),
        Item(ItemType::Ball, 0,0,0, 9),
        Item(ItemType::Ball, 0,0,0, 10),
        Item(ItemType::Ball, 0,0,0, 11),
    };
}