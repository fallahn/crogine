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

#include <Content.hpp>

#include <crogine/detail/Types.hpp>

#include <cstdint>
#include <array>
#include <string>

namespace inv
{
    enum ItemType
    {
        Driver, ThreeW, FiveW, NineI, EightI, SevenI,
        SixI, FiveI, FourI, PitchWedge, GapWedge,
        SandWedge, Ball,

        Count
    };

    static inline const std::array<std::string, ItemType::Count> ItemStrings =
    {
        "Driver", "3 Wood", "5 Wood", "9 Iron", "8 Iron", "7 Iron",
        "6 Iron", "5 Iron", "4 Iron", "Pitch Wedge", "Gap Wedge",
        "Sand Wedge", "Pack Of 25 Balls",
    };

    struct StatLabel final
    {
        std::string stat0;
        std::string stat1;

        StatLabel(const std::string& s0, const std::string& s1)
            :stat0(s0), stat1(s1) { }
    };

    //5 categories in the shop / types of item
    static inline const std::array<StatLabel, 5u> StatLabels =
    {
        StatLabel("Accuracy: ","Distance: "),
        StatLabel("Accuracy: ","Distance: "),
        StatLabel("Spin Influence: ","Punch Distance: "),
        StatLabel("Flop Height: ","Backspin: "),
        StatLabel("Hook/Slice Reduction: ","")
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
        std::array<std::uint8_t, MaxCharBytes + 1> name = {}; //utf8 string

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
        std::uint32_t loadoutCount = 2; //number of unlocked loadouts (unused)

        std::array<Loadout, MaxLoadouts> loadouts = {}; //unused - implement this if people like the idea of saving loadouts
        std::array<std::int32_t, MaxItems> inventory = {}; //indices of shop-bought items

        std::int32_t balance = 5000;

        std::int32_t manufacturerFlags = 0;

        //in case of expansion in the future.
        /*const */std::array<std::int32_t, 32> Reserved = {};

        Inventory()
        {
            std::fill(inventory.begin(), inventory.end(), -1);
            std::fill(Reserved.begin(), Reserved.end(), 0);
        }
    };

    static inline bool read(Inventory& dst)
    {
        const std::string fileName("equip.inv");
        const std::string filePath = Content::getBaseContentPath() + fileName;

        cro::RaiiRWops file;
        file.file = SDL_RWFromFile(filePath.c_str(), "rb");
        if (file.file)
        {
            if (SDL_RWread(file.file, &dst, sizeof(Inventory), 1) == 0)
            {
                LogE << "Failed reading " << filePath << ", " << SDL_GetError() << std::endl;
                return false;
            }

            dst.balance = std::min(999999, dst.balance);
            return true;
        }

        return false;
    }
    static inline bool write(const Inventory& src)
    {
        const std::string fileName("equip.inv");
        const std::string filePath = Content::getBaseContentPath() + fileName;

        cro::RaiiRWops file;
        file.file = SDL_RWFromFile(filePath.c_str(), "wb");
        if (file.file)
        {
            if (SDL_RWwrite(file.file, &src, sizeof(Inventory), 1) < 1)
            {
                LogE << "Failed writing " << filePath << ", " << SDL_GetError() << std::endl;
                return false;
            }
            return true;
        }

        return false;
    }

    struct ManufID final
    {
        enum
        {
            Gallawent, Dong, Fellowcraft, Akrun, Dannis, Clix,
            BeyTree, TunnelRock, Flaxen, Hardings, Woodgear,
            BriltonStockley, Default,

            Count
        };
    };

    static inline const std::array<std::string, ManufID::Count> Manufacturers =
    {
        "Gallawent", "Dong", "FellowCraft", "Akrun", "Dannis", "Clix",
        "BeyTree", "Tunnelrock", "Flaxen", "Hardings", "Woodgear",
        "Brilton & Stockley",

        "Default"
    };

    //total items available in the shop
    static inline const std::array<Item, MaxItems> Items =
    {
        //type, stat 1, stat 2, price, manufacturer ID
        
        //driver-wood stat 1 accuracy
        //stat 2 distance
        Item(ItemType::Driver, 8,-6,2300, 0),//8,-6
        Item(ItemType::Driver, 5,-3,2400, 1),//5,-3
        Item(ItemType::Driver, 3,-1,2500, 2),//3,-1
        Item(ItemType::Driver, 1, 0,2550, 3),
        Item(ItemType::Driver, -1,2,2600, 4),//-1,2
        Item(ItemType::Driver, -2,3,2750, 5),//-2,3
        Item(ItemType::Driver, -4,4,2800, 6),//-4,4

        Item(ItemType::ThreeW, 7,-6,2050, 0),//7,-6
        Item(ItemType::ThreeW, 3,-4,2140, 1),//3,-4
        Item(ItemType::ThreeW, 4,-3,2200, 2),//4,-3
        Item(ItemType::ThreeW, 2,-1,2350, 3),
        Item(ItemType::ThreeW, -1,2,2540, 4),//-1,2
        Item(ItemType::ThreeW, -3,4,2600, 5),//-3,4
        Item(ItemType::ThreeW, -2,3,2650, 6),//-2,3

        Item(ItemType::FiveW,  4,-1,2000, 0),//4,-1
        Item(ItemType::FiveW,  5,-2,2100, 1),//5,-2
        Item(ItemType::FiveW,  3,-1,2250, 2),//3,-1
        Item(ItemType::FiveW,  2,-1,2280, 3),
        Item(ItemType::FiveW,  0, 1,2400, 4),//0,1
        Item(ItemType::FiveW,  -2,3,2550, 5),//-2,3
        Item(ItemType::FiveW,  -3,3,2700, 6),//-3,3
        
        //irons stat1 more accuracy,
        //stat2 spin effect - pos more spin
        Item(ItemType::NineI,  3,-4,720, 0),//3,-4
        Item(ItemType::NineI,  2,-3,700, 1),//2,-3
        Item(ItemType::NineI,  1,-1,810, 2),//1,-1
        Item(ItemType::NineI,  0, 1,780, 3),
        Item(ItemType::NineI,  -1,3,850, 4),//-1,3
        Item(ItemType::NineI,  -2,5,990, 5),//-2,5
        Item(ItemType::NineI,  -2,6,920, 6),//-2,6
        
        Item(ItemType::EightI, 3, -5,720, 0),//3,-5
        Item(ItemType::EightI, 2, -1,700, 1),//2,-1
        Item(ItemType::EightI, 1,  0,810, 2),//1,0
        Item(ItemType::EightI, -1, 2,780, 3),
        Item(ItemType::EightI, -2, 4,850, 4),//-2,4
        Item(ItemType::EightI, -2, 5,990, 5),//-2,5
        Item(ItemType::EightI, -1, 5,920, 6),//-1,5
        
        Item(ItemType::SevenI,  5,-6,720, 0),//5,-6
        Item(ItemType::SevenI,  3,-3,700, 1),//3,-3
        Item(ItemType::SevenI,  1, 0,810, 2),//1,0
        Item(ItemType::SevenI,  0, 1,780, 3),
        Item(ItemType::SevenI,  -2,4,850, 4),//-2,4
        Item(ItemType::SevenI,  -4,6,990, 5),//-4,6
        Item(ItemType::SevenI,  -2,5,920, 6),//-2,5
        
        Item(ItemType::SixI,  4,-4,720, 0),//4,-4
        Item(ItemType::SixI,  1,-3,700, 1),//1,-3
        Item(ItemType::SixI,  0,-1,810, 2),//0,-1
        Item(ItemType::SixI,  -2,1,780, 3),
        Item(ItemType::SixI,  -2,3,850, 4),//-2,3
        Item(ItemType::SixI,  -3,4,990, 5),//-3,4
        Item(ItemType::SixI,  -3,6,920, 6),//-3,6
        
        Item(ItemType::FiveI,  5,-4,720, 0),//5,-4
        Item(ItemType::FiveI,  1,-2,700, 1),//1,-2
        Item(ItemType::FiveI,  0, 1,810, 2),//0,1
        Item(ItemType::FiveI,  -1,2,780, 3),
        Item(ItemType::FiveI,  -3,4,850, 4),//-3,4
        Item(ItemType::FiveI,  -2,3,990, 5),//-2,3
        Item(ItemType::FiveI,  -2,5,920, 6),//-2,5
        
        Item(ItemType::FourI,  6,-5,720, 0),//6,-5
        Item(ItemType::FourI,  3,-3,700, 1),//3,-3
        Item(ItemType::FourI,  2,-1,810, 2),//2,-1
        Item(ItemType::FourI, -1, 0,780, 3),
        Item(ItemType::FourI,  -1,2,850, 4),//-1,2
        Item(ItemType::FourI,  -3,5,990, 5),//-3,5
        Item(ItemType::FourI,  -2,4,920, 6),//-2,4
        
        //wedges stat1 more accuracy
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